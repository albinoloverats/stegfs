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

#include <sys/stat.h>

#include "help.h"
#include "dir.h"
#include "stegfs.h"

#define DIRECTORY_ROOT "/"
#define DIRECTORY_PROC "+proc"
#define DIRECTORY_BMAP "+bitmap"

#define PATH_PROC DIRECTORY_ROOT DIRECTORY_PROC
#define PATH_BMAP PATH_PROC DIRECTORY_ROOT DIRECTORY_BMAP

//#define SYMBOL_SEP '/'
#define SYMBOL_DIR '+'
//#define SYMBOL_LNK ','


/*
 * standard file system functions (used by fuse)
 */
static int fuse_stegfs_getattr(const char *, struct stat *);
static int fuse_stegfs_readlink(const char *, char *, size_t);
static int fuse_stegfs_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
static int fuse_stegfs_unlink(const char *);
static int fuse_stegfs_read(const char *, char *, size_t, off_t , struct fuse_file_info *);
static int fuse_stegfs_write(const char *, const char *, size_t, off_t , struct fuse_file_info *);
static int fuse_stegfs_open(const char *, struct fuse_file_info *);
static int fuse_stegfs_release(const char *, struct fuse_file_info *);

/*
 * empty functions; required by fuse, but not used by stegfs
 */
static int fuse_stegfs_mknod(const char *, mode_t, dev_t);
static int fuse_stegfs_truncate(const char *, off_t);
static int fuse_stegfs_utime(const char *, struct utimbuf *);
static int fuse_stegfs_chmod(const char *, mode_t);
static int fuse_stegfs_chown(const char *, uid_t, gid_t);
static int fuse_stegfs_flush(const char *, struct fuse_file_info *);

static struct fuse_operations fuse_stegfs_functions =
{
    .getattr  = fuse_stegfs_getattr,
    .readdir  = fuse_stegfs_readdir,
    .unlink   = fuse_stegfs_unlink,
    .read     = fuse_stegfs_read,
    .write    = fuse_stegfs_write,
    .open     = fuse_stegfs_open,
    .release  = fuse_stegfs_release,
    .truncate = fuse_stegfs_truncate,
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

static int fuse_stegfs_getattr(const char *path, struct stat *stbuf)
{
    errno = EXIT_SUCCESS;

    stegfs_t file_system = stegfs_info();
    char *f = dir_get_file(path); /* used to check for symlink */
    /* common attributes for root/files/directories */
    stbuf->st_dev     = (dev_t)MAGIC_0;
    stbuf->st_ino     = 0;
    stbuf->st_mode    = S_IFDIR | 0700;
    stbuf->st_nlink   = 2;
    stbuf->st_uid     = fuse_get_context()->uid;
    stbuf->st_gid     = fuse_get_context()->gid;
    stbuf->st_atime   = time(NULL);
    stbuf->st_ctime   = time(NULL);
    stbuf->st_mtime   = time(NULL);
    stbuf->st_size    = 0x0;
    stbuf->st_blksize = 512; /* it's not the ideal size but 80 isn't allowed */

    if (path_equals(DIRECTORY_ROOT, path))
        /* path is / so get basic info about the file system itself */
        stbuf->st_size = file_system.size;
    else if (path_equals(PATH_PROC, path) || path_equals(PATH_BMAP, path))
        /* if path == /+proc/ */
        stbuf->st_mode = S_IFDIR | 0500;
    else if (path_starts_with(PATH_PROC, path) && !path_starts_with(PATH_BMAP, path))
    {
        /*
         * if we're looking at files in /+proc/ treat them as blocks (as
         * that's hat we're representing)
         */
        stbuf->st_mode  = S_IFBLK;
        stbuf->st_nlink = 1;
        stbuf->st_size  = 0;
    }
    else if (f[0] != SYMBOL_DIR)
    {
        stegfs_file_t file;
        memset(&file, 0x00, sizeof file);
        file.path = dir_get_path(path);
        file.name = dir_get_file(path);
        file.pass = dir_get_pass(path);
        /* check if the file has been cached */
        bool cached = false;
        for (uint64_t i = 0; i < file_system.cache.size; i++)
            if (stegfs_file_cached(file))
            {
                for (int j = 0; j < MAX_COPIES; j++)
                    file.inodes[j] = file_system.cache.file[i].inodes[j];
                file.time = file_system.cache.file[i].time;
                file.size = file_system.cache.file[i].size;
                cached = true;
                break;
            }
        /* if file wasn't cached; find it */
        if (cached || stegfs_file_stat(&file))
        {
            for (int i = 0; i < MAX_COPIES; i++)
                if (file.inodes[i])
                {
                    stbuf->st_ino = (ino_t)file.inodes[i];
                    break;
                }
            stbuf->st_mode  = S_IFREG | 0600;
            stbuf->st_nlink = 1;
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
    stbuf->st_blocks = (int)(stbuf->st_size / 512);
    free(f);

    return -errno;
}

static int fuse_stegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    filler(buf,  ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    stegfs_t file_system = stegfs_info();

    if (path_equals(DIRECTORY_ROOT, path))
        /* if in / add /+proc */
        filler(buf, DIRECTORY_PROC, NULL, 0);
    else if (path_equals(PATH_PROC, path))
        for (uint64_t i = 0; i < file_system.size / SIZE_BYTE_BLOCK; i++)
            if (file_system.used[i])
            {
                char b[21] = { 0x0 }; // max digits for UINT64_MAX
                snprintf(b, sizeof b, "%ju", i);
                filler(buf, b, NULL, 0);
            }

    /* add cached files to directory (if they're in this directory) */
    for (uint64_t i = 0; i < file_system.cache.size; i++)
        if (file_system.cache.file[i].path && path_equals(path, file_system.cache.file[i].path))
            filler(buf, file_system.cache.file[i].name, NULL, 0);

    return -errno;
}

static int fuse_stegfs_unlink(const char *path)
{
    errno = EXIT_SUCCESS;

    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
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

    int r;

    stegfs_t file_system = stegfs_info();
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);

    for (uint64_t i = 0; i < file_system.cache.size; i++)
        if (stegfs_file_cached(file))
        {
            if ((unsigned)(offset + size) > file_system.cache.file[i].size)
                size = file_system.cache.file[i].size - offset;
            memcpy(buf, file_system.cache.file[i].data + offset, size);
            r = size;
            goto done;
        }

    errno = ENOENT;
    r = -errno;

done:
    free(file.path);
    free(file.name);
    free(file.pass);

    return r;
}

static int fuse_stegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    int r;

    stegfs_t file_system = stegfs_info();
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);

    /*
     * if the file is cached (has been read/written to already) then
     * just buffer this data until it's released/flushed
     */
buffer:
    for (uint64_t i = 0; i < file_system.cache.size; i++)
        if (stegfs_file_cached(file))
        {
            if (!stegfs_file_will_fit(file_system.cache.file))
            {
                r = -errno;
                goto done;
            }
            if (!file_system.cache.file[i].write)
            {
                r = -EBADF;
                goto done;
            }
            file_system.cache.file[i].size = file_system.cache.file[i].size > size + offset ? file_system.cache.file[i].size : size + offset;
            file_system.cache.file[i].data = realloc(file_system.cache.file[i].data, file_system.cache.file[i].size);
            file_system.cache.file[i].time = time(NULL);
            memcpy(file_system.cache.file[i].data + offset, buf, size);
            r = size;
            goto done;
        }
    /* the file wasn't found to exists, so create ti and try again */
    stegfs_file_create(path, true);
    goto buffer;
    /*
     * we “should” probably use some kind of loop - but fuck-it, I'm
     * feeling rebelious ;-)
     */
done:
    free(file.path);
    free(file.name);
    free(file.pass);

    return r;
}

static int fuse_stegfs_open(const char *path, struct fuse_file_info *info)
{
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);

    if (!stegfs_file_read(&file))
        stegfs_file_create(path, info->flags & O_WRONLY);

    free(file.path);
    free(file.name);
    free(file.pass);

    return -EXIT_SUCCESS;
}

static int fuse_stegfs_flush(const char *path, struct fuse_file_info *info)
{
    errno = EXIT_SUCCESS;

    stegfs_t file_system = stegfs_info();
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);

    for (uint64_t i = 0; i < file_system.cache.size; i++)
        if (stegfs_file_cached(file) && file_system.cache.file[i].write)
            if (stegfs_file_will_fit(&file_system.cache.file[i]))
                errno = EXIT_SUCCESS;

    free(file.path);
    free(file.name);
    free(file.pass);

    return -errno;
}

static int fuse_stegfs_truncate(const char *path, off_t offset)
{
    struct stat s;
    fuse_stegfs_getattr(path, &s);
    char *buf = malloc(s.st_size);
    struct fuse_file_info info;
    fuse_stegfs_read(path, buf, s.st_size, 0, &info);
    fuse_stegfs_unlink(path);
    fuse_stegfs_write(path, buf, offset, 0, &info);
    return -EXIT_SUCCESS;
}

static int fuse_stegfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    stegfs_file_create(path, false);
    return -EXIT_SUCCESS;
}

static int fuse_stegfs_release(const char *path, struct fuse_file_info *info)
{
    stegfs_t file_system = stegfs_info();
    stegfs_file_t file;
    memset(&file, 0x00, sizeof file);
    file.path = dir_get_path(path);
    file.name = dir_get_file(path);
    file.pass = dir_get_pass(path);

    for (uint64_t i = 0; i < file_system.cache.size; i++)
        if (stegfs_file_cached(file) && file_system.cache.file[i].write && stegfs_file_will_fit(&file_system.cache.file[i]))
        {
            if (stegfs_file_write(&file_system.cache.file[i]))
                errno = EXIT_SUCCESS;
            file_system.cache.file[i].write = false;
            break;
        }

    free(file.path);
    free(file.name);
    free(file.pass);

    return -EXIT_SUCCESS; /* fuse says the return value is ignored */
}

/*
 * empty functions; required by fuse, but not used by stegfs
 */

static int fuse_stegfs_readlink(const char *path, char *buf, size_t size)
{
    return -ENOTSUP;
}

static int fuse_stegfs_utime(const char *path, struct utimbuf *utime)
{
    return -ENOTSUP;
}

static int fuse_stegfs_chmod(const char *path, mode_t mode)
{
    return -ENOTSUP;
}

static int fuse_stegfs_chown(const char *path, uid_t uid, gid_t gid)
{
    return -ENOTSUP;
}

int main(int argc, char **argv)
{
    char *fs = NULL;
    char *mp = NULL;

    bool h = false;

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
                 * is needed to overcome this) so until that time, we'll
                 * force that condition here
                 */
                asprintf(&argv[i], "-s");
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

    if (!h && !stegfs_init(fs))
    {
        fprintf(stderr, "Could not initialise file system!\n");
        return EXIT_FAILURE;
    }

    return fuse_main(argc, argv, &fuse_stegfs_functions, NULL);
}
