/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2021, albinoloverats ~ Software Development
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
#include "common/mem.h"
#include "common/ccrypt.h"
#include "common/tlv.h"
#include "common/config.h"

#include "stegfs.h"


#define MKFS "mkstegfs"


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
			{
				fprintf(stderr, "Could not open the block device\n");
				exit(EXIT_FAILURE);
			}
			*size = lseek(fs, 0, SEEK_END);
			break;
		case S_IFDIR:
		case S_IFCHR:
		case S_IFLNK:
		case S_IFSOCK:
		case S_IFIFO:
			fprintf(stderr, "Unable to create file system on specified device \"%s\"\n", path);
			exit(EXIT_FAILURE);
		case S_IFREG:
			if (!force && !recreate && !dry)
			{
				fprintf(stderr, "File by that name already exists - use -f to force\n");
				exit(EXIT_FAILURE);
			}
			/*
			 * file does exist; use its current size as the desired capacity
			 * (unless the user has specified a new size)
			 */
			if ((fs = open(path, O_RDWR /*| F_WRLCK*/, S_IRUSR | S_IWUSR)) < 0)
			{
				fprintf(stderr, "Could not open file system \"%s\"\n", path);
				exit(EXIT_FAILURE);
			}
			{
				uint64_t z = lseek(fs, 0, SEEK_END);
				if (!z && !*size)
				{
					fprintf(stderr, "Missing required file system size\n");
					exit(EXIT_FAILURE);
				}
				if (!*size)
					*size = z;
			}
			break;
		default:
			/*
			 * file doesn't exist - good, lets create it...
			 */
			if (!dry && (fs = open(path, O_RDWR /*| F_WRLCK*/ | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
			{
				fprintf(stderr, "Could not open file system \"%s\"\n", path);
				exit(EXIT_FAILURE);
			}
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

static uint64_t adjust_size(char *z)
{
	if (!z)
		return 0;
	char *f = NULL;
	uint64_t size = strtol(z, &f, 0);
	switch (toupper(f[0]))
	{
		case 'E':
			size *= KILOBYTE; /* Ratio between EB and PB (ditto below) */
			__attribute__((fallthrough));
		case 'P':
			size *= KILOBYTE;
			__attribute__((fallthrough));
		case 'T':
			size *= KILOBYTE;
			__attribute__((fallthrough));
		case 'G':
			size *= KILOBYTE;
			__attribute__((fallthrough));
		case 'M':
			__attribute__((fallthrough));
		case '\0':
			size *= MEGABYTE;
			break;
		default:
			fprintf(stderr, "unknown size suffix %c\n", f[0]);
			exit(EXIT_FAILURE);
	}
	return size;
}

static gcry_cipher_hd_t crypto_init(enum gcry_cipher_algos c, enum gcry_cipher_modes m)
{
	/* obtain a cipher handle */
	gcry_cipher_hd_t cipher_handle;
	gcry_cipher_open(&cipher_handle, c, m, GCRY_CIPHER_SECURE);
	/* create the initial key for the encryption algorithm */
	size_t key_length = gcry_cipher_get_algo_keylen(c);
	uint8_t *key = m_gcry_calloc_secure(key_length, sizeof( uint8_t ));
	gcry_create_nonce(key, key_length);
	gcry_cipher_setkey(cipher_handle, key, key_length);
	gcry_free(key);
	/* create the iv for the encryption algorithm */
	size_t iv_length = gcry_cipher_get_algo_blklen(c);
	uint8_t *iv = m_gcry_calloc_secure(iv_length, sizeof( uint8_t ));
	gcry_create_nonce(iv, iv_length);
	gcry_cipher_setiv(cipher_handle, iv, iv_length);
	return cipher_handle;
}

static void superblock_info(stegfs_block_s *sb, const char *cipher, const char *mode, const char *hash, const char *mac, uint8_t copies, uint64_t kdf)
{
	TLV tlv = tlv_init();

	tlv_s t = { TAG_STEGFS, strlen(STEGFS_NAME), (byte_t *)STEGFS_NAME };
	tlv_append(tlv, t);

	t.tag = TAG_VERSION;
	t.length = strlen(STEGFS_VERSION);
	t.value = (byte_t *)STEGFS_VERSION;
	tlv_append(tlv, t);

	t.tag = TAG_BLOCKSIZE;
	uint32_t blocksize = htonl(SIZE_BYTE_BLOCK);
	t.length = sizeof blocksize;
	t.value = m_malloc(sizeof blocksize);
	memcpy(t.value, &blocksize, sizeof blocksize);
	tlv_append(tlv, t);
	free(t.value);

	t.tag = TAG_HEADER_OFFSET;
	uint32_t head_offset = htonl(OFFSET_BYTE_HEAD);
	t.length = sizeof head_offset;
	t.value = m_malloc(sizeof head_offset);
	memcpy(t.value, &head_offset, sizeof head_offset);
	tlv_append(tlv, t);
	free(t.value);

	t.tag = TAG_CIPHER;
	t.length = strlen(cipher);
	t.value = (byte_t *)cipher;
	tlv_append(tlv, t);

	t.tag = TAG_MODE;
	t.length = strlen(mode);
	t.value = (byte_t *)mode;
	tlv_append(tlv, t);

	t.tag = TAG_HASH;
	t.length = strlen(hash);
	t.value = (byte_t *)hash;
	tlv_append(tlv, t);

	t.tag = TAG_MAC;
	t.length = strlen(mac);
	t.value = (byte_t *)mac;
	tlv_append(tlv, t);

	t.tag = TAG_DUPLICATION;
	t.length = sizeof copies;
	t.value = m_malloc(sizeof copies);
	memcpy(t.value, &copies, sizeof copies);
	tlv_append(tlv, t);
	free(t.value);

	t.tag = TAG_KDF;
	t.length = sizeof kdf;
	t.value = m_malloc(sizeof kdf);
	uint64_t k = htonll(kdf);
	memcpy(t.value, &k, sizeof kdf);
	tlv_append(tlv, t);
	free(t.value);

	uint64_t tags = htonll(tlv_size(tlv));
	memcpy(sb->data, &tags, sizeof tags);
	memcpy(sb->data + sizeof tags, tlv_export(tlv), tlv_length(tlv));

	return;
}

int main(int argc, char **argv)
{

	LIST args = list_init(config_named_compare, false, false);
	list_add(args, &((config_named_s){ 'c', "cipher",         _("algorithm"),  _("Algorithm to use to encrypt data; use ‘list’ to show available cipher algorithms"),        { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 's', "hash",           _("algorithm"),  _("Hash algorithm to generate key; use ‘list’ to show available hash algorithms"),            { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'm', "mode",           _("mode"),       _("The encryption mode to use; use ‘list’ to show available cipher modes"),                   { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'a', "mac",            _("mac"),        _("The MAC algorithm to use; use ‘list’ to show available MACs"),                             { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'i', "kdf-iterations", _("iterations"), _("Number of iterations the KDF should use"),                                                 { CONFIG_ARG_REQ_INTEGER, { .integer = 0     } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'p', "paranoid",       NULL,            _("Enable paranoia mode"),                                                                    { CONFIG_ARG_BOOLEAN,     { .boolean = false } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'x', "duplicates",     "#",             _("Number of times each file should be duplicated"),                                          { CONFIG_ARG_REQ_INTEGER, { .integer = 0     } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'z', "size",           _("size"),       _("Desired file system size, required when creating a file system in a normal file"),         { CONFIG_ARG_REQ_STRING,  { .string  = NULL  } }, false, false, false, false }));
	list_add(args, &((config_named_s){ 'f', "force",          NULL,            _("Force overwrite existing file, required when overwriting a file system in a normal file"), { CONFIG_ARG_REQ_BOOLEAN, { .boolean = false } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'r', "rewrite-sb",     NULL,            _("Rewrite the superblock (perhaps it became corrupt)"),                                      { CONFIG_ARG_REQ_BOOLEAN, { .boolean = false } }, false, true,  false, false }));
	list_add(args, &((config_named_s){ 'd', "dry-run",        NULL,            _("Dry run - print details about the file system that would have been created"),              { CONFIG_ARG_REQ_BOOLEAN, { .boolean = false } }, false, false, false, false }));

	LIST extra = list_default();
	list_add(extra, &((config_unnamed_s){ "device", { CONFIG_ARG_STRING,  { 0x0 } }, true,  false }));

	LIST notes = list_default();
	list_add(notes, _("If you’re feeling extra paranoid you can now disable to stegfs file system header. This will also disable the checks when mounting; therefore anything could happen ;-)"));

	config_about_s about =
	{
		MKFS,
		STEGFS_VERSION,
		PROJECT_URL,
		NULL
	};
	config_init(about);
	config_parse(argc, argv, args, extra, notes);

	list_deinit(notes);

	char *path = ((config_unnamed_s *)list_get(extra, 0))->response.value.string;

	list_deinit(extra);

	char *c         = ((config_named_s *)list_get(args,  0))->response.value.string;
	char *h         = ((config_named_s *)list_get(args,  1))->response.value.string;
	char *m         = ((config_named_s *)list_get(args,  2))->response.value.string;
	char *a         = ((config_named_s *)list_get(args,  3))->response.value.string;
	uint64_t kdf    = ((config_named_s *)list_get(args,  4))->response.value.integer;
	bool paranoid   = ((config_named_s *)list_get(args,  5))->response.value.boolean;
	uint32_t copies = (uint32_t)((config_named_s *)list_get(args, 6))->response.value.integer ? : COPIES_DEFAULT;
	char *s         = ((config_named_s *)list_get(args,  7))->response.value.string;
	bool force      = ((config_named_s *)list_get(args,  8))->response.value.boolean;
	bool rewrite    = ((config_named_s *)list_get(args,  9))->response.value.boolean;
	bool dry_run    = ((config_named_s *)list_get(args, 10))->response.value.boolean;

	list_deinit(args);

	enum gcry_cipher_algos cipher = c ? cipher_id_from_name(c) : DEFAULT_CIPHER;
	if (cipher == GCRY_CIPHER_NONE)
	{
		fprintf(stderr, "Unknown cipher \"%s\"\n", c);
		return EXIT_FAILURE;
	}
	enum gcry_md_algos hash = h ? hash_id_from_name(h) : DEFAULT_HASH;
	if (hash == GCRY_MD_NONE)
	{
		fprintf(stderr, "Unknown hash \"%s\"\n", h);
		return EXIT_FAILURE;
	}
	enum gcry_cipher_modes mode = m ? mode_id_from_name(m) : DEFAULT_MODE;
	if (mode == GCRY_CIPHER_MODE_NONE)
	{
		fprintf(stderr, "Unknown cipher mode \"%s\"\n", m);
		return EXIT_FAILURE;
	}
	enum gcry_mac_algos mac = a ? mac_id_from_name(a) : DEFAULT_MAC;
	if (mac == GCRY_MAC_NONE)
	{
		fprintf(stderr, "Unknown MAC \"%s\"\n", a);
		return EXIT_FAILURE;
	}
	if (!kdf)
		kdf = DEFAULT_KDF_ITERATIONS;
	if (c)
		free(c);
	if (h)
		free(h);
	if (m)
		free(m);
	if (a)
		free(a);

	uint64_t size = s ? adjust_size(s) : 0;

	int64_t fs = open_filesystem(path, &size, force, rewrite, dry_run);

	if (!(size || rewrite))
	{
		fprintf(stderr, "Invalid file system size \"%" PRIu64 "\" / \"%s\"\n", size, s);
		return EXIT_FAILURE;
	}
	if (s)
		free(s);

	uint64_t blocks = size / SIZE_BYTE_BLOCK;
	void *mm = NULL;
	if (dry_run)
		printf("Test run     : File system not modified\n");
	else
	{
		lockf(fs, F_LOCK, 0);
		ftruncate(fs, size);
		if ((mm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fs, 0)) == MAP_FAILED)
			die("Could not mmap file system \"%d\"", errno);
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
	printf("Duplication  : %*d ×\n", rewrite ? 0 : r, copies);
	printf("Cipher       : %s\n", cipher_name_from_id(cipher));
	printf("Cipher mode  : %s\n", mode_name_from_id(mode));
	printf("Hash         : %s\n", hash_name_from_id(hash));
	printf("MAC          : %s\n", mac_name_from_id(mac));

	if (rewrite || dry_run)
		goto superblock;
	/*
	 * write “encrypted” blocks
	 */
	uint8_t rnd[MEGABYTE] = { 0x00 };
#ifndef __DEBUG__
	gcry_create_nonce(rnd, sizeof rnd);
#endif

	gcry_cipher_hd_t gc = crypto_init(cipher, mode);
	printf("\e[?25l"); /* hide cursor */
	for (uint64_t i = 0; i < size / MEGABYTE; i++)
	{
		printf("\rWriting      : %'*.3f %%", r, PERCENT * i / (size / MEGABYTE));
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
	if (dry_run)
	{
		printf("\nTest run     : File system not modified\n\n");
		return EXIT_SUCCESS;
	}

	printf("Superblock   : ");
	if (paranoid)
		goto done;

	stegfs_block_s sb;
	gcry_create_nonce(&sb, sizeof sb);
	sb.path[0] = htonll(PATH_MAGIC_0);
	sb.path[1] = htonll(PATH_MAGIC_1);

	superblock_info(&sb, cipher_name_from_id(cipher), mode_name_from_id(mode), hash_name_from_id(hash), mac_name_from_id(mac), copies, kdf);

	sb.hash[0] = htonll(HASH_MAGIC_0);
	sb.hash[1] = htonll(HASH_MAGIC_1);
	sb.hash[2] = htonll(HASH_MAGIC_2);
	sb.next = htonll(blocks);
	memcpy(mm, &sb, sizeof sb);
	msync(mm, sizeof sb, MS_SYNC);
done:
	munmap(mm, size);
	close(fs);
	printf("%s\n\e[?25h", paranoid ? "Ignored" : "Done"); /* show cursor again */

	return EXIT_SUCCESS;
}
