/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2018, albinoloverats ~ Software Development
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
#include "init.h"

extern bool is_stegfs(void)
{
	return false;
}

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

static void superblock_info(stegfs_block_t *sb, const char *cipher, const char *mode, const char *hash, const char *mac, uint8_t copies)
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

	t.tag = TAG_MAC;
	t.length = strlen(mac);
	t.value = (byte_t *)mac;
	tlv_append(&tlv, t);

	t.tag = TAG_DUPLICATION;
	t.length = sizeof copies;
	t.value = malloc(sizeof copies);
	memcpy(t.value, &copies, sizeof copies);
	tlv_append(&tlv, t);
	free(t.value);

	uint64_t tags = htonll(tlv_count(tlv));
	memcpy(sb->data, &tags, sizeof tags);
	memcpy(sb->data + sizeof tags, tlv_export(tlv), tlv_size(tlv));

	return;
}

int main(int argc, char **argv)
{
	args_t args = init(argc, argv, NULL);

	char path[PATH_MAX + 1] = "";

	realpath(args.fs, path);
	if (!strlen(path))
	{
		fprintf(stderr, "Missing file system target!\n");
		return EXIT_FAILURE;
	}

	if (!(args.size || args.rewrite_sb))
	{
		fprintf(stderr, "Invalid file system size!\n");
		return EXIT_FAILURE;
	}

	int64_t fs = open_filesystem(path, &args.size, args.force, args.rewrite_sb, args.dry_run);

	uint64_t blocks = args.size / SIZE_BYTE_BLOCK;
	void *mm = NULL;
	if (args.dry_run)
		printf("Test run     : File system not modified\n");
	else
	{
		lockf(fs, F_LOCK, 0);
		ftruncate(fs, args.size);
		if ((mm = mmap(NULL, args.size, PROT_READ | PROT_WRITE, MAP_SHARED, fs, 0)) == MAP_FAILED)
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

	double z = args.size / MEGABYTE;
	char units[] = "MB";
	adjust_units(&z, units);
	sprintf(s1, "%f", z);
	s2 = strchr(s1, '.');
	l = s2 - s1;
	printf("Size         : %'*.*g %s\n", r, (l + 2), z, units);
	if ((z = ((double)args.size / SIZE_BYTE_BLOCK * SIZE_BYTE_DATA) / MEGABYTE) < 1)
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
	printf("Largest file : %'*.*g %s\n", r, (l + 2), z / args.duplicates, units);
	printf("Duplication  : %*d ×\n", args.rewrite_sb ? 0 : r, args.duplicates);
	printf("Cipher       : %s\n", cipher_name_from_id(args.cipher));
	printf("Cipher mode  : %s\n", mode_name_from_id(args.mode));
	printf("Hash         : %s\n", hash_name_from_id(args.hash));
	printf("MAC          : %s\n", mac_name_from_id(args.mac));

	if (args.rewrite_sb || args.dry_run)
		goto superblock;
	/*
	 * write “encrypted” blocks
	 */
	uint8_t rnd[MEGABYTE] = { 0x00 };
#ifndef __DEBUG__
	gcry_create_nonce(rnd, sizeof rnd);
#endif

	gcry_cipher_hd_t gc = crypto_init(args.cipher, args.mode);
	printf("\e[?25l"); /* hide cursor */
	for (uint64_t i = 0; i < args.size / MEGABYTE; i++)
	{
		printf("\rWriting      : %'*.3f %%", r, PERCENT * i / (args.size / MEGABYTE));
#ifndef __DEBUG__
		gcry_cipher_encrypt(gc, rnd, sizeof rnd, NULL, 0);
#else
		(void)gc;
#endif
		memcpy(mm + (i * sizeof rnd), rnd, sizeof rnd);
		msync(mm + (i * sizeof rnd), sizeof rnd, MS_SYNC);
	}
	printf("\rWriting      : %'*.3f %%\n", r, PERCENT);

superblock:
	if (args.dry_run)
	{
		printf("\nTest run     : File system not modified\n\n");
		return EXIT_SUCCESS;
	}

	printf("Superblock   : ");
	if (args.paranoid)
		goto done;

	stegfs_block_t sb;
	gcry_create_nonce(&sb, sizeof sb);
	sb.path[0] = htonll(PATH_MAGIC_0);
	sb.path[1] = htonll(PATH_MAGIC_1);

	superblock_info(&sb, cipher_name_from_id(args.cipher), mode_name_from_id(args.mode), hash_name_from_id(args.hash), mac_name_from_id(args.mac), args.duplicates);

	sb.hash[0] = htonll(HASH_MAGIC_0);
	sb.hash[1] = htonll(HASH_MAGIC_1);
	sb.hash[2] = htonll(HASH_MAGIC_2);
	sb.next = htonll(blocks);
	memcpy(mm, &sb, sizeof sb);
	msync(mm, sizeof sb, MS_SYNC);
done:
	munmap(mm, args.size);
	close(fs);
	printf("%s\n\e[?25h", args.paranoid ? "Ignored" : "Done"); /* show cursor again */

	return EXIT_SUCCESS;
}
