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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mcrypt.h>
#include <libintl.h>

#include "common/common.h"

#include "src/fuse-stegfs.h"
#include "src/lib-stegfs.h"

static uint64_t size_in_mb(char *);

int main(int argc, char **argv)
{
    init("mk" APP, VER, NULL);

    uint64_t  fs_size = 0x0;
    char     *fs_name = NULL;

    bool force   = false;
    bool restore = false;

    if (argc < ARGS_MINIMUM)
        return show_usage();
    while (true)
    {
        static struct option long_options[] =
        {
            {"filesystem", required_argument, 0, 'f'},
            {"size"      , required_argument, 0, 's'},
            {"force"     , no_argument      , 0, 'x'},
            {"restore"   , no_argument      , 0, 'r'},
            {"help"      , no_argument      , 0, 'h'},
            {"licence"   , no_argument      , 0, 'l'},
            {"version"   , no_argument      , 0, 'v'},
            {0, 0, 0, 0}
        };
        int32_t optex = 0;
        int32_t opt = getopt_long(argc, argv, "f:s:xrhlv", long_options, &optex);
        if (opt < 0)
            break;
        switch (opt)
        {
            case 'f':
                fs_name = strdup(optarg);
                break;
            case 's':
                fs_size = size_in_mb(optarg);
                break;
            case 'r':
                restore = true;
            case 'x':
                force = true;
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
    /*
     * is this a device or a file?
     */
    int64_t fs = 0x0;
    struct stat fs_stat;
    stat(fs_name, &fs_stat);
    switch (fs_stat.st_mode & S_IFMT)
    {
        case S_IFBLK:
            /*
             * use a device as the file system
             */
            if ((fs = open(fs_name, O_WRONLY | F_WRLCK, S_IRUSR | S_IWUSR)) < 0)
                die(_("could not open the block device"));
            fs_size = lseek(fs, 0, SEEK_END) / RATIO_BYTE_MB;
            break;
        case S_IFDIR:
        case S_IFCHR:
        case S_IFLNK:
        case S_IFSOCK:
        case S_IFIFO:
            die(_("unable to create file system on specified device"));
        case S_IFREG:
            if (!force)
                die(_("file by that name already exists - use -x to force"));
        default:
            /*
             * file doesn't exist - good, lets create it...
             */
        {
            int64_t flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef _GNU_SOURCE
            flags |= F_WRLCK;
#endif
            if (restore)
                flags ^= O_TRUNC; /* don't truncate the file if we're restoring the sb */
            if ((fs = open(fs_name, flags, S_IRUSR | S_IWUSR)) < 0)
                die(_("could not open the file system"));
        }
        break;
    }
    uint64_t fs_blocks = 0x0;
    if (restore)
    {
        msg(_("restoring superblock on %s"), fs_name);
        fs_blocks = lseek(fs, 0, SEEK_END) / SIZE_BYTE_BLOCK;
    }
    else
    {
        /*
         * check the size of the file system
         */
        if (fs_size < 1)
            die(_("cannot have a file system with size < 1MB"));
        /*
         * display some information about the soon-to-be file system to the
         * user
         */
        msg(_("location      : %s"), fs_name);
        fs_blocks = fs_size * RATIO_BYTE_MB / SIZE_BYTE_BLOCK;
        msg(_("total blocks  : %8ju"), fs_blocks);
        {
            char *units = strdup("MB");
            float volume = fs_size;
            if (volume >= RATIO_MB_TB)
            {
                volume /= RATIO_MB_TB;
                units = strdup("TB");
            }
            else if (volume >= RATIO_MB_GB)
            {
                volume /= RATIO_MB_GB;
                units = strdup("GB");
            }
            msg(_("volume        : %8.2f %s"), volume, units);
            free(units);
        }
        {
            char *units = strdup("MB");
            float fs_data = fs_size * SIZE_BYTE_DATA;
            fs_data /= SIZE_BYTE_BLOCK;
            if (fs_data >= RATIO_MB_TB)
            {
                fs_data /= RATIO_MB_TB;
                units = strdup("TB");
            }
            else if (fs_data >= RATIO_MB_GB)
            {
                fs_data /= RATIO_MB_GB;
                units = strdup("GB");
            }
            msg(_("data capacity : %8.2f %s"), fs_data, units);
            free(units);
        }
        {
            char *units = strdup("MB");
            float fs_avail = fs_size * SIZE_BYTE_DATA;
            fs_avail /= SIZE_BYTE_BLOCK;
            fs_avail /= MAX_COPIES;
            if (fs_avail >= RATIO_MB_TB)
            {
                fs_avail /= RATIO_MB_TB;
                units = strdup("TB");
            }
            else if (fs_avail >= RATIO_MB_GB)
            {
                fs_avail /= RATIO_MB_GB;
                units = strdup("GB");
            }
            msg(_("usable space  : %8.2f %s"), fs_avail, units);
            free(units);
        }
        /*
         * create lots of noise
         */
        lseek(fs, 0, SEEK_SET);
        for (uint64_t i = 0; i < fs_size; i++)
        {
            stegfs_block_t buffer[BLOCKS_PER_MB];

            for (uint16_t j = 0; j < BLOCKS_PER_MB; j++)
            {
                uint8_t path[SIZE_BYTE_PATH] =
                {
                    0x1B, 0x9D, 0xCC, 0x02,
                    0xA6, 0xAF, 0x57, 0xA1,
                    0xAC, 0xF2, 0x76, 0x8B,
                    0x29, 0x2B, 0xBE, 0x33
                };
                /*
                 * using 8bit bytes makes using rand48() easier
                 */
                uint8_t data[SIZE_BYTE_DATA];
                uint8_t hash[SIZE_BYTE_HASH];
                uint8_t next[SIZE_BYTE_NEXT];

                for (uint8_t k = 0; k < SIZE_BYTE_DATA; k++)
                    data[k] = mrand48();
                for (uint8_t k = 0; k < SIZE_BYTE_HASH; k++)
                    hash[k] = mrand48();
                for (uint8_t k = 0; k < SIZE_BYTE_NEXT; k++)
                    next[k] = mrand48();

                memcpy(buffer[j].path, path, SIZE_BYTE_PATH);
                memcpy(buffer[j].data, data, SIZE_BYTE_DATA);
                memcpy(buffer[j].hash, hash, SIZE_BYTE_HASH);
                memcpy(buffer[j].next, next, SIZE_BYTE_NEXT);
            }
            if (write(fs, buffer, RATIO_BYTE_MB) != RATIO_BYTE_MB)
                msg(_("could not create the file system"));
        }
    }
    /*
     * write a 'super' block at the beginning
     */
    {
        stegfs_block_t sb;

        char sid[SIZE_BYTE_PATH] = { 0x00 };
        strncpy(sid + 1, SUPER_ID, sizeof( sid ) - 2);
        sid[0x00] = 0x02;
        sid[0x0F] = 0x03;
        memcpy(sb.path, sid, SIZE_BYTE_PATH);

        memset(sb.data, 0xFF, SIZE_BYTE_DATA);

        sb.hash[0] = MAGIC_0;
        sb.hash[1] = MAGIC_1;
        sb.hash[2] = MAGIC_2;

        memcpy(sb.next, &fs_blocks, SIZE_BYTE_NEXT);

        lseek(fs, 0, SEEK_SET);
        if ( write(fs, &sb, sizeof( stegfs_block_t )) != sizeof( stegfs_block_t ))
            msg(_("could not create the file system"));
    }

    close(fs);
    return errno;
}

uint64_t size_in_mb(char *s)
{
    /*
     * parse the value for our file system size (if using a file on an
     * existing file system) allowing for suffixes: MB, GB or TB - a
     * size less than 1MB is silly :p and more than 1TB is insane
     */
    char *f;
    uint64_t v = strtol(s, &f, 0);
    switch (toupper(f[0]))
    {
        case 'T':
            v *= RATIO_MB_TB;
            break;
        case 'G':
            v *= RATIO_MB_GB;
            break;
        case 'M':
        case '\0':
            break;
        default:
            die(_("unknown size suffix %c"), f[0]);
    }
    return v;
}

int64_t show_help(void)
{
    /*
     * TODO translate
     */
    show_version();
    show_usage();
    fprintf(stderr, "\nOptions:\n\n");
    fprintf(stderr, "  -f, --filesystem  FILE SYSTEM  Where to write the file system to\n");
    fprintf(stderr, "  -s, --size        SIZE         Size of the file system in MB\n");
    fprintf(stderr, "  -x, --force                    Force overwriting an existing file\n");
    fprintf(stderr, "  -r, --restore                  Restore the default stegfs superblock\n");
    fprintf(stderr, "  -h, --help                     Show this help list\n");
    fprintf(stderr, "  -l, --licence                  Show overview of GNU GPL\n");
    fprintf(stderr, "  -v, --version                  Show version information\n\n");
    fprintf(stderr, "  A tool for creating stegfs images.  If a device is used instead of\n");
    fprintf(stderr, "  a file (eg: /dev/sda1) a size is not needed as the file system will\n");
    fprintf(stderr, "  use all available space on that device/partition.\n");
    return EXIT_SUCCESS;
}
