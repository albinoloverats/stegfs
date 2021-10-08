/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2021, albinoloverats ~ Software Development
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
#include <gcrypt.h>

#define STEGFS_NAME    "stegfs"
#define STEGFS_VERSION "202X.XX"

#define PROJECT_URL "https://albinoloverats.net/projects/encrypt"

/* size (in bytes) for various blocks of data */
#define SIZE_BYTE_BLOCK       0x0800    /*!< 2,048 bytes */
#define SIZE_BYTE_PATH        0x0020    /*!<    32 bytes */
#define SIZE_BYTE_DATA_201508 0x07B8    /*!< 1,976 bytes */
//#define SIZE_BYTE_DATA_202XXX 0x0780    /*!< 1,920 bytes */
#define SIZE_BYTE_DATA  SIZE_BYTE_DATA_201508

#define SIZE_BYTE_HASH        0x0020    /*!<    32 bytes */
#define SIZE_BYTE_NEXT        0x0008    /*!<     8 bytes */
/* next block (not defined) */

#define SIZE_BYTE_HEAD        0x0400    /*!< 1,024 bytes (data in header block) */
#define OFFSET_BYTE_HEAD  (SIZE_BYTE_DATA-SIZE_BYTE_HEAD) /*!< Offset of file data in header block */

/* size in 64 bit ints of parts of block */
#define SIZE_LONG_PATH          0x04
#define SIZE_LONG_DATA_201508   0xF7
//#define SIZE_LONG_DATA_202XXX   0xF0
#define SIZE_LONG_DATA          SIZE_LONG_DATA_201508
#define SIZE_LONG_HASH          0x04
/* next block (not defined) */

#define COPIES_MAX 64
#define COPIES_DEFAULT 8
#define SYM_LENGTH -1

#define SUPER_ID STEGFS_NAME " " STEGFS_VERSION

/*
 * File system header for unsupported first attempt at a file system.
 */
#define HASH_MAGIC_201001_0 0xa157afa602cc9d1bllu
#define HASH_MAGIC_201001_1 0x33be2b298b76f2acllu
#define HASH_MAGIC_201001_2 0xc903284d7c593af6llu

/*
 * File system header for first re-write; where backwards compatibility
 * was (hopefully) preserved
 */
#define HASH_MAGIC_201508_2 0x8c9b291a9e55c137llu

#define HASH_MAGIC_0 0x5287505e71e039dfllu
#define HASH_MAGIC_1 0xebccb02ab09ba26fllu
#define HASH_MAGIC_2 0x089e07f0da733557llu

/*
 * Previously all 0xFF - these aren't checked during the mount process
 * but are purely to "make pretty" the beginning of the file system
 */
#define PATH_MAGIC_0 0x7374656766732D32llu
#define PATH_MAGIC_1 0x3031382E58580000llu


/*!
 * \brief  Version enum
 *
 * An enum to indicate the file system version.
 */
typedef enum
{
	VERSION_UNKNOWN = 0,
	VERSION_2010_01,
	VERSION_2015_08,

	VERSION_202X_XX,
	VERSION_CURRENT
}
version_e;


#define DEFAULT_CIPHER          GCRY_CIPHER_RIJNDAEL256
#define DEFAULT_MODE            GCRY_CIPHER_MODE_CBC
#define DEFAULT_HASH            GCRY_MD_SHA256
#define DEFAULT_MAC             GCRY_MAC_HMAC_SHA256
#define DEFAULT_KDF_ITERATIONS  32768

#define PATH_BLOC DIR_SEPARATOR "bloc"

#define PASSWORD_SEPARATOR ':'

/*!
 * \brief  Tag (TLV) enum
 *
 * An enum to identify tags in the file system superblock TLV array.
 */
typedef enum
{
	TAG_STEGFS,
	TAG_VERSION,
	TAG_CIPHER,
	TAG_HASH,
	TAG_MODE,
	TAG_BLOCKSIZE,
	TAG_HEADER_OFFSET,
	TAG_DUPLICATION,
	TAG_MAC,
	TAG_KDF,
	TAG_MAX
}
stegfs_tag_e;

/*!
 * \brief  Initialisation enum
 *
 * Return value enum for file system initialisation.
 */
typedef enum
{
	STEGFS_INIT_OKAY,
	STEGFS_INIT_UNKNOWN,
	STEGFS_INIT_NOT_STEGFS,
	STEGFS_INIT_OLD_STEGFS,
	STEGFS_INIT_MISSING_TAG,
	STEGFS_INIT_INVALID_TAG,
	STEGFS_INIT_CORRUPT_TAG
}
stegfs_init_e;

/*!
 * \brief  Structure to hold information about a file
 *
 * Public file information structure containing an easy-to-access
 * representation of all important details about a file.
 */
typedef struct stegfs_file_t
{
	char      *path;               /*!< The path component of /path/file:password */
	char      *name;               /*!< The file name part of /path/file:password */
	char      *pass;               /*!< Password component of /path/file:password */
	uint64_t   size;               /*!< File size */
	time_t     time;               /*!< Last modified timestamp */
	uint8_t   *data;               /*!< File data */
	/* you can’t have more than 64 copies; you just can’t */
	uint64_t   inodes[COPIES_MAX]; /*!< The available inodes */
	uint64_t  *blocks[COPIES_MAX]; /*!< The complete list of used blocks */
	bool       write;              /*!< Whether the file was opened for write access */
}
stegfs_file_t;

/*!
 * \brief  Cache structure
 *
 * A cache tree element. If the element represents a directory it will
 * have a name and number of entries, as well as pointers to all child
 * elements. Whereas a file will (mostly) just fill out the file
 * structure.
 */
typedef struct _stegfs_cache
{
	char *name;                   /*!< The name of the directory/file */
	uint64_t ents;                /*!< The number of child elements */
	struct _stegfs_cache **child; /*!< Array of pointers to child elements */
	stegfs_file_t *file;          /*!< File details (if applicable) */
}
stegfs_cache_t;

/*!
 * \brief  A "bitmap" of in-use blocks
 *
 * A structure to keep track of blocks currently in use by files on the
 * file system. When debugging, keep track of which file a particular
 * block is being used by.
 */
typedef struct stegfs_blocks_t
{
	uint64_t used; /*!< Count of used blocks */
	bool *in_use;  /*!< Used block tracker */
	char **file;   /*!< File using the given block */
}
stegfs_blocks_t;

/*!
 * \brief  Structure to hold file system information
 *
 * Static data structure stored in the backend, which holds information
 * about the currently mounted file system.
 */
typedef struct stegfs_t
{
	int64_t                handle;         /*!< Handle to file system file/device */
	uint64_t               size;           /*!< Size of file system in bytes (not capacity) */
	void                  *memory;         /*!< mmap pointer */
	enum gcry_cipher_algos cipher;         /*!< Cipher algorithm used by the file system */
	enum gcry_cipher_modes mode;           /*!< Cipher mode used by the file system */
	enum gcry_md_algos     hash;           /*!< Hash algorithm used by the file system */
	enum gcry_mac_algos    mac;            /*!< MAC algorithm used by the file system */
	uint64_t               kdf_iterations; /*!< KDF iterations */
	uint32_t               copies;         /*!< File duplication */
	size_t                 blocksize;      /*!< File system block size; if it needs to be bigger than 4,294,967,295 we have issues */
	off_t                  head_offset;    /*!< Start location of file data in header blocks; only 32 bits (like blocksize) */
	stegfs_blocks_t        blocks;         /*!< In use block tracker */
	stegfs_cache_t         cache;          /*!< File cache version 2 */
	version_e              version;        /*!< File system version */
	bool                   show_bloc;      /*!< Expose the /bloc/ block list */
}
stegfs_t;

/*!
 * \brief  Structure for each file system block
 *
 * Simple structure which represents an individual file system data
 * block.
 */
typedef struct stegfs_block_t
{
	uint64_t path[SIZE_LONG_PATH]; /*!< Hash of block path    */
	uint8_t  data[SIZE_BYTE_DATA]; /*!< Block data (1,976 bytes or 247 64-bit words */
	uint64_t hash[SIZE_LONG_HASH]; /*!< Hash of block data    */
	uint64_t next;                 /*!< Address of next block */
} __attribute__((packed))
stegfs_block_t;

/*!
 * \brief         Initialise stegfs library, set internal data structures
 * \param[in]  f  Name and path to file system
 * \param[in]  p  Paranoid mode
 * \param[in]  c  Cipher algorithm
 * \param[in]  m  Cipher mode
 * \param[in]  h  Hash algorithm
 * \param[in]  a  MAC algorithm
 * \param[in]  x  Duplication copies
 * \param[in]  b  Expose the /bloc/ block list
 * \returns       The initialisation status
 *
 * Initialise the file system and popular static information structures,
 * making note whether to cache file details.
 */
extern stegfs_init_e stegfs_init(const char * const restrict f, bool p,
		enum gcry_cipher_algos c,
		enum gcry_cipher_modes m,
		enum gcry_md_algos h,
		enum gcry_mac_algos a,
		uint64_t kdf, uint32_t x, bool b);

/*!
 * \brief         Retrieve information about the file system
 * \returns       The file system data information structure
 *
 * Retrieve useful information about the file system from the backend.
 * Details returned are the file system ID, size in bytes, and size in
 * blocks.
 */
extern stegfs_t stegfs_info(void);

/*
 * \brief         Deinitialise the file system
 *
 * Unmount the file system, sync all data, clear all memory.
 */
extern void stegfs_deinit(void);

/*!
 * \brief         Check if a file will fit
 * \param[in]  f  File info structure
 * \return        True if the file will fit
 *
 * Check if there is likely enough remaining capacity of the file system
 * for the given file to fit. It takes in to account the size of the
 * file, duplicated and all known files and their duplicates.
 */
extern bool stegfs_file_will_fit(stegfs_file_t *f);

/*!
 * \brief         Create a new file
 * \param[in]  p  The files path
 * \param[in]  w  Is the file opened with write access
 *
 * Create a new file. This doesn't actually allocate any space but
 * rather just adds an entry to the internal cache.
 */
extern void stegfs_file_create(const char * const restrict p, bool w);

#define STEGFS_FILE_STAT_ARGS_COUNT(...) STEGFS_FILE_STAT_ARGS_COUNT2(__VA_ARGS__, 2, 1) /*!< Function overloading argument count (part 1) */
#define STEGFS_FILE_STAT_ARGS_COUNT2(_1, _2, _, ...) _                                   /*!< Function overloading argument count (part 2) */

#define stegfs_file_stat_1(A)     stegfs_file_stat_aux(A, false) /*!< Call stegfs_file_stat_aux with value of false for second parameter */
#define stegfs_file_stat_2(A, B)  stegfs_file_stat_aux(A, B)     /*!< Call stegfs_file_stat_aux with both user supplied parameters */
#define stegfs_file_stat(...) CONCAT(stegfs_file_stat_, STEGFS_FILE_STAT_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Decide how to call stegfs_file_stat */

/*!
 * \brief         Primary stat function
 * \param[in]  f  File structure for the file being stat'd
 * \param[in]  q  Whether to stop as soon as a complete copy has been found
 * \return        True if the file was found
 *
 * Attempt to find details about a file. If the file does exist and
 * q==true then tis function will return as soon as one copy has been
 * found. Otherwise it will keep going to attempt to find all blocks
 * used by all copies of the file.
 */
extern bool stegfs_file_stat_aux(stegfs_file_t *f, bool q);

/*!
 * \brief         Read a file from the file system
 * \param[in]  f  File structure for the file being read
 * \return        True if the file was read successfully
 *
 * Read a file from the file system.
 */
extern bool stegfs_file_read(stegfs_file_t *f);

/*!
 * \brief         Write a file to the file system
 * \param[in]  f  File structure for the file being written
 *
 * Write a file to the file system.
 */
extern bool stegfs_file_write(stegfs_file_t *f);

/*!
 * \brief         Delete a file from the file system
 * \param[in]  f  File structure for the file being deleted
 *
 * Delete a file from the file system.
 */
extern void stegfs_file_delete(stegfs_file_t *f);

/*!
 * \brief         Add a entry to the cache
 * \param[in]  p  The path of the file to add
 * \param[in]  f  The file info structure of the file to add
 *
 * Add an entry to the in-memory file system cache. This allows things
 * like directory look-ups to work.
 */
extern void stegfs_cache_add(const char * const restrict p, stegfs_file_t *f);

/*!
 * \brief         Check the cache for an particular entry
 * \param[in]  p  The path of the file to look for
 * \param[out] f  Caller allocated memory for found cache entry
 * \return        Pointer to the cache entry (do not modify)
 *
 * Check whether a particular path exists in the file systems in-memory
 * cache. If you want a modifiable cache structure use the parameter f
 * as the return value points to the one used by the caching code - do
 * not modify.
 */
extern stegfs_cache_t *stegfs_cache_exists(const char * const restrict p, stegfs_cache_t *f) __attribute__((nonnull(1)));

/*!
 * \brief         Remove an entry from the cache
 * \param[in]  p  The path of the entry to remove
 *
 * Remove a path from the in-memory cache.
 */
extern void stegfs_cache_remove(const char * const restrict p) __attribute__((nonnull(1)));

#endif /* ! _STEGFS_H_ */
