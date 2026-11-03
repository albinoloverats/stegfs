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
	VERSION_UNKNOWN = 0, /*!< Unrecognized filesystem version */
	VERSION_2010_01,     /*!< Unsupported 2010.01 format */
	VERSION_2015_08,     /*!< 2015.08 format */

	VERSION_202X_XX,     /*!< 202X.XX format */
	VERSION_CURRENT      /*!< Current format marker */
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
	TAG_STEGFS,        /*!< Filesystem identifier */
	TAG_VERSION,       /*!< Filesystem format version */
	TAG_CIPHER,        /*!< Cipher algorithm */
	TAG_HASH,          /*!< Hash algorithm */
	TAG_MODE,          /*!< Cipher mode */
	TAG_BLOCKSIZE,     /*!< Filesystem block size */
	TAG_HEADER_OFFSET, /*!< Inline-data offset in inode blocks */
	TAG_DUPLICATION,   /*!< Number of file copies */
	TAG_MAC,           /*!< Message authentication algorithm */
	TAG_KDF,           /*!< Key-derivation iteration count */
	TAG_MAX            /*!< One past the last valid tag */
}
stegfs_tag_e;

/*!
 * \brief  Initialisation enum
 *
 * Return value enum for file system initialisation.
 */
typedef enum
{
	STEGFS_INIT_OKAY,         /*!< Initialization succeeded */
	STEGFS_INIT_UNKNOWN,      /*!< Backing image could not be opened, locked, or mapped */
	STEGFS_INIT_NOT_STEGFS,   /*!< Image does not contain a stegfs filesystem */
	STEGFS_INIT_OLD_STEGFS,   /*!< Image uses an unsupported legacy format */
	STEGFS_INIT_MISSING_TAG,  /*!< Required superblock tag is absent */
	STEGFS_INIT_INVALID_TAG,  /*!< Superblock tag has an invalid value */
	STEGFS_INIT_CORRUPT_TAG   /*!< Superblock tag data is malformed or corrupt */
}
stegfs_init_e;

/*!
 * \brief  Structure to hold information about a file
 *
 * Public file information structure containing an easy-to-access
 * representation of all important details about a file.
 */
typedef struct stegfs_file_s
{
	char      *path;               /*!< Parent directory path */
	char      *name;               /*!< File name without its password suffix */
	char      *pass;               /*!< File password */
	uint64_t   size;               /*!< File size in bytes */
	time_t     time;               /*!< File modification timestamp */
	uint8_t   *data;               /*!< File contents, when loaded */
	/* you can’t have more than 64 copies; you just can’t */
	uint64_t   inodes[COPIES_MAX]; /*!< Inode block address for each copy; zero if unknown or absent */
	uint64_t  *blocks[COPIES_MAX]; /*!< Block-address list for each copy */
	uint64_t   open_count;         /*!< Number of active FUSE handles referencing this cached file */
	bool       loaded;             /*!< Whether file contents have been loaded into data */
	bool       dirty;              /*!< Whether cached contents need to be written to storage */
	bool       unlinked;           /*!< Whether removed from the namespace while handles remain open */
}
stegfs_file_s;

/*!
 * \brief  Cache structure
 *
 * A node in the in-memory namespace tree. Directory nodes have child
 * entries; file nodes reference their cached file information.
 */
typedef struct _stegfs_cache
{
	char *name;                    /*!< Entry name */
	uint64_t ents;                 /*!< Number of child nodes */
	struct _stegfs_cache **child;  /*!< Child nodes for a directory */
	stegfs_file_s *file;           /*!< Cached file data for a file node; NULL for directories */
}
stegfs_cache_s;

/*!
 * \brief  A "bitmap" of in-use blocks
 *
 * Tracks blocks currently in use. The optional file-name array is
 * populated when block-list debugging is enabled.
 */
typedef struct stegfs_blocks_s
{
	uint64_t used; /*!< Number of blocks marked in use */
	bool *in_use;  /*!< Per-block allocation state */
	char **file;   /*!< Optional per-block owning-file label, when /bloc is enabled */
}
stegfs_blocks_s;

/*!
 * \brief  Structure to hold file system information
 *
 * Runtime state for the currently mounted filesystem, including its
 * backing image mapping, format settings, block tracker, and cache.
 */
typedef struct stegfs_s
{
	int64_t                handle;         /*!< File descriptor for the backing image */
	uint64_t               size;           /*!< Size of the backing image in bytes */
	void                  *memory;         /*!< Mapping of the backing image */
	enum gcry_cipher_algos cipher;         /*!< Cipher algorithm used by the file system */
	enum gcry_cipher_modes mode;           /*!< Cipher mode used by the file system */
	enum gcry_md_algos     hash;           /*!< Hash algorithm used by the file system */
	enum gcry_mac_algos    mac;            /*!< MAC algorithm used by the file system */
	uint64_t               kdf_iterations; /*!< Key-derivation iteration count */
	uint32_t               copies;         /*!< Number of stored copies of each file */
	size_t                 blocksize;      /*!< Size of each filesystem block in bytes */
	off_t                  head_offset;    /*!< Offset of inline file data within an inode block */
	stegfs_blocks_s        blocks;         /*!< In use block tracker */
	stegfs_cache_s         cache;          /*!< Cached namespace tree */
	version_e              version;        /*!< File system version */
	bool                   show_bloc;      /*!< Whether the virtual /bloc/ block list is exposed */
}
stegfs_s;

/*!
 * \brief  Structure for each file system block
 *
 * Simple structure which represents an individual file system data
 * block.
 */
typedef struct stegfs_block_s
{
	uint64_t path[SIZE_LONG_PATH]; /*!< Plaintext path marker/hash bytes */
	uint8_t  data[SIZE_BYTE_DATA]; /*!< Encrypted payload area */
	uint64_t hash[SIZE_LONG_HASH]; /*!< Payload hash */
	uint64_t next;                 /*!< Next block address, or zero at end of chain */
} __attribute__((packed))
stegfs_block_s;

/*!
 * \brief         Initialise stegfs library, set internal data structures
 * \param[in]  fs        Path to the backing filesystem image
 * \param[in]  paranoid  Whether to enable paranoid initialization checks
 * \param[in]  cipher    Cipher algorithm
 * \param[in]  mode      Cipher mode
 * \param[in]  hash      Hash algorithm
 * \param[in]  mac       MAC algorithm
 * \param[in]  kdf       Key-derivation iteration count
 * \param[in]  dups      Number of file copies
 * \param[in]  show_bloc Whether to expose the /bloc/ block list
 * \returns       The initialisation status
 *
 * Open and validate the backing image, initialize filesystem state, and
 * select the format parameters used for file operations.
 */
extern stegfs_init_e stegfs_init(const char * const restrict fs, bool paranoid,
		enum gcry_cipher_algos cipher,
		enum gcry_cipher_modes mode,
		enum gcry_md_algos hash,
		enum gcry_mac_algos mac,
		uint64_t kdf, uint32_t dups, bool show_bloc);

/*!
 * \brief         Retrieve information about the file system
 * \returns       The file system data information structure
 *
 * Retrieve useful information about the file system from the backend.
 * Returns a copy of the current runtime state.
 */
extern stegfs_s stegfs_info(void);

/*!
 * \brief         Deinitialise the file system
 *
 * Release the mapping, open descriptor, cache, and other in-memory
 * filesystem state.
 */
extern void stegfs_deinit(void);

/*!
 * \brief         Check if a file will fit
 * \param[in]  file  File info structure
 * \return        True if the file will fit
 *
 * Check whether the filesystem has enough unallocated blocks for any
 * missing inode copies and additional data blocks required by this file.
 * Sets errno on failure and clears it on success.
 */
extern bool stegfs_file_will_fit(stegfs_file_s *file);

/*!
 * \brief         Create a new file
 * \param[in]  path   File path
 * \param[in]  write  Whether the new cached file should initially be marked dirty
 *
 * Add a new empty file to the cache. Storage blocks are allocated when
 * the file is written.
 */
extern void stegfs_file_create(const char * const restrict path, bool write);

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
 * Locate a file's inode and copy block chains. If q is true, stop after
 * finding a complete copy; otherwise inspect the copies to collect
 * their block information.
 */
extern bool stegfs_file_stat_aux(stegfs_file_s *f, bool q);

/*!
 * \brief         Read a file from the file system
 * \param[in]  f  File structure to populate from storage
 * \return        True if the file was read successfully
 *
 * Read the stored file contents into f, updating its metadata and cache
 * entry on success.
 */
extern bool stegfs_file_read(stegfs_file_s *f);

/*!
 * \brief         Write a file to the file system
 * \param[in]  f  File structure containing the file contents and metadata
 *
 * Write the contents to storage, updating the file's block metadata and
 * cache entry on success.
 */
extern bool stegfs_file_write(stegfs_file_s *f);

/*!
 * \brief         Delete a file from the file system
 * \param[in]  f  File structure for the file being deleted
 *
 * Delete the file's storage blocks and remove its namespace cache entry.
 */
extern void stegfs_file_delete(stegfs_file_s *f);

/*!
 * \brief         Release storage blocks for a file without freeing its cache object
 * \param[in]  file  File structure whose storage blocks should be released
 *
 * Frees the known on-image blocks for an already located file while
 * retaining its in-memory data and cache object for any open handles.
 */
extern void stegfs_file_unlink(stegfs_file_s *file);

/*!
 * \brief         Add a directory to the in-memory cache
 * \param[in]  path  Directory path to add
 *
 * Adds the directory and any missing parent nodes to the cache.
 */
extern void stegfs_directory_create(const char * const restrict path);

/*!
 * \brief         Add a directory or file to the cache
 * \param[in]  p  Directory path, or NULL when adding a file
 * \param[in]  f  File information to copy into the cache, or NULL for a directory
 *
 * Adds the directory and any missing parents, or copies file metadata,
 * block addresses, and loaded contents into its namespace entry. Cache
 * reference state for an existing file node is retained.
 */
extern void stegfs_cache_add(const char * const restrict p, stegfs_file_s *f);

/*!
 * \brief         Check the cache for a particular entry
 * \param[in]  p  The path of the file to look for
 * \param[out] f  Optional caller-provided structure to receive a shallow copy
 * \return        Pointer to the cache entry (do not modify)
 *
 * Look up a path in the in-memory namespace. Returns NULL when absent
 * or unlinked. The returned pointer refers to the cache's live node and
 * must not be modified; f, when non-NULL, receives a shallow copy.
 */
extern stegfs_cache_s *stegfs_cache_exists(const char * const restrict p, stegfs_cache_s *f) __attribute__((nonnull(1)));

/*!
 * \brief         Remove an entry from the cache
 * \param[in]  p  The path of the entry to remove
 *
 * Remove a path from the in-memory cache.
 */
extern void stegfs_cache_remove(const char * const restrict p) __attribute__((nonnull(1)));

/*!
 * \brief         Remove a specific file object from the cache
 * \param[in]  file  File object whose matching cache node should be removed
 *
 * Removes the cache node only if it still refers to f, so a newer file
 * at the same path is not removed accidentally.
 */
extern void stegfs_cache_remove_file(stegfs_file_s * const restrict file) __attribute__((nonnull(1)));

#endif /* ! _STEGFS_H_ */
