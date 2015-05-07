/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2015, albinoloverats ~ Software Development
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

#include <mhash.h>

#include "common/dir.h"

#include "help.h"
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
static int fuse_stegfs_create(const char *, mode_t, struct fuse_file_info *);
static int fuse_stegfs_mknod(const char *, mode_t, dev_t);
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
    .statfs   = fuse_stegfs_statfs,
    .getattr  = fuse_stegfs_getattr,
    .mkdir    = fuse_stegfs_mkdir,
    .rmdir    = fuse_stegfs_rmdir,
    .readdir  = fuse_stegfs_readdir,
    .unlink   = fuse_stegfs_unlink,
    .read     = fuse_stegfs_read,
    .write    = fuse_stegfs_write,
    .open     = fuse_stegfs_open,
    .release  = fuse_stegfs_release,
    .truncate = fuse_stegfs_truncate,
    .create   = fuse_stegfs_create,
    .mknod    = fuse_stegfs_mknod,
    /*
     * empty functions; required by fuse, but not used by stegfs
     */
    .readlink = fuse_stegfs_readlink,
    .utime    = fuse_stegfs_utime,
    .chmod    = fuse_stegfs_chmod,
    .chown    = fuse_stegfs_chown,
    .flush    = fuse_stegfs_flush
};

static int fuse_stegfs_statfs(const char *path, struct statvfs *stvbuf)
{
    errno = EXIT_SUCCESS;

    stegfs_t file_system = stegfs_info();

    stvbuf->f_bsize   = SIZE_BYTE_BLOCK;
    stvbuf->f_frsize  = SIZE_BYTE_DATA;
    stvbuf->f_blocks  = (file_system.size / SIZE_BYTE_BLOCK) - 1;
    stvbuf->f_bfree   = stvbuf->f_blocks;
    for (uint64_t i = 0; i < stvbuf->f_blocks; i++)
        if (file_system.used[i])
            stvbuf->f_bfree--;
    stvbuf->f_bavail  = stvbuf->f_bfree;
    stvbuf->f_files   = stvbuf->f_blocks;
    stvbuf->f_ffree   = stvbuf->f_bfree;
    stvbuf->f_favail  = stvbuf->f_bfree;
    stvbuf->f_fsid    = MAGIC_0;
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
    stbuf->st_dev     = (dev_t)MAGIC_0;
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
        stbuf->st_mode  = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
        for (uint64_t i = 0; i < file_system.cache2.ents; i++)
            if (file_system.cache2.child[i] && !file_system.cache2.child[i]->file)
                stbuf->st_nlink++;
    }
#ifdef USE_PROC
    else if (path_equals(PATH_PROC, path))
        /* if path == /proc */
        stbuf->st_mode = S_IFDIR | 0500;
    else if (path_starts_with(PATH_PROC, path))
    {
        /*
         * if we’re looking at files in /proc treat them as blocks (as
         * that’s hat we’re representing)
         */
        stbuf->st_mode  = S_IFBLK;
        stbuf->st_nlink = 1;
    }
#endif
    else
    {
        stegfs_cache2_t c;
        memset(&c, 0x00, sizeof c);
        if (stegfs_cache2_exists(path, &c))
        {
            if (c.file)
            {
                for (int i = 0; i < MAX_COPIES; i++)
                    if (c.file->inodes[i])
                    {
                        stbuf->st_ino = (ino_t)(c.file->inodes[i] % (file_system.size / SIZE_BYTE_BLOCK));
                        break;
                    }
                stbuf->st_mode  = S_IFREG | 0600;
                stbuf->st_ctime = c.file->time;
                stbuf->st_mtime = c.file->time;
                stbuf->st_size  = c.file->size;
            }
            else
            {
                stbuf->st_mode  = S_IFDIR | 0700;
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
                for (int i = 0; i < MAX_COPIES; i++)
                    if (file.inodes[i])
                    {
                        stbuf->st_ino = (ino_t)(file.inodes[i] % (file_system.size / SIZE_BYTE_BLOCK));
                        break;
                    }
                stbuf->st_mode  = S_IFREG | 0600;
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
        stbuf->st_blocks = d.quot + (d.rem ? 1 : 0);
    }
    else if (stbuf->st_mode & S_IFDIR)
    {
        MHASH hash = mhash_init(MHASH_TIGER);
        mhash(hash, path, strlen(path));
        void *h = mhash_end(hash);
        memcpy(&(stbuf->st_ino), h, sizeof stbuf->st_ino);
        free(h);
        stbuf->st_ino %= (file_system.size / SIZE_BYTE_BLOCK);
        stbuf->st_size = SIZE_BYTE_DATA;
    }
    free(f);

    return -errno;
}

static int fuse_stegfs_mkdir(const char *path, mode_t mode)
{
    errno = EXIT_SUCCESS;

    stegfs_cache2_add(path, NULL);

    return -errno;
}

static int fuse_stegfs_rmdir(const char *path)
{
    errno = EXIT_SUCCESS;

#ifdef USE_PROC
    if (path_equals(path, PATH_PROC))
    {
        errno = EBUSY;
        return -errno;
    }
#endif

    stegfs_cache2_t *c = NULL;
    if ((c = stegfs_cache2_exists(path, NULL)))
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
                stegfs_cache2_remove(path);
        }
    }

    return -errno;
}

static int fuse_stegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    filler(buf,  ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    stegfs_t file_system = stegfs_info();

    if (path_equals(DIR_SEPARATOR, path))
    {
        for (uint64_t i = 0; i < file_system.cache2.ents; i++)
            if (file_system.cache2.child[i]->name)
                filler(buf, file_system.cache2.child[i]->name, NULL, 0);
    }
#ifdef USE_PROC
    else if (path_equals(PATH_PROC, path))
    {
        for (uint64_t i = 0; i < file_system.size / SIZE_BYTE_BLOCK; i++)
            if (file_system.used[i])
            {
                char b[21] = { 0x0 }; // max digits for UINT64_MAX
                snprintf(b, sizeof b, "%ju", i);
                filler(buf, b, NULL, 0);
            }
    }
#endif
    else
    {
        stegfs_cache2_t c;
        memset(&c, 0x00, sizeof c);
        if (stegfs_cache2_exists(path, &c))
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

    stegfs_cache2_t *c = NULL;
    if ((c = stegfs_cache2_exists(path, NULL)) && c->file)
    {
        if ((unsigned)(offset + size) > c->file->size)
            size = c->file->size - offset;
        memcpy(buf, c->file->data + offset, size);
        return size;
    }

    errno = ENOENT;

    return -errno;
}

static int fuse_stegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    /*
     * if the file is cached (has been read/written to already) then
     * just buffer this data until it’s released/flushed
     */
    while (true)
    {
        stegfs_cache2_t *c = NULL;
        if ((c = stegfs_cache2_exists(path, NULL)) && c->file)
        {
            if (!stegfs_file_will_fit(c->file))
                return -errno;
            if (!c->file->write)
            {
                errno = EBADF;
                return -errno;
            }
            c->file->size = c->file->size > size + offset ? c->file->size : size + offset;
            c->file->data = realloc(c->file->data, c->file->size);
            c->file->time = time(NULL);
            memcpy(c->file->data + offset, buf, size);
            return size;
        }
        /*
         * the file wasn’t found, so create it and try again, it should
         * then fail if the file cannot fit, as at this point all that’s
         * happening is increasing the cached buffer
         */
        stegfs_file_create(path, true);
    }

    return -EIO;
}

static int fuse_stegfs_open(const char *path, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    stegfs_cache2_t *c = NULL;
    if ((c = stegfs_cache2_exists(path, NULL)) && c->file)
    {
        c->file->pass = dir_get_pass(path);
        if (!stegfs_file_read(c->file))
            errno = EACCES;
    }

    return -errno;
}

static int fuse_stegfs_flush(const char *path, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    stegfs_cache2_t *c = NULL;
    if ((c = stegfs_cache2_exists(path, NULL)) && c->file)
        if (stegfs_file_will_fit(c->file))
            errno = EXIT_SUCCESS;

    return -errno;
}

static int fuse_stegfs_truncate(const char *path, off_t offset)
{
    errno = EXIT_SUCCESS;

    struct stat s;
    fuse_stegfs_getattr(path, &s);
    char *buf = malloc(offset);
    memset(buf, 0x00, offset);

    fuse_stegfs_read(path, buf, offset, 0, NULL);
    fuse_stegfs_unlink(path);
    fuse_stegfs_write(path, buf, offset, 0, NULL);

    free(buf);

    return -errno;
}

static int fuse_stegfs_create(const char *path, mode_t mode, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    stegfs_file_create(path, true);

    return -errno;
}

static int fuse_stegfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    errno = EXIT_SUCCESS;

    stegfs_file_create(path, false);

    return -errno;
}

static int fuse_stegfs_release(const char *path, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    stegfs_cache2_t *c = NULL;
    if ((c = stegfs_cache2_exists(path, NULL)) && c->file)
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

/*
 * empty functions; required by fuse, but not used by stegfs
 */

static int fuse_stegfs_readlink(const char *path, char *buf, size_t size)
{
    errno = ENOTSUP;
    return -errno;
}

static int fuse_stegfs_utime(const char *path, struct utimbuf *utime)
{
    errno = ENOTSUP;
    return -errno;
}

static int fuse_stegfs_chmod(const char *path, mode_t mode)
{
    errno = ENOTSUP;
    return -errno;
}

static int fuse_stegfs_chown(const char *path, uid_t uid, gid_t gid)
{
    errno = ENOTSUP;
    return -errno;
}

int main(int argc, char **argv)
{
    char *fs = NULL;
    char *mp = NULL;

    bool h = false;
#ifdef __DEBUG__
    bool d = false;
#endif

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--licence") || !strcmp(argv[i], "-l"))
            show_licence();
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v"))
            show_version();
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
        {
            show_help();
            h = true;
            break; // break to allow FUSE to show its options
        }
#ifdef __DEBUG__
        if (!strcmp(argv[i], "-d"))
            d = true;
#endif
        struct stat s;
        memset(&s, 0x00, sizeof s);
        stat(argv[i], &s);
        switch (s.st_mode & S_IFMT)
        {
            case S_IFBLK:
            case S_IFLNK:
            case S_IFREG:
                fs = strdup(argv[i]);
                /*
                 * stegfs is currently single threaded (much more work
                 * is needed to overcome this) so until that time, we’ll
                 * force that condition here
                 */
                argv[i] = "-s";
                break;
            case S_IFDIR:
                mp = strdup(argv[i]);
                break;
            default:
                /* ignore unknown options (they might be fuse options) */
                break;
        }
    }

    if (!h && !(mp && fs))
    {
        fprintf(stderr, "Missing file system and/or mount point!\n");
        show_usage();
    }
    free(mp);

    errno = EXIT_SUCCESS;
    if (!h && !stegfs_init(fs))
    {
        if (errno == (int)MAGIC_201001_0)
            fprintf(stderr, "Previous version of stegfs!\n");
        else
            perror("Could not initialise file system!");
        return errno;
    }
    free(fs);

#ifdef __DEBUG__
    if (!d)
    {
        argv[argc] = strdup("-d");
        argc++;
    }
#endif
    return fuse_main(argc, argv, &fuse_stegfs_functions, NULL);
}
