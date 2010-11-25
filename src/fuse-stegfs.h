/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright (c) 2007-2010, albinoloverats ~ Software Development
 * email: stegfs@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or TNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _FUSE_STEGFS_H_
    #define _FUSE_STEGFS_H_

    #include "src/lib-stegfs.h"

    #define APP "stegfs"
    #define VER "201001+"
    #ifdef DEBUGGING
        #define LOG "LOG"
    #else /*    DEBUGGING */
        #define LOG NULL
    #endif /* ! DEBUGGING */

    #define FILE_ROOT "/"
    #define FILE_PROC FILE_ROOT "+proc"
    #define FILE_BMAP "+bitmap"
    #define PATH_PROC PATH_ROOT FILE_PROC 
    #define PATH_BMAP FILE_PROC FILE_ROOT FILE_BMAP

    #define SYMBOL_SEP  '/'
    #define SYMBOL_DIR  '+'
    #define SYMBOL_LINK ','

    #define ARGS_MINIMUM 2
    #define ARGS_DEFAULT 4

#endif /* _FUSE_STEGFS_H_ */

#ifdef _IN_FUSE_STEGFS_
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
        .readlink = fuse_stegfs_readlink,
        .readdir  = fuse_stegfs_readdir,
        .unlink   = fuse_stegfs_unlink,
        .read     = fuse_stegfs_read,
        .write    = fuse_stegfs_write,
        .open     = fuse_stegfs_open,
        .release  = fuse_stegfs_release,
        .truncate = fuse_stegfs_truncate,
        /*
         * empty functions; required by fuse, but not used by stegfs
         */
        .mknod    = fuse_stegfs_mknod,
        .utime    = fuse_stegfs_utime,
        .chmod    = fuse_stegfs_chmod,
        .chown    = fuse_stegfs_chown,
        .flush    = fuse_stegfs_flush
    };
    static pthread_mutex_t fuse_lock;
    static list_t *fuse_cache;

#endif /* _IN_FUSE_STEGFS_ */
