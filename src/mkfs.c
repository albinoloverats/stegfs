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
#include <stdio.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>

#include <locale.h>
#include <fcntl.h>
#include <unistd.h>

#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>

#include <gcrypt.h>

#include "common/common.h"
#include "common/error.h"
#include "common/ccrypt.h"
#include "common/tlv.h"

#include "stegfs.h"


static int64_t open_filesystem(const char * const restrict path, uint64_t *size, bool force, bool recreate, bool dry)
{
    int64_t fs = 0x0;
    struct stat s;
    memset(&s, 0x00, sizeof s);
    stat(path, &s);
    switch (s.st_mode & S_IFMT)
    {
        case S_IFBLK:
            /*
             * use a device as the file system
             */
            if ((fs = open(path, O_RDWR /*| F_WRLCK*/, S_IRUSR | S_IWUSR)) < 0)
                die("could not open the block device");
            *size = lseek(fs, 0, SEEK_END);
            break;
        case S_IFDIR:
        case S_IFCHR:
        case S_IFLNK:
        case S_IFSOCK:
        case S_IFIFO:
            die("unable to create file system on specified device \"%s\"", path);
        case S_IFREG:
            if (!force && !recreate && !dry)
                die("file by that name already exists - use -f to force");
            /*
             * file does exist; use its current size as the desired capacity
             * (unless the user has specified a new size)
             */
            if ((fs = open(path, O_RDWR /*| F_WRLCK*/, S_IRUSR | S_IWUSR)) < 0)
                die("could not open file system %s", path);
            {
                uint64_t z = lseek(fs, 0, SEEK_END);
                if (!z && !*size)
                    die("missing required file system size");
                if (!*size)
                    *size = z;
            }
            break;
        default:
            /*
             * file doesn't exist - good, lets create it...
             */
            if (!dry && (fs = open(path, O_RDWR /*| F_WRLCK*/ | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
                die("could not open file system %s", path);
        break;
    }
    if (dry)
        close(fs);
    return fs;
}

static uint64_t parse_size(const char * const restrict s)
{
    /*
     * parse the value for our file system size (if using a file on an
     * existing file system) allowing for suffixes: MB, GB, TB, PB and
     * EB - sizes less than 1MB or greater than an Exbibyte are not yet
     * supported
     */
    char *f;
    uint64_t z = strtol(s, &f, 0);
    switch (toupper(f[0]))
    {
        case 'E':
            z *= KILOBYTE; /* Ratio between EB and PB (ditto below) */
        case 'P':
            z *= KILOBYTE;
        case 'T':
            z *= KILOBYTE;
        case 'G':
            z *= KILOBYTE;
        case 'M':
        case '\0':
            z *= MEGABYTE;
            break;
        default:
            die("unknown size suffix %c", f[0]);
    }
    return z;
}

static void adjust_units(double *size, char *units)
{
    if (*size >= KILOBYTE)
    {
        *size /= KILOBYTE;
        strcpy(units, "GB");
    }
    if (*size >= KILOBYTE)
    {
        *size /= KILOBYTE;
        strcpy(units, "TB");
    }
    if (*size >= KILOBYTE)
    {
        *size /= KILOBYTE;
        strcpy(units, "PB");
    }
    if (*size >= KILOBYTE)
    {
        *size /= KILOBYTE;
        strcpy(units, "EB");
    }
    return;
}

static gcry_cipher_hd_t crypto_init(enum gcry_cipher_algos c, enum gcry_cipher_modes m)
{
    /* obtain a cipher handle */
    gcry_cipher_hd_t cipher_handle;
    gcry_cipher_open(&cipher_handle, c, m, GCRY_CIPHER_SECURE);
    /* create the initial key for the encryption algorithm */
    size_t key_length = gcry_cipher_get_algo_keylen(c);
    uint8_t *key = gcry_calloc_secure(key_length, sizeof( uint8_t ));
    gcry_create_nonce(key, key_length);
    gcry_cipher_setkey(cipher_handle, key, key_length);
    gcry_free(key);
    /* create the iv for the encryption algorithm */
    size_t iv_length = gcry_cipher_get_algo_blklen(c);
    uint8_t *iv = gcry_calloc_secure(iv_length, sizeof( uint8_t ));
    gcry_create_nonce(iv, iv_length);
    gcry_cipher_setiv(cipher_handle, iv, iv_length);
    return cipher_handle;
}

static void superblock_info(stegfs_block_t *sb, const char *cipher, const char *mode, const char *hash, uint32_t copies)
{
    TLV_HANDLE tlv = tlv_init();

    tlv_t t = { TAG_STEGFS, strlen(STEGFS_NAME), (byte_t *)STEGFS_NAME };
    tlv_append(&tlv, t);

    t.tag = TAG_VERSION;
    t.length = strlen(STEGFS_VERSION);
    t.value = (byte_t *)STEGFS_VERSION;
    tlv_append(&tlv, t);

    t.tag = TAG_BLOCKSIZE;
    uint32_t blocksize = htonl(SIZE_BYTE_BLOCK);
    t.length = sizeof blocksize;
    t.value = malloc(sizeof blocksize);
    memcpy(t.value, &blocksize, sizeof blocksize);
    tlv_append(&tlv, t);
    free(t.value);

    t.tag = TAG_HEADER_OFFSET;
    uint32_t head_offset = htonl(OFFSET_BYTE_HEAD);
    t.length = sizeof head_offset;
    t.value = malloc(sizeof head_offset);
    memcpy(t.value, &head_offset, sizeof head_offset);
    tlv_append(&tlv, t);
    free(t.value);

    t.tag = TAG_CIPHER;
    t.length = strlen(cipher);
    t.value = (byte_t *)cipher;
    tlv_append(&tlv, t);

    t.tag = TAG_MODE;
    t.length = strlen(mode);
    t.value = (byte_t *)mode;
    tlv_append(&tlv, t);

    t.tag = TAG_HASH;
    t.length = strlen(hash);
    t.value = (byte_t *)hash;
    tlv_append(&tlv, t);

    t.tag = TAG_DUPLICATION;
    copies = htonl(copies);
    t.length = sizeof copies;
    t.value = malloc(sizeof copies);
    memcpy(t.value, &copies, sizeof copies);
    tlv_append(&tlv, t);
    free(t.value);

    memcpy(sb->data, tlv_export(tlv), tlv_size(tlv));

    return;
}

static void print_usage_help(char *arg)
{
    fprintf(stderr, "Usage: %s [-s size] <file system>\n", arg);
    fprintf(stderr, "\n");
    fprintf(stderr, "  -s  size       Desired file system size, required when creating a file system in\n");
    fprintf(stderr, "                 a normal file\n");
    fprintf(stderr, "  -f             Force overwrite existing file, required when overwriting a file\n");
    fprintf(stderr, "                 system in a normal file\n");
    fprintf(stderr, "  -r             Rewrite the superblock (perhaps it became corrupt)\n");
    fprintf(stderr, "  -t             Test run - print details about the file system that would have been created\n");
    fprintf(stderr, "  -c  algorithm  Algorithm to use to encrypt data\n");
    fprintf(stderr, "  -m  mode       The encryption mode to use\n");
    fprintf(stderr, "  -d  algorithm  Hash algorithm to generate key and check data integrity\n");
    fprintf(stderr, "  -p  copies     File duplication; number of copies\n");
    fprintf(stderr, "  -x             Enable extra paranoid node\n");
    fprintf(stderr, "\nNotes:\n  • Size option isn't necessary when creating a file system on a block device.\n");
    fprintf(stderr, "  • Size are in megabytes unless otherwise specified: GB, TB, PB, or EB.\n");
    fprintf(stderr, "  • Defaults for cipher algorithms and data duplication are:\n");
    fprintf(stderr, "    • %s\n", cipher_name_from_id(DEFAULT_CIPHER));
    fprintf(stderr, "    • %s\n", mode_name_from_id(DEFAULT_MDOE));
    fprintf(stderr, "    • %s\n", hash_name_from_id(DEFAULT_HASH));
    fprintf(stderr, "    • 8 copies of each file\n");
    fprintf(stderr, "  • If you’re feeling extra paranoid you can now disable to stegfs file\n");
    fprintf(stderr, "    system header. This will also disable the checks when mounting and then\n");
    fprintf(stderr, "    anything could happen ;-) This will force the use of the above defaults.\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc <= 1)
        print_usage_help(argv[0]);

    char path[PATH_MAX + 1] = "";
    uint64_t size = 0;
    bool force = false;
    bool recreate = false;
    bool dry = false;
    bool paranoid = false;
    uint32_t copies = DEFAULT_COPIES;
    enum gcry_cipher_algos c = DEFAULT_CIPHER;
    enum gcry_cipher_modes m = DEFAULT_MDOE;
    enum gcry_md_algos     h = DEFAULT_HASH;

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp("--help", argv[i]) || !strcmp("-h", argv[i]))
            print_usage_help(argv[0]);
        else if (!strcmp("-s", argv[i]))
            size = parse_size(argv[(++i)]);
        else if (!strcmp("-f", argv[i]))
            force = true;
        else if (!strcmp("-r", argv[i]))
            recreate = true;
        else if (!strcmp("-t", argv[i]))
            dry = true;
        else if (!strcmp("-c", argv[i]))
            c = cipher_id_from_name(argv[++i]);
        else if (!strcmp("-m", argv[i]))
            m = mode_id_from_name(argv[++i]);
        else if (!strcmp("-d", argv[i]))
            h = hash_id_from_name(argv[++i]);
        else if (!strcmp("-p", argv[i]))
            copies = strtol(argv[++i], NULL, 0);
        else if (!strcmp("-x", argv[i]))
            paranoid = true;
        else
            realpath(argv[i], path);
    }

    if (paranoid)
    {
        c = DEFAULT_CIPHER;
        m = DEFAULT_MDOE;
        h = DEFAULT_HASH;
    }

    if (!strlen(path))
    {
        fprintf(stderr, "Missing file system target!\n");
        return EXIT_FAILURE;
    }

    int64_t fs = open_filesystem(path, &size, force, recreate, dry);

    if (!size)
    {
        fprintf(stderr, "Invalid file system size!\n");
        return EXIT_FAILURE;
    }

    uint64_t blocks = size / SIZE_BYTE_BLOCK;
    void *mm = NULL;
    if (dry)
        printf("Test run     : File system not modified\n");
    else
    {
        lockf(fs, F_LOCK, 0);
        ftruncate(fs, size);
        if ((mm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fs, 0)) == MAP_FAILED)
        {
            perror(NULL);
            return errno;
        }
    }

    setlocale(LC_NUMERIC, "");
    printf("Location     : %s\n", path);

    /*
     * display some “useful” information about the file system being
     * created
     */
    char s1[27];
    char *s2;
    int l;

    sprintf(s1, "%'" PRIu64, blocks);
    int r = strlen(s1);
    if (r < 7)
        r = 7;
    printf("Blocks       : %*s\n", r, s1);

    double z = size / MEGABYTE;
    char units[] = "MB";
    adjust_units(&z, units);
    sprintf(s1, "%f", z);
    s2 = strchr(s1, '.');
    l = s2 - s1;
    printf("Size         : %'*.*g %s\n", r, (l + 2), z, units);
    if ((z = ((double)size / SIZE_BYTE_BLOCK * SIZE_BYTE_DATA) / MEGABYTE) < 1)
    {
        z *= KILOBYTE;
        strcpy(units, "KB");
    }
    else
        strcpy(units, "MB");
    adjust_units(&z, units);
    sprintf(s1, "%f", z);
    s2 = strchr(s1, '.');
    l = s2 - s1;
    printf("Capacity     : %'*.*g %s\n", r, (l + 2), z, units);
    printf("Largest file : %'*.*g %s\n", r, (l + 2), z / copies, units);

    if (recreate || dry)
        goto superblock;
    /*
     * write “encrypted” blocks
     */
    uint8_t rnd[MEGABYTE];
    gcry_create_nonce(rnd, sizeof rnd);

    gcry_cipher_hd_t gc = crypto_init(c, m);
    printf("\e[?25l"); /* hide cursor */
    for (uint64_t i = 0; i < size / MEGABYTE; i++)
    {
        printf("\rWriting      : %'*.3f %%", r, PERCENT * i / (size / MEGABYTE));
        gcry_cipher_encrypt(gc, rnd, sizeof rnd, NULL, 0);
        memcpy(mm + (i * sizeof rnd), rnd, sizeof rnd);
        msync(mm + (i * sizeof rnd), sizeof rnd, MS_SYNC);
    }
    printf("\rWriting      : %'*.3f %%\n", r, PERCENT);

superblock:

    printf("Duplication  : %*d ×\n", recreate ? 0 : r, copies);

    printf("Cipher       : %s\n", cipher_name_from_id(c));
    printf("Cipher mode  : %s\n", mode_name_from_id(m));
    printf("Hash         : %s\n", hash_name_from_id(h));

    if (dry)
    {
        printf("Test run     : File system not modified\n");
        return EXIT_SUCCESS;
    }

    printf("Superblock   : ");
    if (paranoid)
        goto done;

    stegfs_block_t sb;
    gcry_create_nonce(&sb, sizeof sb);
    memset(sb.path, 0xFF, sizeof sb.path);

    superblock_info(&sb, cipher_name_from_id(c), mode_name_from_id(m), hash_name_from_id(h), copies);

    sb.hash[0] = htonll(MAGIC_0);
    sb.hash[1] = htonll(MAGIC_1);
    sb.hash[2] = htonll(MAGIC_2);
    sb.next = htonll(blocks);
    memcpy(mm, &sb, sizeof sb);
    msync(mm, sizeof sb, MS_SYNC);
done:
    munmap(mm, size);
    close(fs);
    printf("%s\n\e[?25h", paranoid ? "Ignored" : "Done"); /* show cursor again */

    return EXIT_SUCCESS;
}
