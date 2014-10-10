/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2014, albinoloverats ~ Software Development
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

#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <sys/time.h>

#include <mcrypt.h>
#include <mhash.h>

#include "common/common.h"

#include "stegfs.h"
#include "dir.h"


static void cache_upsert(const stegfs_file_t * const restrict) __attribute__((nonnull(1)));
static bool cache_exists(const stegfs_file_t * const restrict, uint64_t *) __attribute__((nonnull(1, 2)));
static void cache_remove(const stegfs_file_t * const restrict) __attribute__((nonnull(1)));

static bool block_read(uint64_t, stegfs_block_t *, MCRYPT mc, const char * const restrict);
static bool block_write(uint64_t, stegfs_block_t, MCRYPT mc, const char * const restrict);
static void block_delete(uint64_t);

static bool block_in_use(uint64_t, const char * const restrict);
static uint64_t block_assign(const char * const restrict);

static MCRYPT crypto_init(const stegfs_file_t * const restrict, uint8_t);
static void seed_prng(void);


static stegfs_t file_system;


extern bool stegfs_init(const char * const restrict fs)
{
    // TODO check file system header - if there's an error: return false
    seed_prng();
    file_system.handle = open(fs, O_RDWR, S_IRUSR | S_IWUSR);
    file_system.size = lseek(file_system.handle, 0, SEEK_END);
    file_system.used = calloc(file_system.size / SIZE_BYTE_BLOCK, sizeof( bool ));
    file_system.cache.size = 0;
    file_system.cache.file = NULL;
    return true;
}

extern stegfs_t stegfs_info(void)
{
    return file_system;
}

extern bool stegfs_files_equal(stegfs_file_t a, stegfs_file_t b)
{
    if (!a.path || !b.path || strcmp(a.path, b.path))
        return false;
    if (!a.name || !b.name || strcmp(a.name, b.name))
        return false;
    return true;
}

extern bool stegfs_file_will_fit(stegfs_file_t *file)
{
    uint64_t blocks_needed = (file->size / SIZE_BYTE_DATA + 1) * MAX_COPIES;
    uint64_t blocks_total = file_system.size / SIZE_BYTE_BLOCK;
    if (blocks_needed > blocks_total)
    {
        stegfs_file_delete(file);
        errno = EFBIG; /* file would not fit in the file system */
        return false;
    }
    uint64_t blocks_used = 0;
    for (uint64_t i = 0; i < file_system.size / SIZE_BYTE_BLOCK; i++)
        if (file_system.used[i])
            blocks_used++;
    uint64_t blocks_available = blocks_total - blocks_used;
    if (blocks_needed > blocks_available)
    {
        stegfs_file_delete(file);
        errno = ENOSPC; /* file won't fit in remaining space */
        return false;
    }
    errno = EXIT_SUCCESS;
    return true;
}

extern void stegfs_file_create(const char * const restrict path, bool write)
{
    stegfs_file_t file = { 0x0 };
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);
    file.write = write;
    file.time = time(NULL);
    cache_upsert(&file);
    return;
}

extern bool stegfs_file_stat(stegfs_file_t *file)
{
    /*
     * figure out where the file's inode blocks are
     */
    MHASH hash = mhash_init(MHASH_TIGER);
    mhash(hash, file->path, strlen(file->path));
    mhash(hash, file->name, strlen(file->name));
    uint8_t *inodes = mhash_end(hash);
    /*
     * calculate and read inodes
     */
    int available_inodes = MAX_COPIES;
    int corrupt_copies = 0;
    for (int i = 0; i < MAX_COPIES; i++)
    {
        for (int j = (sizeof (uint64_t) / sizeof (uint8_t) ); j >= 0; --j)
            file->inodes[i] = (file->inodes[i] << 0x08) | inodes[i + j];
        file->inodes[i] %= file_system.size / SIZE_BYTE_BLOCK;
        MCRYPT mc = crypto_init(file, i);
        stegfs_block_t inode;
        memset(&inode, 0x00, SIZE_BYTE_BLOCK);
        if (block_read(file->inodes[i], &inode, mc, file->path))
        {
            file_system.used[file->inodes[i]] = true;
            memcpy(&file->size, inode.next, sizeof file->size);
            file->size = ntohll(file->size);
            for (int i = 0; i <= MAX_COPIES; i++)
            {
                MCRYPT nc = crypto_init(file, i);
                uint64_t val;
                memcpy(&val, inode.data + i * sizeof val, sizeof val);
                val = ntohll(val);
                if (i < MAX_COPIES)
                {
                    uint64_t blocks = file->size / SIZE_BYTE_DATA + 1;
                    file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
                    memset(file->blocks[i], 0x00, blocks * sizeof blocks);
                    file->blocks[i][0] = blocks;
                    file->blocks[i][1] = val;
                    /*
                     * travsers file block tree
                     */
                    for (uint64_t j = 2 ; j <= blocks; j++)
                    {
                        stegfs_block_t block;
                        memset(&block, 0x00, SIZE_BYTE_BLOCK);
                        if (block_read(file->blocks[i][j - 1], &block, nc, file->path))
                        {
                            file_system.used[file->blocks[i][j - 1]] = true;
                            memcpy(&val, block.next, sizeof file->blocks[i][j]);
                            file->blocks[i][j] = ntohll(val);
                        }
                        else
                        {
                            /*
                             * note-to-self: because of the memset above, all
                             * incomplete blockchains will have trailing 0's,
                             * this comes in handy when saving...
                             */
                            corrupt_copies++;
                            break;
                        }
                    }
                }
                else
                    file->time = val;
                mcrypt_generic_deinit(nc);
                mcrypt_module_close(nc);
            }
            break;
        }
        else
            available_inodes--;
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
    }
    free(inodes);
    /*
     * as long as there's a valid inode and a complete copy we're good
     */
    if (available_inodes && corrupt_copies < MAX_COPIES)
    {
        cache_upsert(file);
        return true;
    }
    return false;
}

extern bool stegfs_file_read(stegfs_file_t *file)
{
    if (!stegfs_file_stat(file))
        return false;
    /*
     * read the file data
     */
    file->data = realloc(file->data, file->size);
    for (int i = 0; i < MAX_COPIES; i++)
    {
        uint64_t blocks = file->size / SIZE_BYTE_DATA + 1;
        if (file->blocks[i][0] != blocks)
            continue; /* this copy is corrupt; try the next */
        MCRYPT mc = crypto_init(file, i);
        for (uint64_t j = 1, k = 0; j <= file->blocks[i][0] && file->blocks[i][j]; j++, k++)
        {
            /*
             * we should be largely confident that we'll be able to read
             * the complete file as outherwise the stat would have
             * failed
             */
            stegfs_block_t block;
            block_read(file->blocks[i][j], &block, mc, file->path);
            size_t l = sizeof block.data;
            if (l + k * sizeof block.data > file->size)
                l = l - ((l + k * sizeof block.data) - file->size);
            memcpy(file->data + k * sizeof block.data, block.data, l);
        }
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
        cache_upsert(file);
        return true;
    }
    /*
     * somehow we failed to read a complete copy of the file, despite
     * knowing that a complete copy existed when stat'd
     */
    return false;
}

extern bool stegfs_file_write(stegfs_file_t *file)
{
    uint64_t blocks = file->size / SIZE_BYTE_DATA + 1;
    uint64_t z = file->size;

    if (!stegfs_file_stat(file))
        for (int i = 0; i < MAX_COPIES; i++)
        {
            /*
             * note-to-self: allocate 2 more blocks than is necessary so
             * that block[0] indicates how many blocks there are (needed)
             * and block[last] is kept 0x00 as a overrun guard
             */
            file->blocks[i] = calloc(blocks + 2, sizeof blocks);
            file->blocks[i][0] = blocks;
            for (uint64_t j = 1; j <= blocks; j++)
                if (!(file->blocks[i][j] = block_assign(file->path)))
                {
                    errno = ENOSPC;
                    return false;
                }
        }
    file->size = z; /* stat can couse size to be reset to 0 */

    if (blocks > file->blocks[0][0]) /* need more blocks than we have */
        for (int i = 0; i < MAX_COPIES; i++)
        {
            file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
            for (uint64_t j = file->blocks[i][0]; j <= blocks; j++)
                if (!file->blocks[i][j])
                    if (!(file->blocks[i][j] = block_assign(file->path)))
                    {
                        errno = ENOSPC;
                        return false;
                    }
        }
    else if (blocks < file->blocks[0][0]) /* have more blocks than we need */
        for (int i = 0; i < MAX_COPIES; i++)
        {
            for (uint64_t j = blocks + 1; j <= file->blocks[i][0]; j++)
                block_delete(file->blocks[i][j]);
            file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
        }
    /*
     * note-to-self: from above, any of the blockchains could have a
     * string of 0's (possibly in the middle if it's been expanded) so
     * we need to reassign those now
     */
    for (int i = 0; i < MAX_COPIES; i++)
    {
        for (uint64_t j = 1; j < file->blocks[i][0]; j++)
            if (!(file->blocks[i][j] = block_assign(file->path)))
            {
                errno = ENOSPC;
                return false;
            }
        file->blocks[i][blocks + 1] = 0x0;
    }
    /*
     * write the data
     */
    bool e = false;
    for (int i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT mc = crypto_init(file, i);
        for (uint64_t j = 1, k = 0; j <= blocks; j++, k++)
        {
            stegfs_block_t block;
            size_t l = sizeof block.data;
            if (l + k * sizeof block.data > file->size)
                l = l - ((l + k * sizeof block.data) - file->size);
            memset(block.data, 0x00, SIZE_BYTE_DATA);
            memcpy(block.data, file->data + k * sizeof block.data, l);
            z = htonll(file->blocks[i][j + 1]);
            memcpy(block.next, &z, sizeof block.next);
            if (!block_write(file->blocks[i][j], block, mc, file->path))
                e = true;
        }
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
    }
    /*
     * write file inode blocks
     */
    stegfs_block_t inode;
    uint64_t first[MAX_COPIES + 1] = { 0x0 };
    for (int i = 0; i < MAX_COPIES; i++)
        first[i] = htonll(file->blocks[i][1]);
    first[MAX_COPIES] = htonll(file->time);
    memcpy(inode.data, first, sizeof first);
    z = htonll(file->size);
    memcpy(inode.next, &z, sizeof file->size);
    for (int i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT mc = crypto_init(file, i);
        if (block_write(file->inodes[i], inode, mc, file->path))
            file_system.used[file->inodes[i]] = true;
        else
            e = true;
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
    }
//    if (!e)
//        cache_upsert(file);
    return !e;
}

extern void stegfs_file_delete(stegfs_file_t *file)
{
    if (!stegfs_file_stat(file))
        goto rfc;
    for (int i = 0; i < MAX_COPIES; i++)
        block_delete(file->inodes[i]);
    uint64_t blocks = file->size / SIZE_BYTE_DATA + 1;
    for (int i = 0; i < MAX_COPIES; i++)
        for (uint64_t j = 1; j <= blocks && file->blocks[i][j]; j++)
            block_delete(file->blocks[i][j]);
rfc:
    cache_remove(file);
    return;
}

/*
 * block functions
 */

static bool block_read(uint64_t bid, stegfs_block_t *block, MCRYPT mc, const char * const restrict path)
{
    errno = EXIT_SUCCESS;
    if (bid * SIZE_BYTE_BLOCK + SIZE_BYTE_BLOCK > file_system.size)
    {
        errno = EINVAL;
        return false;
    }
    ssize_t z = pread(file_system.handle, block, sizeof( stegfs_block_t ), bid * SIZE_BYTE_BLOCK);
    if (z < 0)
        return false;
    /* check path hash */
    MHASH hash = mhash_init(MHASH_TIGER);
    mhash(hash, path, strlen(path));
    void *p = mhash_end(hash);
    if (memcmp(block->path, p, sizeof block->path))
    {
        free(p);
        return false;
    }
    free(p);
    /* decrypt block */
#ifndef __DEBUG__
        uint8_t data[SIZE_BYTE_SERPENT * 7] = { 0x00 };
        memcpy(data, ((uint8_t *)block) + SIZE_BYTE_SERPENT, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
        mdecrypt_generic(mc, data, sizeof data);
        memcpy(((uint8_t *)block) + SIZE_BYTE_SERPENT, data, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
#endif
    /* check data hash */
    hash = mhash_init(MHASH_TIGER);
    mhash(hash, block->data, sizeof block->data);
    p = mhash_end(hash);
    if (memcmp(block->hash, p, sizeof block->hash))
    {
        free(p);
        return false;
    }
    free(p);
    return z == sizeof (stegfs_block_t);
}

static bool block_write(uint64_t bid, stegfs_block_t block, MCRYPT mc, const char * const restrict path)
{
    errno = EXIT_SUCCESS;
    if (bid * SIZE_BYTE_BLOCK + SIZE_BYTE_BLOCK > file_system.size)
    {
        errno = EINVAL;
        return false;
    }
    /* compute path hash */
    MHASH hash = mhash_init(MHASH_TIGER);
    mhash(hash, path, strlen(path));
    void *p = mhash_end(hash);
    memcpy(block.path, p, sizeof block.path);
    free(p);
    /* compute data hash */
    hash = mhash_init(MHASH_TIGER);
    mhash(hash, block.data, sizeof block.data);
    p = mhash_end(hash);
    memcpy(block.hash, p, sizeof block.hash);
    free(p);
    /* encrypt the data */
#ifndef __DEBUG__
    uint8_t data[SIZE_BYTE_SERPENT * 7] = { 0x00 };
    memcpy(data, ((uint8_t *)&block) + SIZE_BYTE_SERPENT, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
    mcrypt_generic(mc, data, sizeof data);
    memcpy(((uint8_t *)&block) + SIZE_BYTE_SERPENT, data, SIZE_BYTE_BLOCK - SIZE_BYTE_PATH);
#endif
    return pwrite(file_system.handle, &block, sizeof block, bid * SIZE_BYTE_BLOCK) == sizeof block;
}

static void block_delete(uint64_t bid)
{
    uint8_t block[SIZE_BYTE_BLOCK];
    for (int k = 0; k < SIZE_BYTE_BLOCK; k++)
        block[k] = (uint8_t)(lrand48() & 0xFF);
    if (bid * SIZE_BYTE_BLOCK + SIZE_BYTE_BLOCK > file_system.size)
        return;
    pwrite(file_system.handle, block, sizeof block, bid * SIZE_BYTE_BLOCK);
    file_system.used[bid] = false;
    return;
}

static bool block_in_use(uint64_t bid, const char * const restrict path)
{
    if (!bid)
        return true; /* superblock always in use */
    if (bid * SIZE_BYTE_BLOCK + SIZE_BYTE_BLOCK > file_system.size)
        return true;
    /*
     * check if the block is in the cache
     */
    if (file_system.used[bid])
        return true;
    /*
     * block not found in cache; check if this might belong to a file in
     * this directory, or parent directory
     */
#ifndef __DEBUG__
    char *p = NULL;
    uint16_t hierarchy = dir_get_deep(path);
    for (uint16_t i = 1; i < hierarchy; i++)
    {
        char *e = dir_get_part(path, i);
        asprintf(&p, "%s/%s", p ? : "", e ? : "");
        if (e)
            free(e);
        MHASH hash = mhash_init(MHASH_TIGER);
        mhash(hash, p, strlen(p));
        void *h = mhash_end(hash);
        stegfs_block_t block;
        pread(file_system.handle, &block, sizeof block, bid * SIZE_BYTE_BLOCK);
        bool r = !memcmp(h, block.path, SIZE_BYTE_PATH);
        free(h);
        if (r)
            return true;
    }
    free(p);
#endif
    return false;
}

static uint64_t block_assign(const char * const restrict path)
{
    uint64_t block;
    uint64_t tries = 0;
    do
    {
        block = (lrand48() << 32 | lrand48()) % (file_system.size / SIZE_BYTE_BLOCK);
        if ((++tries) > file_system.size / SIZE_BYTE_BLOCK)
            return 0;
    }
    while (block_in_use(block, path));
    file_system.used[block] = true;
    return block;
}

/*
 * cache functions
 */

static void cache_upsert(const stegfs_file_t * const restrict file)
{
    uint64_t o = 0;
    if (!cache_exists(file, &o))
    {
        for (uint64_t i = 0; i < file_system.cache.size; i++)
            if (!file_system.cache.file[i].path)
            {
                o = i;
                goto cache_update;
            }
        file_system.cache.file = realloc(file_system.cache.file, sizeof (stegfs_file_t) * (++file_system.cache.size));
        memset(&file_system.cache.file[o], 0x00, sizeof file_system.cache.file[o]);
    }
cache_update: /* upsert cache */
    /* set path and name */
    asprintf(&file_system.cache.file[o].path, "%s", file->path);
    asprintf(&file_system.cache.file[o].name, "%s", file->name);
    asprintf(&file_system.cache.file[o].pass, "%s", file->pass);
    /* set inodes */
    for (int i = 0; i < MAX_COPIES; i++)
        file_system.cache.file[o].inodes[i] = file->inodes[i];
    /* set time and size */
        file_system.cache.file[o].write = file->write;
    file_system.cache.file[o].time = file->time;
    file_system.cache.file[o].size = file->size;
    if (!file_system.cache.file[o].size)
        return;
    /* copy data */
    if (file->data)
    {
        file_system.cache.file[o].data = realloc(file_system.cache.file[o].data, file_system.cache.file[o].size);
        memcpy(file_system.cache.file[o].data, file->data, file_system.cache.file[o].size);
    }
    /* copy blocks */
    uint64_t blocks = file_system.cache.file[o].size / SIZE_BYTE_DATA + 1;
    for (int i = 0; i < MAX_COPIES; i++)
    {
        file_system.cache.file[o].blocks[i] = realloc(file_system.cache.file[o].blocks[i], (blocks + 1) * sizeof blocks);
        memset(file_system.cache.file[o].blocks[i], 0x00, blocks * sizeof blocks);
        file_system.cache.file[o].blocks[i][0] = blocks;
        for (uint64_t j = 1 ; j <= blocks && file->blocks[i] && file->blocks[i][j]; j++)
            file_system.cache.file[o].blocks[i][j] = file->blocks[i][j];
    }
    return;
}

static bool cache_exists(const stegfs_file_t * const restrict file, uint64_t *index)
{
    for (*index = 0; *index < file_system.cache.size; (*index)++)
        if (file_system.cache.file[*index].path && stegfs_files_equal(file_system.cache.file[*index], *file))
            return true;
    return false;
}

static void cache_remove(const stegfs_file_t * const restrict file)
{
    uint64_t i = 0;
    if (cache_exists(file, &i))
    {
        free(file_system.cache.file[i].path);
        file_system.cache.file[i].path = NULL;
        free(file_system.cache.file[i].name);
        file_system.cache.file[i].pass = NULL;
        free(file_system.cache.file[i].pass);
        file_system.cache.file[i].pass = NULL;
        for (int j = 0; j < MAX_COPIES; j++)
            file_system.cache.file[i].inodes[j] = 0;
        file_system.cache.file[i].write = false;
        file_system.cache.file[i].size = 0;
        file_system.cache.file[i].time = 0;
        if (file_system.cache.file[i].data)
            free(file_system.cache.file[i].data);
        file_system.cache.file[i].data = NULL;
        for (int j = 0; j < MAX_COPIES; j++)
        {
            if (file_system.cache.file[i].blocks[j])
                free(file_system.cache.file[i].blocks[j]);
            file_system.cache.file[i].blocks[j] = NULL;
        }
        memset(&file_system.cache.file[i], 0x00, sizeof file_system.cache.file[i]);
    }
    return;
}

static MCRYPT crypto_init(const stegfs_file_t * const restrict file, uint8_t ivi)
{
    MCRYPT c = mcrypt_module_open(MCRYPT_SERPENT, NULL, MCRYPT_CBC, NULL);
    /* create the initial key for the encryption algorithm */
    uint8_t key[SIZE_BYTE_TIGER] = { 0x00 };
    MHASH hash = mhash_init(MHASH_TIGER);
    mhash(hash, file->path, strlen(file->path));
    mhash(hash, file->name, strlen(file->name));
    if (file->pass)
        mhash(hash, file->pass, strlen(file->pass));
    else
        mhash(hash, "", strlen(""));
    void *h = mhash_end(hash);
    memcpy(key, h, SIZE_BYTE_TIGER);
    free(h);
    /* create the initial iv for the encryption algorithm */
    uint8_t ivs[SIZE_BYTE_SERPENT] = { 0x00 };
    hash = mhash_init(MHASH_TIGER);
    if (file->pass)
        mhash(hash, file->pass, strlen(file->pass));
    else
        mhash(hash, "", strlen(""));
    mhash(hash, file->name, strlen(file->name));
    mhash(hash, file->path, strlen(file->path));
    mhash(hash, &ivi, sizeof ivi);
    h = mhash_end(hash);
    memcpy(ivs, h, SIZE_BYTE_SERPENT);
    free(h);
    /* done */
    mcrypt_generic_init(c, key, sizeof key, ivs);
    return c;
}

static void seed_prng(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t t = now.tv_sec * MILLION + now.tv_usec;
    uint16_t s[RANDOM_SEED_SIZE] = { 0x0 };
    MHASH h = mhash_init(MHASH_TIGER);
    mhash(h, &t, sizeof( uint64_t ));
    uint8_t * const restrict ph = mhash_end(h);
    memcpy(s, ph, RANDOM_SEED_SIZE * sizeof( uint16_t ));
    free(ph);
    seed48(s);
    return;
}
