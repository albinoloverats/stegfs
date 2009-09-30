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

#ifndef _VSTEGFS_H_
#define _VSTEGFS_H_

#define APP "vstegfs"
#define VER "200910"

#ifndef EDIED /* EDIED is more fitting, but not always defined */
#define EDIED ENODATA
#endif

/* size (in bytes) for various blocks of data */
#define SB_SERPENT 0x10               /*  16 bytes -- 128 bits */
#define SB_TIGER   0x18               /*  24 bytes -- 192 bits */

#define SB_BLOCK   0x80               /* 128 bytes -- full block */
#define SB_PATH    SB_TIGER * 2 / 3   /*  16 bytes -- 2/3 size of full tiger hash */
#define SB_DATA    0x50               /*  80 bytes */
#define SB_HASH    SB_TIGER           /*  24 bytes */
#define SB_NEXT    0x08               /*   8 bytes */

/* size in 64bit ints of parts of block */
#define SL_PATH 0x02
#define SL_DATA 0x0A
#define SL_HASH 0x03
#define SL_NEXT 0x01

#define BLOCK_PER_MB 0x2000

#define SB_1MB 0x00100000 /* size in bytes of 1 MB */
#define SM_1GB 0x00000400 /* size in MB of 1 GB */
#define SM_1TB SB_1MB     /* size in MB of 1 TB (1:1 as B:MB) */

#define MAX_COPIES 9

#define ROOT_PATH APP

#define SUPER_ID ROOT_PATH " " VER
#define MAGIC_0  0xA157AFA602CC9D1BLL
#define MAGIC_1  0x33BE2B298B76F2ACLL
#define MAGIC_2  0xC903284D7C593AF6LL

#define ONE_MILLION 1000000
#define SS_48B 3

#define ARGS_MINIMUM 2
#define ARGS_DEFAULT 4

typedef struct vstat_t
{
    uint64_t  fs;
    FILE     *file;
    uint64_t *size;
    time_t   *time;
    char     *name;
    char     *path;
    char     *pass;
}
vstat_t;

typedef struct vblock_t
{
    uint64_t path[SL_PATH]; /*  16 bytes */
    uint8_t  data[SB_DATA]; /*  80 bytes */
    uint64_t hash[SL_HASH]; /*  24 bytes */
    uint64_t next[SL_NEXT]; /*   8 bytes */
}
vblock_t;

typedef struct vlist_t
{
    char *name;
    char *path;
}
vlist_t;

extern   void   vstegfs_init(int64_t, const char *, bool);
extern  int64_t vstegfs_save(vstat_t);
extern  int64_t vstegfs_load(vstat_t);
extern uint64_t vstegfs_find(vstat_t);
extern  int64_t vstegfs_kill(vstat_t);
extern   char **vstegfs_known_list(const char *);

#endif /* ! _VSTEGFS_H_ */

#ifdef _VSTEG_S_
static   void   add_known_list(vstat_t);
static   void   del_known_list(vstat_t);
static MCRYPT   vstegfs_crypt_init(vstat_t *, uint8_t);
static uint64_t vstegfs_header(vstat_t *, vblock_t *);
static  int64_t block_open(uint64_t, uint64_t, MCRYPT, vblock_t *);
static  int64_t vstegfs_block_save(uint64_t, uint64_t, MCRYPT, vblock_t *);
static   bool   is_block_ours(uint64_t, uint64_t, uint64_t *);
static uint64_t calc_next_block(uint64_t, char *);
static   void   rng_seed(void);
#endif /* _VSTEG_S_ */
