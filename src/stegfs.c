/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2017, albinoloverats ~ Software Development
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
#include <stdio.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>

#include <gcrypt.h>

#include "common/common.h"
#include "common/ccrypt.h"
#include "common/tlv.h"
#include "common/dir.h"

#include "stegfs.h"


static bool block_read(uint64_t, stegfs_block_t *, gcry_cipher_hd_t, const char * const restrict);
static bool block_write(uint64_t, stegfs_block_t, gcry_cipher_hd_t, const char * const restrict);
static void block_delete(uint64_t);

static bool block_in_use(uint64_t, const char * const restrict);
static uint64_t block_assign(const char * const restrict);

static gcry_cipher_hd_t cipher_init(const stegfs_file_t * const restrict, uint8_t);


static stegfs_t file_system;

extern stegfs_init_e stegfs_init(const char * const restrict fs, bool paranoid, enum gcry_cipher_algos cipher, enum gcry_cipher_modes mode, enum gcry_md_algos hash, uint32_t dups)
{
	if ((file_system.handle = open(fs, O_RDWR, S_IRUSR | S_IWUSR)) < 0)
		return STEGFS_INIT_UNKNOWN;
	lockf(file_system.handle, F_LOCK, 0);
	file_system.size = lseek(file_system.handle, 0, SEEK_END);
	if ((file_system.memory = mmap(NULL, file_system.size, PROT_READ | PROT_WRITE, MAP_SHARED, file_system.handle, 0)) == MAP_FAILED)
		return STEGFS_INIT_UNKNOWN;

	file_system.cache2.name = strdup(DIR_SEPARATOR);
	file_system.cache2.ents = 0;
	file_system.cache2.child = NULL;
	file_system.cache2.file = NULL;
#ifdef USE_PROC
	stegfs_cache2_add(PATH_PROC, NULL);
#endif
	if (paranoid)
	{
		file_system.cipher = cipher;
		file_system.mode = mode;
		file_system.hash = hash;
		file_system.blocksize = SIZE_BYTE_BLOCK;
		file_system.head_offset = OFFSET_BYTE_HEAD;
		file_system.copies = dups;
		goto done;
	}

	stegfs_block_t block;
	memcpy(&block, file_system.memory, sizeof block);
	/* quick check for previous version; account for all byte orders */
	if ((block.hash[0] == MAGIC_201001_0 || htonll(block.hash[0]) == MAGIC_201001_0) && (block.hash[1] == MAGIC_201001_1 || htonll(block.hash[1]) == MAGIC_201001_1) && (block.hash[2] == MAGIC_201001_2 || htonll(block.hash[2]) == MAGIC_201001_2))
		return STEGFS_INIT_OLD_STEGFS;

	if (ntohll(block.hash[0]) != MAGIC_0 ||
		ntohll(block.hash[1]) != MAGIC_1 ||
		ntohll(block.hash[2]) != MAGIC_2)
		return STEGFS_INIT_NOT_STEGFS;

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
	if (!tlv_has_tag(tlv, TAG_STEGFS) || !tlv_has_tag(tlv, TAG_VERSION) || !tlv_has_tag(tlv, TAG_CIPHER) || !tlv_has_tag(tlv, TAG_MODE) || !tlv_has_tag(tlv, TAG_HASH) || !tlv_has_tag(tlv, TAG_BLOCKSIZE) || !tlv_has_tag(tlv, TAG_HEADER_OFFSET) || !tlv_has_tag(tlv, TAG_DUPLICATION))
		return STEGFS_INIT_MISSING_TAG;

	if (strncmp((char *)tlv_value_of(tlv, TAG_STEGFS), STEGFS_NAME, strlen(STEGFS_NAME)))
		return STEGFS_INIT_INVALID_TAG;
	if (strncmp((char *)tlv_value_of(tlv, TAG_VERSION), STEGFS_VERSION, strlen(STEGFS_VERSION)))
		return STEGFS_INIT_INVALID_TAG;
	/* get cipher info */
	char *c = strndup((char *)tlv_value_of(tlv, TAG_CIPHER), tlv_size_of(tlv, TAG_CIPHER));
	file_system.cipher = cipher_id_from_name(c);
	free(c);
	char *m = strndup((char *)tlv_value_of(tlv, TAG_MODE), tlv_size_of(tlv, TAG_MODE));
	file_system.mode = mode_id_from_name(m);
	free(m);
	/* get hash info */
	char *h = strndup((char *)tlv_value_of(tlv, TAG_HASH), tlv_size_of(tlv, TAG_HASH));
	file_system.hash = hash_id_from_name(h);
	free(h);
	if (file_system.cipher == GCRY_CIPHER_NONE || file_system.hash == GCRY_MD_NONE || file_system.mode == GCRY_CIPHER_MODE_NONE)
		return STEGFS_INIT_INVALID_TAG;
	/* get fs block size */
	memcpy(&file_system.blocksize, tlv_value_of(tlv, TAG_BLOCKSIZE), tlv_size_of(tlv, TAG_BLOCKSIZE));
	file_system.blocksize = ntohl(file_system.blocksize);
	/* get number of bytes file data in file header */
	memcpy(&file_system.head_offset, tlv_value_of(tlv, TAG_HEADER_OFFSET), tlv_size_of(tlv, TAG_HEADER_OFFSET));
	file_system.head_offset = ntohl(file_system.head_offset);
	/* get number of copies */
	memcpy(&file_system.copies, tlv_value_of(tlv, TAG_DUPLICATION), tlv_size_of(tlv, TAG_DUPLICATION));
	file_system.copies = ntohl(file_system.copies);

	if (ntohll(block.next) != file_system.size / file_system.blocksize)
		return STEGFS_INIT_CORRUPT_TAG;
	if (file_system.head_offset > (ssize_t)file_system.blocksize)
		return STEGFS_INIT_CORRUPT_TAG;

	tlv_deinit(&tlv);

done:
	file_system.blocks.used = 1;
	file_system.blocks.in_use = calloc(file_system.size / file_system.blocksize, sizeof(bool));

	init_crypto();
	return STEGFS_INIT_OKAY;
}

extern void stegfs_deinit(void)
{
	msync(file_system.memory, file_system.size,  MS_SYNC);
	munmap(file_system.memory, file_system.size);
	close(file_system.handle);

	free(file_system.blocks.in_use);

	stegfs_cache2_remove(DIR_SEPARATOR);
	free(file_system.cache2.name);

	return;
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
	stegfs_block_t block;
	lldiv_t d = lldiv(file->size - (file->size < (sizeof block.data - file_system.head_offset) ? file->size : (sizeof block.data - file_system.head_offset)), SIZE_BYTE_DATA);
	uint64_t blocks_needed = (d.quot + (d.rem > 0)) * file_system.copies;

	uint64_t blocks_total = (file_system.size / file_system.blocksize) - 1;
	if (blocks_needed > blocks_total)
	{
		stegfs_file_delete(file);
		return errno = EFBIG, false; /* file would not fit in the file system */
	}
	if (blocks_needed > blocks_total - file_system.blocks.used)
	{
		stegfs_file_delete(file);
		return errno = ENOSPC, false; /* file won’t fit in remaining space */
	}
	return errno = EXIT_SUCCESS, true;
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
	 * figure out where the files’ inode blocks are
	 */
	gcry_md_hd_t hash;
	gcry_md_open(&hash, GCRY_MD_SHA512, GCRY_MD_FLAG_SECURE);
	gcry_md_write(hash, file->path, strlen(file->path));
	gcry_md_write(hash, file->name, strlen(file->name));
	uint8_t *inodes = gcry_md_read(hash, GCRY_MD_SHA512);
	size_t len = gcry_md_get_algo_dlen(GCRY_MD_SHA512);
	/*
	 * calculate inode values; must be done here, so we have all of them
	 * and not just the first that we can read (I wonder if there is a
	 * better way than rotating the data…)
	 */
	for (unsigned i = 0; i < file_system.copies; i++)
	{
		memcpy(&file->inodes[i], inodes, sizeof file->inodes[i]);
		uint8_t b = inodes[0];
		memmove(inodes, inodes + 1, len - 1);
		inodes[len] = b;
	}
	gcry_md_close(hash);
	/*
	 * read file inode, pray for success, then see if we can get a
	 * complete copy of the file
	 */
	unsigned available_inodes = file_system.copies;
	unsigned corrupt_copies = 0;
	for (unsigned i = 0; i < file_system.copies; i++)
	{
		gcry_cipher_hd_t cipher_handle = cipher_init(file, i);
		stegfs_block_t inode;
		memset(&inode, 0x00, sizeof inode);
		if (block_read(file->inodes[i], &inode, cipher_handle, file->path))
		{
			if ((file->size = ntohll(inode.next)) > file_system.size)
			{
				available_inodes--;
				gcry_cipher_close(cipher_handle);
				continue;
			}
			file_system.blocks.in_use[file->inodes[i] % (file_system.size / file_system.blocksize)] = true;
			file_system.blocks.used++;

			uint64_t first[SIZE_LONG_DATA] = { 0x0 };
			memcpy(first, inode.data, sizeof first);
			file->time = htonll(first[0]);
			for (unsigned j = 0, l = 1; j < file_system.copies; j++, l++)
			{
				gcry_cipher_hd_t another = cipher_init(file, j);
				lldiv_t d = lldiv(file->size - (file->size < (sizeof inode.data - file_system.head_offset) ? file->size : (sizeof inode.data - file_system.head_offset)), SIZE_BYTE_DATA);
				uint64_t blocks = d.quot + (d.rem > 0);
				file->blocks[j] = realloc(file->blocks[j], (blocks + 2) * sizeof blocks);
				memset(file->blocks[j], 0x00, blocks * sizeof blocks);
				file->blocks[j][0] = blocks;
				if (blocks)
					file->blocks[j][1] = htonll(first[l]);
				/*
				 * traverse file block tree; whilst the whole block
				 * is read, the actual file data is discarded
				 */
				for (uint64_t k = 2 ; k <= blocks; k++)
				{
					stegfs_block_t block;
					memset(&block, 0x00, sizeof block);
					if (block_read(file->blocks[j][k - 1], &block, another, file->path))
					{
						file_system.blocks.in_use[file->blocks[j][k - 1] % (file_system.size / file_system.blocksize)] = true;
						file_system.blocks.used++;
						file->blocks[j][k] = ntohll(block.next);
					}
					else
					{
						/*
						 * note-to-self: because of the memset above, all
						 * incomplete blockchains will have trailing 0’s,
						 * this comes in handy when saving (and debugging)
						 */
						corrupt_copies++;
						break;
					}
				}
				gcry_cipher_close(another);
			}
			break;
		}
		else
			available_inodes--;
		gcry_cipher_close(cipher_handle);
	}
	/*
	 * as long as there’s a valid inode and one complete copy we’re good
	 */
	if (available_inodes && corrupt_copies < file_system.copies)
	{
		stegfs_cache2_add(NULL, file);
		return true;
	}
	for (unsigned i = 0; i < file_system.copies; i++)
		if (file->blocks[i])
		{
			for (uint64_t j = 1; j < file->blocks[i][0]; j++)
			{
				file_system.blocks.in_use[file->blocks[i][j] % (file_system.size / file_system.blocksize)] = false;
				file_system.blocks.used--;
			}
			free(file->blocks[i]);
			file->blocks[i] = NULL;
		}
	return errno = ENOENT, false;
}

extern bool stegfs_file_read(stegfs_file_t *file)
{
	if (!stegfs_file_stat(file))
		return false;
	/*
	 * read the start of the file data
	 */
	file->data = realloc(file->data, file->size);
	for (unsigned i = 0, c = 0; i < file_system.copies && !c; i++)
	{
		gcry_cipher_hd_t cipher_handle = cipher_init(file, i);
		stegfs_block_t inode;
		memset(&inode, 0x00, sizeof inode);
		if (block_read(file->inodes[i], &inode, cipher_handle, file->path))
		{
			memcpy(file->data, inode.data + file_system.head_offset, file->size < (sizeof inode.data - file_system.head_offset) ? file->size : (sizeof inode.data - file_system.head_offset));
			c = 1;
		}
		gcry_cipher_close(cipher_handle);
	}
	/*
	 * and then the rest of it
	 */
	stegfs_block_t block;
	for (unsigned i = 0, corrupt_copies = 0; i < file_system.copies; i++)
	{
		lldiv_t d = lldiv(file->size - (file->size < (sizeof block.data - file_system.head_offset) ? file->size : (sizeof block.data - file_system.head_offset)), SIZE_BYTE_DATA);
		uint64_t blocks = d.quot + (d.rem > 0);
		if (file->blocks[i][0] != blocks)
			continue; /* this copy is corrupt; try the next */
		bool failed = false;
		gcry_cipher_hd_t cipher_handle = cipher_init(file, i);
		for (uint64_t j = 1, k = 0; j <= file->blocks[i][0] && file->blocks[i][j]; j++, k++)
		{
			/*
			 * we should be largely confident that we’ll be able to read
			 * the complete file as outherwise the stat would have
			 * failed
			 */
			memset(&block, 0x00, sizeof block);
			if (block_read(file->blocks[i][j], &block, cipher_handle, file->path))
			{
				size_t l = sizeof block.data;
				if ((l + k * sizeof block.data) > (file->size - (sizeof block.data - file_system.head_offset)))
					l = l - ((l + k * sizeof block.data) - (file->size - (sizeof block.data - file_system.head_offset)));
				memcpy(file->data + (sizeof block.data - file_system.head_offset) + k * sizeof block.data, block.data, l);
			}
			else
			{
				failed = true;
				corrupt_copies++;
				break;
			}
		}
		gcry_cipher_close(cipher_handle);
		if (failed)
			continue;
		stegfs_cache2_add(NULL, file);
		return corrupt_copies < file_system.copies;
	}
	/*
	 * somehow we failed to read a complete copy of the file, despite
	 * knowing that a complete copy existed when stat’d
	 */
	return errno = EIO, false;
}

extern bool stegfs_file_write(stegfs_file_t *file)
{
	stegfs_block_t block;
	lldiv_t d = lldiv(file->size - (file->size < (sizeof block.data - file_system.head_offset) ? file->size : (sizeof block.data - file_system.head_offset)), SIZE_BYTE_DATA);
	uint64_t blocks = d.quot + (d.rem > 0);
	uint64_t z = file->size;

	if (!stegfs_file_stat(file))
	{
		for (unsigned i = 0; i < file_system.copies; i++)
		{
			/*
			 * allocate inodes, mark as in use (inode locations are calculated
			 * in stegfs_file_stat)
			 */
			file_system.blocks.in_use[file->inodes[i] % (file_system.size / file_system.blocksize)] = true;
			file_system.blocks.used++;
			/*
			 * note-to-self: allocate 2 more blocks than is necessary so
			 * that block[0] indicates how many blocks there are (needed)
			 * and block[last] is kept 0x00 as an “end of chain” guard
			 */
			file->blocks[i] = calloc(blocks + 2, sizeof blocks);
			file->blocks[i][0] = blocks;
			for (uint64_t j = 1; j <= blocks; j++)
				if (!(file->blocks[i][j] = block_assign(file->path)))
				{
					/* failed to allocate space; free what we had claimed */
					for (unsigned k = 0; k <= i; k++)
					{
						file_system.blocks.in_use[file->inodes[i] % (file_system.size / file_system.blocksize)] = false;
						file_system.blocks.used--;
						if (file->blocks[k])
						{
							for (uint64_t l = 1; l <= j; l++)
							{
								file_system.blocks.in_use[file->blocks[k][l] % (file_system.size / file_system.blocksize)] = false;
								file_system.blocks.used--;
							}
							free(file->blocks[k]);
							file->blocks[k] = NULL;
						}
					}
					return errno = ENOSPC, false;
				}
		}
	}
	file->size = z; /* stat can cause size to be reset to 0 */
	if (blocks > file->blocks[0][0]) /* need more blocks than we have */
	{
		for (unsigned i = 0; i < file_system.copies; i++)
		{
			file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
			for (uint64_t j = file->blocks[i][0]; j <= blocks; j++)
				if (!(file->blocks[i][j] = block_assign(file->path)))
				{
					/* failed to allocate space; free what we had claimed */
					for (unsigned k = 0; k <= i; k++)
						for (uint64_t l = file->blocks[k][0]; l <= j; l++)
						{
							file_system.blocks.in_use[file->blocks[k][l] % (file_system.size / file_system.blocksize)] = false;
							file_system.blocks.used--;
						}
					return errno = ENOSPC, false;
				}
		}
		for (unsigned i = 0; i < file_system.copies; i++)
			file->blocks[i][0] = blocks;
	}
	else if (blocks < file->blocks[0][0]) /* have more blocks than we need */
		for (unsigned i = 0; i < file_system.copies; i++)
		{
			for (uint64_t j = blocks + 1; j <= file->blocks[i][0]; j++)
				block_delete(file->blocks[i][j]);
			file->blocks[i] = realloc(file->blocks[i], (blocks + 2) * sizeof blocks);
			file->blocks[i][0] = blocks;
		}
	/*
	 * write the data
	 */
	for (unsigned i = 0; i < file_system.copies; i++)
	{
		gcry_cipher_hd_t cipher_handle = cipher_init(file, i);
		for (uint64_t j = 1, k = 0; j <= blocks; j++, k++)
		{
			memset(&block, 0x00, sizeof block);
			size_t l = sizeof block.data;
			if ((l + k * sizeof block.data) > (file->size - (sizeof block.data - file_system.head_offset)))
				l = l - ((l + k * sizeof block.data) - (file->size - (sizeof block.data - file_system.head_offset)));
			gcry_create_nonce(block.data, sizeof block.data);
			memcpy(block.data, file->data + (sizeof block.data - file_system.head_offset) + k * sizeof block.data, l);
			block.next = htonll(file->blocks[i][j + 1]);
			if (!block_write(file->blocks[i][j], block, cipher_handle, file->path))
			{
				/* see below (where inode blocks are written) */
				for (unsigned k = 0; k <= i; k++)
					for (uint64_t l = 1; l <= j; l++)
						block_delete(file->blocks[k][l]);
				gcry_cipher_close(cipher_handle);
				return false;
			}
		}
		gcry_cipher_close(cipher_handle);
	}
	/*
	 * write file inode blocks
	 */
	stegfs_block_t inode;
	memset(&inode, 0x00, sizeof inode);
	uint64_t first[SIZE_LONG_DATA] = { 0x0 };
	if (blocks)
		for (unsigned i = 0, j = 1; i < file_system.copies; i++, j++)
			first[j] = htonll(file->blocks[i][1]);
	else
		gcry_create_nonce((void *)first, sizeof first);
	first[0] = htonll(file->time);
	gcry_create_nonce(inode.data, sizeof inode.data);
	memcpy(inode.data, first, sizeof first);
	if (file->data && file->size)
		memcpy(inode.data + file_system.head_offset, file->data, file->size < (sizeof inode.data - file_system.head_offset) ? file->size : (sizeof inode.data - file_system.head_offset));
	inode.next = htonll(file->size);
	for (unsigned i = 0; i < file_system.copies; i++)
	{
		gcry_cipher_hd_t cipher_handle = cipher_init(file, i);
		if (!block_write(file->inodes[i], inode, cipher_handle, file->path))
		{
			for (unsigned j = 0; j <= i; j++)
				/*
				 * it’s likely that if a write failed above, it won’t work
				 * here either, but at least the block will be marked as
				 * available
				 * (in fact if a call to write fails it’s likely all subsequent
				 * write will fail too)
				 */
				block_delete(file->inodes[j]);
			stegfs_file_delete(file);
			gcry_cipher_close(cipher_handle);
			return false;
		}
		gcry_cipher_close(cipher_handle);
	}

	stegfs_cache2_add(NULL, file);
	return true;
}

extern void stegfs_file_delete(stegfs_file_t *file)
{
	char *p = NULL;
	if (!stegfs_file_stat(file))
		goto rfc;
	stegfs_block_t block;
	lldiv_t d = lldiv(file->size - (file->size < (sizeof block.data - file_system.head_offset) ? file->size : (sizeof block.data - file_system.head_offset)), SIZE_BYTE_DATA);
	uint64_t blocks = d.quot + (d.rem > 0);
	for (unsigned i = 0; i < file_system.copies; i++)
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

static bool block_read(uint64_t bid, stegfs_block_t *block, gcry_cipher_hd_t cipher, const char * const restrict path)
{
	errno = EXIT_SUCCESS;
	bid %= (file_system.size / file_system.blocksize);
	if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
		return errno = EINVAL, false;
	memcpy(block, file_system.memory + (bid * file_system.blocksize), sizeof(stegfs_block_t));
	size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
	uint8_t *hash_buffer = gcry_malloc_secure(hash_length);
	/* ignore path check in root */
	if (!path_equals(path, DIR_SEPARATOR))
	{
		/* check path hash */
		gcry_md_hash_buffer(file_system.hash, hash_buffer, path, strlen(path));
		if (memcmp(block->path, hash_buffer, hash_length))
		{
			gcry_free(hash_buffer);
			return false;
		}
	}
#ifndef __DEBUG__
	/* decrypt block, but not the path */
	void *ptr = block;
	ptr += sizeof block->path;
	gcry_cipher_decrypt(cipher, ptr, file_system.blocksize - sizeof block->path, NULL, 0);
#else
	(void)cipher;
#endif
	/* check data hash */
	gcry_md_hash_buffer(file_system.hash, hash_buffer, block->data, sizeof block->data);
	if (memcmp(block->hash, hash_buffer, hash_length))
	{
		gcry_free(hash_buffer);
		return false;
	}
	gcry_free(hash_buffer);
	return true;
}

static bool block_write(uint64_t bid, stegfs_block_t block, gcry_cipher_hd_t cipher, const char * const restrict path)
{
	errno = EXIT_SUCCESS;
	bid %= (file_system.size / file_system.blocksize);
	if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
		return errno = EINVAL, false;
	gcry_create_nonce((void *)block.path, sizeof block.path);
	size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
	uint8_t *hash_buffer = gcry_malloc_secure(hash_length);
	if (!path_equals(path, DIR_SEPARATOR))
	{
		/* compute path hash */
		gcry_md_hash_buffer(file_system.hash, hash_buffer, path, strlen(path));
		memcpy(block.path, hash_buffer, hash_length > sizeof block.path ? sizeof block.path : hash_length);
	}
	/* compute data hash (includes 0x00 after EOF) */
	gcry_md_hash_buffer(file_system.hash, hash_buffer, block.data, sizeof block.data);
	memcpy(block.hash, hash_buffer, hash_length > sizeof block.hash ? sizeof block.hash : hash_length);
	gcry_free(hash_buffer);
#ifndef __DEBUG__
	/* encrypt the data, but not the path */
	void *ptr = &block;
	ptr += sizeof block.path;
	gcry_cipher_encrypt(cipher, ptr, file_system.blocksize - sizeof block.path, NULL, 0);
#else
	(void)cipher;
#endif
	memcpy(file_system.memory + (bid * file_system.blocksize), &block, sizeof block);
	msync(file_system.memory + (bid * file_system.blocksize), sizeof block, MS_SYNC);
	return true;
}

static void block_delete(uint64_t bid)
{
	bid %= (file_system.size / file_system.blocksize);
	stegfs_block_t block;
	memcpy(&block, file_system.memory + (bid * file_system.blocksize), sizeof block);
	gcry_create_nonce(&block, file_system.blocksize);
	if (!bid || (bid * file_system.blocksize + file_system.blocksize > file_system.size))
		return;
	memcpy(file_system.memory + (bid * file_system.blocksize), &block, sizeof block);
	msync(file_system.memory + (bid * file_system.blocksize), sizeof block, MS_SYNC);
	file_system.blocks.in_use[bid] = false;
	file_system.blocks.used--;
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
	if (file_system.blocks.in_use[bid])
		return true;
	/*
	 * block not found in cache; check if this might belong to a file in
	 * this directory, or any parent directory
	 */
#ifndef __DEBUG__
	size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
	uint8_t *hash_buffer = gcry_malloc_secure(hash_length);
	char *p = NULL;
	uint16_t hierarchy = dir_get_deep(path);
	for (uint16_t i = 1; i < hierarchy; i++)
	{
		char *e = dir_get_part(path, i);
		asprintf(&p, "%s/%s", p ? : "", e ? : "");
		if (e)
			free(e);
		gcry_md_hash_buffer(file_system.hash, hash_buffer, p, strlen(p));
		if (!memcmp(hash_buffer, file_system.memory + (bid * file_system.blocksize), hash_length))
		{
			gcry_free(hash_buffer);
			/*
			 * block detected as being used by a file that exists closer
			 * to the root of the system; mark it as such
			 */
			file_system.blocks.in_use[bid] = true;
			file_system.blocks.used++;
			return true;
		}
	}
	free(p);
	gcry_free(hash_buffer);
#else
	(void)path;
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
		/* eventually “timeout” after trying as many blocks as exists */
		if ((++tries) > file_system.size / file_system.blocksize)
			return 0;
	}
	while (block_in_use(block, path));
	file_system.blocks.in_use[block % (file_system.size / file_system.blocksize)] = true;
	file_system.blocks.used++;
	return block;
}

static gcry_cipher_hd_t cipher_init(const stegfs_file_t * const restrict file, uint8_t ivi)
{
	/* obtain handles */
	gcry_cipher_hd_t cipher;
	gcry_cipher_open(&cipher, file_system.cipher, file_system.mode, GCRY_CIPHER_SECURE);
	gcry_md_hd_t hash;
	gcry_md_open(&hash, file_system.hash, GCRY_MD_FLAG_SECURE);
	/* create the initial key for the encryption algorithm */
	size_t key_length = gcry_cipher_get_algo_keylen(file_system.cipher);
	size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
	/* allocate space for whichever is larger */
	uint8_t *key = gcry_calloc_secure(key_length > hash_length ? key_length : hash_length, sizeof(uint8_t));
	gcry_md_write(hash, file->path, strlen(file->path));
	gcry_md_write(hash, file->name, strlen(file->name));
	if (file->pass)
		gcry_md_write(hash, file->pass, strlen(file->pass));
	else
		gcry_md_write(hash, "", strlen(""));
	memcpy(key, gcry_md_read(hash, file_system.hash), key_length);
	gcry_cipher_setkey(cipher, key, key_length);
	gcry_free(key);
	/* create the iv for the encryption algorithm */
	gcry_md_reset(hash);
	size_t iv_length = gcry_cipher_get_algo_blklen(file_system.cipher);
	/* allocate space for whichever is larger */
	uint8_t *iv = gcry_calloc_secure(iv_length > hash_length ? iv_length : hash_length, sizeof(uint8_t));
	if (file->pass)
		gcry_md_write(hash, file->pass, strlen(file->pass));
	else
		gcry_md_write(hash, "", strlen(""));
	gcry_md_write(hash, file->name, strlen(file->name));
	gcry_md_write(hash, file->path, strlen(file->path));
	gcry_md_write(hash, &ivi, sizeof ivi);
	memcpy(iv, gcry_md_read(hash, file_system.hash), iv_length);
	gcry_cipher_setiv(cipher, iv, iv_length);
	gcry_md_close(hash);
	return cipher;
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
	uint16_t hierarchy = dir_get_deep(p);
	for (uint16_t i = 1; i < hierarchy; i++)
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
			ptr->child[k] = calloc(sizeof(stegfs_cache2_t), sizeof(uint8_t));
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
	ptr->child[j] = calloc(sizeof(stegfs_cache2_t), sizeof(uint8_t));
	ptr = ptr->child[j];
	ptr->name = name;
c2a3:
	if (file && file != ptr->file)
	{
		if (!ptr->file)
			ptr->file = calloc(sizeof(stegfs_file_t), sizeof(uint8_t));
		/* set path and name */
		asprintf(&ptr->file->path, "%s", file->path);
		asprintf(&ptr->file->name, "%s", file->name);
		asprintf(&ptr->file->pass, "%s", file->pass);
		/* set inodes */
		for (unsigned i = 0; i < file_system.copies; i++)
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
			stegfs_block_t block;
			lldiv_t d = lldiv(file->size - (file->size < (sizeof block.data - file_system.head_offset) ? file->size : (sizeof block.data - file_system.head_offset)), SIZE_BYTE_DATA);
			uint64_t blocks = d.quot + (d.rem > 0);
			for (unsigned i = 0; i < file_system.copies; i++)
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
//    if (name)
//        free(name);
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
	uint16_t hierarchy = dir_get_deep(path);
	for (uint16_t i = 1; i < hierarchy; i++)
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
			goto done;
	}
	for (uint64_t i = 0; i < ptr->ents; i++)
		if (ptr->child[i]->name && !strcmp(ptr->child[i]->name, name))
		{
			free(name);
			if (entry)
				memcpy(entry, ptr->child[i], sizeof(stegfs_cache2_t));
			return ptr->child[i];
		}
done:
	if (name)
		free(name);
	return NULL;
}

extern void stegfs_cache2_remove(const char * const restrict path)
{
	stegfs_cache2_t *ptr = &(file_system.cache2);
	char *name = dir_get_name(path, PASSWORD_SEPARATOR);
	uint16_t hierarchy = dir_get_deep(path);
	for (uint16_t i = 1; i < hierarchy; i++)
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
			goto done;
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
		goto done;
	if (ptr->file)
	{
		free(ptr->file->path);
		free(ptr->file->name);
		free(ptr->file->pass);
		if (ptr->file->data)
			free(ptr->file->data);
		for (unsigned i = 0; i < file_system.copies; i++)
			if (ptr->file->blocks[i])
			{
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
done:
	if (name)
		free(name);
	return;
}
