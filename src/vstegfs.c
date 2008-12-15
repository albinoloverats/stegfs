/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2008, albinoloverats ~ Software Development
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "vstegfs.h"
#include "dir.h"
#include "tiger.h"
#include "serpent.h"

extern uint64_t filesystem;
extern uint64_t filesystem_size;

/*
 * main functions for writing files to and from from the file system
 */

uint32_t file_write(FILE *file, size_t fsize, char *filename, char *filepath, char *password)
{
    errno = EXIT_SUCCESS;
    uint8_t *data = calloc(DATA, sizeof (char));;
    uint64_t first = 0, current = 0, next = 0;
    srand48(time(0));

    if ((fsize * COPIES / DATA) + (HEADERS * BLOCK) > filesystem_size)
    {
        //fprintf(stderr, "Insufficient space available\n");
        return ENOSPC;
    }
    /*
     * create the key/subkeys and set the IV
     */
    char *key_mat = NULL;
    uint8_t *IV = calloc(SERPENT_B, sizeof (char));
    asprintf(&key_mat, "%s\255%s\255%s", filename, password, filepath);
    uint32_t *subkeys = generate_key(key_mat);
    /* 
     * start the header blocks
     */
    uint64_t *start = calloc(COPIES, sizeof (uint64_t));
    uint64_t *headers = hash_data((uint8_t *)key_mat, strlen(key_mat));
    for (uint32_t i = 0; i < HEADERS; i++)
    {
        lseek64(filesystem, (headers[i] % filesystem_size) * BLOCK, SEEK_SET);
        for (uint32_t j = 0; j < DATA; j++)
            data[j] = 0xFF;
        block_write(filepath, data, fsize, IV, subkeys);
    }
    /* 
     * each file is stored 10x to try and ensure data survival
     */
    for (uint32_t i = 0; i < COPIES; i++)
    {
        memset(IV, 0x14 * i, SERPENT_B);
        /* 
         * remember the first block so we can write the header correctly at the end
         */
        for (uint32_t j = 0; j < 2; j++)
            first = (first << WORD) | (uint32_t)mrand48();
        first %= filesystem_size;
        current = first;
        start[i] = first;
        fseek(file, 0, SEEK_SET);
        while ((fread(data, sizeof (char), DATA, file)) > 0)
        {
            uint64_t failed = 0; // if this values exceeds the size of the file system assume there is no space left
            /* 
             * compute the next block number and check if we might have used it before
             */
            do
            {
                for (uint32_t j = 0; j < 2; j++)
                    next = (next << WORD) | (uint32_t)mrand48();
                next %= filesystem_size; // ensure that the 'new' block is within the limits of the current file system
                if (failed > filesystem_size)
                    return ENOSPC;
                else
                    failed++;
            }
            while (block_check(filepath, next));
            lseek64(filesystem, current * BLOCK, SEEK_SET);
            block_write(filepath, data, next, IV, subkeys);
            current = next;
            /* 
             * clear data before next use
             */
            for (uint32_t j = 0; j < DATA; j++)
                data[j] = 0x00;
        }
    }

    /* 
     * now that's done, we can finish the header blocks
     */
    memset(IV, 0, SERPENT_B);
    for (uint32_t i = 0; i < HEADERS; i++)
    {
        lseek64(filesystem, (headers[i] % filesystem_size) * BLOCK, SEEK_SET);
        uint32_t z = 0;

        for (uint32_t j = 0; j < COPIES; j++)
            for (uint32_t k = 0; k < BYTE; k++)
                data[z++] = (start[j] & (0xFFLL << (56 - (k * BYTE)))) >> (56 - (k * BYTE));
        block_write(filepath, data, fsize, IV, subkeys);
    }
    return errno;
}

uint32_t file_read(FILE *file, char *filename, char *filepath, char *password)
{
    errno = EXIT_SUCCESS;
    uint8_t *data = calloc(DATA, sizeof (char));;
    uint64_t current = 0;
    uint64_t next = 0;
    short failed = FALSE;

    srand48(time(0));
    uint64_t fsize = 0;

    /*
     * create the key/subkeys and set the IV
     */
    char *key_mat = NULL;
    uint8_t *IV = calloc(SERPENT_B, sizeof (char));
    asprintf(&key_mat, "%s\255%s\255%s", filename, password, filepath);
    uint32_t *subkeys = generate_key(key_mat);

    /* 
     * find the header blocks
     */
    uint64_t *start = calloc(COPIES, sizeof (uint64_t));
    uint64_t *headers = hash_data((uint8_t *)key_mat, strlen(key_mat));
    for (uint32_t i = 0; i < HEADERS; i++)
    {
        if (block_check(filepath, headers[i] % filesystem_size))
        {
            lseek64(filesystem, (headers[i] % filesystem_size) * BLOCK, SEEK_SET);
            if ((fsize = block_read(filepath, data, IV, subkeys)) > 0)
                break;
        }
    }
    if (!fsize)
    {
        //fprintf(stderr, "Failed to find uncorrupt file header\n");
        return ENOENT;
    }
    /* 
     * from data, we can now extract the starting blocks for each branch
     */
    memset(IV, 0, SERPENT_B);
    for (uint32_t i = 0; i < COPIES; i++)
    {
        uint64_t k = 0;
        for (uint32_t j = 0; j < BYTE; j++)
            k = (k << 8) | data[j + (i * 8)];
        start[i] = k;
    }
    /* 
     * calculate the number of blocks used
     */
    div_t result;
    result = div(fsize, DATA);
    uint64_t fblocks = result.quot;
    uint64_t frem = result.rem;

    if (frem)
        fblocks++;
    /* 
     * now we check through each copy of the file to try and extract it
     */
    uint32_t i;
    for (i = 0; i < COPIES; i++)
    {
        memset(IV, 0x14 * i, SERPENT_B);
        failed = FALSE;
        current = start[i];
        fseek(file, 0, SEEK_SET);
        for (uint32_t j = 0; j < fblocks; j++)
        {
            if (block_check(filepath, current))
            {
                lseek64(filesystem, current * BLOCK, SEEK_SET);
                if ((next = block_read(filepath, data, IV, subkeys)))
                {
                    if (j == (fblocks - 1))
                        fwrite(data, sizeof (char), frem, file);
                    else
                        fwrite(data, sizeof (char), DATA, file);
                }
                else
                    failed = TRUE;
            }
            else
                failed = TRUE;
            if (failed)
            {
//#ifdef __amd64__
                //fprintf(stderr, "Copy %i is corrupted at block %i/%lu\n", i + 1, j + 1, fblocks + 1);
//#else
                //fprintf(stderr, "Copy %i is corrupted at block %i/%llu\n", i + 1, j + 1, fblocks + 1);
//#endif
                break;
            }
            current = next;
        }
        if (!failed)
            break; // we found a full copy of the file :)
    }
    if (failed)
    {
        //fprintf(stderr, "Failed to find full copy of file to extract\n");
        errno = EIO;
    }// else
        //fprintf(stdout, "Finished successful recovery of copy %i\n", i + 1);
    return errno;
}

uint32_t file_check(char *filename, char *filepath, char *password)
{
    /*
     * create the key/subkeys and set the IV
     */
    char *key_mat = NULL;
    uint8_t *data = calloc(DATA, sizeof (char));;
    uint8_t *IV = calloc(SERPENT_B, sizeof (char));
    asprintf(&key_mat, "%s\255%s\255%s", filename, password, filepath);
    uint32_t *subkeys = generate_key(key_mat);
    /* 
     * return the size found in the first header block
     * (note that it might not actually exist)
     */
    uint64_t *headers = hash_data((uint8_t *)key_mat, strlen(key_mat));
    lseek64(filesystem, (headers[0] % filesystem_size) * BLOCK, SEEK_SET);
    return block_read(filepath, data, IV, subkeys);
}

uint64_t *file_find(char *filename, char *filepath, char *password)
{
    /*
     * create the key
     */
    char *key_mat = NULL;
    asprintf(&key_mat, "%s\255%s\255%s", filename, password, filepath);
    /* 
     * return the hashed header blocks
     */
    return hash_data((uint8_t *)key_mat, strlen(key_mat));
}

uint32_t file_unlink(char *filename, char *filepath, char *password)
{
    /*
     * create the key/subkeys and set the IV
     */
    char *key_mat = NULL;
    asprintf(&key_mat, "%s\255%s\255%s", filename, password, filepath);
    /* 
     * corrupt the header blocks so the file cannot be found
     */
    srand48(time(0));
    uint64_t *headers = hash_data((uint8_t *)key_mat, strlen(key_mat));
    uint8_t *block = calloc(BLOCK, sizeof (char));

    for (uint32_t i = 0; i < HEADERS; i++)
    {
        if (block_check(filepath, headers[i] % filesystem_size))
        {
            lseek64(filesystem, (headers[i] % filesystem_size) * BLOCK, SEEK_SET);
            for (uint32_t i = 0; i < BLOCK; i++)
                block[i] = (char)(lrand48() & 0xFF);
            write(filesystem, block, BLOCK);
        }
    }
    free(block);

    return EXIT_SUCCESS;
}


/*
 * functions for writing and reading a single block
 */

uint32_t block_write(char *filepath, uint8_t *data, uint64_t next, uint8_t *IV, uint32_t *subkeys)
{
    errno = EXIT_SUCCESS;
    uint8_t *block = calloc(BLOCK, sizeof (char));

//    for (uint32_t i = 0; i < GROUP; i++)
//        block[i] = (uint32_t)mrand48() % 0xFF;

    memmove(block + OFF_DATA, data, DATA);
    memmove(block + OFF_CHECKSUM, hash_data(data, DATA), CHECKSUM);
    next = check_endian(next);
    memmove(block + OFF_NEXT, &next, NEXT);

    block_encrypt(block, IV, subkeys);

    memmove(block + OFF_GROUP, hash_path((uint8_t *)filepath, strlen(filepath)), GROUP);
    write(filesystem, block, BLOCK);
    free(block);
    return errno;
}

uint64_t block_read(char *filepath, uint8_t *data, uint8_t *IV, uint32_t *subkeys)
{
    uint64_t next = 0;
    uint8_t *eblck = calloc(BLOCK, sizeof (char));
    uint8_t *block = calloc(BLOCK, sizeof (char));
    uint8_t *hash = calloc(CHECKSUM, sizeof (char));
    read(filesystem, eblck, BLOCK);

    memcpy(block + OFF_DATA, eblck + OFF_DATA, BLOCK - GROUP);
    block_decrypt(block, IV, subkeys);
    
    memmove(data, block + OFF_DATA, DATA);
    memmove(hash, block + OFF_CHECKSUM, CHECKSUM);
    if (memcmp(hash, hash_data(data, DATA), TIGER_B) != 0)
        return 0;
    memmove(&next, block + OFF_NEXT, NEXT);
    next = check_endian(next);

    free(block);
    free(eblck);
    free(hash);
    return next;
}

uint32_t block_check(char *filepath, uint64_t next)
{
    uint8_t *buf = calloc(TIGER128_B, sizeof (char));
    char *dir = NULL;
    short ret = FALSE;
    lseek64(filesystem, next * BLOCK, SEEK_SET);
    read(filesystem, buf, TIGER128_B);
    lseek64(filesystem, next * BLOCK, SEEK_SET);
    /* 
     * memcmp returns 0 if the values are the same;
     */
    uint32_t j = dir_count_sub(filepath);
    for (uint32_t i = 1; i <= j; i++)
    {
        asprintf(&dir, "%s/%s", dir ? dir : "", dir_get_part(filepath, i));
        uint8_t *tmp = hash_path((uint8_t *)dir, strlen(dir));
        if (memcmp(buf, tmp, TIGER128_B) == 0)
            ret = TRUE;
        free(tmp);
    }
    free(dir);
    free(buf);
    return ret;
}


/*
 * functions for encrypting and decrypting a single block
 */

void block_encrypt(uint8_t *block, uint8_t *IV, uint32_t *subkeys)
{
    uint8_t *plaintext = calloc(SERPENT_B, sizeof (char));
    uint8_t *ciphertext = calloc(SERPENT_B, sizeof (char));
    for (uint32_t i = 1; i < 8; i++)
    {
        memmove(plaintext, block + (SERPENT_B * i), SERPENT_B);

        uint32_t t[4], pt[4], ct[4];
        memcpy(pt, plaintext, CHUNK_SERPENT);
        memcpy(t, IV, CHUNK_SERPENT);
        for (uint32_t i = 0; i < 4; i++)
            pt[i] ^= t[i];
        serpent_encrypt(pt, ct, subkeys);
        memcpy(ciphertext, ct, CHUNK_SERPENT);
        memcpy(IV, ciphertext, CHUNK_SERPENT);

        memmove(block + (SERPENT_B * i), ciphertext, SERPENT_B);
    }
    free(plaintext);
    free(ciphertext);
}

void block_decrypt(uint8_t *block, uint8_t *IV, uint32_t *subkeys)
{
    uint8_t *plaintext = calloc(SERPENT_B, sizeof (char));
    uint8_t *ciphertext = calloc(SERPENT_B, sizeof (char));
    for (uint32_t i = 1; i < 8; i++)
    {
        memmove(ciphertext, block + (SERPENT_B * i), SERPENT_B);

        uint32_t t[4], pt[4], ct[4];
        memcpy(ct, ciphertext, CHUNK_SERPENT);
        serpent_decrypt(ct, pt, subkeys);
        memcpy(t, IV, CHUNK_SERPENT);
        for (uint32_t i = 0; i < 4; i++)
            pt[i] ^= t[i];
        memcpy(IV, ct, CHUNK_SERPENT);
        memcpy(plaintext, pt, CHUNK_SERPENT);

        memmove(block + (SERPENT_B * i), plaintext, SERPENT_B);
    }
    free(plaintext);
    free(ciphertext);
}


/*
 * miscellaneous functions
 */

void *hash_data(uint8_t *data, uint64_t len)
{
    uint64_t *sum = calloc(3, sizeof (uint64_t));
    tiger((uint64_t *)data, len, sum);
    for (uint32_t i = 0; i < 3; i++)
        sum[i] = check_endian(sum[i]);
    return sum;
}

void *hash_path(uint8_t *filepath, uint64_t len)
{
    uint8_t chr[1];
    uint32_t val = 0;
    for (uint32_t i = 0; i < len; i++)
        val += filepath[i];
    chr[0] = (val % 0x08) + 48;
    return hash_data(chr, 1);
}

uint64_t check_endian(uint64_t val)
{
    uint64_t ret = 0;
#ifdef LITTLE_ENDIAN
    ret |= (val & (0xFFLL  << 56)) >> 56;
    ret |= (val & (0xFFLL  << 48)) >> 40;
    ret |= (val & (0xFFLL  << 40)) >> 24;
    ret |= (val & (0xFFLL  << 32)) >>  8;
    ret |= (val & (0xFFLL  << 24)) <<  8;
    ret |= (val & (0xFFLL  << 16)) << 24;
    ret |= (val & (0xFFLL  <<  8)) << 40;
    ret |= (val &  0xFFLL) << 56;
#else
    ret = val;
#endif
    return ret;
}
