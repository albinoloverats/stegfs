/*
 * stegfs ~ a steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
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

#define FUSE_USE_VERSION 27

#include <fuse.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>

#include "common/common.h"
#include "common/list.h"

#define _IN_FUSE_STEGFS_
#include "src/fuse-stegfs.h"
#undef _IN_FUSE_STEGFS_
#include "src/dir.h"


static int fuse_stegfs_getattr(const char *path, struct stat *stbuf)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    char *f = dir_get_file(path);
    /*
     * common attributes for root/files/directories
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
    if (!strcmp(path, FILE_ROOT))
    {
        /*
         * get basic info about the file system itself
         */
        stegfs_fs_info_t info;
        lib_stegfs_info(&info);
        stbuf->st_size = info.size;
    }
    else if (!strncmp(path, PATH_BMAP, strlen(PATH_BMAP)) && strcmp(path, PATH_BMAP))
    {
        /*
         * if we're looking at files in /+proc/+bitmap, treat them as blocks; however, treat
         * /proc/+bitmap itself as a directory
         */
        stbuf->st_mode  = S_IFBLK;
        stbuf->st_nlink = 1;
        stbuf->st_size  = 0;
    }
    else if (!strcmp(path, FILE_PROC))
    {
        stbuf->st_mode = S_IFDIR | 0500;
    }
    else if (!strncmp(path, FILE_PROC, strlen(FILE_PROC)) && strcmp(path, PATH_BMAP))
    {
        stbuf->st_mode = S_IFLNK | 0600;
    }
    else if (f[0] != SYMBOL_DIR && strcmp(path, FILE_ROOT))
    {
        /*
         * find info about a file
         */
        char *p = NULL;
        if (asprintf(&p, "%s%s", PATH_ROOT, path))
        {
            stegfs_file_t this;
            this.mode = STEGFS_READ;
            this.name = dir_get_file(p);
            this.path = dir_get_path(p);
            this.pass = dir_get_pass(p);
            this.data = NULL;
            stbuf->st_ino = lib_stegfs_stat(&this, NULL);
            stbuf->st_mode  = S_IFREG | 0600;
            stbuf->st_nlink = 1;
            stbuf->st_ctime = this.time;
            stbuf->st_mtime = this.time;
            stbuf->st_size  = this.size;
            free(p);
        }
        else
        {
            msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
            e = -ENOMEM;
        }
    }
    stbuf->st_blocks = (int)(stbuf->st_size / stbuf->st_blksize) + 1;
    free(f);

    STEGFS_UNLOCK(fuse_lock);
    return -e;
}

static int fuse_stegfs_readlink(const char *path, char *buf, size_t size)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    if (!strncmp(path, FILE_PROC, strlen(FILE_PROC)))
    {
        snprintf(buf, size, "../%s", path + strlen(FILE_PROC) + strlen(FILE_ROOT));
        for (uint64_t i = 0; i < strlen(buf); i++)
            if (buf[i] == SYMBOL_LINK)
                buf[i] = SYMBOL_SEP;
    }
    else
    {
        msg(NULL);
        e = -ENOENT;
    }

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    filler(buf,  ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    if (!strcmp(path, FILE_ROOT))
        filler(buf, FILE_PROC + 1, NULL, 0);
    if (!strcmp(path, FILE_PROC))
        filler(buf, FILE_BMAP, NULL, 0);
    if (!strcmp(path, PATH_BMAP))
    {
        /*
         * read the list of used blocks from the bitmap
         */
        stegfs_fs_info_t fs;
        lib_stegfs_info(&fs);
        uint64_t size = fs.blocks * sizeof( uint8_t ) / CHAR_BIT;
        uint8_t *map = lib_stegfs_cache_map();
        if (map)
            for (uint64_t i = 0; i < size; i++)
                for (uint8_t j = 0; j < CHAR_BIT; j++)
                    if (map[i] & (0x01 << j))
                    {
                        char *m = NULL;
                        if (asprintf(&m, "%lu", i * CHAR_BIT + j))
                        {
                            filler(buf, m, NULL, 0);
                            free(m);
                        }
                        else
                        {
                            msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
                            e = -ENOMEM;
                        }
                    }
    }
    else
    {
        list_t *files = lib_stegfs_cache_get();
        if (files)
        {
            char *p = NULL;
            if (asprintf(&p, "%s%s", PATH_ROOT, strcmp(path, FILE_ROOT) ? path : ""))
            {
                uint64_t max = list_size(files);
                for (uint64_t i = 0; i < max; i++)
                {
                    stegfs_cache_t *x = (stegfs_cache_t *)(list_get(files, i)->object);
                    if (!strcmp(p, PATH_PROC) && strlen(x->name))
                    {
                        char *f = NULL;
                        if (asprintf(&f, "%s/%s", x->path + strlen(PATH_ROOT), x->name))
                        {
                            for (uint64_t j = 0; j < strlen(f); j++)
                                if (f[j] == SYMBOL_SEP)
                                    f[j] = SYMBOL_LINK;
                            filler(buf, f + 1, NULL, 0);
                            free(f);
                        }
                        else
                        {
                            msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
                            e = -ENOMEM;
                        }
                    }
                    else if (!strcmp(p, x->path))
                        filler(buf, x->name, NULL, 0);
                }
                free(p);
            }
            else
            {
                msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
                e = -ENOMEM;
            }
        }
    }

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_unlink(const char *path)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    char *p = NULL;
    if (asprintf(&p, "%s%s", PATH_ROOT, path))
    {
        stegfs_file_t this;
        this.mode = STEGFS_READ;
        this.name = dir_get_file(p);
        this.path = dir_get_path(p);
        this.pass = dir_get_pass(p);
        this.data = NULL;
        lib_stegfs_kill(&this);
        free(p);
    }
    else
    {
        msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
        e = -ENOMEM;
    }

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    bool found = false;
    uint64_t max = list_size(fuse_cache);
    for (uint64_t i = 0; i < max; i++)
    {
        stegfs_file_t *this = (stegfs_file_t *)(list_get(fuse_cache, i)->object);
        if (this->id == info->fh)
        {
            if ((this->mode == STEGFS_READ) && (this->size))
            {
                if (offset + size > this->size)
                    size = this->size - offset;
                memcpy(buf, this->data + offset, size);
                e = size;
            }
            else
            {
                msg(_("file %#016lx not opened for reading"), this->id);
                e = -EBADF;
            }
            found = true;
            break;
        }
    }
    if (!found)
        e = -EBADF;

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    bool found = false;
    uint64_t max = list_size(fuse_cache);
    for (uint64_t i = 0; i < max; i++)
    {
        stegfs_file_t *this = (stegfs_file_t *)(list_get(fuse_cache, i)->object);
        if (this->id == info->fh)
        {
            if (this->mode == STEGFS_WRITE)
            {
                /*
                 * only increae the buffer if we need to
                 */
                void *x = NULL;
                if (offset + size > this->size)
                {
                    this->size = offset + size;
                    x = realloc(this->data, this->size + 1);
                }
                else
                    x = this->data;
                if (x)
                {
                    this->data = x;
                    memcpy(this->data + offset, buf, size);
                    e = size;
                }
                else
                {
                    msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
                    e = -ENOMEM;
                }
            }
            else
            {
                msg(_("file %#016lx not opened for writing"), this->id);
                e = -EBADF;
            }
            found = true;
            break;
        }
    }
    if (!found)
        e = -EBADF;

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_open(const char *path, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    char *p = NULL;
    if (asprintf(&p, "%s%s", PATH_ROOT, path) < 0)
    {
        msg(_("out of memory @ %s:%i"), __FILE__, __LINE__);
        e = -ENOMEM;
    }
    stegfs_file_t *this = calloc(1, sizeof( stegfs_file_t ));
    if (this)
    {
        uint64_t max = list_size(fuse_cache);
        for (uint64_t i = 0; i < max; i++)
        {
            stegfs_file_t *x = (stegfs_file_t *)(list_get(fuse_cache, i)->object);
            if (x->id > i)
            {
                this->id = i;
                break;
            }
        }
        if (!this->id)
            this->id = max + 1;
        this->size = 0;
        this->time = time(NULL);
        this->name = dir_get_file(p);
        this->path = dir_get_path(p);
        this->pass = dir_get_pass(p);
        this->data = NULL;
        if (info->flags & O_WRONLY)
        {
            this->mode = STEGFS_WRITE;
        }
        else
        {
            this->mode = STEGFS_READ;
            lib_stegfs_load(this);
        }
        free(p);
        list_append(&fuse_cache, this);
        info->fh = this->id;
    }
    else
    {
        msg(_("could not find space in cache for file \"%s\""), path);
        e = -ENOMEM;
    }

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_release(const char *path, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    bool found = false;
    uint64_t max = list_size(fuse_cache);
    for (uint64_t i = 0; i < max; i++)
    {
        stegfs_file_t *this = (stegfs_file_t *)(list_get(fuse_cache, i)->object);
        if (this->id == info->fh)
        {
            found = true;
            free(this->name);
            free(this->path);
            free(this->pass);
            if (this->data)
                free(this->data);
            list_remove(&fuse_cache, i);
            break;
        }
    }
    if (!found)
        e = -EBADF;

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_flush(const char *path, struct fuse_file_info *info)
{
    STEGFS_LOCK(fuse_lock);

    int e = -EXIT_SUCCESS;
    bool found = false;
    uint64_t max = list_size(fuse_cache);
    for (uint64_t i = 0; i < max; i++)
    {
        stegfs_file_t *this = (stegfs_file_t *)(list_get(fuse_cache, i)->object);
        if (this->id == info->fh)
        {
            found = true;
            if (this->mode == STEGFS_WRITE && this->size)
                e = lib_stegfs_save(this);
            break;
        }
    }
    if (!found)
        e = -EBADF;

    STEGFS_UNLOCK(fuse_lock);
    return e;
}

static int fuse_stegfs_truncate(const char *path, off_t offset)
{
    /*
     * this will always succeed because whenever a file is opened its buffer is always 0
     */
    return -EXIT_SUCCESS;
}

/*
 * empty functions; required by fuse, but not used by stegfs
 */
static int fuse_stegfs_mknod(const char *path, mode_t mode, dev_t rdev)
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
    init(APP, VER, LOG);

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
            case '?':
            default:
                die(_("unknown option %c"), opt);
        }
    }

    if (!mount || !fs)
        return show_usage();

    lib_stegfs_init(fs, do_cache);

    fuse_cache = list_create();
    stegfs_file_t *x = calloc(1, sizeof( stegfs_file_t ));
    list_append(&fuse_cache, x);

    char **args = NULL;
    if (!(args = calloc(debug ? ARGS_DEFAULT + 1 : ARGS_DEFAULT, sizeof( char * ))))
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

    return fuse_main(debug ? ARGS_DEFAULT + 1 : ARGS_DEFAULT, args, &fuse_stegfs_functions, NULL);
}
