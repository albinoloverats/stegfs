/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
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
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include "common/common.h"

#include "src/vstegfs.h"
#include "src/dir.h"

#define APP "vstegfs"
#define VER "200903-"

#define PAGESIZE 4096 /* the number of bytes given us by the kernel */

typedef struct cache
{
    uint8_t *data;
    size_t   size;
}
cache;

static cache cache_write;
static cache cache_read;

static uint64_t filesystem;

static int vstegfs_getattr(const char *path, struct stat *stbuf)
{
    char *this = dir_get_file(path);
    uint64_t inod = 0x0, size = 0x0;
    if (!strcmp(path, "/"))
    {
        /*
         * TODO the root dir could return size == total capacity
         * (maybe?)
         */
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
        stbuf->st_uid = fuse_get_context()->uid;
        stbuf->st_gid = fuse_get_context()->gid;
    }
    else if (this[0] == '+')
    {
        stbuf->st_mode = S_IFDIR | 0700;
        stbuf->st_nlink = 2;
    }
    else
    {
        char *p = NULL;
        asprintf(&p, "%s%s", ROOT_PATH, path);
        msg("look up = %s", p);
        vstat_t vs;
        {
            vs.fs   = filesystem;
            vs.file = NULL;
            vs.size = NULL;
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);
        }
        size = vstegfs_find(vs);
        free(p);
        free(vs.name);
        free(vs.path);
        free(vs.pass);
        stbuf->st_mode = S_IFREG | 0600;
        stbuf->st_nlink = 1;
    }
    stbuf->st_ino = inod;
    stbuf->st_uid = fuse_get_context()->uid;
    stbuf->st_gid = fuse_get_context()->gid;
    stbuf->st_size = size;
    stbuf->st_blksize = SB_BLOCK;
    stbuf->st_blocks = (int)((size / SB_BLOCK) + 1);

    free(this);
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
    char *p = NULL;
    asprintf(&p, "%s%s", ROOT_PATH, path);
    msg("unlink = %s", p);

    vstat_t vk;
    {
        vk.fs   = filesystem;
        vk.file = NULL;
        vk.size = NULL;
        vk.name = dir_get_file(p);
        vk.path = dir_get_path(p);
        vk.pass = dir_get_pass(p);
    }
    vstegfs_kill(vk);

    free(p);
    free(vk.name);
    free(vk.path);
    free(vk.pass);
    return EXIT_SUCCESS;
}

static int vstegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if (!offset)
    {
        char *p = NULL;
        asprintf(&p, "%s%s", ROOT_PATH, path);
        msg("read = %s", p);

        {
            vstat_t vs;
            vs.fs   = filesystem;
            vs.file = NULL;
            vs.size = NULL;
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);

            cache_read.size = vstegfs_find(vs);
            cache_read.data = realloc(cache_read.data, cache_read.size);

            free(vs.name);
            free(vs.path);
            free(vs.pass);
        }

        FILE *stream = open_memstream((char **)&cache_read.data, &cache_read.size);

        {
            vstat_t vs;
            vs.fs   = filesystem;
            vs.file = stream;
            vs.size = NULL;
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);

            errno = vstegfs_open(vs);

            free(vs.name);
            free(vs.path);
            free(vs.pass);
        }

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
        size = errno;
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

    char *p = NULL;
    asprintf(&p, "%s%s", ROOT_PATH, path);
    msg("write = %s", p);

    FILE *stream = fmemopen(cache_write.data, cache_write.size, "r");

    vstat_t vs;
    {
        vs.fs   = filesystem;
        vs.file = stream;
        vs.size = &cache_write.size;
        vs.name = dir_get_file(p);
        vs.path = dir_get_path(p);
        vs.pass = dir_get_pass(p);

        errno = vstegfs_save(vs);

        free(vs.name);
        free(vs.path);
        free(vs.pass);
    }

    fclose(stream);
    free(p);

    cache_write.size = 0x0;
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


static struct fuse_operations vstegfs_oper =
{
    .getattr	= vstegfs_getattr,
    .readdir	= vstegfs_readdir,
    .unlink     = vstegfs_unlink,
    .read       = vstegfs_read,
    .write      = vstegfs_write,
    /*
     * null functions (return success every time)
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
    init(APP, VER);

    if (argc < 2)
        return show_usage();

    char *fs = NULL, *mount = NULL;
    bool debug = false;
    while (true)
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
        int optex = 0;
        int opt = getopt_long(argc, argv, "df:m:hlv", long_options, &optex);
        if (opt == -1)
            break;
        switch (opt)
        {
            case 'd':
                debug = true;
                break;
            case 'f':
                fs = strdup(optarg);
                break;
            case 'm':
                mount = strdup(optarg);
                break;
            case 'h':
                return show_help();
            case 'l':
                return show_licence();
            case 'v':
                return show_version();
            case '?':
            default:
                die("unknown option %c", opt);
        }
    }

    if (!mount || !fs)
        return show_usage();

    if ((filesystem = open(fs, O_RDWR, S_IRUSR | S_IWUSR)) < 3)
        die("could not open file system");

    char **args = calloc(debug ? 5 : 4, sizeof( char * ));
    args[0] = strdup(argv[0]);
    args[1] = strdup("-o");
    args[2] = strdup("use_ino");
    if (debug)
        args[3] = strdup("-d");
    args[debug ? 4 : 3] = strdup(mount);

    return fuse_main(debug ? 5 : 4, args, &vstegfs_oper, NULL);
}

int64_t show_help(void)
{
    show_version();
    show_usage();
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  -f, --filesystem  FILE SYSTEM  Where to write the file system to\n");
    fprintf(stderr, "  -m, --mount       MOUNT POINT  Where to mount the file system\n");
    fprintf(stderr, "  -h, --help                     Show this help list\n");
    fprintf(stderr, "  -l, --licence                  Show overview of GNU GPL\n");
    fprintf(stderr, "  -v, --version                  Show version information\n\n");
    fprintf(stderr, "  Fuse executable to mount vstegfs file systems\n");
    return EXIT_SUCCESS;
}
