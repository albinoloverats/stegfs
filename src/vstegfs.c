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

#include "common/common.h"

#define _VSTEG_S_
#include "src/vstegfs.h"

extern int64_t vstegfs_save(vstat_t f)
{
    /*
     * some initial preparations - such as: is the file larger than the
     * file system? because that wouldn't be good :s
     */
    uint64_t f_size = (fseek(f.file, 0, SEEK_END) ? 0 : ftell(f.file));
    uint64_t fs_size = lseek(f.fs, 0, SEEK_END);
    if (MAX_COPIES * f_size > fs_size * 5 / 8)
        return EFBIG;
    uint64_t fs_blocks = fs_size / SB_BLOCK;
    srand48(time(0));
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    uint64_t path[2];
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memcpy(path, ph, SB_PATH); /* we only need the 1st 128 bits ATM */
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
            memcpy(b.path,  path, SB_PATH);
            memcpy(b.data,  data, SB_DATA);
            memset(b.hash,  0x00, SB_HASH);
            memcpy(b.next, &next, SB_NEXT);
            /*
             * save the block - if there is a problem, give up with IO
             * error
             */
            hex((uint8_t *)&b, SB_BLOCK);
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
        asprintf(&fp, "%s/%s:%s", f.path, f.name, f.pass ?: ROOT_PATH);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memcpy(header, ph, SB_HASH);
        free(ph);
        free(fp);
    }
    /*
     * construct the header block, then save it in each of its
     * locations
     */
    {
        vblock_t b;
        memcpy(b.path, path, SB_PATH);
        memset(b.data, 0x00, SB_DATA);
        for (uint8_t i = 0; i < MAX_COPIES; i++)
            memcpy(b.data + i * sizeof( uint64_t ), &start[i], sizeof( uint64_t ));
        for (uint8_t i = 1; i <= sizeof( uint64_t ); i++)
            b.data[SB_DATA - i] = mrand48();
        memcpy(b.next, &f_size, sizeof( uint64_t ));
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
     * success :D
     */
    return EXIT_SUCCESS;
}

extern int64_t vstegfs_open(vstat_t f)
{
    uint64_t path[2];
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memcpy(path, ph, SB_PATH);
        free(ph);
    }
    /*
     * find a valid header block
     */
    vblock_t hb;
    uint64_t f_size = vstegfs_header(&f, &hb);
    /*
     * if we don't know the size of the file then we didn't find an
     * uncorrupted header - give up
     */
    if (!f_size)
        return ENOENT;
    uint64_t start[MAX_COPIES] = { 0x0 };
    memcpy(start, hb.data, sizeof( uint64_t ) * MAX_COPIES);

    /*
     * try each of the starting blocks and see if we can
     * find a full copy of the file
     */
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
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
            {
                hex((uint8_t *)&b, SB_BLOCK);
                break;
            }
            hex((uint8_t *)&b, SB_BLOCK);
            /*
             * woo - we now have successfully decrypted another block
             * for this file
             */
            bytes += SB_DATA;
            uint8_t need = SB_DATA;
            if (bytes > f_size)
                /* that was the final block, but we don't need all of it */
                need = SB_DATA - (bytes - f_size);
            fwrite (&b.data, sizeof( uint8_t ), need, f.file);

            if (bytes >= f_size)
                return EXIT_SUCCESS; /* done */

            memcpy(&next, b.next, SB_NEXT);
        }
        mcrypt_generic_deinit(c);
        mcrypt_module_close(c);
    }
    /*
     * if we make it this far something has gone wrong
     */
    return ENOENT;
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
        asprintf(&fp, "%s/%s:%s", f.path, f.name, f.pass ?: ROOT_PATH);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memcpy(header, ph, SB_HASH);
        free(ph);
        free(fp);
    }
    uint64_t path[2];
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f.path, strlen(f.path));
        uint8_t *ph = mhash_end(h);
        memcpy(path, ph, SB_PATH);
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

            memcpy(b.path, path, SB_PATH);
            memcpy(b.data, data, SB_DATA);
            memcpy(b.next, next, SB_NEXT);

            vstegfs_block_save(f.fs, head, c, &b);

            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
    }
}

static uint64_t vstegfs_header(vstat_t *f, vblock_t *b)
{
    /*
     * find the header blocks for the file
     */
    uint16_t header[MAX_COPIES] = { 0x0 };
    {
        char *fp = NULL;
        asprintf(&fp, "%s/%s:%s", f->path, f->name, f->pass ?: ROOT_PATH);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fp, strlen(fp));
        uint8_t *ph = mhash_end(h);
        memcpy(header, ph, SB_HASH);
        free(ph);
        free(fp);
    }
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    uint64_t path[2];
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, f->path, strlen(f->path));
        uint8_t *ph = mhash_end(h);
        memcpy(path, ph, SB_PATH);
        free(ph);
    }
    uint64_t fs_blocks = lseek(f->fs, 0, SEEK_END) / SB_BLOCK;
    uint64_t f_size = 0x0;
    /*
     * try to find a header
     */
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        uint64_t head = 0x0;
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
                memcpy(&f_size, b->next, SB_NEXT);
            }
            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
        if (f_size)
            break;
    }
    return f_size;
}

extern MCRYPT vstegfs_crypt_init(vstat_t *f, uint8_t ivi)
{
    MCRYPT c = mcrypt_module_open(MCRYPT_SERPENT, NULL, MCRYPT_CBC, NULL);
    uint8_t key[SB_TIGER] = { 0x00 };
    {
        char *fk = NULL;
        asprintf(&fk, "%s:%s", f->name, f->pass ?: ROOT_PATH);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fk, strlen(fk));
        uint8_t *ph = mhash_end(h);
        memcpy(key, ph, SB_TIGER);
        free(ph);
        free(fk);
    }
    uint8_t ivs[SB_SERPENT] = { 0x00 };
    {
        char *iv_s = NULL;
        asprintf(&iv_s, "%s+%i", f->name, ivi);
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, iv_s, strlen(iv_s));
        uint8_t *ph = mhash_end(h);
        memcpy(ivs, ph, SB_SERPENT);
        free(ph);
        free(iv_s);
    }
    mcrypt_generic_init(c, key, sizeof( key ), ivs);
    return c;
}

extern int64_t vstegfs_block_save(uint64_t fs, uint64_t pos, MCRYPT c, vblock_t *b)
{
    errno = EXIT_SUCCESS;
    /*
     * calculate the hash of the data
     */
    {
        uint8_t data[SB_DATA] = { 0x00 };
        memcpy(data, (uint8_t *)b->data, SB_DATA);

        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, data, SB_DATA);
        uint8_t *ph = mhash_end(h);

        memcpy((uint8_t *)b->hash, ph, SB_HASH);
        free(ph);
    }
    uint8_t d[SB_SERPENT * 7] = { 0x00 };
    memcpy(d, ((uint8_t *)b) + SB_SERPENT, SB_BLOCK - SB_PATH);
    mcrypt_generic(c, d, sizeof( d ));
    memcpy(((uint8_t *)b) + SB_SERPENT, d, SB_BLOCK - SB_PATH);
    /*
     * write the encrypted block to this block location
     */
    pwrite(fs, b, sizeof( vblock_t ), pos * SB_BLOCK);
    return errno;
}

static int64_t block_open(uint64_t fs, uint64_t pos, MCRYPT c, vblock_t *b)
{
    errno = EXIT_SUCCESS;
    pread(fs, b, sizeof( vblock_t ), pos * SB_BLOCK);
    uint8_t d[SB_SERPENT * 7] = { 0x00 };
    memcpy(d, ((uint8_t *)b) + SB_SERPENT, SB_BLOCK - SB_PATH);
    mdecrypt_generic(c, d, sizeof( d ));
    memcpy(((uint8_t *)b) + SB_SERPENT, d, SB_BLOCK - SB_PATH);
    /*
     * check the hash of the decrypted data
     */
    uint8_t hash[SB_HASH] = { 0x00 };
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, b->data, SB_DATA);
        uint8_t *ph = mhash_end(h);
        memcpy(hash, ph, SB_HASH);
        free(ph);
    }
    if (memcmp(hash, b->hash, SB_HASH))
        return EXIT_FAILURE;
    return errno;
}

static bool is_block_ours(uint64_t fs, uint64_t pos, uint64_t *hash)
{
    uint8_t buf[SB_PATH] = { 0x00 };
    if (pread(fs, buf, SB_PATH, pos * SB_BLOCK) < 0)
        return false; /* we screwed up, so just assume it's not ours :p */
    if (memcmp(buf, hash, SB_PATH))
        return false; /* they're different, so not ours */
    return true; /* looks like the hashes match, must be one of ours :) */
}

static uint64_t calc_next_block(uint64_t fs, char *path)
{
    uint64_t fsb = lseek(fs, 0, SEEK_END) / SB_BLOCK;
    uint64_t block = (((uint64_t)mrand48() << 0x20) | mrand48()) % fsb;
    int16_t att = 0; /* give us up to 32767 attempts to find a block */
    bool found = true;
    /*
     * we will skip past the root directory - files in the root can be
     * overwritten by any other files, even those also in the root -
     * this may seem a little odd, but trust me, it's for the best
     */
    char *token = NULL;
    char *p = strdup(path);
    char *s = NULL;
    while ((token = strsep(&p, "/")))
    {
        if (!strcmp(token, ROOT_PATH))
            continue;
        found = false;
        asprintf(&s, "%s/%s", s ? s : "", token);
        uint8_t hash[SB_PATH] = { 0x00 };
        {
            MHASH h = mhash_init(MHASH_TIGER);
            mhash(h, s, strlen(s));
            uint8_t *ph = mhash_end(h);
            memcpy(hash, ph, SB_PATH);
            free(ph);
        }
        if (att++ < 0)
            break;
        block = (((uint64_t)mrand48() << 0x20) | mrand48()) % fsb;

        if (is_block_ours(fs, block, (uint64_t *)hash))
            continue;
        /*
         * we've found a block which doesn't live along our tree/path -
         * use it
         */
        found = true;
        break;
    }
    free(s);
    return found ? block : 0;
}
