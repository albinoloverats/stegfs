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

    #include <inttypes.h>
    #include <stdbool.h>
    #include <unistd.h>
    #include <mhash.h>
    #include <mcrypt.h>
    #include <pthread.h>

    #include "common/list.h"

    #ifndef EDIED /* EDIED is more fitting, but not always defined */
        #define EDIED ENODATA
    #endif /* ! EDIED */

    /* size (in bytes) for various blocks of data */
    #define SIZE_BYTE_SERPENT   0x10                      /*  16 bytes -- 128 bits */
    #define SIZE_BYTE_TIGER     0x18                      /*  24 bytes -- 192 bits */

    #define SIZE_BYTE_BLOCK     0x80                      /* 128 bytes -- full block */
    #define SIZE_BYTE_PATH      SIZE_BYTE_TIGER * 2 / 3   /*  16 bytes -- 2/3 size of full tiger hash */
    #define SIZE_BYTE_DATA      0x50                      /*  80 bytes */
    #define SIZE_BYTE_HASH      SIZE_BYTE_TIGER           /*  24 bytes */
    #define SIZE_BYTE_NEXT      0x08                      /*   8 bytes */

    #define RATIO_DATA_BLOCK    SIZE_BYTE_DATA / SIZE_BYTE_BLOCK

    /* size in 64bit ints of parts of block */
    #define SIZE_LONG_PATH 0x02
    #define SIZE_LONG_DATA 0x0A
    #define SIZE_LONG_HASH 0x03
    #define SIZE_LONG_NEXT 0x01

    #define BLOCKS_PER_MB 0x2000

    #define RATIO_BYTE_MB   0x00100000      /* size in bytes of 1 MB */
    #define RATIO_MB_GB     0x00000400      /* size in MB of 1 GB */
    #define RATIO_MB_TB     RATIO_BYTE_MB   /* size in MB of 1 TB (1:1 as B:MB) */

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

    /*!
     * \brief  Structure to hold file system information
     *
     * Static data structure stored in the backend, which holds information
     * about the currently mounted file system
     */
    typedef struct stegfs_file_system_t
    {
        uint64_t id;     /*!< File system ID */
        uint64_t size;   /*!< Size of file system in bytes */
        uint64_t blocks; /*!< Size of file system in blocks */
        uint8_t *bitmap; /*!< Optional pointer to bitmap of in-use blocks */
        list_t  *files;  /*!< Optional pointer to list of known files */
    }
    stegfs_fs_info_t;

    /*!
     * \brief  File access modes for files in a stegfs
     *
     * Enumeration to indicate how the file was opened and which I/O operations
     * are allowed
     */
    typedef enum stegfs_mode_e
    {
        STEGFS_READ, /*!< File opened for Read access */
        STEGFS_WRITE /*!< File opened for Write access */
    }
    stegfs_mode_e;

    /*!
     * \brief  Structure to hold information about a file
     *
     * Public file information structure containing an easy-to-access
     * representation of all important details about a file
     */
    typedef struct stegfs_file_t
    {
        uint64_t      id;   /*!< File ID */
        uint64_t      size; /*!< File size */
        time_t        time; /*!< Last access timestamp */
        stegfs_mode_e mode; /*!< Access mode R/W */
        char         *name; /*!< File name component of /path/file:password */
        char         *path; /*!< Full path component of /path/file:password */
        char         *pass; /*!< Password component of /path/file:password */
        void         *data; /*!< File data */
    }
    stegfs_file_t;

    /*!
     * \brief  Structure for each file system block
     *
     * Simple structure which represents an individual file system data block
     */
    typedef struct stegfs_block_t
    {
        uint64_t path[SIZE_LONG_PATH]; /*!< Hash of block path   (16 bytes) */
        uint8_t  data[SIZE_BYTE_DATA]; /*!< Block data           (80 bytes) */
        uint64_t hash[SIZE_LONG_HASH]; /*!< Hash of block data   (24 bytes) */
        uint64_t next[SIZE_LONG_NEXT]; /*!< Address of next block (8 bytes) */
    }
    stegfs_block_t;

    /*!
     * \brief  Structure for file system cache object
     *
     * Internal data structure for cache known files and their position within
     * the file system
     */
    typedef struct stegfs_cache_t
    {
        uintptr_t id;   /*!< File cache ID */
        char     *name; /*!< File name component of /path/file:password */
        char     *path; /*!< Full path component of /path/file:password */
    }
    stegfs_cache_t;

    /*!
     * \brief          Initialise stegfs library, set internal data structures
     * \param[in]  fs  Name and path to file system
     * \param[in]  c   Whether to cache 'in-use' status of block
     *
     * Initialise the file system and popular static information structures,
     * making note whether to cache file details
     */
    extern void lib_stegfs_init(const char * const restrict fs, const bool c) __attribute__((nonnull(1)));

    /*!
     * \brief                Retrieve information about the file system
     * \param[in,out]  info  Pointer to file system info structure
     *
     * Retrieve useful information about the file system from the backend.
     * Details returned are the file system ID, size in bytes, and size in
     * blocks
     */
    extern void lib_stegfs_info(stegfs_fs_info_t * const restrict info) __attribute__((nonnull(1)));

    extern list_t *lib_stegfs_cache_get(void);
    extern uint8_t *lib_stegfs_cache_map(void);

    /*
     * stat'ing a file is different - failure is indicated with a return value of 0
     * because all positive values are allowable inode values; 0 is the only invalid
     * inode
     */
    extern uint64_t lib_stegfs_stat(stegfs_file_t *, stegfs_block_t *);
    extern int64_t lib_stegfs_kill(stegfs_file_t *);
    /*
     * load/save return values < 0 for an error; this value can then directly be used
     * as the return value from the fuse functions
     */
    extern int64_t lib_stegfs_load(stegfs_file_t *);
    extern int64_t lib_stegfs_save(stegfs_file_t *);

#endif /* ! _LIB_STEGFS_H_ */

#ifdef _IN_LIB_STEGFS_
    #include <mcrypt.h>

    static void lib_stegfs_init_hash(stegfs_file_t *, void *, void *);
    static MCRYPT lib_stegfs_init_crypt(stegfs_file_t *, uint8_t);

    static void lib_stegfs_cache_add(const stegfs_file_t * const restrict file) __attribute__((nonnull(1)));
    static void lib_stegfs_cache_del(const stegfs_file_t * const restrict file) __attribute__((nonnull(1)));

    static int64_t lib_stegfs_block_save(uint64_t, MCRYPT, stegfs_block_t *);
    static int64_t lib_stegfs_block_load(uint64_t, MCRYPT, stegfs_block_t *);

    static uint64_t lib_stegfs_block_find(const char * const restrict path) __attribute__((nonnull(1)));
    static bool lib_stegfs_block_ours(const uint64_t offset, const uint64_t * const restrict hash) __attribute__((nonnull(2)));

    static void lib_stegfs_random_seed(void);

    static stegfs_fs_info_t *file_system;
//    static pthread_mutex_t stegfs_lock;

    #undef _IN_LIB_STEGFS_
#endif
