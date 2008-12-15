/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2008, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
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

#define FUSE_USE_VERSION 27

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "vstegfs.h"
#include "dir.h"

#define APP_NAME "vstegfs"

#define PAGESIZE 4096 // the number of bytes given us by the kernel

typedef struct cache
{
    char *data;
    size_t size;
} cache;

cache cache_write;
cache cache_read;

uint64_t filesystem;
uint64_t filesystem_size;

static int vstegfs_getattr(const char *path, struct stat *stbuf)
{
    char *name = dir_get_file(path);
    memset(stbuf, 0, sizeof (struct stat));
    uint64_t inod = 0, size = 0;
    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
        stbuf->st_uid = fuse_get_context()->uid;
        stbuf->st_gid = fuse_get_context()->gid;
    }
    else if (name[0] == '+')
    {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    }
    else
    {
        char *p;
        asprintf(&p, "vstegfs%s", path);
        size = file_check(dir_get_file(p), dir_get_path(p), dir_get_pass(p));
        free(p);
        inod = (file_find(dir_get_file(p), dir_get_path(p), dir_get_pass(p)))[0] % filesystem_size;
        stbuf->st_mode = S_IFREG | 0600;
        stbuf->st_nlink = 1;
    }
    stbuf->st_ino = inod;
    stbuf->st_uid = fuse_get_context()->uid;
    stbuf->st_gid = fuse_get_context()->gid;
    stbuf->st_size = size;
    stbuf->st_blksize = BLOCK;
    stbuf->st_blocks = (int)((size / BLOCK) + 1);

    return EXIT_SUCCESS;
}

static int vstegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    return EXIT_SUCCESS;
}

static int vstegfs_unlink(const char *path)
{
    char *p;
    asprintf(&p, "vstegfs%s", path);
    file_unlink(dir_get_file(p), dir_get_path(p), dir_get_pass(p));
    free(p);
    return EXIT_SUCCESS;
}

static int vstegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if (!offset)
    {
        char *p;
        asprintf(&p, "vstegfs%s", path);
        
        cache_read.size = file_check(dir_get_file(p), dir_get_path(p), dir_get_pass(p));
        cache_read.data = realloc(cache_read.data, cache_read.size);

        FILE *stream = open_memstream(&cache_read.data, &cache_read.size);
        errno = file_read(stream, dir_get_file(p), dir_get_path(p), dir_get_pass(p));
        fclose(stream);
        free(p);
    }

    if (offset < cache_read.size)
    {
        if (offset + size > cache_read.size)
            size = cache_read.size - offset;
        memcpy(buf, cache_read.data + offset, size);
    }
    else
        size = 0;
    return size;
}

static int vstegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    errno = EXIT_SUCCESS;

    cache_write.size += size;
    cache_write.data  = realloc(cache_write.data, cache_write.size + 1);
    memcpy(cache_write.data + offset, buf, size);

    if (size == PAGESIZE)
        return size;

    char *p;
    asprintf(&p, "vstegfs%s", path);

    FILE *stream = fmemopen(cache_write.data, cache_write.size, "r");
    errno = file_write(stream, cache_write.size, dir_get_file(p), dir_get_path(p), dir_get_pass(p));
    fclose(stream);

    free(p);
    cache_write.size = 0;
    free(cache_write.data);
    cache_write.data = NULL;

    if (errno)
        return -errno;
    return size;
}

static int vstegfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    return EXIT_SUCCESS;
}

static int vstegfs_truncate(const char *path, off_t offset)
{
    return EXIT_SUCCESS;
}

static int vstegfs_utime(const char *path, struct utimbuf *utime)
{
    return EXIT_SUCCESS;
}

static int vstegfs_chmod(const char *path, mode_t mode)
{
    return EXIT_SUCCESS;
}

static int vstegfs_chown(const char *path, uid_t uid, gid_t gid)
{
    return EXIT_SUCCESS;
}

static int vstegfs_null_0(const char *path, struct fuse_file_info *fi)
{
    return EXIT_SUCCESS;
}


static struct fuse_operations vstegfs_oper = {
    .getattr	= vstegfs_getattr,
    .readdir	= vstegfs_readdir,
    .unlink     = vstegfs_unlink,
    .read       = vstegfs_read,
    .write      = vstegfs_write,
    /*
     * null functions (return success everytime)
     */
    .mknod      = vstegfs_mknod,
    .truncate   = vstegfs_truncate,
    .utime      = vstegfs_utime,
    .chmod      = vstegfs_chmod,
    .chown      = vstegfs_chown,
    .open       = vstegfs_null_0,
    .flush      = vstegfs_null_0,
    .release    = vstegfs_null_0,
};

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        show_usage();
        return EXIT_FAILURE;
    }

    char *fs = NULL, *mount = NULL;
    uint32_t debug = FALSE;
    while (TRUE)
    {
        static struct option long_options[] = {
            {"debug"     , no_argument      , 0, 'd'},
            {"filesystem", required_argument, 0, 'f'},
            {"mount"     , required_argument, 0, 'm'},
            {"help"      , no_argument      , 0, 'h'},
            {"licence"   , no_argument      , 0, 'l'},
            {"version"   , no_argument      , 0, 'v'},
            {0, 0, 0, 0}
        };
        int32_t optex = 0;
        int32_t opt = getopt_long(argc, argv, "df:m:hlv", long_options, &optex);
        if (opt == -1)
            break;
        switch (opt)
        {
            case 'd':
                debug = TRUE;
                break;
            case 'f':
                fs = strdup(optarg);
                break;
            case 'm':
                mount = strdup(optarg);
                break;
            case 'h':
                show_help();
                return EXIT_SUCCESS;
            case 'l':
                show_licence();
                return EXIT_SUCCESS;
            case 'v':
                show_version();
                return EXIT_SUCCESS;
            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }

    if (!mount || !fs)
    {
        show_usage();
        return EXIT_FAILURE;
    }
    if ((filesystem = open64(fs, O_RDWR, S_IRUSR | S_IWUSR)) < 3)
    {
        perror("Could not open file system");
        return errno;
    }
    filesystem_size = lseek64(filesystem, 0LL, SEEK_END) / BLOCK;

    char **args = calloc(debug ? 5 : 4, sizeof (char *));
    args[0] = strdup(argv[0]);
    args[1] = strdup("-o");
    args[2] = strdup("use_ino");
    if (debug)
        args[3] = strdup("-d");
    args[debug ? 4 : 3] = strdup(mount);

    return fuse_main(debug ? 5 : 4, args, &vstegfs_oper, NULL);
}

void show_help(void)
{
    show_version();
    fprintf(stderr, "\n");
    show_usage();
    fprintf(stderr, "\n  -f, --filesystem  FILE SYSTEM  Where to write the file system to");
    fprintf(stderr, "\n  -m, --mount       MOUNT POINT  Where to mount the file system");
    fprintf(stderr, "\n  -h, --help                     Show this help list");
    fprintf(stderr, "\n  -l, --licence                  Show overview of GNU GPL");
    fprintf(stderr, "\n  -v, --version                  Show version information");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n  Fuse executable to mount vstegfs file systems");
    fprintf(stderr, "\n");
}

void show_licence(void)
{
    fprintf(stderr, "This program is free software: you can redistribute it and/or modify\n");
    fprintf(stderr, "it under the terms of the GNU General Public License as published by\n");
    fprintf(stderr, "the Free Software Foundation, either version 3 of the License, or\n");
    fprintf(stderr, "(at your option) any later version.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "This program is distributed in the hope that it will be useful,\n");
    fprintf(stderr, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    fprintf(stderr, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    fprintf(stderr, "GNU General Public License for more details.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "You should have received a copy of the GNU General Public License\n");
    fprintf(stderr, "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
}

void show_usage(void)
{
    fprintf(stderr, "Usage\n  %s [OPTION] [ARGUMENT]\n", APP_NAME);
}

void show_version(void)
{
    fprintf(stderr, "%s\n", APP_NAME);
#ifdef RELEASE
    fprintf(stderr, "  Official Release %i\n", RELEASE);
#endif
    fprintf(stderr, "  SVN Revision %s\n", REVISION);
}
