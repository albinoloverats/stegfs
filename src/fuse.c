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
#define PAGESIZE 4096 /* the number of bytes given us by the kernel/fuse module */

#ifndef BUFFER_DEFAULT
    #define BUFFER_DEFAULT 0x7F
#endif
#define MIN_FH 3

#include <fuse.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include "common/common.h"

#include "src/vstegfs.h"
#include "src/dir.h"

typedef struct cache_t
{
    int64_t  stat;
    bool     used;
    bool     done;
    uint8_t *data;
    uint64_t part;
    uint64_t size;
}
cache_t;

/*
 * still not thread safe, but better than it was
 */
static cache_t **cache;
static uint64_t buffer_s = BUFFER_DEFAULT;
static int64_t filesystem;

static int vstegfs_getattr(const char *path, struct stat *stbuf)
{
    char *this = dir_get_file(path);

    /*
     * common attributes for files/directories
     */
    stbuf->st_ino     = 0x0;
    stbuf->st_mode    = S_IFDIR | 0700;
    stbuf->st_nlink   = 2;
    stbuf->st_uid     = fuse_get_context()->uid;
    stbuf->st_gid     = fuse_get_context()->gid;
    stbuf->st_atime   = time(NULL);
    stbuf->st_ctime   = time(NULL);
    stbuf->st_mtime   = time(NULL);
    stbuf->st_size    = 0x0;
    stbuf->st_blksize = SB_BLOCK;

    if (!strcmp(path, "/"))
    {
        stbuf->st_size = lseek(filesystem, 0, SEEK_END);
    }
    else if (this[0] != '+')
    {
        char *p = NULL;
        if (asprintf(&p, "%s%s", ROOT_PATH, path) < 0)
            return -ENOMEM;

        vstat_t vs;
        {
            vs.fs   = filesystem;
            vs.file = NULL;
            vs.size = calloc(1, sizeof( uint64_t ));
            vs.time = calloc(1, sizeof( time_t ));
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);
        }

        stbuf->st_ino   = vstegfs_find(vs);

        stbuf->st_mode  = S_IFREG | 0600;
        stbuf->st_nlink = 1;
        stbuf->st_ctime = *vs.time;
        stbuf->st_mtime = *vs.time;
        stbuf->st_size  = *vs.size;

        free(vs.size);
        free(vs.time);
        free(vs.name);
        free(vs.path);
        free(vs.pass);
        free(p);
    }

    stbuf->st_blocks = (int)(stbuf->st_size / stbuf->st_blksize);

    free(this);
    return EXIT_SUCCESS;
}

static int vstegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    char *p = NULL;
    if (!strcmp(path, "/"))
        p = strdup(ROOT_PATH);
    else
        if (asprintf(&p, "%s%s", ROOT_PATH, path) < 0)
            return -ENOMEM;
    /*
     * always set . and ..
     */
    filler(buf,  ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    char **list = vstegfs_known_list(p);
    free(p);
    if (!list)
        return EXIT_SUCCESS;

    uint32_t i = 0;
    while (list[i])
    {
        filler(buf, list[i], NULL, 0);
        free(list[i]);
        i++;
    }
    free(list);
    return EXIT_SUCCESS;
}

static int vstegfs_unlink(const char *path)
{
    char *p = NULL;
    if (asprintf(&p, "%s%s", ROOT_PATH, path) < 0)
        return -ENOMEM;

    vstat_t vk;
    {
        vk.fs   = filesystem;
        vk.file = NULL;
        vk.size = NULL;
        vk.time = NULL;
        vk.name = dir_get_file(p);
        vk.path = dir_get_path(p);
        vk.pass = dir_get_pass(p);
    }
    int64_t s = vstegfs_kill(vk);

    free(p);
    free(vk.name);
    free(vk.path);
    free(vk.pass);
    return s;
}

static int vstegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if (!offset)
    {
        cache[fi->fh]->stat = EXIT_SUCCESS;
        cache[fi->fh]->done = false;

        char *p = NULL;
        if (asprintf(&p, "%s%s", ROOT_PATH, path) < 0)
            return -ENOMEM;

        {
            vstat_t vs;
            vs.fs   = filesystem;
            vs.file = NULL;
            vs.time = NULL;
            vs.size = &cache[fi->fh]->size;
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);

            vstegfs_find(vs);
            if (!(cache[fi->fh]->data = malloc(cache[fi->fh]->size)))
                return -ENOMEM;


            free(vs.name);
            free(vs.path);
            free(vs.pass);
        }

        uint64_t expected = cache[fi->fh]->size;
        FILE *stream = open_memstream((char **)&cache[fi->fh]->data, &expected);

        {
            vstat_t vs;
            vs.fs   = filesystem;
            vs.file = stream;
            vs.size = &cache[fi->fh]->part;
            vs.time = NULL;
            vs.name = dir_get_file(p);
            vs.path = dir_get_path(p);
            vs.pass = dir_get_pass(p);

            cache[fi->fh]->stat = vstegfs_load(vs);

            free(vs.name);
            free(vs.path);
            free(vs.pass);
        }

        fclose(stream);
        free(p);
    }

    if (cache[fi->fh]->done)
    {
        free(cache[fi->fh]->data);
        cache[fi->fh]->data = NULL;
        cache[fi->fh]->used = false;
        return -cache[fi->fh]->stat;
    }

    if ((cache[fi->fh]->part < cache[fi->fh]->size) && (offset + size > cache[fi->fh]->part))
        cache[fi->fh]->done = true;

    if (offset < cache[fi->fh]->size)
    {
        if (offset + size > cache[fi->fh]->size)
            size = cache[fi->fh]->size - offset;
        memcpy(buf, cache[fi->fh]->data + offset, size);
    }

    return size;
}

static int vstegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    {
        cache[fi->fh]->stat = EXIT_SUCCESS;
        cache[fi->fh]->size += size;
        void *x = realloc(cache[fi->fh]->data, cache[fi->fh]->size + 1);
        if (!x)
            return -ENOMEM;

        cache[fi->fh]->data = x;
        memcpy(cache[fi->fh]->data + offset, buf, size);
    }

    if (size == PAGESIZE)
        return size;

    char *p = NULL;
    if (asprintf(&p, "%s%s", ROOT_PATH, path) < 0)
        return -ENOMEM;

    FILE *stream = fmemopen(cache[fi->fh]->data, cache[fi->fh]->size, "r");

    vstat_t vs;
    {
        vs.fs   = filesystem;
        vs.file = stream;
        vs.size = &cache[fi->fh]->size;
        time_t now = time(NULL);
        vs.time = &now;
        vs.name = dir_get_file(p);
        vs.path = dir_get_path(p);
        vs.pass = dir_get_pass(p);

        cache[fi->fh]->stat = vstegfs_save(vs);

        free(vs.name);
        free(vs.path);
        free(vs.pass);
    }

    fclose(stream);
    free(p);

    free(cache[fi->fh]->data);
    cache[fi->fh]->data = NULL;
    cache[fi->fh]->used = false;

    return cache[fi->fh]->stat ? -cache[fi->fh]->stat : size;
}

static int vstegfs_open(const char *path, struct fuse_file_info *fi)
{
    for (uint64_t i = MIN_FH; i < buffer_s; i++)
        if (!cache[i]->used)
        {
            cache[i]->used = true;
            fi->fh = i;
            return EXIT_SUCCESS;
        }

    uint64_t z = buffer_s + BUFFER_DEFAULT;
    if (z < buffer_s)
        return -ENFILE;

    void *x = realloc(cache, z * sizeof( cache_t * ));
    if (!x)
        return -ENOMEM;

    cache = x;
    for (uint64_t i = buffer_s; i < z; i++)
        if (!(cache[i] = calloc(1, sizeof( cache_t ))))
            return -ENOMEM;
    buffer_s = z;

    /*
     * return ourself, we should easily then find the 1st newly created entry
     */
    return vstegfs_open(path, fi);
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

static int vstegfs_null(const char *path, struct fuse_file_info *fi)
{
    return EXIT_SUCCESS;
}

static struct fuse_operations vstegfs_oper =
{
    .getattr  = vstegfs_getattr,
    .readdir  = vstegfs_readdir,
    .unlink   = vstegfs_unlink,
    .read     = vstegfs_read,
    .write    = vstegfs_write,
    .open     = vstegfs_open,
    /*
     * null functions
     */
    .mknod    = vstegfs_mknod,
    .truncate = vstegfs_truncate,
    .utime    = vstegfs_utime,
    .chmod    = vstegfs_chmod,
    .chown    = vstegfs_chown,
    .release  = vstegfs_null,
    .flush    = vstegfs_null
};

int main(int argc, char **argv)
{
    init(APP, VER);

    if (argc < ARGS_MINIMUM)
        return show_usage();

    char *fs = NULL, *mount = NULL;
    bool debug = false, do_cache = true;

    while (true)
    {
        static struct option long_options[] =
        {
            {"debug"     , no_argument      , 0, 'd'},
            {"filesystem", required_argument, 0, 'f'},
            {"mount"     , required_argument, 0, 'm'},
            {"nocache"   , no_argument      , 0, 'n'},
            {"buffer"    , required_argument, 0, 'b'},
            {"help"      , no_argument      , 0, 'h'},
            {"licence"   , no_argument      , 0, 'l'},
            {"version"   , no_argument      , 0, 'v'},
            {0, 0, 0, 0}
        };
        int optex = 0;
        int opt = getopt_long(argc, argv, "df:m:b:nhlv", long_options, &optex);
        if (opt < 0)
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
            case 'n':
                do_cache = false;
                break;
            case 'b':
                buffer_s = strtol(optarg, NULL, 10);
                break;
            case 'h':
                return show_help();
            case 'l':
                return show_licence();
            case 'v':
                return show_version();
            case '?':
            default:
                die(_("unknown option %c"), opt);
        }
    }

    if (!mount || !fs)
        return show_usage();

    if (buffer_s < MIN_FH)
        buffer_s = BUFFER_DEFAULT;

    if ((filesystem = open(fs, O_RDWR, S_IRUSR | S_IWUSR)) < 3)
        die(_("could not open file system %s"), fs);

    vstegfs_init(filesystem, do_cache);

    char **args = calloc(debug ? ARGS_DEFAULT + 1 : ARGS_DEFAULT, sizeof( char * ));
    if (!args)
        die(_("out of memory @ %s:%i"), __FILE__, __LINE__);

    {
        uint8_t i = 0;
        args[i++] = strdup(argv[0]);
        args[i++] = strdup("-o");
        args[i++] = strdup("use_ino");
        if (debug)
            args[i++] = strdup("-d");
        args[i++] = strdup(mount);
    }

    if (!(cache = calloc(buffer_s, sizeof( cache_t * ))))
        die(_("out of memory @ %s:%i"), __FILE__, __LINE__);

    for (uint64_t i = MIN_FH; i < buffer_s; i++)
        if (!(cache[i] = calloc(1, sizeof( cache_t ))))
            die(_("out of memory @ %s:%i"), __FILE__, __LINE__);

    return fuse_main(debug ? ARGS_DEFAULT + 1 : ARGS_DEFAULT, args, &vstegfs_oper, NULL);
}

int64_t show_help(void)
{
    show_version();
    show_usage();
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  -f, --filesystem  FILE SYSTEM  Location of the file system to mount\n");
    fprintf(stderr, "  -m, --mount       MOUNT POINT  Where to mount the file system\n");
    fprintf(stderr, "  -n, --nocache                  Do not cache directory entries and used blocks\n");
    fprintf(stderr, "  -b, --buffer      BUFFER       Initial buffer size (default is: %i)\n", BUFFER_DEFAULT);
    fprintf(stderr, "  -h, --help                     Show this help list\n");
    fprintf(stderr, "  -l, --licence                  Show overview of GNU GPL\n");
    fprintf(stderr, "  -v, --version                  Show version information\n\n");
    fprintf(stderr, "  Fuse executable to mount vstegfs file systems\n");
    return EXIT_SUCCESS;
}
