/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2015, albinoloverats ~ Software Development
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
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <mcrypt.h>
#include <mhash.h>

#include "common/common.h"
#include "common/tlv.h"
#include "common/rand.h"
#include "common/dir.h"

#include "stegfs.h"


static bool block_read(uint64_t, stegfs_block_t *, MCRYPT mc, const char * const restrict);
static bool block_write(uint64_t, stegfs_block_t, MCRYPT mc, const char * const restrict);
static void block_delete(uint64_t);

static bool block_in_use(uint64_t, const char * const restrict);
static uint64_t block_assign(const char * const restrict);

static MCRYPT cipher_init(const stegfs_file_t * const restrict, uint8_t);
static MHASH hash_init(void);


static stegfs_t file_system;


extern bool stegfs_init(const char * const restrict fs)
{
    file_system.handle = open(fs, O_RDWR, S_IRUSR | S_IWUSR);
    file_system.size = lseek(file_system.handle, 0, SEEK_END);

    file_system.cache2.name = strdup(DIR_SEPARATOR);
    file_system.cache2.ents = 0;
    file_system.cache2.child = NULL;
    file_system.cache2.file = NULL;
#ifdef USE_PROC
    stegfs_cache2_add(PATH_PROC, NULL);
#endif

    stegfs_block_t block;
    if (!pread(file_system.handle, &block, sizeof block, 0))
        return false;
    if (ntohll(block.hash[0]) != MAGIC_0 ||
        ntohll(block.hash[1]) != MAGIC_1 ||
        ntohll(block.hash[2]) != MAGIC_2)
        return false;
    TLV_HANDLE tlv = tlv_init();
    for (int i = 0, j = 0; i < TAG_MAX; i++)
    {
        tlv_t t;
        memcpy(&t.tag, block.data + j, sizeof t.tag);
        j += sizeof t.tag;
        memcpy(&t.length, block.data + j, sizeof t.length);
        j += sizeof t.length;
        t.length = ntohs(t.length);
        t.value = malloc(t.length);
        memcpy(t.value, block.data + j, t.length);
        j += t.length;
        tlv_append(&tlv, t);
        free(t.value);
    }
    if (!tlv_has_tag(tlv, TAG_STEGFS) ||
        !tlv_has_tag(tlv, TAG_VERSION) ||
        !tlv_has_tag(tlv, TAG_CIPHER) ||
        !tlv_has_tag(tlv, TAG_HASH) ||
        !tlv_has_tag(tlv, TAG_MODE) ||
        !tlv_has_tag(tlv, TAG_BLOCKSIZE))
        return false;

    if (strncmp((char *)tlv_value_of(tlv, TAG_STEGFS), STEGFS_NAME, strlen(STEGFS_NAME)))
        return false;
    if (strncmp((char *)tlv_value_of(tlv, TAG_VERSION), STEGFS_VERSION, strlen(STEGFS_VERSION)))
        return false;
    file_system.cipher = strndup((char *)tlv_value_of(tlv, TAG_CIPHER), tlv_size_of(tlv, TAG_CIPHER));
    file_system.hash = strndup((char *)tlv_value_of(tlv, TAG_HASH), tlv_size_of(tlv, TAG_HASH));
    file_system.mode = strndup((char *)tlv_value_of(tlv, TAG_MODE), tlv_size_of(tlv, TAG_MODE));
    memcpy(&file_system.blocksize, tlv_value_of(tlv, TAG_BLOCKSIZE), tlv_size_of(tlv, TAG_BLOCKSIZE));
    file_system.blocksize = ntohl(file_system.blocksize);
    file_system.used = calloc(file_system.size / file_system.blocksize, sizeof(bool));
    if (ntohll(block.next[0]) != file_system.size / file_system.blocksize)
        return false;

    tlv_deinit(&tlv);
    rand_seed();
    return true;
}

extern stegfs_t stegfs_info(void)
{
    return file_system;
}

extern bool stegfs_files_equal(stegfs_file_t a, stegfs_file_t b)
{
    if (!a.path || !b.path || !path_equals(a.path, b.path))
        return false;
    if (!a.name || !b.name || strcmp(a.name, b.name))
        return false;
    return true;
}

extern bool stegfs_file_will_fit(stegfs_file_t *file)
{
    lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
    uint64_t blocks_needed = (d.quot + (d.rem ? 1 : 0)) * MAX_COPIES;

    uint64_t blocks_total = (file_system.size / file_system.blocksize) - 1;
    if (blocks_needed > blocks_total)
    {
        stegfs_file_delete(file);
        errno = EFBIG; /* file would not fit in the file system */
        return false;
    }
    uint64_t blocks_used = 0;
    for (uint64_t i = 0; i < file_system.size / file_system.blocksize; i++)
        if (file_system.used[i])
            blocks_used++;
    uint64_t blocks_available = blocks_total - blocks_used;
    if (blocks_needed > blocks_available)
    {
        stegfs_file_delete(file);
        errno = ENOSPC; /* file won’t fit in remaining space */
        return false;
    }
    errno = EXIT_SUCCESS;
    return true;
}

extern void stegfs_file_create(const char * const restrict path, bool write)
{
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_name(path, PASSWORD_SEPARATOR);
    file.pass = dir_get_pass(path);
    file.write = write;
    file.time = time(NULL);
    stegfs_cache2_add(NULL, &file);
    return;
}

extern void stegfs_directory_create(const char * const restrict path)
{
    stegfs_cache2_add(path, NULL);
}

extern bool stegfs_file_stat(stegfs_file_t *file)
{
    /*
     * figure out where the file’s inode blocks are
     */
    MHASH hash = hash_init();
    mhash(hash, file->path, strlen(file->path));
    mhash(hash, file->name, strlen(file->name));
    uint8_t *inodes = mhash_end(hash);
    /*
     * calculate inode values; must be done here, so we have all of them
     * and not just the first that we can read
     */
    int available_inodes = MAX_COPIES;
    int corrupt_copies = 0;
    for (int i = 0; i < MAX_COPIES; i++)
        for (int j = (sizeof(uint64_t) / sizeof(uint8_t) ); j >= 0; --j)
            file->inodes[i] = (file->inodes[i] << 0x08) | inodes[i + j];
    /*
     * read file inode, pray for success, then see if we can get a
     * complete copy of the file
     */
    for (int i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT mc = cipher_init(file, i);
        stegfs_block_t inode;
        memset(&inode, 0x00, file_system.blocksize);
        if (block_read(file->inodes[i], &inode, mc, file->path))
        {
            memcpy(&file->size, inode.next, sizeof file->size);
            if ((file->size = ntohll(file->size)) > file_system.size)
            {
                available_inodes--;
                continue;
            }
            file_system.used[file->inodes[i] % (file_system.size / file_system.blocksize)] = true;
            for (int i = 0; i <= MAX_COPIES; i++)
            {
                MCRYPT nc = cipher_init(file, i);
                uint64_t val;
                memcpy(&val, inode.data + i * sizeof val, sizeof val);
                val = ntohll(val);
                if (i < MAX_COPIES)
                {
                    lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
                    uint64_t blocks = d.quot + (d.rem ? 1 : 0);
                    file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
                    memset(file->blocks[i], 0x00, blocks * sizeof blocks);
                    file->blocks[i][0] = blocks;
                    file->blocks[i][1] = val;
                    /*
                     * travsers file block tree; whilst the whole block
                     * is read, the actual file data is discarded
                     */
                    for (uint64_t j = 2 ; j <= blocks; j++)
                    {
                        stegfs_block_t block;
                        memset(&block, 0x00, file_system.blocksize);
                        if (block_read(file->blocks[i][j - 1], &block, nc, file->path))
                        {
                            file_system.used[file->blocks[i][j - 1] % (file_system.size / file_system.blocksize)] = true;
                            memcpy(&val, block.next, sizeof file->blocks[i][j]);
                            file->blocks[i][j] = ntohll(val);
                        }
                        else
                        {
                            /*
                             * note-to-self: because of the memset above, all
                             * incomplete blockchains will have trailing 0’s,
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
     * as long as there’s a valid inode and one complete copy we’re good
     */
    if (available_inodes && corrupt_copies < MAX_COPIES)
    {
        stegfs_cache2_add(NULL, file);
        return true;
    }
    for (int i = 0; i < MAX_COPIES; i++)
        if (file->blocks[i])
        {
            for (uint64_t j = 1; j < file->blocks[i][0]; j++)
                file_system.used[file->blocks[i][j] % (file_system.size / file_system.blocksize)] = false;
            free(file->blocks[i]);
            file->blocks[i] = NULL;
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
        lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
        uint64_t blocks = d.quot + (d.rem ? 1 : 0);
        if (file->blocks[i][0] != blocks)
            continue; /* this copy is corrupt; try the next */
        MCRYPT mc = cipher_init(file, i);
        for (uint64_t j = 1, k = 0; j <= file->blocks[i][0] && file->blocks[i][j]; j++, k++)
        {
            /*
             * we should be largely confident that we’ll be able to read
             * the complete file as outherwise the stat would have
             * failed
             */
            stegfs_block_t block;
            memset(&block, 0x00, sizeof block);
            block_read(file->blocks[i][j], &block, mc, file->path);
            size_t l = sizeof block.data;
            if (l + k * sizeof block.data > file->size)
                l = l - ((l + k * sizeof block.data) - file->size);
            memcpy(file->data + k * sizeof block.data, block.data, l);
        }
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
        stegfs_cache2_add(NULL, file);
        return true;
    }
    /*
     * somehow we failed to read a complete copy of the file, despite
     * knowing that a complete copy existed when stat’d
     */
    return false;
}

extern bool stegfs_file_write(stegfs_file_t *file)
{
    lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
    uint64_t blocks = d.quot + (d.rem ? 1 : 0);
    uint64_t z = file->size;

    if (!stegfs_file_stat(file))
    {
        for (int i = 0; i < MAX_COPIES; i++)
        {
            /*
             * allocate inodes, mark as in use (inode locations are calculated
             * in stegfs_file_stat)
             */
            file_system.used[file->inodes[i] % (file_system.size / file_system.blocksize)] = true;
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
                    /* failed to allocate space; free what we had claimed */
                    for (int k = 0; k <= i; k++)
                    {
                        file_system.used[file->inodes[i] % (file_system.size / file_system.blocksize)] = false;
                        if (file->blocks[k])
                        {
                            for (uint64_t l = 1; l <= j; l++)
                                file_system.used[file->blocks[k][l] % (file_system.size / file_system.blocksize)] = false;
                            free(file->blocks[k]);
                            file->blocks[k] = NULL;
                        }
                    }
                    errno = ENOSPC;
                    return false;
                }
        }
    }
    file->size = z; /* stat can couse size to be reset to 0 */

    if (blocks > file->blocks[0][0]) /* need more blocks than we have */
    {
        for (int i = 0; i < MAX_COPIES; i++)
        {
            file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
            for (uint64_t j = file->blocks[i][0]; j <= blocks; j++)
                if (!(file->blocks[i][j] = block_assign(file->path)))
                {
                    /* failed to allocate space; free what we had claimed */
                    for (int k = 0; k <= i; k++)
                        for (uint64_t l = file->blocks[k][0]; l <= j; l++)
                            file_system.used[file->blocks[k][l] % (file_system.size / file_system.blocksize)] = false;
                    errno = ENOSPC;
                    return false;
                }
        }
        for (int i = 0; i < MAX_COPIES; i++)
            file->blocks[i][0] = blocks;
    }
    else if (blocks < file->blocks[0][0]) /* have more blocks than we need */
        for (int i = 0; i < MAX_COPIES; i++)
        {
            for (uint64_t j = blocks + 1; j <= file->blocks[i][0]; j++)
                block_delete(file->blocks[i][j]);
            file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
            file->blocks[i][0] = blocks;
        }
#if 0
    /*
     * note-to-self: from above, any of the blockchains could have a
     * string of 0’s (possibly in the middle if it’s been expanded) so
     * we need to reassign those now
     */
    for (int i = 0; i < MAX_COPIES; i++)
    {
        for (uint64_t j = 1; j < file->blocks[i][0]; j++)
            if (!file->blocks[i][j] && !(file->blocks[i][j] = block_assign(file->path)))
            {
                stegfs_file_delete(file);
                errno = ENOSPC;
                return false;
            }
        file->blocks[i][blocks + 1] = 0x0;
    }
#endif
    /*
     * write the data
     */
    for (int i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT mc = cipher_init(file, i);
        for (uint64_t j = 1, k = 0; j <= blocks; j++, k++)
        {
            stegfs_block_t block;
            memset(&block, 0x00, sizeof block);
            size_t l = sizeof block.data;
            if (l + k * sizeof block.data > file->size)
                l = l - ((l + k * sizeof block.data) - file->size);
            memset(block.data, 0x00, SIZE_BYTE_DATA);
            memcpy(block.data, file->data + k * sizeof block.data, l);
            z = htonll(file->blocks[i][j + 1]);
            memcpy(block.next, &z, sizeof block.next);
            if (!block_write(file->blocks[i][j], block, mc, file->path))
            {
                /* see below (where inode blocks are written) */
                for (int k = 0; k <= i; k++)
                    for (uint64_t l = 1; l <= j; l++)
                        block_delete(file->blocks[k][l]);
                return false;
            }
        }
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
    }
    /*
     * write file inode blocks
     */
    stegfs_block_t inode;
    memset(&inode, 0x00, sizeof inode);
    uint64_t first[SIZE_LONG_DATA] = { 0x0 };
    for (int i = 0; i < MAX_COPIES; i++)
        first[i] = htonll(file->blocks[i][1]);
    first[MAX_COPIES] = 0x0000000000000000LL; /* TODO think of something to go here */
    first[SIZE_LONG_DATA - 1] = htonll(file->time);
    memcpy(inode.data, first, sizeof first);
    z = htonll(file->size);
    memcpy(inode.next, &z, sizeof file->size);
    for (int i = 0; i < MAX_COPIES; i++)
    {
        MCRYPT mc = cipher_init(file, i);
        if (!block_write(file->inodes[i], inode, mc, file->path))
        {
            for (int j = 0; j <= i; j++)
                /*
                 * it’s likely that if a write failed above, it won’t work
                 * here either, but at least the block will be marked as
                 * available
                 * (in fact if a call to write fails it’s likely all subsequent
                 * write will fail too)
                 */
                block_delete(file->inodes[j]);
            stegfs_file_delete(file);
            return false;
        }
        mcrypt_generic_deinit(mc);
        mcrypt_module_close(mc);
    }

    stegfs_cache2_add(NULL, file);
    return true;
}

extern void stegfs_file_delete(stegfs_file_t *file)
{
    char *p = NULL;
    if (!stegfs_file_stat(file))
        goto rfc;
    lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
    uint64_t blocks = d.quot + (d.rem ? 1 : 0);
    for (int i = 0; i < MAX_COPIES; i++)
    {
        block_delete(file->inodes[i]);
        for (uint64_t j = 1; j <= blocks && file->blocks[i][j]; j++)
            block_delete(file->blocks[i][j]);
    }
rfc:
    asprintf(&p, "%s/%s", path_equals(file->path, DIR_SEPARATOR) ? "" : file->path, file->name);
    stegfs_cache2_remove(p);
    free(p);
    return;
}

/*
 * block functions
 */

static bool block_read(uint64_t bid, stegfs_block_t *block, MCRYPT mc, const char * const restrict path)
{
    errno = EXIT_SUCCESS;
    bid %= (file_system.size / file_system.blocksize);
    if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
    {
        errno = EINVAL;
        return false;
    }
    ssize_t z = pread(file_system.handle, block, sizeof(stegfs_block_t), bid * file_system.blocksize);
    if (z < 0)
        return false;
    MHASH hash;
    void *p;
    /* ignore path check in root */
    if (!path_equals(path, DIR_SEPARATOR))
    {
        /* check path hash */
        hash = hash_init();
        mhash(hash, path, strlen(path));
        p = mhash_end(hash);
        if (memcmp(block->path, p, SIZE_BYTE_PATH))
        {
            free(p);
            return false;
        }
        free(p);
    }
#ifndef __DEBUG__
    /* decrypt block */
    uint32_t sz = SIZE_BYTE_SERPENT * (file_system.blocksize - SIZE_BYTE_PATH) / SIZE_BYTE_SERPENT;
    uint8_t *data = malloc(sz);
    memset(data, 0x00, sz);
    memcpy(data, ((uint8_t *)block) + SIZE_BYTE_PATH, file_system.blocksize - SIZE_BYTE_PATH);
    mdecrypt_generic(mc, data, sz);
    memcpy(((uint8_t *)block) + SIZE_BYTE_PATH, data, file_system.blocksize - SIZE_BYTE_PATH);
    free(data);
#endif
    /* check data hash */
    hash = hash_init();
    mhash(hash, block->data, sizeof block->data);
    p = mhash_end(hash);
    if (memcmp(block->hash, p, SIZE_BYTE_HASH))
    {
        free(p);
        return false;
    }
    free(p);
    return z == sizeof(stegfs_block_t);
}

static bool block_write(uint64_t bid, stegfs_block_t block, MCRYPT mc, const char * const restrict path)
{
    errno = EXIT_SUCCESS;
    bid %= (file_system.size / file_system.blocksize);
    if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
    {
        errno = EINVAL;
        return false;
    }
    if (path_equals(path, DIR_SEPARATOR))
        rand_nonce((uint8_t *)&block.path, SIZE_BYTE_PATH);
    else
    {
        /* compute path hash */
        MHASH hash = hash_init();
        mhash(hash, path, strlen(path));
        void *p = mhash_end(hash);
        memcpy(block.path, p, SIZE_BYTE_PATH);
        free(p);
    }
    /* compute data hash */
    MHASH hash = hash_init();
    mhash(hash, block.data, sizeof block.data);
    void *p = mhash_end(hash);
    memcpy(block.hash, p, SIZE_BYTE_HASH);
    free(p);
#ifndef __DEBUG__
    /* encrypt the data */
    uint32_t sz = SIZE_BYTE_SERPENT * (file_system.blocksize - SIZE_BYTE_PATH) / SIZE_BYTE_SERPENT;
    uint8_t *data = malloc(sz);
    memset(data, 0x00, sz);
    memcpy(data, ((uint8_t *)&block) + SIZE_BYTE_PATH, file_system.blocksize - SIZE_BYTE_PATH);
    mcrypt_generic(mc, data, sz);
    memcpy(((uint8_t *)&block) + SIZE_BYTE_PATH, data, file_system.blocksize - SIZE_BYTE_PATH);
    free(data);
#endif
    return pwrite(file_system.handle, &block, sizeof block, bid * file_system.blocksize) == sizeof block;
}

static void block_delete(uint64_t bid)
{
    bid %= (file_system.size / file_system.blocksize);
    uint8_t *block = malloc(file_system.blocksize);
    /* keep the same path as before */
    pread(file_system.handle, block, file_system.blocksize, bid * file_system.blocksize);
    rand_nonce(block, file_system.blocksize);
    if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
        return;
    pwrite(file_system.handle, block, file_system.blocksize, bid * file_system.blocksize);
    free(block);
    file_system.used[bid] = false;
    return;
}

static bool block_in_use(uint64_t bid, const char * const restrict path)
{
    bid %= (file_system.size / file_system.blocksize);
    if (!bid)
        return true; /* superblock always in use */
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
        MHASH hash = hash_init();
        mhash(hash, p, strlen(p));
        void *h = mhash_end(hash);
        stegfs_block_t block;
        pread(file_system.handle, &block, sizeof block, bid * file_system.blocksize);
        bool r = !memcmp(h, block.path, SIZE_BYTE_PATH);
        free(h);
        if (r)
            return true;
    }
    free(p);
#endif
    return false;
}

/*
 * NB block_assign will return a block number in the range of 0 to UINT64_MAX
 * so that when the value is stored in the file system it’s not mostly
 * 0’s - we don’t want an attacked to know that most of some block of
 * cipher text is 0’s - translation into the valid range is done as
 * necessary by block_ functions
 */
static uint64_t block_assign(const char * const restrict path)
{
    uint64_t block;
    uint64_t tries = 0;
    do
    {
        block = (lrand48() << 32 | lrand48());// % (file_system.size / file_system.blocksize);
        if ((++tries) > file_system.size / file_system.blocksize)
            return 0;
    }
    while (block_in_use(block, path));
    file_system.used[block % (file_system.size / file_system.blocksize)] = true;
    return block;
}

static MCRYPT cipher_init(const stegfs_file_t * const restrict file, uint8_t ivi)
{
    MCRYPT c = mcrypt_module_open(file_system.cipher, NULL, file_system.mode, NULL);
    /* create the initial key for the encryption algorithm */
    uint8_t key[SIZE_BYTE_TIGER] = { 0x00 };
    MHASH hash = hash_init();
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
    hash = hash_init();
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

static MHASH hash_init(void)
{
    for (size_t i = 0; i < mhash_count(); i++)
        if (mhash_get_hash_name_static(i) && !strcasecmp(file_system.hash, (char *)mhash_get_hash_name_static(i)))
            return mhash_init(i);
    return NULL;
}

/*
 * cache functions
 */

/*
 * add path (directory) or file to the cache; if adding a file, the path
 * can be null as it will be taken from the file object
 */
extern void stegfs_cache2_add(const char * const restrict path, stegfs_file_t *file)
{
    stegfs_cache2_t *ptr = NULL;//&(file_system.cache2);
    char *p = NULL;
    if (path)
        p = strdup(path);
    else
        asprintf(&p, "%s/%s", path_equals(file->path, DIR_SEPARATOR) ? "" : file->path, file->name);
    if ((ptr = stegfs_cache2_exists(p, NULL)))
        goto c2a3; /* already in cache */
    ptr = &(file_system.cache2);
    char *name = dir_get_name(p, PASSWORD_SEPARATOR);
    uint16_t d = dir_get_deep(p);
    for (uint16_t i = 1; i < d; i++)
    {
        char *e = dir_get_part(p, i);
        bool found = false;
        for (uint64_t j = 0; j < ptr->ents; j++)
            if (ptr->child[j]->name && !strcmp(ptr->child[j]->name, e))
            {
                ptr = ptr->child[j];
                found = true;
                break;
            }
        if (!found)
        {
            /*
             * use existing, NULL entry in array if available
             */
            uint64_t k = ptr->ents;
            for (uint64_t j = 0; j < ptr->ents; j++)
                if (!ptr->child[j]->name)
                {
                    k = j;
                    goto c2a1;
                }
            ptr->child = realloc(ptr->child, (k + 1) * sizeof(stegfs_cache2_t *));
            ptr->ents++;
c2a1:
            ptr->child[k] = malloc(sizeof(stegfs_cache2_t));
            memset(ptr->child[k], 0x00, sizeof(stegfs_cache2_t));
            ptr->child[k]->name = strdup(e);
            ptr = ptr->child[k];
        }
        free(e);
    }
    /* ditto */
    uint64_t j = ptr->ents;
    for (uint64_t i = 0; i < ptr->ents; i++)
        if (!ptr->child[i]->name)
        {
            j = i;
            goto c2a2;
        }
    ptr->child = realloc(ptr->child, (j + 1) * sizeof(stegfs_cache2_t *));
    ptr->ents++;
c2a2:
    ptr->child[j] = malloc(sizeof(stegfs_cache2_t));
    memset(ptr->child[j], 0x00, sizeof(stegfs_cache2_t));
    ptr = ptr->child[j];
    ptr->name = name;
c2a3:
    if (file && file != ptr->file)
    {
        if (!ptr->file)
        {
            ptr->file = malloc(sizeof(stegfs_file_t));
            memset(ptr->file, 0x00, sizeof(stegfs_file_t));
        }
        /* set path and name */
        asprintf(&ptr->file->path, "%s", file->path);
        asprintf(&ptr->file->name, "%s", file->name);
        asprintf(&ptr->file->pass, "%s", file->pass);
        /* set inodes */
        for (int i = 0; i < MAX_COPIES; i++)
            ptr->file->inodes[i] = file->inodes[i];
        /* set time and size */
            ptr->file->write = file->write;
        ptr->file->time = file->time;
        ptr->file->size = file->size;
        if (ptr->file->size)
        {
            /* copy data */
            if (file->data)
            {
                ptr->file->data = realloc(ptr->file->data, ptr->file->size);
                memcpy(ptr->file->data, file->data, ptr->file->size);
            }
            /* copy blocks */
            lldiv_t d = lldiv(file->size, SIZE_BYTE_DATA);
            uint64_t blocks = d.quot + (d.rem ? 1 : 0);
            for (int i = 0; i < MAX_COPIES; i++)
            {
                ptr->file->blocks[i] = realloc(ptr->file->blocks[i], (blocks + 1) * sizeof blocks);
                memset(ptr->file->blocks[i], 0x00, blocks * sizeof blocks);
                ptr->file->blocks[i][0] = blocks;
                for (uint64_t j = 1 ; j <= blocks && file->blocks[i] && file->blocks[i][j]; j++)
                    ptr->file->blocks[i][j] = file->blocks[i][j];
            }
        }
    }
    free(p);
    return;
}

/*
 * Check whether a given path is in the cache, copy the file object if
 * it’s a file, and return a pointer to the original
 */
extern stegfs_cache2_t *stegfs_cache2_exists(const char * const restrict path, stegfs_cache2_t *entry)
{
    stegfs_cache2_t *ptr = &(file_system.cache2);
    char *name = dir_get_name(path, PASSWORD_SEPARATOR);
    uint16_t d = dir_get_deep(path);
    for (uint16_t i = 1; i < d; i++)
    {
        bool found = false;
        char *e = dir_get_part(path, i);
        for (uint64_t j = 0; j < ptr->ents; j++)
            if (ptr->child[j]->name && !strcmp(ptr->child[j]->name, e))
            {
                ptr = ptr->child[j];
                found = true;
                break;
            }
        free(e);
        if (!found)
            return NULL;
    }
    for (uint64_t i = 0; i < ptr->ents; i++)
        if (ptr->child[i]->name && !strcmp(ptr->child[i]->name, name))
        {
            if (entry)
                memcpy(entry, ptr->child[i], sizeof(stegfs_cache2_t));
            return ptr->child[i];
        }
    return NULL;
}

extern void stegfs_cache2_remove(const char * const restrict path)
{
    stegfs_cache2_t *ptr = &(file_system.cache2);
    char *name = dir_get_name(path, PASSWORD_SEPARATOR);
    uint16_t d = dir_get_deep(path);
    for (uint16_t i = 1; i < d; i++)
    {
        bool found = false;
        char *e = dir_get_part(path, i);
        for (uint64_t j = 0; j < ptr->ents; j++)
            if (ptr->child[j]->name && !strcmp(ptr->child[j]->name, e))
            {
                ptr = ptr->child[j];
                found = true;
                break;
            }
        free(e);
        if (!found)
            return;
    }
    bool found = false;
    for (uint64_t i = 0; i < ptr->ents; i++)
        if (ptr->child[i]->name && !strcmp(ptr->child[i]->name, name))
        {
            ptr = ptr->child[i];
            found = true;
            break;
        }
    if (!found)
        return;
    if (ptr->file)
    {
        free(ptr->file->path);
        free(ptr->file->name);
        free(ptr->file->pass);
        if (ptr->file->data)
            free(ptr->file->data);
        for (int i = 0; i < MAX_COPIES; i++)
            if (ptr->file->blocks[i])
            {
#if 0
                for (uint64_t j = 1; j < file->blocks[i][0]; j++)
                    if (file->blocks[i][j])
                        file_system.used[file->blocks[i][j] % (file_system.size / file_system.blocksize)] = false;
#endif
                free(ptr->file->blocks[i]);
                ptr->file->blocks[i] = NULL;
            }
        free(ptr->file);
    }
    if (ptr->child)
    {
        for (uint64_t i = 0; i < ptr->ents; i++)
            if (ptr->child[i])
            {
                char *c = NULL;
                asprintf(&c, "%s/%s", path, ptr->child[i]->name);
                stegfs_cache2_remove(c);
                free(c);
            }
        free(ptr->child);
    }
    free(ptr->name);
    ptr->name = NULL;
    return;
}
