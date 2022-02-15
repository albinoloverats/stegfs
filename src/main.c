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


#include <errno.h>
#include <error.h>

#define FUSE_USE_VERSION 27
#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include <limits.h>
#include <fcntl.h>

#include <sys/statvfs.h>
#include <sys/stat.h>

#include <gcrypt.h>

#include "common/config.h"
#include "common/ccrypt.h"
#include "common/dir.h"

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

static struct fuse_operations fuse_stegfs_functions =
{
	.statfs    = fuse_stegfs_statfs,
	.getattr   = fuse_stegfs_getattr,
	.mkdir     = fuse_stegfs_mkdir,
	.rmdir     = fuse_stegfs_rmdir,
	.readdir   = fuse_stegfs_readdir,
	.unlink    = fuse_stegfs_unlink,
	.read      = fuse_stegfs_read,
	.write     = fuse_stegfs_write,
	.open      = fuse_stegfs_open,
	.release   = fuse_stegfs_release,
	.truncate  = fuse_stegfs_truncate,
	.ftruncate = fuse_stegfs_ftruncate,
#ifdef STEGFS_FALLOCATE
	.fallocate = fuse_stegfs_fallocate,
#endif
	.create    = fuse_stegfs_create,
	.mknod     = fuse_stegfs_mknod,
	.destroy   = fuse_stegfs_destroy,
	.readlink  = fuse_stegfs_readlink,
	/*
	 * empty functions; required by fuse, but not used by stegfs
	 */
	.utime     = fuse_stegfs_utime,
	.chmod     = fuse_stegfs_chmod,
	.chown     = fuse_stegfs_chown,
	.flush     = fuse_stegfs_flush
};

extern bool is_stegfs(void)
{
	return true;
}

static int fuse_stegfs_statfs(const char *path, struct statvfs *stvbuf)
{
	errno = EXIT_SUCCESS;

	(void)path;

	stegfs_t file_system = stegfs_info();

	stvbuf->f_bsize   = SIZE_BYTE_BLOCK;
	stvbuf->f_frsize  = SIZE_BYTE_DATA;
	stvbuf->f_blocks  = (file_system.size / SIZE_BYTE_BLOCK) - 1;
	stvbuf->f_bfree   = stvbuf->f_blocks - file_system.blocks.used;
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

	stegfs_t file_system = stegfs_info();
	char *f = dir_get_name(path, PASSWORD_SEPARATOR); /* used to check for symlink */
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

		char *b = dir_get_name(path, PASSWORD_SEPARATOR);
		uint64_t ino = strtol(b, NULL, 0);
		char *f = file_system.blocks.file[ino];
		if (f)
			stbuf->st_size = strlen(f);
		free(b);
	}
	else
	{
		stegfs_cache_t c;
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
			stegfs_file_t file;
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
	if (stbuf->st_mode & S_IFREG)
	{
		stbuf->st_nlink = 1;
		stbuf->st_blksize = SIZE_BYTE_DATA;
		lldiv_t d = lldiv(stbuf->st_size, stbuf->st_blksize);
		stbuf->st_blocks = d.quot + (d.rem > 0);
	}
	else if (stbuf->st_mode & S_IFDIR)
	{
		size_t hash_length = gcry_md_get_algo_dlen(file_system.hash);
		uint8_t *hash_buffer = gcry_malloc_secure(hash_length);
		gcry_md_hash_buffer(file_system.hash, hash_buffer, path, strlen(path));
		memcpy(&(stbuf->st_ino), hash_buffer, sizeof stbuf->st_ino);
		gcry_free(hash_buffer);
		stbuf->st_ino %= (file_system.size / SIZE_BYTE_BLOCK);
		stbuf->st_size = SIZE_BYTE_DATA;
	}
	free(f);

	return -errno;
}

static int fuse_stegfs_mkdir(const char *path, mode_t mode)
{
	errno = EXIT_SUCCESS;

	(void)mode;

	stegfs_cache_add(path, NULL);

	return -errno;
}

static int fuse_stegfs_rmdir(const char *path)
{
	errno = EXIT_SUCCESS;

	stegfs_t file_system = stegfs_info();
	if (file_system.show_bloc && path_equals(path, PATH_BLOC))
		return errno = EBUSY, -errno;

	stegfs_cache_t *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)))
	{
		if (c->file)
			errno = ENOTDIR;
		else
		{
			bool empty = true;
			for (uint64_t i = 0; i < c->ents; i++)
				if (c->child[i]->name)
					empty = false;
			if (!empty)
				errno = ENOTEMPTY;
			else
				stegfs_cache_remove(path);
		}
	}

	return -errno;
}

static int fuse_stegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)offset;
	(void)info;

	filler(buf,  ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	stegfs_t file_system = stegfs_info();

	if (path_equals(DIR_SEPARATOR, path))
	{
		for (uint64_t i = 0; i < file_system.cache.ents; i++)
			if (file_system.cache.child[i]->name)
				filler(buf, file_system.cache.child[i]->name, NULL, 0);
	}
	else if (file_system.show_bloc && path_equals(PATH_BLOC, path))
	{
		for (uint64_t i = 0; i < file_system.size / SIZE_BYTE_BLOCK; i++)
			if (file_system.blocks.in_use[i])
			{
				char b[21] = { 0x0 }; // max digits for UINT64_MAX
				snprintf(b, sizeof b, "%ju", i);
				filler(buf, b, NULL, 0);
			}
	}
	else
	{
		stegfs_cache_t c;
		memset(&c, 0x00, sizeof c);
		if (stegfs_cache_exists(path, &c))
			for (uint64_t i = 0; i < c.ents; i++)
				if (c.child[i]->name)
					filler(buf, c.child[i]->name, NULL, 0);
	}

	return -errno;
}

static int fuse_stegfs_unlink(const char *path)
{
	errno = EXIT_SUCCESS;

	stegfs_file_t file;
	memset(&file, 0x00, sizeof file);
	file.path = dir_get_path(path);
	file.name = dir_get_name(path, PASSWORD_SEPARATOR);
	file.pass = dir_get_pass(path);

	stegfs_file_delete(&file);

	free(file.path);
	free(file.name);
	free(file.pass);

	return -errno;
}

static int fuse_stegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	stegfs_cache_t *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)) && c->file)
	{
		if ((unsigned)(offset + size) > c->file->size)
			size = c->file->size - offset;
		memcpy(buf, c->file->data + offset, size);
		return size;
	}

	return errno = ENOENT, -errno;
}

static int fuse_stegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	/*
	 * if the file is cached (has been read/written to already) then
	 * just buffer this data until it’s released/flushed
	 */
	while (true)
	{
		stegfs_cache_t *c = NULL;
		if ((c = stegfs_cache_exists(path, NULL)) && c->file)
		{
			if (!stegfs_file_will_fit(c->file))
				return -errno;
			if (!c->file->write)
				return errno = EBADF, -errno;
			c->file->size = c->file->size > size + offset ? c->file->size : size + offset;
			c->file->data = realloc(c->file->data, c->file->size);
			c->file->time = time(NULL);
			memcpy(c->file->data + offset, buf, size);
			return size;
		}
		else if (c && !c->file)
			return errno = EISDIR, -errno;
		/*
		 * the file wasn’t found, so create it and try again, it should
		 * then fail if the file cannot fit, as at this point all that’s
		 * happening is increasing the cached buffer
		 */
		stegfs_file_create(path, true);
	}

	return errno = EIO, -errno;
}

static int fuse_stegfs_open(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	stegfs_cache_t *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)) && c->file)
	{
		c->file->pass = dir_get_pass(path);
		if (!stegfs_file_read(c->file))
			errno = EACCES;
		/* TODO use the fields in info for something meaningful */
	}

	return -errno;
}

static int fuse_stegfs_flush(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	stegfs_cache_t *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)) && c->file)
		if (stegfs_file_will_fit(c->file))
			errno = EXIT_SUCCESS;

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

	stegfs_cache_t *c = NULL;
	while (true)
	{
		if ((c = stegfs_cache_exists(path, NULL)) && c->file)
		{
			char *buf = calloc(offset, sizeof(uint8_t));
			fuse_stegfs_read(path, buf, offset, 0, info);
			fuse_stegfs_unlink(path);
			fuse_stegfs_write(path, buf, offset, 0, info);
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

#ifdef STEGFS_FALLOCATE
static int fuse_stegfs_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	/* this isn't finished or working properly */

	stegfs_cache_t *c = NULL;
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
			char *buf = calloc(sz, sizeof(uint8_t));
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
	(void)info;

	stegfs_file_create(path, true);

	return -errno;
}

static int fuse_stegfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	errno = EXIT_SUCCESS;

	(void)mode;
	(void)rdev;

	stegfs_file_create(path, false);

	return -errno;
}

static int fuse_stegfs_release(const char *path, struct fuse_file_info *info)
{
	errno = EXIT_SUCCESS;

	(void)info;

	stegfs_cache_t *c = NULL;
	if ((c = stegfs_cache_exists(path, NULL)) && c->file)
	{
		if (c->file->write && stegfs_file_will_fit(c->file))
		{
			if (stegfs_file_write(c->file))
				errno = EXIT_SUCCESS;
			c->file->write = false;
		}
		free(c->file->data);
		c->file->data = NULL;
		free(c->file->pass);
		c->file->pass = NULL;
	}

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

	stegfs_t file_system = stegfs_info();
	if (file_system.show_bloc && path_starts_with(PATH_BLOC, path))
	{
		char *b = dir_get_name(path, PASSWORD_SEPARATOR);
		uint64_t ino = strtol(b, NULL, 0);
		char *f = file_system.blocks.file[ino];
		if (f)
			snprintf(buf, size, "%s", f);
		free(b);
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
	char **fargs = calloc(argc, sizeof( char * ));
	fargs[0] = argv[0];

	LIST args = list_init(config_arg_comp, false, false);
	list_add(args, &((config_named_t){ 'c', "cipher",         _("algorithm"),  _("Algorithm to use to encrypt data; use ‘list’ to show available cipher algorithms"), CONFIG_ARG_REQ_STRING,  { .string  = NULL  }, false, false, false }));
	list_add(args, &((config_named_t){ 's', "hash",           _("algorithm"),  _("Hash algorithm to generate key; use ‘list’ to show available hash algorithms"),     CONFIG_ARG_REQ_STRING,  { .string  = NULL  }, false, false, false }));
	list_add(args, &((config_named_t){ 'm', "mode",           _("mode"),       _("The encryption mode to use; use ‘list’ to show available cipher modes"),            CONFIG_ARG_REQ_STRING,  { .string  = NULL  }, false, false, false }));
	list_add(args, &((config_named_t){ 'a', "mac",            _("mac"),        _("The MAC algorithm to use; use ‘list’ to show available MACs"),                      CONFIG_ARG_REQ_STRING,  { .string  = NULL  }, false, false, false }));
	list_add(args, &((config_named_t){ 'i', "kdf-iterations", _("iterations"), _("Number of iterations the KDF should use"),                                          CONFIG_ARG_REQ_NUMBER,  { .number  = 0     }, false, false, false }));
	list_add(args, &((config_named_t){ 'p', "paranoid",       NULL,            _("Enable paranoia mode"),                                                             CONFIG_ARG_BOOLEAN,     { .boolean = false }, false, true,  false }));
	list_add(args, &((config_named_t){ 'x', "duplicates",     "#",             _("Number of times each file should be duplicated"),                                   CONFIG_ARG_REQ_NUMBER,  { .number  = 0     }, false, true,  false }));
	list_add(args, &((config_named_t){ 'b', "show-bloc",      NULL,            _("Expose the /bloc/ in-use block list directory"),                                    CONFIG_ARG_BOOLEAN,     { .boolean = false }, false, true,  false }));
	list_add(args, &((config_named_t){ 'd', NULL,             NULL,            _("Enable debug output (forces foreground and single-thread)"),                        CONFIG_ARG_BOOLEAN,     { .boolean = false }, false, false, false }));
	list_add(args, &((config_named_t){ 'f', NULL,             NULL,            _("Foreground operation"),                                                             CONFIG_ARG_BOOLEAN,     { .boolean = false }, false, false, false }));
	list_add(args, &((config_named_t){ 't', NULL,             NULL,            _("Disable multi-threaded operation (FUSE option -s)"),                                CONFIG_ARG_BOOLEAN,     { .boolean = false }, false, false, false }));
	list_add(args, &((config_named_t){ 'o', NULL,             "opt,[opt...]",  _("FUSE mount options--see FUSE documentation for details"),                           CONFIG_ARG_LIST_STRING, { .list    = NULL  }, false, false, false }));

	LIST extra = list_default();
	list_add(extra, &((config_unnamed_t){ "file system", CONFIG_ARG_STRING,  { .string = NULL }, true,  false }));
	list_add(extra, &((config_unnamed_t){ "mount point", CONFIG_ARG_STRING,  { .string = NULL }, true,  false }));

	LIST notes = list_default();
	list_add(notes, _("It doesn't matter which order the file system and mount point are specified as stegfs will figure that out. All other options are passed to FUSE."));
	list_add(notes, _("If you’re feeling extra paranoid you can now disable to stegfs file system header. This will also disable the checks when mounting and thus anything could happen ;-)"));

	config_about_t about =
	{
		STEGFS_NAME,
		STEGFS_VERSION,
		PROJECT_URL,
		NULL
	};
	config_init(about);
	config_parse(argc, argv, args, extra, notes, false);

	list_deinit(&notes);

	char *fs  = NULL;
	char *mnt = NULL;
	struct stat s = { 0x0 };
	stat(((config_unnamed_t *)list_get(extra, 0))->response_value.string, &s);
	if (S_ISDIR(s.st_mode))
	{
		// 1st argument is directory, so it's the mount point
		mnt = ((config_unnamed_t *)list_get(extra, 0))->response_value.string;
		fs  = ((config_unnamed_t *)list_get(extra, 1))->response_value.string;
	}
	else
	{
		fs  = ((config_unnamed_t *)list_get(extra, 0))->response_value.string;
		mnt = ((config_unnamed_t *)list_get(extra, 1))->response_value.string;
	}
	list_deinit(&extra);

	// config is actually read from the fs unless paranoid == true
	enum gcry_cipher_algos cipher = DEFAULT_CIPHER;
	enum gcry_cipher_modes mode   = DEFAULT_MODE;
	enum gcry_md_algos     hash   = DEFAULT_HASH;
	enum gcry_mac_algos    mac    = DEFAULT_MAC;
	uint64_t kdf_iters            = DEFAULT_KDF_ITERATIONS;
	bool paranoid                 = ((config_named_t *)list_get(args, 5))->response_value.boolean;
	uint8_t duplicates            = COPIES_DEFAULT;
	bool show_bloc                = ((config_named_t *)list_get(args, 7))->response_value.boolean;

	if (paranoid)
	{
		cipher     = cipher_id_from_name(((config_named_t *)list_get(args, 0))->response_value.string);
		mode       =   mode_id_from_name(((config_named_t *)list_get(args, 1))->response_value.string);
		hash       =   hash_id_from_name(((config_named_t *)list_get(args, 2))->response_value.string);
		mac        =    mac_id_from_name(((config_named_t *)list_get(args, 3))->response_value.string);
		kdf_iters  =                     ((config_named_t *)list_get(args, 4))->response_value.number;
		duplicates =            (uint8_t)((config_named_t *)list_get(args, 6))->response_value.number;
	}

	/*
	 * deal with FUSE options
	 */

	bool debug         =          ((config_named_t *)list_get(args,  8))->response_value.boolean;
	bool foreground    = debug || ((config_named_t *)list_get(args,  9))->response_value.boolean;
	bool single_thread = debug || ((config_named_t *)list_get(args, 10))->response_value.boolean;

	int fuse_argc = 3;
	char **fuse_argv = calloc(fuse_argc, sizeof (char *));
	fuse_argv[0] = argv[0];
	fuse_argv[1] = mnt;
	if (debug)
	{
		fuse_argc++;
		fuse_argv = realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-d";
	}
	if (foreground)
	{
		fuse_argc++;
		fuse_argv = realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-f";
	}
	if (single_thread)
	{
		fuse_argc++;
		fuse_argv = realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 2] = "-s";
	}
	LIST fuse_options = ((config_named_t *)list_get(args, 11))->response_value.list;
	ITER iter = list_iterator(fuse_options);
	while (list_has_next(iter))
	{
		fuse_argc += 2;
		fuse_argv = realloc(fuse_argv, fuse_argc * sizeof (char *));
		fuse_argv[fuse_argc - 3] = "-o";
		fuse_argv[fuse_argc - 2] = (char *)list_get_next(iter);
	}
	fuse_argc--;
	fuse_argv[fuse_argc] = NULL;

	list_deinit(&args);

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
