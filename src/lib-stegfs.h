/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright (c) 2007-2010, albinoloverats ~ Software Development
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

#ifndef _LIB_STEGFS_H_
    #define _LIB_STEGFS_H_

    #include "common/list.h"

    #ifndef EDIED /* EDIED is more fitting, but not always defined */
        #define EDIED ENODATA
    #endif /* ! EDIED */

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

    #define PATH_ROOT "stegfs"

    #define MAX_COPIES 9

    #define INIT_HEADER { 0x0000, 0x0000, 0x0000,\
                          0x0000, 0x0000, 0x0000,\
                          0x0000, 0x0000, 0x0000 }

    #define INIT_PATH { 0x0000000000000000LL, 0x0000000000000000LL }

    #define SUPER_ID PATH_ROOT " " VER

    #define MAGIC_0  0xA157AFA602CC9D1BLL
    #define MAGIC_1  0x33BE2B298B76F2ACLL
    #define MAGIC_2  0xC903284D7C593AF6LL

    #define MAX_BLOCK_LOOKUP (int16_t)4096 /* 32767 */

    #define STEGFS_LOCK(M)     pthread_mutex_lock(&M)
    #define STEGFS_UNLOCK(M)   pthread_mutex_unlock(&M)

    typedef struct stegfs_file_system_t
    {
        uint64_t id;
        uint64_t size;
        uint64_t blocks;
        uint8_t *bitmap;
        list_t  *files;
    }
    stegfs_fs_info_t;

    typedef enum stegfs_mode_e
    {
        STEGFS_READ,
        STEGFS_WRITE
    }
    stegfs_mode_e;

    typedef struct stegfs_file_t
    {
        uintptr_t     id;
        uint64_t      size;
        time_t        time;
        stegfs_mode_e mode;
        char         *name;
        char         *path;
        char         *pass;
        void         *data;
    }
    stegfs_file_t;

    typedef struct stegfs_block_t
    {
        uint64_t path[SL_PATH]; /*  16 bytes */
        uint8_t  data[SB_DATA]; /*  80 bytes */
        uint64_t hash[SL_HASH]; /*  24 bytes */
        uint64_t next[SL_NEXT]; /*   8 bytes */
    }
    stegfs_block_t;

    typedef struct stegfs_cache_t
    {
        uintptr_t id;
        char     *name;
        char     *path;
    }
    stegfs_cache_t;

    extern void lib_stegfs_init(const char *, bool);
    extern void lib_stegfs_info(stegfs_fs_info_t *);

    extern list_t *lib_stegfs_cache_get(void);
    extern uint8_t *lib_stegfs_cache_map(void);

    extern int64_t lib_stegfs_stat(stegfs_file_t *, stegfs_block_t *);
    extern int64_t lib_stegfs_kill(stegfs_file_t *);
    extern int64_t lib_stegfs_load(stegfs_file_t *);
    extern int64_t lib_stegfs_save(stegfs_file_t *);

#endif /* ! _LIB_STEGFS_H_ */

#ifdef _IN_LIB_STEGFS_
    #include <mcrypt.h>

    static void lib_stegfs_init_hash(stegfs_file_t *, void *, void *);
    static MCRYPT lib_stegfs_init_crypt(stegfs_file_t *, uint8_t);

    static void lib_stegfs_cache_add(stegfs_file_t *);
    static void lib_stegfs_cache_del(stegfs_file_t *);

    static int64_t lib_stegfs_block_save(uint64_t, MCRYPT, stegfs_block_t *);
    static int64_t lib_stegfs_block_load(uint64_t, MCRYPT, stegfs_block_t *);

    static uint64_t lib_stegfs_block_find(char *);
    static bool lib_stegfs_block_ours(uint64_t, uint64_t *);

    static stegfs_fs_info_t *file_system;
#endif /* _IN_LIB_STEGFS_ */
