/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright (c) 2007-2011, albinoloverats ~ Software Development
 * email: stegfs@albinoloverats.net
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

#define _IN_LIB_STEGFS_
#include "lib-stegfs.h"
#include "dir.h"

#include "common/common.h"
#include "common/list.h"

#include <fcntl.h>
#include <libintl.h>
#include <sys/time.h>

extern void lib_stegfs_init(const char * const restrict fs, const bool c)
{
    file_system = calloc(1, sizeof( stegfs_fs_info_t ));
    if (!file_system)
        die(_("out of memory @ %s:%i"), __FILE__, __LINE__);
    if ((file_system->id = open(fs, O_RDWR, S_IRUSR | S_IWUSR)) < 3)
        die(_("could not open file system %s"), fs);
    /*
     * check the first block to see if everything looks okay - actually
     * just check the magic numbers of the end of the block; in the 
     * future we could check version and algorithm info from the TLV
     * data block
     */
#ifndef DEBUGGING
    stegfs_block_t super;
    lseek(file_system->id, 0, SEEK_SET);
    if (read(file_system->id, &super, sizeof( stegfs_block_t )) != sizeof( stegfs_block_t ))
        msg("%s", strerror(errno));
    if ((super.hash[0] != MAGIC_0) || (super.hash[1] != MAGIC_1) || (super.hash[2] != MAGIC_2))
    {
        msg(_("magic number failure in superblock for %s"), fs);
        die(_("use mkstegfs to restore superblock"));
    }
#endif /* ! DEBUGGING */
    file_system->size = lseek(file_system->id, 0, SEEK_END);
    file_system->blocks = file_system->size / SIZE_BYTE_BLOCK;
    if (c)
    {
        /*
         * allocate memory to store bitmap, and list of file names
         */
        file_system->bitmap = calloc(file_system->blocks / CHAR_BIT, sizeof( uint8_t ));
        file_system->files = list_create(NULL);
        stegfs_cache_t *x = calloc(1, sizeof( stegfs_cache_t ));
        x->id = 0;
        x->name = strdup("");
        x->path = strdup("");
        list_append(&file_system->files, x);
    }
    return;
}

extern void lib_stegfs_info(stegfs_fs_info_t * const restrict info)
{
    info->id = file_system->id;
    info->size = file_system->size;
    info->blocks = file_system->blocks;
    return;
}

static void lib_stegfs_cache_add(const stegfs_file_t * const restrict file)
{
    const uint64_t max = list_size(file_system->files);
    for (uint64_t i = 0; i < max; i++)
    {
        const stegfs_cache_t * const restrict x = list_get(file_system->files, i);
        if (!strcmp(file->path, x->path) && !strcmp(file->name, x->name))
            return; /* found a file in the same directory with the same name; we're done */
    }
    stegfs_cache_t * const restrict x = calloc(1, sizeof( stegfs_cache_t ));
    x->name = strdup(file->name);
    x->path = strdup(file->path);
    list_append(&file_system->files, x);
    return;
}

static void lib_stegfs_cache_del(const stegfs_file_t * const restrict file)
{
    const uint64_t max = list_size(file_system->files);
    for (uint64_t i = 0; i < max; i++)
    {
        stegfs_cache_t * const restrict x = list_get(file_system->files, i);
        if (!strcmp(file->path, x->path) && !strcmp(file->name, x->name))
        {
            free(x->name);
            free(x->path);
            free(x);
            list_remove(&file_system->files, i);
            break; /* found a file in the same directory with the same name; we're done */
        }
    }
    return;
}

extern list_t *lib_stegfs_cache_get(void)
{
    return file_system->files;
}

extern uint8_t *lib_stegfs_cache_map(void)
{
    return file_system->bitmap;
}

static void lib_stegfs_init_hash(stegfs_file_t *file, void *header, void *path)
{
    /*
     * find the header blocks for the file
     */
    if (header)
    {
        char *fp = NULL;
        if (asprintf(&fp, "%s/%s:%s", file->path, file->name, file->pass ? : PATH_ROOT))
        {
            MHASH h = mhash_init(MHASH_TIGER);
            mhash(h, fp, strlen(fp));
            uint8_t *ph = mhash_end(h);
            memmove(header, ph, sizeof( uint16_t ) * MAX_COPIES);
            free(ph);
            free(fp);
        }
    }
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    if (path)
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, file->path, strlen(file->path));
        uint8_t *ph = mhash_end(h);
        memmove(path, ph, SIZE_BYTE_PATH);
        free(ph);
    }
    return;
}

static MCRYPT lib_stegfs_init_crypt(stegfs_file_t *file, uint8_t ivi)
{
    MCRYPT c = mcrypt_module_open(MCRYPT_SERPENT, NULL, MCRYPT_CBC, NULL);
    uint8_t key[SIZE_BYTE_TIGER] = { 0x00 };
    /*
     * create the initial key for the encryption algorithm
     */
    char *fk = NULL;
    if (asprintf(&fk, "%s:%s", file->name, strlen(file->pass) ? file->pass : PATH_ROOT))
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, fk, strlen(fk));
        uint8_t *ph = mhash_end(h);
        memmove(key, ph, SIZE_BYTE_TIGER);
        free(ph);
        free(fk);
    }
    uint8_t ivs[SIZE_BYTE_SERPENT] = { 0x00 };
    /*
     * create the initial iv for the encryption algorithm
     */
    char *iv_s = NULL;
    if (asprintf(&iv_s, "%s+%i", file->name, ivi))
    {
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, iv_s, strlen(iv_s));
        uint8_t *ph = mhash_end(h);
        memmove(ivs, ph, SIZE_BYTE_SERPENT);
        free(ph);
        free(iv_s);
    }
    mcrypt_generic_init(c, key, sizeof( key ), ivs);
    return c;
}

extern uint64_t lib_stegfs_stat(stegfs_file_t *file, stegfs_block_t *block)
{
    uint16_t header[MAX_COPIES] = INIT_HEADER;
    uint64_t path[SIZE_LONG_PATH] = INIT_PATH;
    bool found = false;
    uint64_t inode = 0x0;
    lib_stegfs_init_hash(file, &header, &path);
    bool fb = false;
    if (!block)
    {
        fb = true;
        if (!(block = calloc(1, sizeof( stegfs_block_t ))))
            return 0;
    }
    if (block)
    {
        /*
         * try to find a header
         */
        for (uint8_t i = 0; i < MAX_COPIES; i++)
        {
            for (int8_t j = (sizeof( uint64_t ) / sizeof( uint16_t )); j >= 0; --j)
                inode = (inode << 0x10) | header[i + j];
            inode %= file_system->blocks;
            if (lib_stegfs_block_ours(inode, path))
            {
                /*
                 * found a header block which might be ours, if we can read
                 * its contents we're in business
                 */
                MCRYPT c = lib_stegfs_init_crypt(file, i);
                if (c)
                {
                    if (!lib_stegfs_block_load(inode, c, block))
                    {
                        /*
                        * if we're here we were able to successfully open the header
                        */
                        memmove(&file->size, block->next, SIZE_BYTE_NEXT);
                        uint64_t data[SIZE_LONG_DATA] = { 0x0 };
                        memmove(data, block->data, SIZE_BYTE_DATA);
                        memmove(&file->time, &data[SIZE_LONG_DATA - 1], sizeof( time_t ));
                        found = true;
                    }
                    mcrypt_generic_deinit(c);
                    mcrypt_module_close(c);
                }
                else
                    break;
            }
            if (file->size)
                break;
        }
    }
    if (fb)
        free(block);
    return found ? inode : 0;
}

extern int64_t lib_stegfs_kill(stegfs_file_t *file)
{
    /*
     * TODO find all blocks for this file and wipe them
     */
    lib_stegfs_cache_del(file);
    /*
     * TODO find a way to unmark blocks in the bitmap
     */
    return -EXIT_SUCCESS;
}

extern int64_t lib_stegfs_save(stegfs_file_t *file)
{
    /*
     * some initial preparations - such as: is the file larger than the
     * file system? because that wouldn't be good :s
     */
    if (MAX_COPIES * file->size > file_system->size * RATIO_DATA_BLOCK)
        return -EFBIG;
    /*
     * compute path hash (same for all copies and blocks of the file)
     */
    uint16_t header[MAX_COPIES] = INIT_HEADER;
    uint64_t path[SIZE_LONG_PATH] = INIT_PATH;
    lib_stegfs_init_hash(file, &header, &path);
    /*
     * work through the file to be hidden (MAX_COPIES) times
     */
    uint64_t start[MAX_COPIES] = { 0x0 };
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        if (!(start[i] = lib_stegfs_block_find(file->path)))
            return -ENOSPC;
        MCRYPT c = lib_stegfs_init_crypt(file, i);
        if (!c)
            return -ENOMEM;
        /*
         * process the data: encrypt and store
         */
        uint64_t next = start[i];
        uint64_t p = 0x0;
        for (uint64_t j = 0; j < file->size; j += SIZE_BYTE_DATA)
        {
            uint8_t data[SIZE_BYTE_DATA] = { 0x00 };
            memmove(data, file->data + p, SIZE_BYTE_DATA);
            p += SIZE_BYTE_DATA;
            stegfs_block_t block;
            uint64_t cb = next;
            if (!(next = lib_stegfs_block_find(file->path)))
                return -ENOSPC;
            /*
             * build the block; this includes putting in the data,
             * calculating a hash of the data and making a note of the
             * next block location
             */
            memmove(block.path,  path, SIZE_BYTE_PATH);
            memmove(block.data,  data, SIZE_BYTE_DATA);
            memset(block.hash,   0x00, SIZE_BYTE_HASH);
            memmove(block.next, &next, SIZE_BYTE_NEXT);
            /*
             * save the block - if there is a problem, give up with IO
             * error
             */
            if (lib_stegfs_block_save(cb, c, &block))
                return -EIO;
        }
        mcrypt_generic_deinit(c);
        mcrypt_module_close(c);
    }
    /*
     * construct the header block, then save it in each of its locations
     */
    {
        stegfs_block_t block;
        memmove(block.path, path, SIZE_BYTE_PATH);
        memset(block.data, 0x00, SIZE_BYTE_DATA);
        uint8_t i = 0;
        for (i = 0; i < MAX_COPIES; i++)
            memmove(block.data + i * sizeof( uint64_t ), &start[i], sizeof( uint64_t ));
        memmove(block.data + i * sizeof( uint64_t ), &file->time, sizeof( time_t ));
        memmove(block.next, &file->size, SIZE_BYTE_NEXT);
        for (uint8_t i = 0; i < MAX_COPIES; i++)
        {
            /*
             * each header block is encrypted independently
             */
            uint64_t head = 0x0;
            for (int8_t j = (sizeof( uint64_t ) / sizeof( uint16_t )); j >= 0; --j)
                head = (head << 0x10) | header[i + j];
            head %= file_system->blocks;
            /*
             * write this copy of the header
             */
            MCRYPT c = lib_stegfs_init_crypt(file, i);
            if (!c)
                return -ENOMEM;
            if (lib_stegfs_block_save(head, c, &block))
                return -EIO;
            mcrypt_generic_deinit(c);
            mcrypt_module_close(c);
        }
    }
    /*
     * add file to list of known filenames
     */
    lib_stegfs_cache_add(file);
    /*
     * success :D
     */
    return -EXIT_SUCCESS;
}

static int64_t lib_stegfs_block_save(uint64_t offset, MCRYPT crypto, stegfs_block_t *block)
{
    /*
     * calculate the hash of the data
     */
    errno = EXIT_SUCCESS;
    uint8_t hdata[SIZE_BYTE_DATA] = { 0x00 };
    memmove(hdata, (uint8_t *)block->data, SIZE_BYTE_DATA);
    MHASH h = mhash_init(MHASH_TIGER);
    mhash(h, hdata, SIZE_BYTE_DATA);
    uint8_t *ph = mhash_end(h);
    memmove((uint8_t *)block->hash, ph, SIZE_BYTE_HASH);
    free(ph);
    /*
     * encrypt the data
     */
#ifndef DEBUGGING
    uint8_t cdata[SIZE_BYTE_SERPENT * 7] = { 0x00 };
    memmove(cdata, ((uint8_t *)block) + SIZE_BYTE_SERPENT, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
    mcrypt_generic(crypto, cdata, sizeof( cdata ));
    memmove(((uint8_t *)block) + SIZE_BYTE_SERPENT, cdata, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
#endif /* ! DEBUGGING */
    /*
     * write the encrypted block to this block location
     */
    return pwrite(file_system->id, block, sizeof( stegfs_block_t ), offset * SIZE_BYTE_BLOCK) != sizeof( stegfs_block_t ) ? -errno : -EXIT_SUCCESS;
}

extern int64_t lib_stegfs_load(stegfs_file_t *file)
{
    uint64_t path[SIZE_LONG_PATH] = INIT_PATH;
    lib_stegfs_init_hash(file, NULL, path);
    stegfs_block_t head_block;
    lib_stegfs_stat(file, &head_block);
    /*
     * if we don't know the size of the file then we didn't find an
     * uncorrupted header - give up
     */
    if (!file->size || (MAX_COPIES * file->size > file_system->size * RATIO_DATA_BLOCK))
        return -ENODATA;
    void *x = realloc(file->data, file->size);
    if (!x)
        return -ENOMEM;
    file->data = x;
    uint64_t start[MAX_COPIES] = { 0x0 };
    memmove(start, head_block.data, sizeof( uint64_t ) * MAX_COPIES);
    /*
     * try each of the starting blocks and see if we can
     * find a full copy of the file
     */
    uint64_t total = 0x0;
    for (uint8_t i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT c = lib_stegfs_init_crypt(file, i);
        if (!c)
            return -ENOMEM;
        uint64_t next = start[i];
        uint64_t bytes = 0x0;
        uint64_t offset = 0x0;
        /*
         * now we're ready to read this copy of the file
         */
        while (lib_stegfs_block_ours(next, path))
        {
            stegfs_block_t block;
            if (lib_stegfs_block_load(next, c, &block))
                break;
            /*
             * woo - we now have successfully decrypted another block
             * for this file
             */
            bytes += SIZE_BYTE_DATA;
            uint8_t need = SIZE_BYTE_DATA;
            if (bytes > file->size)
                /* that was the final block, but we don't need all of it */
                need = SIZE_BYTE_DATA - (bytes - file->size);
            memmove(file->data + offset, &block.data, need);
            offset += need;
            if (file_system->bitmap)
            {
                /*
                 * now we know this block is being used
                 */
                lldiv_t a = lldiv(next, CHAR_BIT);
                file_system->bitmap[a.quot] = file_system->bitmap[a.quot] | (0x01 << a.rem);
            }
            if (bytes >= file->size)
            {
                lib_stegfs_cache_add(file); /* we now know this files exists :) */
                return -EXIT_SUCCESS;
            }
            memmove(&next, block.next, SIZE_BYTE_NEXT);
        }
        if (bytes > total)
            total = bytes;
        mcrypt_generic_deinit(c);
        mcrypt_module_close(c);
    }
    /*
     * if we make it this far something has gone wrong
     */
    file->size = total;
    return -EDIED;
}

static int64_t lib_stegfs_block_load(uint64_t offset, MCRYPT crypto, stegfs_block_t *block)
{
    errno = EXIT_SUCCESS;
    if (pread(file_system->id, block, sizeof( stegfs_block_t ), offset * SIZE_BYTE_BLOCK) == sizeof( stegfs_block_t ))
    {
#ifndef DEBUGGING
        uint8_t data[SIZE_BYTE_SERPENT * 7] = { 0x00 };
        memmove(data, ((uint8_t *)block) + SIZE_BYTE_SERPENT, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
        mdecrypt_generic(crypto, data, sizeof( data ));
        memmove(((uint8_t *)block) + SIZE_BYTE_SERPENT, data, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
#endif /* ! DEBUGGING */
        /*
         * check the hash of the decrypted data
         */
        uint8_t hash[SIZE_BYTE_HASH] = { 0x00 };
        MHASH h = mhash_init(MHASH_TIGER);
        mhash(h, block->data, SIZE_BYTE_DATA);
        uint8_t *ph = mhash_end(h);
        memmove(hash, ph, SIZE_BYTE_HASH);
        free(ph);
        if (memcmp(hash, block->hash, SIZE_BYTE_HASH))
            errno = EXIT_FAILURE;
    }
    return -errno;
}

static uint64_t lib_stegfs_block_find(const char * const restrict path)
{
    uint64_t block = 0x0;
    int16_t att = 0;
    bool found = false;
    uint16_t depth = dir_get_deep(path);
    do
    {
        if (++att >= MAX_BLOCK_LOOKUP)
            return 0; /* out of time with no block found */
        lib_stegfs_random_seed();
        /*
         * pick us a 64bit block number
         */
        block = (((uint64_t)mrand48() << 0x20) | mrand48()) % file_system->blocks;
        /*
         * check the bitmap for this block
         */
        if (file_system->bitmap)
        {
            lldiv_t a = lldiv(block, CHAR_BIT);
            if (file_system->bitmap[a.quot] & (0x01 << a.rem))
                continue; /* block in use, try again */
            /*
             * set block as in use in bitmap
             */
            file_system->bitmap[a.quot] |= (0x01 << a.rem);
        }
        bool used = false;
        char *cwd = NULL;
        for (uint16_t i = 0; i < depth; i++)
        {
            char * const p = dir_get_part(path, i);
            if (asprintf(&cwd, "%s/%s%s", cwd ? cwd : "", cwd ? "" : "\b", p))
            {
                free(p);
                uint64_t const hash[SIZE_LONG_PATH] = { 0x0 };
                MHASH h = mhash_init(MHASH_TIGER);
                mhash(h, cwd, strlen(cwd));
                uint8_t * const restrict ph = mhash_end(h);
                memmove((void *)hash, ph, SIZE_BYTE_PATH);
                free(ph);
                if (lib_stegfs_block_ours(block, hash))
                {
                    used = true;
                    break; /* this block exists somewhere down our path */
                }
            }
            else
            {
                if (cwd)
                    free(cwd);
                return 0;
            }
        }
        if (!used)
            found = true;
        free(cwd);
    }
    while (!found);
    return found ? block : 0;
}

static bool lib_stegfs_block_ours(const uint64_t offset, const uint64_t * const restrict hash)
{
    const uint8_t const data[SIZE_BYTE_PATH] = { 0x00 };
    if (pread(file_system->id, (void *)data, SIZE_BYTE_PATH, offset * SIZE_BYTE_BLOCK) < 0)
        return true; /* we screwed up, so just assume it one of ours :p */
    if (memcmp(data, hash, SIZE_BYTE_PATH))
        return false; /* hash is different, not along our path, use it */
    return true;
}

static void lib_stegfs_random_seed(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t t = now.tv_sec * ONE_MILLION + now.tv_usec;
    uint16_t s[RANDOM_SEED_SIZE] = { 0x0 };
    MHASH h = mhash_init(MHASH_TIGER);
    mhash(h, &t, sizeof( uint64_t ));
    uint8_t * const restrict ph = mhash_end(h);
    memmove(s, ph, RANDOM_SEED_SIZE * sizeof( uint16_t ));
    free(ph);
    seed48(s);
    return;
}
