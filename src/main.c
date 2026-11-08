/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2026, albinoloverats ~ Software Development
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
#include <error.h>

#define FUSE_USE_VERSION 27
#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include <limits.h>
#include <fcntl.h>

#include <sys/statvfs.h>
#include <sys/stat.h>

#include <gcrypt.h>

/* submodule includes */

#include "mem.h"
#include "config.h"
#include "ccrypt.h"
#include "dir.h"

/* project includes */

#include "stegfs.h"


/*
 * standard file system functions (used by fuse)
 */
static int fuse_stegfs_statfs(const char *, struct statvfs *);
static int fuse_stegfs_getattr(const char *, struct stat *);
static int fuse_stegfs_mkdir(const char *, mode_t);
static int fuse_stegfs_rmdir(const char *);
static int fuse_stegfs_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
static int fuse_stegfs_unlink(const char *);
static int fuse_stegfs_read(const char *, char *, size_t, off_t , struct fuse_file_info *);
static int fuse_stegfs_write(const char *, const char *, size_t, off_t , struct fuse_file_info *);
static int fuse_stegfs_open(const char *, struct fuse_file_info *);
static int fuse_stegfs_release(const char *, struct fuse_file_info *);
static int fuse_stegfs_truncate(const char *, off_t);
static int fuse_stegfs_ftruncate(const char *, off_t, struct fuse_file_info *);
#ifdef STEGFS_FALLOCATE
static int fuse_stegfs_fallocate(const char *, int, off_t, off_t, struct fuse_file_info *);
#endif
static int fuse_stegfs_create(const char *, mode_t, struct fuse_file_info *);
static int fuse_stegfs_mknod(const char *, mode_t, dev_t);
static void fuse_stegfs_destroy(void *);
/*
 * empty functions; required by fuse, but not used by stegfs
 */
static int fuse_stegfs_readlink(const char *, char *, size_t);
static int fuse_stegfs_utime(const char *, struct utimbuf *);
static int fuse_stegfs_chmod(const char *, mode_t);
static int fuse_stegfs_chown(const char *, uid_t, gid_t);
static int fuse_stegfs_flush(const char *, struct fuse_file_info *);

static pthread_rwlock_t filesystem_lock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct open_file_s
{
	struct open_file_s *next;
	stegfs_file_s *file;
	char *path;
	char *pass;
	bool readable;
	bool writable;
}
open_file_s;

static open_file_s *open_files;

static void filesystem_read_lock(void)
{
	int result = pthread_rwlock_rdlock(&filesystem_lock);
	if (result)
		die("Unable to read-lock filesystem state: %s", strerror(result));
}

static void filesystem_write_lock(void)
{
	int result = pthread_rwlock_wrlock(&filesystem_lock);
	if (result)
		die("Unable to write-lock filesystem state: %s", strerror(result));
}

static void filesystem_unlock(void)
{
	int result = pthread_rwlock_unlock(&filesystem_lock);
	if (result)
		die("Unable to unlock filesystem state: %s", strerror(result));
}

#define FUSE_CALLBACK(name, lock, parameters, arguments) \
	static int dispatch_##name parameters \
	{ \
		lock(); \
		int result = fuse_stegfs_##name arguments; \
		filesystem_unlock(); \
		return result; \
	}

FUSE_CALLBACK(statfs, filesystem_read_lock, (const char *path, struct statvfs *buf), (path, buf))
FUSE_CALLBACK(getattr, filesystem_read_lock, (const char *path, struct stat *buf), (path, buf))
FUSE_CALLBACK(mkdir, filesystem_write_lock, (const char *path, mode_t mode), (path, mode))
FUSE_CALLBACK(rmdir, filesystem_write_lock, (const char *path), (path))
FUSE_CALLBACK(readdir, filesystem_read_lock, (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info), (path, buf, filler, offset, info))
FUSE_CALLBACK(unlink, filesystem_write_lock, (const char *path), (path))
FUSE_CALLBACK(read, filesystem_read_lock, (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info), (path, buf, size, offset, info))
FUSE_CALLBACK(write, filesystem_write_lock, (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info), (path, buf, size, offset, info))
FUSE_CALLBACK(open, filesystem_write_lock, (const char *path, struct fuse_file_info *info), (path, info))
FUSE_CALLBACK(release, filesystem_write_lock, (const char *path, struct fuse_file_info *info), (path, info))
FUSE_CALLBACK(truncate, filesystem_write_lock, (const char *path, off_t offset), (path, offset))
FUSE_CALLBACK(ftruncate, filesystem_write_lock, (const char *path, off_t offset, struct fuse_file_info *info), (path, offset, info))
#ifdef STEGFS_FALLOCATE
FUSE_CALLBACK(fallocate, filesystem_write_lock, (const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *info), (path, mode, offset, length, info))
#endif
FUSE_CALLBACK(create, filesystem_write_lock, (const char *path, mode_t mode, struct fuse_file_info *info), (path, mode, info))
FUSE_CALLBACK(mknod, filesystem_write_lock, (const char *path, mode_t mode, dev_t rdev), (path, mode, rdev))
FUSE_CALLBACK(readlink, filesystem_read_lock, (const char *path, char *buf, size_t size), (path, buf, size))
FUSE_CALLBACK(flush, filesystem_read_lock, (const char *path, struct fuse_file_info *info), (path, info))

#undef FUSE_CALLBACK

static open_file_s *open_file_lookup(const struct fuse_file_info *info)
{
	if (!info || !info->fh)
		return NULL;
	for (open_file_s *file = open_files; file; file = file->next)
		if ((uint64_t)(uintptr_t)file == info->fh)
			return file;
	return NULL;
}

static void open_file_attach(const char *path, struct fuse_file_info *info)
{
	if (!info)
		return;
	open_file_s *file = m_calloc(1, sizeof *file);
	file->path = m_strdup(path);
	file->pass = dir_get_pass(path);
	file->readable = (info->flags & O_ACCMODE) != O_WRONLY;
	file->writable = (info->flags & O_ACCMODE) != O_RDONLY;
	file->next = open_files;
	open_files = file;
	info->fh = (uint64_t)(uintptr_t)file;
	stegfs_cache_s *cached = stegfs_cache_exists(path, NULL);
	if (cached && cached->file)
	{
		file->file = cached->file;
		cached->file->open_count++;
	}
}

static bool path_is_open_unlinked(const char *path)
{
	for (open_file_s *file = open_files; file; file = file->next)
		if (file->file && file->file->unlinked && !strcmp(file->path, path))
			return true;
	return false;
}

static void open_file_detach(open_file_s *file, struct fuse_file_info *info)
{
	open_file_s **link = &open_files;
	while (*link && *link != file)
		link = &(*link)->next;
	if (*link)
		*link = file->next;
	if (info)
		info->fh = 0;
	free(file->path);
	free(file->pass);
	free(file);
}

static void dispatch_destroy(void *ptr)
{
	filesystem_write_lock();
	fuse_stegfs_destroy(ptr);
	filesystem_unlock();
}

static bool block_index_from_path(const char *path, uint64_t *index)
{
	char *name = dir_get_name(path, PASSWORD_SEPARATOR);
	if (!name || !*name)
	{
		free(name);
		return false;
	}
	char *end = NULL;
	errno = 0;
	uintmax_t value = strtoumax(name, &end, 10);
	bool valid = !errno && end && !*end && value <= UINT64_MAX;
	free(name);
	if (!valid)
		return false;
	*index = (uint64_t)value;
	return true;
}

static struct fuse_operations fuse_stegfs_functions =
{
	.statfs    = dispatch_statfs,
	.getattr   = dispatch_getattr,
	.mkdir     = dispatch_mkdir,
	.rmdir     = dispatch_rmdir,
	.readdir   = dispatch_readdir,
	.unlink    = dispatch_unlink,
	.read      = dispatch_read,
	.write     = dispatch_write,
	.open      = dispatch_open,
	.release   = dispatch_release,
	.truncate  = dispatch_truncate,
	.ftruncate = dispatch_ftruncate,
#ifdef STEGFS_FALLOCATE
	.fallocate = dispatch_fallocate,
#endif
	.create    = dispatch_create,
	.mknod     = dispatch_mknod,
	.destroy   = dispatch_destroy,
	.readlink  = dispatch_readlink,
	/*
	 * empty functions; required by fuse, but not used by stegfs
	 */
	.utime     = fuse_stegfs_utime,
	.chmod     = fuse_stegfs_chmod,
	.chown     = fuse_stegfs_chown,
	.flush     = dispatch_flush
};

extern bool is_stegfs(void)
{
	return true;
}

static int fuse_stegfs_statfs(const char *path, struct statvfs *stvbuf)
{
	errno = EXIT_SUCCESS;

	(void)path;

	stegfs_s file_system = stegfs_info();

	stvbuf->f_bsize   = SIZE_BYTE_BLOCK;
	stvbuf->f_frsize  = SIZE_BYTE_DATA;
	stvbuf->f_blocks  = file_system.size / SIZE_BYTE_BLOCK - 1;
	stvbuf->f_bfree   = file_system.blocks.used > 0
		? (stvbuf->f_blocks > file_system.blocks.used - 1
			? stvbuf->f_blocks - (file_system.blocks.used - 1) : 0)
		: stvbuf->f_blocks;
	stvbuf->f_bavail  = stvbuf->f_bfree;
	stvbuf->f_files   = stvbuf->f_blocks;
	stvbuf->f_ffree   = stvbuf->f_bfree;
	stvbuf->f_favail  = stvbuf->f_bfree;
	stvbuf->f_fsid    = HASH_MAGIC_2;
	stvbuf->f_flag    = ST_NOSUID;
	stvbuf->f_namemax = SYM_LENGTH;

	return -errno;
}

static int fuse_stegfs_getattr(const char *path, struct stat *stbuf)
{
	errno = EXIT_SUCCESS;

	stegfs_s file_system = stegfs_info();
	/* common attributes for root/files/directories */
	stbuf->st_dev     = (dev_t)HASH_MAGIC_2;
	stbuf->st_ino     = 0;
	stbuf->st_mode    = 0;//S_IFDIR | 0700;
	stbuf->st_nlink   = 0;
	stbuf->st_uid     = fuse_get_context()->uid;
	stbuf->st_gid     = fuse_get_context()->gid;
	stbuf->st_atime   = time(NULL);
	stbuf->st_ctime   = time(NULL);
	stbuf->st_mtime   = time(NULL);
	stbuf->st_size    = 0;
	stbuf->st_blksize = 0;

	if (path_equals(DIR_SEPARATOR, path))
	{
		stbuf->st_mode  = S_IFDIR | S_IRWXU;
		stbuf->st_nlink = 2;
		for (uint64_t i = 0; i < file_system.cache.ents; i++)
			if (file_system.cache.child[i] && !file_system.cache.child[i]->file)
				stbuf->st_nlink++;
	}
	else if (file_system.show_bloc && path_equals(PATH_BLOC, path))
		/* if path == /bloc/ */
		stbuf->st_mode = S_IFDIR | S_IRUSR | S_IXUSR;
	else if (file_system.show_bloc && path_starts_with(PATH_BLOC, path))
	{
		/*
		 * if we’re looking at files in /bloc/ treat them as blocks (as
		 * that’s what we’re representing)
		 */
		//stbuf->st_mode  = S_IFBLK | S_IRUSR;
		stbuf->st_mode  = S_IFLNK | S_IRUSR;
		stbuf->st_nlink = 1;

		uint64_t ino;
		uint64_t block_count = file_system.blocksize ? file_system.size / file_system.blocksize : 0;
		if (!block_index_from_path(path, &ino) || ino >= block_count)
			errno = ENOENT;
		else if (file_system.blocks.file[ino])
			stbuf->st_size = strlen(file_system.blocks.file[ino]);
	}
	else
	{
		stegfs_cache_s c;
		memset(&c, 0x00, sizeof c);
		if (stegfs_cache_exists(path, &c))
		{
			if (c.file)
			{
				for (unsigned i = 0; i < file_system.copies; i++)
					if (c.file->inodes[i])
					{
						stbuf->st_ino = (ino_t)(c.file->inodes[i] % (file_system.size / SIZE_BYTE_BLOCK));
						break;
					}
				/* it makes little sense (right now) to set this to anything else */
				stbuf->st_mode  = S_IFREG | S_IRUSR | S_IWUSR;
				stbuf->st_ctime = c.file->time;
				stbuf->st_mtime = c.file->time;
				stbuf->st_size  = c.file->size;
			}
			else
			{
				stbuf->st_mode  = S_IFDIR | S_IRWXU;
				stbuf->st_nlink = 2;
				for (uint64_t i = 0; i < c.ents; i++)
					if (c.child[i] && !c.child[i]->file)
						stbuf->st_nlink++;
			}
		}
		else
		{
			if (path_is_open_unlinked(path))
				return errno = ENOENT, -errno;
			stegfs_file_s file;
			memset(&file, 0x00, sizeof file);
			file.path = dir_get_path(path);
			file.name = dir_get_name(path, PASSWORD_SEPARATOR);
			file.pass = dir_get_pass(path);
			if (stegfs_file_stat(&file))
			{
				for (unsigned i = 0; i < file_system.copies; i++)
					if (file.inodes[i])
					{
						stbuf->st_ino = (ino_t)(file.inodes[i] % (file_system.size / SIZE_BYTE_BLOCK));
						break;
					}
				stbuf->st_mode  = S_IFREG | S_IRUSR | S_IWUSR;
				stbuf->st_ctime = file.time;
				stbuf->st_mtime = file.time;
				stbuf->st_size  = file.size;
			}
			else
				errno = ENOENT;
			free(file.path);
			free(file.name);
			free(file.pass);
		}
	}
	if (S_ISREG(stbuf->st_mode))
	{
		stbuf->st_nlink = 1;
		stbuf->st_blksize = SIZE_BYTE_DATA;
		lldiv_t d = lldiv(stbuf->st_size, stbuf->st_blksize);
		stbuf->st_blocks = d.quot + (d.rem > 0);
	}
	else if (S_ISDIR(stbuf->st_mode))
	{
		size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
		uint8_t *hash_buffer = m_gcry_malloc_secure(hash_length);
		gcry_md_hash_buffer(file_system.hash, hash_buffer, path, strlen(path));
		memcpy(&(stbuf->st_ino), hash_buffer, sizeof stbuf->st_ino);
		gcry_free(hash_buffer);
		stbuf->st_ino %= (file_system.size / SIZE_BYTE_BLOCK);
		stbuf->st_size = SIZE_BYTE_DATA;
	}
	return -errno;
}

static int fuse_stegfs_mkdir(const char *path, mode_t mode)
{
	errno = EXIT_SUCCESS;

	(void)mode;

	if (stegfs_cache_exists(path, NULL))
		return errno = EEXIST, -errno;
	stegfs_cache_add(path, NULL);

	return -errno;
}

static int fuse_stegfs_rmdir(const char *path)
{
	errno = EXIT_SUCCESS;

	stegfs_s file_system = stegfs_info();
	if (file_system.show_bloc && path_equals(path, PATH_BLOC))
		return errno = EBUSY, -errno;

	stegfs_cache_s *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)))
	{
		if (c->file)
			errno = ENOTDIR;
		else
		{
			bool empty = true;
			for (uint64_t i = 0; i < c->ents; i++)
				if (c->child[i] && c->child[i]->name)
					empty = false;
			if (!empty)
				errno = ENOTEMPTY;
			else
				stegfs_cache_remove(path);
		}
	}
	else
		errno = ENOENT;

	return -errno;
}

static int fuse_stegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	if (offset < 0)
		return errno = EINVAL, -errno;
	off_t cookie = 1;
	if (offset < cookie && filler(buf, ".", NULL, cookie))
		return 0;
	cookie++;
	if (offset < cookie && filler(buf, "..", NULL, cookie))
		return 0;

	stegfs_s file_system = stegfs_info();

	if (path_equals(DIR_SEPARATOR, path))
	{
		for (uint64_t i = 0; i < file_system.cache.ents; i++)
			if (file_system.cache.child[i]->name
					&& (!file_system.cache.child[i]->file || !file_system.cache.child[i]->file->unlinked))
			{
				cookie = (off_t)i + 3;
				if (offset < cookie && filler(buf, file_system.cache.child[i]->name, NULL, cookie))
					break;
			}
	}
	else if (file_system.show_bloc && path_equals(PATH_BLOC, path))
	{
		for (uint64_t i = 0; i < file_system.size / SIZE_BYTE_BLOCK; i++)
			if (file_system.blocks.in_use[i])
			{
				char b[21] = { 0x0 }; // max digits for UINT64_MAX
				snprintf(b, sizeof b, "%ju", i);
				cookie = (off_t)i + 3;
				if (offset < cookie && filler(buf, b, NULL, cookie))
					break;
			}
	}
	else
	{
		stegfs_cache_s c;
		memset(&c, 0x00, sizeof c);
		if (stegfs_cache_exists(path, &c))
			for (uint64_t i = 0; i < c.ents; i++)
				if (c.child[i]->name
						&& (!c.child[i]->file || !c.child[i]->file->unlinked))
				{
					cookie = (off_t)i + 3;
					if (offset < cookie && filler(buf, c.child[i]->name, NULL, cookie))
						break;
				}
	}

	return -errno;
}

static int fuse_stegfs_unlink(const char *path)
{
	errno = EXIT_SUCCESS;
	stegfs_cache_s *cached = stegfs_cache_exists(path, NULL);
	if (!cached && path_is_open_unlinked(path))
		return errno = ENOENT, -errno;
	if (cached && !cached->file)
		return errno = EISDIR, -errno;
	if (cached && cached->file && cached->file->open_count)
	{
		cached->file->unlinked = true;
		stegfs_file_unlink(cached->file);
		return 0;
	}
	if (cached && cached->file && !cached->file->inodes[0])
	{
		stegfs_cache_remove(path);
		return 0;
	}

	stegfs_file_s file;
	memset(&file, 0x00, sizeof file);
	file.path = dir_get_path(path);
	file.name = dir_get_name(path, PASSWORD_SEPARATOR);
	file.pass = dir_get_pass(path);

	bool exists = stegfs_file_stat(&file);
	if (exists)
		stegfs_file_delete(&file);
	else
		errno = ENOENT;

	free(file.path);
	free(file.name);
	free(file.pass);

	return -errno;
}

static int fuse_stegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	if (offset < 0)
		return errno = EINVAL, -errno;
	open_file_s *opened = open_file_lookup(info);
	if (!opened || strcmp(opened->path, path))
		return errno = EBADF, -errno;
	if (!opened->readable)
		return errno = EBADF, -errno;

	stegfs_file_s *file = opened->file;
	if (file)
	{
		if ((uint64_t)offset >= file->size || !size)
			return 0;
		uint64_t available = file->size - (uint64_t)offset;
		if (size > available)
			size = (size_t)available;
		if (!file->data)
			return errno = EIO, -errno;
		if (size > INT_MAX)
			size = INT_MAX;
		memcpy(buf, file->data + offset, size);
		return size;
	}

	return errno = ENOENT, -errno;
}

static int fuse_stegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	if (offset < 0)
		return errno = EINVAL, -errno;
	if (size > INT_MAX)
		return errno = EOVERFLOW, -errno;
	if (size > UINT64_MAX - (uint64_t)offset)
		return errno = EFBIG, -errno;
	open_file_s *opened = open_file_lookup(info);
	if (!opened || strcmp(opened->path, path))
		return errno = EBADF, -errno;
	if (!opened->writable)
		return errno = EBADF, -errno;
	uint64_t end = (uint64_t)offset + size;

	stegfs_file_s *file = opened->file;
	if (!file)
		return errno = EBADF, -errno;
	uint64_t old_size = file->size;
	uint64_t new_size = old_size > end ? old_size : end;
	stegfs_file_s candidate = *file;
	candidate.size = new_size;
	if (!stegfs_file_will_fit(&candidate))
		return -errno;
	if (new_size > old_size)
	{
		file->data = m_realloc(file->data, new_size);
		memset(file->data + old_size, 0, new_size - old_size);
	}
	file->size = new_size;
	file->time = time(NULL);
	file->dirty = true;
	if (size)
		memcpy(file->data + offset, buf, size);
	return size;
}

static int fuse_stegfs_open(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	stegfs_cache_s *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)) && c->file)
	{
		if (!info)
			return errno = EINVAL, -errno;
		open_file_s *opened = open_file_lookup(info);
		if (opened)
			return strcmp(opened->path, path) ? (errno = EBADF, -errno) : 0;
		if (info->fh)
			return errno = EBADF, -errno;

		char *pass = dir_get_pass(path);
		if (!c->file->loaded && c->file->inodes[0])
		{
			free(c->file->pass);
			c->file->pass = m_strdup(pass);
			if (!stegfs_file_read(c->file))
			{
				free(c->file->pass);
				c->file->pass = NULL;
				free(pass);
				return errno = EACCES, -errno;
			}
		}
		else if (c->file->loaded && c->file->pass
				&& strcmp(c->file->pass, pass ? pass : ""))
		{
			free(pass);
			return errno = EACCES, -errno;
		}
		free(pass);
		open_file_attach(path, info);
	}
	else
		errno = c ? EISDIR : ENOENT;

	return -errno;
}

static int fuse_stegfs_flush(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	open_file_s *opened = open_file_lookup(info);
	if (!opened || strcmp(opened->path, path) || !opened->file)
		return errno = EBADF, -errno;
	stegfs_file_will_fit(opened->file);

	return -errno;
}

static int fuse_stegfs_truncate(const char *path, off_t offset)
{
	errno = EXIT_SUCCESS;

	return fuse_stegfs_ftruncate(path, offset, NULL);
}

static int fuse_stegfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;
	if (offset < 0)
		return errno = EINVAL, -errno;

	stegfs_file_s *file = NULL;
	if (info)
	{
		open_file_s *opened = open_file_lookup(info);
		if (!opened || strcmp(opened->path, path) || !opened->writable)
			return errno = EBADF, -errno;
		file = opened->file;
	}
	else
	{
		stegfs_cache_s *cached = stegfs_cache_exists(path, NULL);
		file = cached ? cached->file : NULL;
	}
	if (file)
	{
		if (!file->data && file->size && !stegfs_file_read(file))
			return -errno;
		stegfs_file_s candidate = *file;
		candidate.size = (uint64_t)offset;
		if (!stegfs_file_will_fit(&candidate))
			return -errno;
		uint64_t old_size = file->size;
		uint64_t new_size = (uint64_t)offset;
		if (new_size != old_size)
		{
			if (new_size)
				file->data = m_realloc(file->data, new_size);
			else
			{
				free(file->data);
				file->data = NULL;
			}
			if (new_size > old_size)
				memset(file->data + old_size, 0, new_size - old_size);
			file->size = new_size;
			file->time = time(NULL);
		}
		file->dirty = true;
		return 0;
	}
	return errno = stegfs_cache_exists(path, NULL) ? EISDIR : ENOENT, -errno;
}

#ifdef STEGFS_FALLOCATE
static int fuse_stegfs_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	/* this isn't finished or working properly */

	stegfs_cache_s *c = NULL;
	while (true)
	{
		if ((c = stegfs_cache_exists(path, NULL)) && c->file)
		{
			uint64_t sz = offset + length;
			switch (mode)
			{
				case -1:                        /* emulate truncate */
					break;
				case 0:                         /* make bigger (not smaller) */
					if (sz < c->file->size)
						sz = c->file->size;
					break;
				case FALLOC_FL_KEEP_SIZE:       /* no size change */
				case FALLOC_FL_PUNCH_HOLE:
					return errno = EOPNOTSUPP, -errno;
				case FALLOC_FL_COLLAPSE_RANGE:  /* remove data from middle */
					if (sz > c->file->size)
						return errno = EINVAL, -errno;
				case FALLOC_FL_ZERO_RANGE:      /* does nothing here, but zeros later */
					break;
			}
			char *buf = m_calloc(sz, sizeof(uint8_t));
			fuse_stegfs_read(path, buf, sz, 0, info);
			fuse_stegfs_unlink(path);
			switch (mode)
			{
				case FALLOC_FL_COLLAPSE_RANGE:
					memmove(buf + offset, buf + offset + length, sz - offset - length);
					sz -= offset - length;
					break;
				case FALLOC_FL_ZERO_RANGE:
					memset(buf + offset, 0x00, length);
					break;
			}
			fuse_stegfs_write(path, buf, sz, 0, info);
			free(buf);
			return -errno;
		}
		else if (c && !c->file)
			return errno = EISDIR, -errno;
		else
			stegfs_file_create(path, true);
	}

	return errno = EIO, -errno;
}
#endif

static int fuse_stegfs_create(const char *path, mode_t mode, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)mode;
	if (stegfs_cache_exists(path, NULL))
		return errno = EEXIST, -errno;

	stegfs_file_create(path, true);
	open_file_attach(path, info);

	return -errno;
}

static int fuse_stegfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	errno = EXIT_SUCCESS;

	(void)mode;
	(void)rdev;

	if (stegfs_cache_exists(path, NULL))
		return errno = EEXIST, -errno;
	stegfs_file_create(path, false);

	return -errno;
}

static int fuse_stegfs_release(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	open_file_s *opened = open_file_lookup(info);
	if (!opened || strcmp(opened->path, path))
		return errno = EBADF, -errno;

	stegfs_file_s *file = opened->file;
	if (file)
	{
		if (file->open_count)
			file->open_count--;
		if (file->unlinked)
		{
			if (!file->open_count)
				stegfs_file_delete(file);
		}
		else
		{
			if (file->dirty)
			{
				free(file->pass);
				file->pass = m_strdup(opened->pass ? opened->pass : "");
				if (stegfs_file_will_fit(file) && stegfs_file_write(file))
					errno = EXIT_SUCCESS;
				else if (!errno)
					errno = EIO;
			}
			if (!file->open_count && !file->dirty && !path_is_open_unlinked(path))
			{
				free(file->data);
				file->data = NULL;
				file->loaded = false;
				free(file->pass);
				file->pass = NULL;
			}
		}
	}
	open_file_detach(opened, info);

	return -errno;
}

static void fuse_stegfs_destroy(void *ptr)
{
	(void)ptr;

	stegfs_deinit();
}

static int fuse_stegfs_readlink(const char *path, char *buf, size_t size)
{
	errno = EXIT_SUCCESS;

	stegfs_s file_system = stegfs_info();
	if (file_system.show_bloc && path_starts_with(PATH_BLOC, path))
	{
		uint64_t ino;
		uint64_t block_count = file_system.blocksize ? file_system.size / file_system.blocksize : 0;
		if (!block_index_from_path(path, &ino) || ino >= block_count)
			errno = ENOENT;
		else if (file_system.blocks.file[ino])
			snprintf(buf, size, "%s", file_system.blocks.file[ino]);
		else
			errno = ENOENT;
	}
	else
	{
		(void)path;
		(void)buf;
		(void)size;
		errno = ENOTSUP;
	}

	return -errno;
}

/*
 * empty functions; required by fuse, but not used by stegfs; return ENOTSUP
 */

static int fuse_stegfs_utime(const char *path, struct utimbuf *utime)
{
	(void)path;
	(void)utime;

	return errno = ENOTSUP, -errno;
}

static int fuse_stegfs_chmod(const char *path, mode_t mode)
{
	(void)path;
	(void)mode;

	return errno = ENOTSUP, -errno;
}

static int fuse_stegfs_chown(const char *path, uid_t uid, gid_t gid)
{
	(void)path;
	(void)uid;
	(void)gid;

	return errno = ENOTSUP, -errno;
}

int main(int argc, char **argv)
{
	char **fargs = m_calloc(argc, sizeof( char * ));
	fargs[0] = argv[0];

	list_t args = list_init(config_named_compare, false, false);
	list_add(args, &((config_named_s){ 'c', "cipher",         _("algorithm"),  _("Algorithm to use to encrypt data; use ‘list’ to show available cipher algorithms"), { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 's', "hash",           _("algorithm"),  _("Hash algorithm to generate key; use ‘list’ to show available hash algorithms"),     { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'm', "mode",           _("mode"),       _("The encryption mode to use; use ‘list’ to show available cipher modes"),            { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'a', "mac",            _("mac"),        _("The MAC algorithm to use; use ‘list’ to show available MACs"),                      { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'i', "kdf-iterations", _("iterations"), _("Number of iterations the KDF should use"),                                          { CONFIG_ARG_REQ_INTEGER, { .integer = 0     } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'p', "paranoid",       NULL,            _("Enable paranoia mode"),                                                             { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'x', "duplicates",     "#",             _("Number of times each file should be duplicated"),                                   { CONFIG_ARG_REQ_INTEGER, { .integer = 0     } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'b', "show-bloc",      NULL,            _("Expose the /bloc/ in-use block list directory"),                                    { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'd', NULL,             NULL,            _("Enable debug output (forces foreground and single-thread)"),                        { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'f', NULL,             NULL,            _("Foreground operation"),                                                             { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 't', NULL,             NULL,            _("Disable multi-threaded operation (FUSE option -s)"),                                { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'o', NULL,             "opt,[opt...]",  _("FUSE mount options--see FUSE documentation for details"),                           { CONFIG_ARG_LIST_STRING, { .list    = NULL  } }, false, false, false, false }));

	list_t extra = list_default();
	list_add(extra, &((config_unnamed_s){ "file system", { CONFIG_ARG_STRING,  { .string = NULL } }, true,  false }));
	list_add(extra, &((config_unnamed_s){ "mount point", { CONFIG_ARG_STRING,  { .string = NULL } }, true,  false }));

	list_t notes = list_default();
	list_add(notes, _("It doesn't matter which order the file system and mount point are specified as stegfs will figure that out. All other options are passed to FUSE."));
	list_add(notes, _("If you’re feeling extra paranoid you can now disable to stegfs file system header. This will also disable the checks when mounting and thus anything could happen ;-)"));

	config_about_s about =
	{
		STEGFS_NAME,
		STEGFS_VERSION,
		PROJECT_URL,
		NULL
	};
	config_init(about);
	config_parse(argc, argv, args, extra, notes, false);

	list_deinit(notes);

	char *fs  = NULL;
	char *mnt = NULL;
	struct stat s = { 0x0 };
	stat(((config_unnamed_s *)list_get(extra, 0))->response.value.string, &s);
	if (S_ISDIR(s.st_mode))
	{
		// 1st argument is directory, so it's the mount point
		mnt = ((config_unnamed_s *)list_get(extra, 0))->response.value.string;
		fs  = ((config_unnamed_s *)list_get(extra, 1))->response.value.string;
	}
	else
	{
		fs  = ((config_unnamed_s *)list_get(extra, 0))->response.value.string;
		mnt = ((config_unnamed_s *)list_get(extra, 1))->response.value.string;
	}
	list_deinit(extra);

	// config is actually read from the fs unless paranoid == true
	enum gcry_cipher_algos cipher = DEFAULT_CIPHER;
	enum gcry_cipher_modes mode   = DEFAULT_MODE;
	enum gcry_md_algos     hash   = DEFAULT_HASH;
	enum gcry_mac_algos    mac    = DEFAULT_MAC;
	uint64_t kdf_iters            = DEFAULT_KDF_ITERATIONS;
	bool paranoid                 = ((config_named_s *)list_get(args, 5))->response.value.boolean;
	uint8_t duplicates            = COPIES_DEFAULT;
	bool show_bloc                = ((config_named_s *)list_get(args, 7))->response.value.boolean;

	if (paranoid)
	{
		cipher     = cipher_id_from_name(((config_named_s *)list_get(args, 0))->response.value.string);
		mode       =   mode_id_from_name(((config_named_s *)list_get(args, 1))->response.value.string);
		hash       =   hash_id_from_name(((config_named_s *)list_get(args, 2))->response.value.string);
		mac        =    mac_id_from_name(((config_named_s *)list_get(args, 3))->response.value.string);
		kdf_iters  =                     ((config_named_s *)list_get(args, 4))->response.value.integer;
		duplicates =            (uint8_t)((config_named_s *)list_get(args, 6))->response.value.integer;
	}

	/*
	 * deal with FUSE options
	 */

	bool debug         =          ((config_named_s *)list_get(args,  8))->response.value.boolean;
	bool foreground    = debug || ((config_named_s *)list_get(args,  9))->response.value.boolean;
	bool single_thread = ((config_named_s *)list_get(args, 10))->response.value.boolean;

	int fuse_argc = 3;
	char **fuse_argv = m_calloc(fuse_argc, sizeof (char *));
	fuse_argv[0] = argv[0];
	fuse_argv[1] = mnt;
	if (debug)
	{
		fuse_argc++;
		fuse_argv = m_realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-d";
	}
	if (foreground)
	{
		fuse_argc++;
		fuse_argv = m_realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-f";
	}
	if (single_thread)
	{
		fuse_argc++;
		fuse_argv = m_realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-s";
	}
	list_t fuse_options = ((config_named_s *)list_get(args, 11))->response.value.list;
	iter_t iter = list_iterator(fuse_options);
	while (list_has_next(iter))
	{
		fuse_argc += 2;
		fuse_argv = m_realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 3] = "-o";
		fuse_argv[fuse_argc - 2] = (char *)list_get_next(iter);
	}
	fuse_argc--;
	fuse_argv[fuse_argc] = NULL;

	list_deinit(args);

	errno = EXIT_SUCCESS;
	switch (stegfs_init(fs, paranoid, cipher, mode, hash, mac, kdf_iters, duplicates, show_bloc))
	{
		case STEGFS_INIT_OKAY:
			goto done;
		case STEGFS_INIT_NOT_STEGFS:
			fprintf(stderr, "Not a stegfs partition!\n");
			break;
		case STEGFS_INIT_OLD_STEGFS:
			fprintf(stderr, "Previous (unsupported) version of stegfs!\n");
			break;
		case STEGFS_INIT_MISSING_TAG:
			fprintf(stderr, "Missing required stegfs metadata!\n");
			break;
		case STEGFS_INIT_INVALID_TAG:
			fprintf(stderr, "Invalid value for stegfs metadata!\n");
			break;
		case STEGFS_INIT_CORRUPT_TAG:
			fprintf(stderr, "Partition size mismatch! (Resizing not allowed!)\n");
			break;
		default:
			fprintf(stderr, "Unknown error initialising stegfs partition!\n");
			break;
	}
	return EXIT_FAILURE;
done:

	struct fuse_args f = FUSE_ARGS_INIT(fuse_argc, fuse_argv);
	fuse_opt_parse(&f, NULL, NULL, NULL);
	return fuse_main(f.argc, f.argv, &fuse_stegfs_functions, NULL);
}
