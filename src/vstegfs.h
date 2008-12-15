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

#ifndef _VSTEGFS_H_
#define _VSTEGFS_H_

#include <inttypes.h>

// #define MAX_SIZE 0xFFFFFFFFFFFFFFFF // maximum number of blocks

#define BLOCKS      8192        // blocks per MB 0x2000
#define BLOCK        128        // block size in bytes 0x0080

#define BYTE           8        // size in bits
#define WORD          32        // size in bits
#define DWORD         64        // size in bits

#define CHUNK         16        // 128 bits, in bytes

#define GROUP         16        // bytes per block
#define DATA          80        // bytes per block
#define CHECKSUM      24        // bytes per block
#define NEXT           8        // bytes per block

#define OFF_GROUP      0        // bytes offset
#define OFF_DATA      16        // bytes offset
#define OFF_CHECKSUM  96        // bytes offset
#define OFF_NEXT     120        // bytes offset

#define TIGER        192
#define TIGER_B       24
#define TIGER128     128
#define TIGER128_B    16

#define SERPENT      128
#define SERPENT_B     16

#define COPIES        10        // number of redundent copies of each file
#define HEADERS        3        // number of copies of the header block

#define TRUE           1
#define FALSE          0

typedef struct v_block
{
    uint8_t path[0x10];
    uint8_t data[0x50];
    uint8_t hash[0x18];
    uint64_t next;
}
v_block;


/*
 * main functions
 */

uint32_t file_write(FILE *, size_t, char *, char *, char *);
uint32_t file_read(FILE *, char *, char *, char *);
uint32_t file_check(char *, char *, char *);
uint64_t *file_find(char *, char *, char *);
uint32_t file_unlink(char *, char *, char *);

uint32_t block_write(char *, uint8_t *, uint64_t, uint8_t *, uint32_t *);
uint64_t block_read(char *, uint8_t *, uint8_t *, uint32_t *);
uint32_t block_check(char *, uint64_t);

void block_encrypt(uint8_t *, uint8_t *, uint32_t *);
void block_decrypt(uint8_t *, uint8_t *, uint32_t *);

void *hash_data(uint8_t *, uint64_t);
void *hash_path(uint8_t *, uint64_t);

uint64_t check_endian(uint64_t);


/*
 * common application functions
 */

void show_help(void);
void show_licence(void);
void show_usage(void);
void show_version(void);

#define RELEASE 2

#endif /* _VSTEGFS_H_ */
