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

#ifndef _STEGFS_H_
#define _STEGFS_H_

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>

#define STEGFS_NAME    "stegfs"
#define STEGFS_VERSION "2015.00"

/* size (in bytes) for various blocks of data */
#define SIZE_BYTE_SERPENT   0x10                      /*    16 bytes -- 128 bits */
#define SIZE_BYTE_TIGER     0x18                      /*    24 bytes -- 192 bits */

#define SIZE_BYTE_BLOCK     0x80                      /*   128 bytes -- full block */
#define SIZE_BYTE_PATH     (SIZE_BYTE_TIGER * 2 / 3)  /*    16 bytes */
#define SIZE_BYTE_DATA      0x50                      /*    80 bytes */
#define SIZE_BYTE_HASH      SIZE_BYTE_TIGER           /*    24 bytes */
#define SIZE_BYTE_NEXT      0x08                      /*     8 bytes */

//#define SIZE_BYTE_MISC (SIZE_BYTE_PATH + SIZE_BYTE_HASH + SIZE_BYTE_NEXT)

/* size in 64 bit ints of parts of block */
#define SIZE_LONG_PATH 0x02
#define SIZE_LONG_DATA 0x0A
#define SIZE_LONG_HASH 0x03
#define SIZE_LONG_NEXT 0x01

#define SYM_LENGTH -1
#define MAX_COPIES 8


#define SUPER_ID STEGFS_NAME " " STEGFS_VERSION

#define MAGIC_0  0xA157AFA602CC9D1BLL
#define MAGIC_1  0x33BE2B298B76F2ACLL
#define MAGIC_2  0xC903284D7C593AF6LL

#ifdef USE_PROC
    #define PATH_PROC DIR_SEPARATOR "proc"
#endif

#define PASSWORD_SEPARATOR ':'


typedef enum
{
    TAG_STEGFS,
    TAG_VERSION,
    TAG_CIPHER,
    TAG_HASH,
    TAG_MODE,
    TAG_BLOCKSIZE,
    TAG_MAX
}
stegfs_tag_e;


/*!
 * \brief  Structure to hold information about a file
 *
 * Public file information structure containing an easy-to-access
 * representation of all important details about a file
 */
typedef struct stegfs_file_t
{
    char     *path;               /*!< The path component of /path/file:password */
    char     *name;               /*!< The file name part of /path/file:password */
    char     *pass;               /*!< Password component of /path/file:password */
    uint64_t  size;               /*!< File size */
    time_t    time;               /*!< Last modified timestamp */
    uint8_t  *data;               /*!< File data */
    uint64_t  inodes[MAX_COPIES]; /*!< The available inodes */
    uint64_t *blocks[MAX_COPIES]; /*!< The complete list of used blocks */
    bool      write;              /*!< Whether the file was opened for write access */
}
stegfs_file_t;

typedef struct _stegfs_cache2
{
    char *name;
    uint64_t ents;
    struct _stegfs_cache2 **child;
    stegfs_file_t *file;
}
stegfs_cache2_t;

/*!
 * \brief  Structure to hold file system information
 *
 * Static data structure stored in the backend, which holds information
 * about the currently mounted file system
 */
typedef struct stegfs_t
{
    uint64_t        size;      /*!< Size of file system in bytes (not capacity) */
    int64_t         handle;    /*!< Handle to file system file/device */
    char           *cipher;
    char           *hash;
    char           *mode;
    uint32_t        blocksize; /*!< File system block size; if it needs to be bigger than 4,294,967,295 we have issues */
    bool           *used;      /*!< Used block tracker */
    stegfs_cache2_t cache2;    /*!< File cache version 2 */
}
stegfs_t;

/*!
 * \brief  Structure for each file system block
 *
 * Simple structure which represents an individual file system data block
 */
typedef struct stegfs_block_t
{
    uint64_t path[SIZE_LONG_PATH]; /*!< Hash of block path    */
    uint8_t  data[SIZE_BYTE_DATA]; /*!< Block data            */
    uint64_t hash[SIZE_LONG_HASH]; /*!< Hash of block data    */
    uint64_t next[SIZE_LONG_NEXT]; /*!< Address of next block */
}
stegfs_block_t;

/*!
 * \brief          Initialise stegfs library, set internal data structures
 * \param[in]  fs  Name and path to file system
 *
 * Initialise the file system and popular static information structures,
 * making note whether to cache file details
 */
extern bool stegfs_init(const char * const restrict fs) __attribute__((nonnull(1)));

/*!
 * \brief                Retrieve information about the file system
 * \returns              The file system data information structure
 *
 * Retrieve useful information about the file system from the backend.
 * Details returned are the file system ID, size in bytes, and size in
 * blocks
 */
extern stegfs_t stegfs_info(void);

extern bool stegfs_files_equal(stegfs_file_t, stegfs_file_t);
extern bool stegfs_file_will_fit(stegfs_file_t *);

extern void stegfs_file_create(const char * const restrict, bool);

extern bool stegfs_file_stat(stegfs_file_t *);
extern bool stegfs_file_read(stegfs_file_t *);
extern bool stegfs_file_write(stegfs_file_t *);
extern void stegfs_file_delete(stegfs_file_t *);

extern void stegfs_cache2_add(const char * const restrict, stegfs_file_t *);
extern stegfs_cache2_t *stegfs_cache2_exists(const char * const restrict, stegfs_cache2_t *) __attribute__((nonnull(1)));
extern void stegfs_cache2_remove(const char * const restrict) __attribute__((nonnull(1)));

#endif /* ! _STEGFS_H_ */
