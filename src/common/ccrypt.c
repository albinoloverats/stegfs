/*
 * Common code for working with libgcrypt
 * Copyright © 2005-2017, albinoloverats ~ Software Development
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

static int algorithm_compare(const void *, const void *);

static const char *correct_sha1(const char * const restrict);
static const char *correct_aes_rijndael(const char * const restrict);
static const char *correct_blowfish128(const char * const restrict);
static const char *correct_twofish256(const char * const restrict);

static bool algorithm_is_duplicate(const char * const restrict);


typedef struct
{
	enum gcry_cipher_modes id;
	const char name[4];
}
block_mode_t;

static const block_mode_t MODES[] =
{
	{ GCRY_CIPHER_MODE_ECB, "ECB" },
	{ GCRY_CIPHER_MODE_CBC, "CBC" },
	{ GCRY_CIPHER_MODE_CFB, "CFB" },
	{ GCRY_CIPHER_MODE_OFB, "OFB" },
	{ GCRY_CIPHER_MODE_CTR, "CTR" },
};


extern void init_crypto(void)
{
	static bool done = false;
	if (done)
		return;
	/*
	 * initialise GNU Crypt library
	 */
	if (!gcry_check_version(GCRYPT_VERSION))
		die(_("Could not find GNU Crypt library"));
	gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
	gcry_control(GCRYCTL_INIT_SECMEM, 10485760, 0);
	gcry_control(GCRYCTL_RESUME_SECMEM_WARN);
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	errno = 0; /* need to reset errno after gcry_check_version() */
	done = true;
	return;
}

extern const char **list_of_ciphers(void)
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
	static const char **l = NULL;
	if (!l)
	{
		if (!(l = gcry_calloc_secure(len + 1, sizeof( char * ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( char * ));
		int j = 0;
		for (int i = 0; i < len; i++)
		{
			const char *n = cipher_name_from_id(lid[i]);
			if (!n)
				continue;
			l[j] = strdup(n);
			j++;
		}
		//l[j] = NULL;
		qsort(l, j, sizeof( char * ), algorithm_compare);
	}
	return (const char **)l;
}

extern const char **list_of_hashes(void)
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
	static const char **l = NULL;
	if (!l)
	{
		if (!(l = gcry_calloc_secure(len + 1, sizeof( char * ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( char * ));
		int j = 0;
		for (int i = 0; i < len; i++)
		{
			const char *n = hash_name_from_id(lid[i]);
			if (!n)
				continue;
			l[j] = strdup(n);
			j++;
		}
		//l[j] = NULL;
		qsort(l, j, sizeof( char * ), algorithm_compare);
	}
	return (const char **)l;
}

extern const char **list_of_modes(void)
{
	static const char **l = NULL;
	if (!l)
	{
		unsigned m = sizeof MODES / sizeof( block_mode_t );
		if (!(l = gcry_calloc_secure(m + 1, sizeof( char * ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( char * ));
		for (unsigned i = 0; i < m; i++)
			l[i] = MODES[i].name;
	}
	return (const char **)l;
}

extern const char **list_of_macs(void)
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
	static const char **l = NULL;
	if (!l)
	{
		if (!(l = gcry_calloc_secure(len + 1, sizeof( char * ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( char * ));
		int j = 0;
		for (int i = 0; i < len; i++)
		{
			const char *n = gcry_mac_algo_name(lid[i]);
			if (!n || !strcmp("?", n))
				continue;
			l[j] = strdup(n);
			j++;
		}
		//l[j] = NULL;
		qsort(l, j, sizeof( char * ), algorithm_compare);
	}
	return (const char **)l;
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

static int algorithm_compare(const void *a, const void *b)
{
	return strcmp(*(char **)a, *(char **)b);
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
