/*
 * Common code for working with libgcrypt
 * Copyright © 2005-2022, albinoloverats ~ Software Development
 * email: webmaster@albinoloverats.net
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
#include <inttypes.h>
#include <stdbool.h>

#include <gcrypt.h>

#include "common.h"
#include "non-gnu.h"
#include "error.h"
#include "ccrypt.h"
#include "list.h"

#define MOSTLY_NEEDED_LIBGCRYPT "1.8.2"

#define NAME_SHA1 "SHA1"
#define NAME_SHA160 "SHA160"
#define NAME_TIGER "TIGER"
#define NAME_TIGER192 "TIGER192"

#define NAME_AES "AES"
#define NAME_RIJNDAEL "RIJNDAEL"
#define NAME_BLOWFISH "BLOWFISH"
#define NAME_BLOWFISH128 "BLOWFISH128"
#define NAME_TWOFISH "TWOFISH"
#define NAME_TWOFISH256 "TWOFISH256"

static const char *correct_sha1(const char * const restrict);
static const char *correct_aes_rijndael(const char * const restrict);
static const char *correct_blowfish128(const char * const restrict);
static const char *correct_twofish256(const char * const restrict);

static bool algorithm_is_duplicate(const char * const restrict);


typedef struct
{
	enum gcry_cipher_modes id;
	const char name[9];
}
block_mode_t;

static const block_mode_t MODES[] =
{
	//{ GCRY_CIPHER_MODE_AESWRAP,        "AESWRAP"  }, // This will require more work in crypt_io
	{ GCRY_CIPHER_MODE_CBC,            "CBC"      },
	//{ GCRY_CIPHER_MODE_CCM,            "CCM"      }, // ditto
	{ GCRY_CIPHER_MODE_CFB,            "CFB"      },
	{ GCRY_CIPHER_MODE_CFB8,           "CFB8"     },
	{ GCRY_CIPHER_MODE_CTR,            "CTR"      },
	{ 14 /*GCRY_CIPHER_MODE_EAX*/,     "EAX"      }, // available on Arch but nowhere else yet
	{ GCRY_CIPHER_MODE_ECB,            "ECB"      },
	{ GCRY_CIPHER_MODE_GCM,            "GCM"      },
	{ 16 /*GCRY_CIPHER_MODE_GCM_SIV*/, "GCM_SIV"  }, // not available anywhere but in git source
	//{ GCRY_CIPHER_MODE_OCB,            "OCB"      }, // requires more work
	{ GCRY_CIPHER_MODE_OFB,            "OFB"      },
	{ GCRY_CIPHER_MODE_POLY1305,       "POLY1305" },
	{ 15 /*GCRY_CIPHER_MODE_SIV*/,     "SIV"      }, // ditto
	{ GCRY_CIPHER_MODE_STREAM,         "STREAM"   },
	{ 13 /*GCRY_CIPHER_MODE_XTS*/,     "XTS"      }, // not available on Slackware
};


extern void init_crypto(void)
{
	static bool done = false;
	if (done)
		return;
	/*
	 * initialise GNU Crypt library
	 */
	if (!gcry_check_version(NEED_LIBGCRYPT_VERSION))
		die(_("libgcrypt is too old (need %s, have %s)"), NEED_LIBGCRYPT_VERSION, gcry_check_version(NULL));
	gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
	gcry_control(GCRYCTL_INIT_SECMEM, 10 * MEGABYTE);
	/*
	 * See libgcrypt source agent/gpg-agent.c:1293 for why this is.
	 * It may cause problems with out-of-memory errors with older
	 * versions, like those on Slaceware and Solaris, but but
	 * Slackware's 1.7.10 works fine otherwise.
	 */
	if (strverscmp(MOSTLY_NEEDED_LIBGCRYPT, gcry_check_version(NULL)) < 0)
		gcry_control(78/*GCRYCTL_AUTO_EXPAND_SECMEM*/, MEGABYTE);
	gcry_control(GCRYCTL_RESUME_SECMEM_WARN);
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED);
	errno = 0; /* need to reset errno after gcry_check_version() */
	done = true;
	return;
}

extern LIST list_of_ciphers(void)
{
	init_crypto();

	enum gcry_cipher_algos lid[0xff] = { GCRY_CIPHER_NONE };
	int len = 0;
	enum gcry_cipher_algos id = GCRY_CIPHER_NONE;
	for (unsigned i = 0; i < sizeof lid; i++)
	{
		if (gcry_cipher_algo_info(id, GCRYCTL_TEST_ALGO, NULL, NULL) == 0)
		{
			lid[len] = id;
			len++;
		}
		id++;
	}
	static LIST *l = NULL;
	if (!l)
	{
		l = list_init((int (*)(const void *, const void *))strcmp, false, true);
		for (int i = 0; i < len; i++)
		{
			const char *n = cipher_name_from_id(lid[i]);
			if (!n)
				continue;
			list_add(l, n);
		}
	}
	return l;
}

extern LIST list_of_hashes(void)
{
	init_crypto();

	enum gcry_md_algos lid[0xff] = { GCRY_MD_NONE };
	int len = 0;
	enum gcry_md_algos id = GCRY_MD_NONE;
	for (unsigned i = 0; i < sizeof lid; i++)
	{
		if (gcry_md_test_algo(id) == 0)
		{
			lid[len] = id;
			len++;
		}
		id++;
	}
	static LIST *l = NULL;
	if (!l)
	{
		l = list_init((int (*)(const void *, const void *))strcmp, false, true);
		for (int i = 0; i < len; i++)
		{
			const char *n = hash_name_from_id(lid[i]);
			if (!n)
				continue;
			list_add(l, n);
		}
	}
	return l;
}

extern LIST list_of_modes(void)
{
	init_crypto();

	static LIST *l = NULL;
	if (!l)
	{
		unsigned m = sizeof MODES / sizeof( block_mode_t );
		l = list_init((int (*)(const void *, const void *))strcmp, false, true);
		for (unsigned i = 0; i < m; i++)
		{
			gcry_cipher_hd_t c;
			gcry_error_t e;
			if ((e = gcry_cipher_open(&c, GCRY_CIPHER_AES, MODES[i].id, 0)) == GPG_ERR_NO_ERROR)
				list_add(l, MODES[i].name);
			else if ((e = gcry_cipher_open(&c, GCRY_CIPHER_CHACHA20, MODES[i].id, 0)) == GPG_ERR_NO_ERROR)
				list_add(l, MODES[i].name);
			gcry_cipher_close(c);
		}
	}
	return l;
}

extern LIST list_of_macs(void)
{
	init_crypto();

	enum gcry_mac_algos lid[0xff] = { GCRY_MAC_NONE };
	int len = 0;
	enum gcry_mac_algos id = GCRY_MAC_NONE;
	for (unsigned i = 0; i < sizeof lid; i++)
	{
		if (gcry_mac_test_algo(id) == 0)
		{
			lid[len] = id;
			len++;
		}
		id++;
	}
	static LIST *l = NULL;
	if (!l)
	{
		l = list_init((int (*)(const void *, const void *))strcmp, false, true);
		for (int i = 0; i < len; i++)
		{
			const char *n = gcry_mac_algo_name(lid[i]);
			if (!n || !strcmp("?", n))
				continue;
			list_add(l, n);
		}
	}
	return l;
}

extern enum gcry_cipher_algos cipher_id_from_name(const char * const restrict n)
{
	int list[0xff] = { 0x00 };
	int len = 0;
	enum gcry_cipher_algos id = GCRY_CIPHER_NONE;
	for (unsigned i = 0; i < sizeof list; i++)
	{
		if (gcry_cipher_algo_info(id, GCRYCTL_TEST_ALGO, NULL, NULL) == 0)
		{
			list[len] = id;
			len++;
		}
		id++;
	}
	for (int i = 0; i < len; i++)
	{
		const char *x = cipher_name_from_id(list[i]);
		if (!x)
			continue;
		if (!strcasecmp(x, n))
			return list[i];
	}
	return GCRY_CIPHER_NONE;
}

extern enum gcry_md_algos hash_id_from_name(const char * const restrict n)
{
	int list[0xff] = { 0x00 };
	int len = 0;
	enum gcry_md_algos id = GCRY_MD_NONE;
	for (unsigned i = 0; i < sizeof list; i++)
	{
		if (gcry_md_test_algo(id) == 0)
		{
			list[len] = id;
			len++;
		}
		id++;
	}
	for (int i = 0; i < len; i++)
	{
		const char *x = hash_name_from_id(list[i]);
		if (!x)
			continue;
		if (!strcasecmp(x, n))
			return list[i];
	}
	return GCRY_MD_NONE;
}

extern enum gcry_cipher_modes mode_id_from_name(const char * const restrict n)
{
	for (unsigned i = 0; i < sizeof MODES / sizeof( block_mode_t ); i++)
		if (!strcasecmp(n, MODES[i].name))
			return MODES[i].id;
	return GCRY_CIPHER_MODE_NONE;
}

extern enum gcry_mac_algos mac_id_from_name(const char * const restrict n)
{
	int list[0xff] = { 0x00 };
	int len = 0;
	enum gcry_mac_algos id = GCRY_MAC_NONE;
	for (unsigned i = 0; i < sizeof list; i++)
	{
		if (gcry_mac_test_algo(id) == 0)
		{
			list[len] = id;
			len++;
		}
		id++;
	}
	for (int i = 0; i < len; i++)
	{
		const char *x = mac_name_from_id(list[i]);
		if (!x)
			continue;
		if (!strcasecmp(x, n))
			return list[i];
	}
	return GCRY_MAC_NONE;
}

extern const char *cipher_name_from_id(enum gcry_cipher_algos c)
{
	const char *n = gcry_cipher_algo_name(c);
	if (!strncasecmp(NAME_AES, n, strlen(NAME_AES)))
		return correct_aes_rijndael(n);
	else if (!strcasecmp(NAME_BLOWFISH, n))
		return correct_blowfish128(n);
	else if (!strcasecmp(NAME_TWOFISH, n))
		return correct_twofish256(n);
	return n;
}

extern const char *hash_name_from_id(enum gcry_md_algos h)
{
	const char *n = gcry_md_algo_name(h);
	if (algorithm_is_duplicate(n))
		return NULL;
	else if (!strncasecmp(NAME_SHA1, n, strlen(NAME_SHA1) - 1))
		return correct_sha1(n);
	return n;
}

extern const char *mode_name_from_id(enum gcry_cipher_modes m)
{
	for (unsigned i = 0; i < sizeof MODES / sizeof( block_mode_t ); i++)
		if (MODES[i].id == m)
			return MODES[i].name;
	return NULL;
}

extern const char *mac_name_from_id(enum gcry_mac_algos m)
{
	return gcry_mac_algo_name(m);
}

extern bool mode_valid_for_cipher(enum gcry_cipher_algos c, enum gcry_cipher_modes m)
{
	/*
	 * The cipher mode to use must be specified via mode. See Available
	 * cipher modes, for a list of supported cipher modes and the
	 * according constants.
	 *
	 * Note that some modes are incompatible with some algorithms - in
	 * particular, stream mode (GCRY_CIPHER_MODE_STREAM) only works with
	 * stream ciphers.
	 *
	 * Poly1305 AEAD mode (GCRY_CIPHER_MODE_POLY1305) only works with
	 * ChaCha20 stream cipher.
	 *
	 * The block cipher modes (GCRY_CIPHER_MODE_ECB,
	 * GCRY_CIPHER_MODE_CBC,GCRY_CIPHER_MODE_CFB, GCRY_CIPHER_MODE_OFB,
	 * GCRY_CIPHER_MODE_CTR and GCRY_CIPHER_MODE_EAX) will work with any
	 * block cipher algorithm.
	 *
	 * GCM mode (GCRY_CIPHER_MODE_CCM), CCM mode (GCRY_CIPHER_MODE_GCM),
	 * OCB mode (GCRY_CIPHER_MODE_OCB), and XTS mode
	 * (GCRY_CIPHER_MODE_XTS) will only work with block cipher
	 * algorithms which have the block size of 16 bytes.
	 */
	if (m == GCRY_CIPHER_MODE_POLY1305)
		return c == GCRY_CIPHER_CHACHA20;
	switch (c)
	{
		case GCRY_CIPHER_ARCFOUR:
		case GCRY_CIPHER_SALSA20:
		case GCRY_CIPHER_SALSA20R12:
		case GCRY_CIPHER_CHACHA20:
			return m == GCRY_CIPHER_MODE_STREAM;
		default:
			break;
	}
	switch (m)
	{
		case GCRY_CIPHER_MODE_CCM:
		case GCRY_CIPHER_MODE_GCM:
		case GCRY_CIPHER_MODE_OCB:
		case 13: // GCRY_CIPHER_MODE_XTS: C'mon Slackware :-/
			return !(gcry_cipher_get_algo_blklen(c) % 16);
		case GCRY_CIPHER_MODE_STREAM:
			/*
			 * Stream mode, only to be used with stream cipher
			 * algorithms.
			 */
			switch (c)
			{
				case GCRY_CIPHER_ARCFOUR:
				case GCRY_CIPHER_SALSA20:
				case GCRY_CIPHER_SALSA20R12:
				case GCRY_CIPHER_CHACHA20:
					return true;
				default:
					return false;
			}
			break;
		case GCRY_CIPHER_MODE_AESWRAP:
			/*
			 * This mode is used to implement the AES-Wrap algorithm
			 * according to RFC-3394. It may be used with any 128 bit
			 * block length algorithm, however the specs require one of
			 * the 3 AES algorithms.
			 */
			switch (c)
			{
				case GCRY_CIPHER_AES128:
				case GCRY_CIPHER_AES192:
				case GCRY_CIPHER_AES256:
					return true;
				default:
					return false;
			}
		default:
			break;
	}
	/*
	 * Hopefully whatever's left is a block cipher and a block cipher
	 * mode.
	 */
	return true;
}

static const char *correct_sha1(const char * const restrict n)
{
	return strcasecmp(n, NAME_SHA1) ? n : NAME_SHA160;
}

static const char *correct_aes_rijndael(const char * const restrict n)
{
	if (!strcasecmp(NAME_AES, n))
		return n; /* use AES (bits/blocks/etc) */
	/*
	 * use rijndael instead of AES as that’s the actual cipher name
	 */
	static char x[16];
	snprintf(x, sizeof x, "%s%s", NAME_RIJNDAEL, n + strlen(NAME_AES));
	return (const char *)x;
}

static const char *correct_blowfish128(const char * const restrict n)
{
	return (void)n , NAME_BLOWFISH128;
}

static const char *correct_twofish256(const char * const restrict n)
{
	return (void)n , NAME_TWOFISH256;
}

static bool algorithm_is_duplicate(const char * const restrict n)
{
	return !strcmp(NAME_TIGER192, n);
}
