/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <time.h>
#include <errno.h>
#include <mhash.h>
#include <mcrypt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/time.h>

#include "common/common.h"

#define _VSTEG_S_
#include "src/vstegfs.h"
#include "src/dir.h"

static vlist_t **known_files;
static uint8_t *known_blocks;

extern void vstegfs_init(int64_t fs, bool do_cache)
{
    rng_seed();
    /*
     * check the first block to see if everything looks okay
     */
    vblock_t sb;

    lseek(fs, 0, SEEK_SET);
    if (read(fs, &sb, sizeof( vblock_t )) != sizeof( vblock_t ))
        msg(strerror(errno));

    if ((sb.hash[0] != MAGIC_0) || (sb.hash[1] != MAGIC_1) || (sb.hash[2] != MAGIC_2))
    {
        msg(_("magic number failure in superblock for %s"), fs);
        die(_("use mkvstegfs to restore superblock"));
    }
    /*
     * if asked, keep a list of files we know about
     */
    if (do_cache)
    {
        known_files = calloc(1, sizeof( vlist_t * ));
        known_files[0] = NULL;

        uint64_t tblocks = 0x0;
        memmove(&tblocks, &sb.next, SB_NEXT);
        known_blocks = calloc(tblocks / CHAR_BIT, sizeof( uint8_t ));
    }
    else
    {
        known_files = NULL;
        known_blocks = NULL;
    }
    return;
}

extern int64_t vstegfs_save(vstat_t f)
{
    /*
     * some initial preparations - such as: is the file larger than the
     * file system? because that wouldn't be good :s
     */
    uint64_t fs_size = lseek(f.fs, 0, SEEK_END);
    if (MAX_COPIES * *f.size > fs_size * 5 / 8)
        return EFBIG;
    uint64_t fs_blocks = fs_size / SB_BLOCK;
    rng_seed();
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    uint64_t path[SL_PATH] = { 0x0 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memmove(path, ph, SB_PATH); /* we only need the 1st 128 bits ATM */
        free(ph);
    }
    uint64_t start[MAX_COPIES] = { 0x0 };
    /*
     * work through the file to be hidden (MAX_COPIES) times
     */
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        /*
         * find somewhere to start this copy of the file
         */
        if (!(start[i] = calc_next_block(f.fs, f.path)))
            return ENOSPC;
        /*
         * initialise the mcrypt library routines, and create a new IV
         * (based on the filename, password and copy number)
         */
        MCRYPT c = vstegfs_crypt_init(&f, i);
        /*
         * make sure we're at the beginning of the file, our data buffer
         * is clean and we're looking at the correct starting block on
         * the file system
         */
        rewind(f.file);
        uint8_t data[SB_DATA] = { 0x00 };
        uint64_t next = start[i];
        /*
         * while we keep reading data, keep encrypting and storing it
         */
        errno = EXIT_SUCCESS;
        /*
         * check if we have more than a full block of data, if we don't
         * then handle that as a special case
         */
        while (fread(data, sizeof( uint8_t ), SB_DATA, f.file) > 0)
        {
            vblock_t b;
            uint64_t this = next;
            if (!(next = calc_next_block(f.fs, f.path)))
                return ENOSPC;
            /*
             * build the block; this includes putting in the data,
             * calculating a hash of the data and making a note of the
             * next block location
             */
            memmove(b.path,  path, SB_PATH);
            memmove(b.data,  data, SB_DATA);
            memset(b.hash,  0x00, SB_HASH);
            memmove(b.next, &next, SB_NEXT);
            /*
             * save the block - if there is a problem, give up with IO
             * error
             */
            if (vstegfs_block_save(f.fs, this, c, &b))
                return EIO;
            /*
             * get ready for the next chunk of data...
             */
            memset(data, 0x00, SB_DATA);
        }
        mcrypt_generic_deinit(c);
        mcrypt_module_close(c);
    }
    /*
     * finalise header blocks
     */
    uint16_t header[MAX_COPIES] = { 0x0 };
    {
        /*
         * this easily (but uniquely) determines where header blocks
         * should be located
         */
        char *fp = NULL;
        if (asprintf(&fp, "%s/%s:%s", f.path, f.name, f.pass ?: ROOT_PATH) < 0)
            die(_("out of memory at %s:%i"), __FILE__, __LINE__);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memmove(header, ph, sizeof( uint16_t ) * MAX_COPIES);
        free(ph);
        free(fp);
    }
    /*
     * construct the header block, then save it in each of its
     * locations
     */
    {
        vblock_t b;
        memmove(b.path, path, SB_PATH);
        memset(b.data, 0x00, SB_DATA);
        uint8_t i = 0;
        for (i = 0; i < MAX_COPIES; i++)
            memmove(b.data + i * sizeof( uint64_t ), &start[i], sizeof( uint64_t ));
        memmove(b.data + i * sizeof( uint64_t ), f.time, sizeof( time_t ));
        memmove(b.next, f.size, SB_NEXT);
        for (uint8_t i = 0; i < MAX_COPIES; i++)
        {
            /*
             * each header block is encrypted independently
             */
            uint64_t head = 0x0;
            for (int8_t j = (sizeof( uint64_t ) / sizeof( uint16_t )); j >= 0; --j)
                head = (head << 0x10) | header[i + j];
            head %= fs_blocks;

            /*
             * write this copy of the header
             */
            MCRYPT c = vstegfs_crypt_init(&f, i);
            if (vstegfs_block_save(f.fs, head, c, &b))
                return EIO;
            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
    }
    /*
     * add file to list of known filenames
     */
    add_known_list(f);
    /*
     * success :D
     */
    return EXIT_SUCCESS;
}

extern int64_t vstegfs_open(vstat_t f)
{
    uint64_t path[SL_PATH] = { 0x0 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memmove(path, ph, SB_PATH);
        free(ph);
    }
    /*
     * find a valid header block
     */
    vblock_t hb;
    vstegfs_header(&f, &hb);
    /*
     * if we don't know the size of the file then we didn't find an
     * uncorrupted header - give up
     */
    if (!*f.size)
        return ENODATA;
    uint64_t start[MAX_COPIES] = { 0x0 };
    memmove(start, hb.data, sizeof( uint64_t ) * MAX_COPIES);

    /*
     * try each of the starting blocks and see if we can
     * find a full copy of the file
     */
    uint64_t total = 0x0;
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        msg(_("looking at copy %i"), i + 1);
        MCRYPT c = vstegfs_crypt_init(&f, i);
        rewind(f.file); /* back to the beginning */
        uint64_t next = start[i];
        uint64_t bytes = 0x0;
        /*
         * now we're ready to read this copy of the file
         */
        while (is_block_ours(f.fs, next, path))
        {
            vblock_t b;
            if (block_open(f.fs, next, c, &b))
                break;
            /*
             * woo - we now have successfully decrypted another block
             * for this file
             */
            bytes += SB_DATA;
            uint8_t need = SB_DATA;
            if (bytes > *f.size)
                /* that was the final block, but we don't need all of it */
                need = SB_DATA - (bytes - *f.size);
            fwrite (&b.data, sizeof( uint8_t ), need, f.file);

            if (known_blocks)
            {
                /*
                 * now we know this block is being used
                 */
                lldiv_t a = lldiv(next, CHAR_BIT);
                known_blocks[a.quot] = known_blocks[a.quot] | (0x01 << a.rem);
            }

            if (bytes >= *f.size)
            {
                add_known_list(f); /* we now know this files exists :) */
                return EXIT_SUCCESS; /* done */
            }

            memmove(&next, b.next, SB_NEXT);
        }
        if (bytes > total)
            total = bytes;
        mcrypt_generic_deinit(c);
        mcrypt_module_close(c);
    }
    /*
     * if we make it this far something has gone wrong
     */
    *f.size = total;
    msg(_("no complete copy of file found; recovered %lu bytes"), *f.size);
    return EDIED;
}

extern char **vstegfs_known_list(const char *path)
{
    if (!known_files)
        return NULL;
    char **list = calloc(1, sizeof( char * ));
    list[0] = NULL;
    uint32_t i = 0, j = 0;
    while (known_files[j])
    {
        if (!strcmp(path, known_files[j]->path))
        {
            list[i] = strdup(known_files[j]->name);
            list = realloc(list, (++i + 1) * sizeof( char * ));
            list[i] = NULL;
        }
        j++;
    }
    return list;
}

static void add_known_list(vstat_t f)
{
    if (!known_files)
        return;
    uint32_t i = 0;
    while(true)
    {
        if (!known_files[i])
        {
            /*
             * we're at the end of list dir list and didnt find it, so
             * add a new entry, and we're done
             */
            known_files[i] = calloc(1, sizeof( vlist_t ));
            known_files[i]->name = strdup(f.name);
            known_files[i]->path = strdup(f.path);
            known_files = realloc(known_files, (++i + 1) * sizeof( char * ));
            known_files[i] = NULL;
            break;
        }
        if (!strcmp(known_files[i]->name, f.name) && !strcmp(known_files[i]->path, f.path))
            break; /* found our file :) */
        i++;
    }
    return;
}

static void del_known_list(vstat_t f)
{
    if (!known_files)
        return;
    /*
     * find out how many files we have in our list
     */
    uint32_t c = 0;
    while (known_files[c++]);

    vlist_t **new = calloc(c - 1, sizeof( vlist_t * ));
    for (uint32_t i = 0, j = 0; i < c - 1; i++)
    {
        if (strcmp(f.path, known_files[i]->path) || strcmp(f.name, known_files[i]->name))
        {
            /*
             * copy all non-deleted files to the updated list
             */
            new[j] = calloc(1, sizeof( vlist_t ));
            new[j]->name = strdup(known_files[i]->name);
            new[j]->path = strdup(known_files[i]->path);
            j++;
        }
        /*
         * free up existing list
         */
        free(known_files[i]->name);
        free(known_files[i]->path);
        free(known_files[i]);
    }
    free(known_files);
    known_files = new;
    return;
}

extern uint64_t vstegfs_find(vstat_t f)
{
    vblock_t b;
    return vstegfs_header(&f, &b);
}

extern void vstegfs_kill(vstat_t f)
{
    /*
     * calculate the locations of the header blocks
     */
    uint16_t header[MAX_COPIES] = { 0x0 };
    {
        char *fp = NULL;
        if (asprintf(&fp, "%s/%s:%s", f.path, f.name, f.pass ?: ROOT_PATH) < 0)
            die(_("out of memory at %s:%i"), __FILE__, __LINE__);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memmove(header, ph, sizeof( uint16_t ) * MAX_COPIES);
        free(ph);
        free(fp);
    }
    uint64_t path[SL_PATH] = { 0x0 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memmove(path, ph, SB_PATH);
        free(ph);
    }
    uint64_t fs_blocks = lseek(f.fs, 0, SEEK_END) / SB_BLOCK;
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        uint64_t head = 0x0;
        for (int8_t j = (sizeof( uint64_t ) / sizeof( uint16_t )); j >= 0; --j)
            head = (head << 0x10) | header[i + j];
        head %= fs_blocks;
        if (is_block_ours(f.fs, head, path))
        {
            /*
             * one of our header blocks, thus it needs randomising
             */
            vblock_t b;
            MCRYPT c = vstegfs_crypt_init(&f, i);

            uint8_t path[SB_PATH];
            uint8_t data[SB_DATA];
            uint8_t next[SB_NEXT];

            for (uint8_t j = 0; j < SB_PATH; j++)
                path[j] = mrand48();
            for (uint8_t j = 0; j < SB_DATA; j++)
                data[j] = mrand48();
            for (uint8_t j = 0; j < SB_NEXT; j++)
                next[j] = mrand48();

            memmove(b.path, path, SB_PATH);
            memmove(b.data, data, SB_DATA);
            memmove(b.next, next, SB_NEXT);

            vstegfs_block_save(f.fs, head, c, &b);

            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
    }
    del_known_list(f);
    return;
}

static uint64_t vstegfs_header(vstat_t *f, vblock_t *b)
{
    /*
     * find the header blocks for the file
     */
    uint16_t header[MAX_COPIES] = { 0x0 };
    {
        char *fp = NULL;
        if (asprintf(&fp, "%s/%s:%s", f->path, f->name, f->pass ?: ROOT_PATH) < 0)
            die(_("out of memory at %s:%i"), __FILE__, __LINE__);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memmove(header, ph, sizeof( uint16_t ) * MAX_COPIES);
        free(ph);
        free(fp);
    }
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    uint64_t path[SL_PATH] = { 0x0 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f->path, strlen(f->path));
        uint8_t *ph = mhash_end(h);
        memmove(path, ph, SB_PATH);
        free(ph);
    }
    uint64_t fs_blocks = lseek(f->fs, 0, SEEK_END) / SB_BLOCK;
    uint64_t f_size = 0x0;
    /*
     * try to find a header
     */
    uint64_t head = 0x0;
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        for (int8_t j = (sizeof( uint64_t ) / sizeof( uint16_t )); j >= 0; --j)
            head = (head << 0x10) | header[i + j];
        head %= fs_blocks;
        if (is_block_ours(f->fs, head, path))
        {
            /*
             * found a header block which might be ours, if we can read
             * its contents we're in business
             */
            MCRYPT c = vstegfs_crypt_init(f, i);
            if (!block_open(f->fs, head, c, b))
            {
                /*
                 * if we're here we were able to successfully open the
                 * header
                 */
                memmove(&f_size, b->next, SB_NEXT);
                if (f->time)
                {
                    uint64_t data[SL_DATA] = { 0x0 };
                    memmove(data, b->data, SB_DATA);
                    memmove(f->time, &data[SL_DATA - 1], sizeof( time_t ));
                }
            }
            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
        if (f_size)
            break;
    }
    *f->size = f_size;
    return head;
}

static MCRYPT vstegfs_crypt_init(vstat_t *f, uint8_t ivi)
{
    MCRYPT c = mcrypt_module_open(MCRYPT_SERPENT, NULL, MCRYPT_CBC, NULL);
    uint8_t key[SB_TIGER] = { 0x00 };
    {
        char *fk = NULL;
        if (asprintf(&fk, "%s:%s", f->name, f->pass ?: ROOT_PATH) < 0)
            die(_("out of memory at %s:%i"), __FILE__, __LINE__);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fk, strlen(fk));
        uint8_t *ph = mhash_end(h);
        memmove(key, ph, SB_TIGER);
        free(ph);
        free(fk);
    }
    uint8_t ivs[SB_SERPENT] = { 0x00 };
    {
        char *iv_s = NULL;
        if (asprintf(&iv_s, "%s+%i", f->name, ivi) < 0)
            die(_("out of memory at %s:%i"), __FILE__, __LINE__);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, iv_s, strlen(iv_s));
        uint8_t *ph = mhash_end(h);
        memmove(ivs, ph, SB_SERPENT);
        free(ph);
        free(iv_s);
    }
    mcrypt_generic_init(c, key, sizeof( key ), ivs);
    return c;
}

static int64_t vstegfs_block_save(uint64_t fs, uint64_t pos, MCRYPT c, vblock_t *b)
{
    errno = EXIT_SUCCESS;
    /*
     * calculate the hash of the data
     */
    {
        uint8_t data[SB_DATA] = { 0x00 };
        memmove(data, (uint8_t *)b->data, SB_DATA);

        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, data, SB_DATA);
        uint8_t *ph = mhash_end(h);

        memmove((uint8_t *)b->hash, ph, SB_HASH);
        free(ph);
    }
    uint8_t d[SB_SERPENT * 7] = { 0x00 };
    memmove(d, ((uint8_t *)b) + SB_SERPENT, SB_BLOCK - SB_PATH);
    mcrypt_generic(c, d, sizeof( d ));
    memmove(((uint8_t *)b) + SB_SERPENT, d, SB_BLOCK - SB_PATH);
    /*
     * write the encrypted block to this block location
     */
    if (pwrite(fs, b, sizeof( vblock_t ), pos * SB_BLOCK) != sizeof( vblock_t ))
        msg(strerror(errno));
    return errno;
}

static int64_t block_open(uint64_t fs, uint64_t pos, MCRYPT c, vblock_t *b)
{
    int64_t err = EXIT_SUCCESS;
    if (pread(fs, b, sizeof( vblock_t ), pos * SB_BLOCK) != sizeof( vblock_t ))
        msg(strerror(errno));
    uint8_t d[SB_SERPENT * 7] = { 0x00 };
    memmove(d, ((uint8_t *)b) + SB_SERPENT, SB_BLOCK - SB_PATH);
    mdecrypt_generic(c, d, sizeof( d ));
    memmove(((uint8_t *)b) + SB_SERPENT, d, SB_BLOCK - SB_PATH);
    /*
     * check the hash of the decrypted data
     */
    uint8_t hash[SB_HASH] = { 0x00 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, b->data, SB_DATA);
        uint8_t *ph = mhash_end(h);
        memmove(hash, ph, SB_HASH);
        free(ph);
    }
    if (memcmp(hash, b->hash, SB_HASH))
        return EXIT_FAILURE;
    return err;
}

static bool is_block_ours(uint64_t fs, uint64_t pos, uint64_t *hash)
{
    uint8_t buf[SB_PATH] = { 0x00 };
    if (pread(fs, buf, SB_PATH, pos * SB_BLOCK) < 0)
        return true; /* we screwed up, so just assume it one of ours :p */
    if (memcmp(buf, hash, SB_PATH))
        return false; /* they're different, not along our path */
    return true;
}

static uint64_t calc_next_block(uint64_t fs, char *path)
{
    uint64_t fsb = lseek(fs, 0, SEEK_END) / SB_BLOCK;
    uint64_t block = 0x0;
    int16_t att = 0; /* give us up to 32767 attempts to find a block */
    bool found = false;
    /*
     * TODO wite a comment :p
     */
    uint16_t depth = dir_get_deep(path);
    do
    {
        rng_seed();
        block = (((uint64_t)mrand48() << 0x20) | mrand48()) % fsb;

        /*
         * check the bitmap for this block
         */
        if (known_blocks)
        {
            lldiv_t a = lldiv(block, CHAR_BIT);
            uint8_t b = known_blocks[a.quot] & (0x01 << a.rem);
            if (b) /* try again */
                continue;
            /*
             * block either was used but we didn't know about it, or
             * it's about to be, add to bitmap
             */
            known_blocks[a.quot] |= (0x01 << a.rem);
        }

        bool used = false;
        char *cwd = NULL;
        for (uint16_t i = 0; i < depth; i++)
        {
            char *p = dir_get_part(path, i);
            if (asprintf(&cwd, "%s/%s%s", cwd ? cwd : "", cwd ? "" : "\b", p) < 0)
                die(_("out of memory at %s:%i"), __FILE__, __LINE__);

            uint64_t hash[SL_PATH] = { 0x0 };
            {
                MHASH h = mhash_init(MHASH_TIGER);
                mhash(h, cwd, strlen(cwd));
                uint8_t *ph = mhash_end(h);
                memmove(hash, ph, SB_PATH);
                free(ph);
            }
            if (is_block_ours(fs, block, hash))
            { /* this block could be ours, so lets just give up now :p */
                used = true;
                break;
            }
        }
        if (!used)
            found = true;
        else
            if (++att < 0)
                break;
        free(cwd);
    }
    while (!found);

    return found ? block : 0;
}

void rng_seed(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t t = now.tv_sec * ONE_MILLION + now.tv_usec;
    uint16_t s[SS_48B] = { 0x0 };

    MHASH h = mhash_init(MHASH_TIGER);
    mhash(h, &t, sizeof( uint64_t ));
    uint8_t *ph = mhash_end(h);
    memmove(s, ph, SS_48B * sizeof( uint16_t ));
    free(ph);

    seed48(s);
    return;
}
