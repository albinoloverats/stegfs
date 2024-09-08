/*
 * Common code which is typically missing on MS Windows
 * Copyright © 2005-2024, albinoloverats ~ Software Development
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

#if defined __APPLE__ || defined _WIN32

#include <string.h>
#include <stdlib.h>

#include "non-gnu.h"
#include "common.h"
#include "error.h"

/* Copyright (C) 1991,1993-1997,99,2000,2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Based on strlen implementation by Torbjorn Granlund (tege@sics.se),
   with help from Dan Sahlin (dan@sics.se) and
   bug fix and commentary by Jim Blandy (jimb@ai.mit.edu);
   adaptation to strchr suggested by Dick Karpinski (dick@cca.ucsf.edu),
   and implemented by Roland McGrath (roland@ai.mit.edu).

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */
extern char *strchrnul(const char *s, int c_in)
{
	/* Find the first occurrence of C in S or the final NUL byte.  */
	const unsigned char *char_ptr;
	const unsigned long long *longword_ptr;
	unsigned long long longword, magic_bits, charmask;
	unsigned char c;

	c = (unsigned char)c_in;

	/* Handle the first few characters by reading one character at a time.
	   Do this until CHAR_PTR is aligned on a longword boundary.  */
	for (char_ptr = (const unsigned char *)s;
		((unsigned long long)char_ptr & (sizeof (longword) - 1)) != 0;
		++char_ptr)
		if (*char_ptr == c || *char_ptr == '\0')
			return (void *)char_ptr;

	/* All these elucidatory comments refer to 4-byte longwords,
	   but the theory applies equally well to 8-byte longwords.  */

	longword_ptr = (unsigned long long *)char_ptr;

	/* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
	   the "holes."  Note that there is a hole just to the left of
	   each byte, with an extra at the end:

	   bits:  01111110 11111110 11111110 11111111
	   bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

	   The 1-bits make sure that carries propagate to the next 0-bit.
	   The 0-bits provide holes for carries to fall into.  */
	switch (sizeof longword)
	{
		case 4: magic_bits = 0x7efefeffLLU; break;
		case 8: magic_bits = ((0x7efefefeLLU << 16) << 16) | 0xfefefeffLLU; break;
		default:
			die(_("unsupported size of unsigned long @ %s:%d:%s [%zu])"), __FILE__, __LINE__, __func__, sizeof longword);
	}

	/* Set up a longword, each of whose bytes is C.  */
	charmask = c | (c << 8);
	charmask |= charmask << 16;
	if (sizeof (longword) > 4)
		/* Do the shift in two steps to avoid a warning if long has 32 bits.  */
		charmask |= (charmask << 16) << 16;
	if (sizeof longword > 8)
		die(_("unsupported size of unsigned long @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof longword);

	/* Instead of the traditional loop which tests each character,
	   we will test a longword at a time.  The tricky part is testing
	   if *any of the four* bytes in the longword in question are zero.  */
	for (;;)
	{
		/* We tentatively exit the loop if adding MAGIC_BITS to
		   LONGWORD fails to change any of the hole bits of LONGWORD.

		   1) Is this safe?  Will it catch all the zero bytes?
		   Suppose there is a byte with all zeros.  Any carry bits
		   propagating from its left will fall into the hole at its
		   least significant bit and stop.  Since there will be no
		   carry from its most significant bit, the LSB of the
		   byte to the left will be unchanged, and the zero will be
		   detected.

		   2) Is this worthwhile?  Will it ignore everything except
		   zero bytes?  Suppose every byte of LONGWORD has a bit set
		   somewhere.  There will be a carry into bit 8.  If bit 8
		   is set, this will carry into bit 16.  If bit 8 is clear,
		   one of bits 9-15 must be set, so there will be a carry
		   into bit 16.  Similarly, there will be a carry into bit
		   24.  If one of bits 24-30 is set, there will be a carry
		   into bit 31, so all of the hole bits will be changed.

		   The one misfire occurs when bits 24-30 are clear and bit
		   31 is set; in this case, the hole at bit 31 is not
		   changed.  If we had access to the processor carry flag,
		   we could close this loophole by putting the fourth hole
		   at bit 32!

		   So it ignores everything except 128’s, when they’re aligned
		   properly.

		   3) But wait!  Aren’t we looking for C as well as zero?
		   Good point.  So what we do is XOR LONGWORD with a longword,
		   each of whose bytes is C.  This turns each byte that is C
		   into a zero.  */

		longword = *longword_ptr++;

		/* Add MAGIC_BITS to LONGWORD.  */
		if ((((longword + magic_bits)

			/* Set those bits that were unchanged by the addition.  */
			^ ~longword)

			 /* Look at only the hole bits.  If any of the hole bits
				are unchanged, most likely one of the bytes was a
				zero.  */
			& ~magic_bits) != 0 ||

			/* That caught zeroes.  Now test for C.  */
			((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
			& ~magic_bits) != 0)
		{
			/* Which of the bytes was C or zero?
			   If none of them were, it was a misfire; continue the search.  */

			const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

			if (*cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (sizeof (longword) > 4)
			{
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
			}
		}
	}
	/* This should never happen. */
	return NULL;
}

#endif /* __APPLE__ || _WIN32 */


#if defined __APPLE__ || defined _WIN32 || defined __FreeBSD__ || defined __sun

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "non-gnu.h"
#include "common.h"
#include "error.h"

/* Compare strings while treating digits characters numerically.
 * Copyright (C) 1997-2018 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 * Contributed by Jean-Fran�ois Bignolles <bignolle@ecoledoc.ibp.fr>, 1997.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the GNU C Library; if not, see
 * <http://www.gnu.org/licenses/>.  */

/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
 * fractionnal parts, S_Z: idem but with leading Zeroes only */
#define  S_N    0x0
#define  S_I    0x3
#define  S_F    0x6
#define  S_Z    0x9

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define  CMP    2
#define  LEN    3

/* Compare S1 and S2 as strings holding indices/version numbers,
 * returning less than, equal to or greater than zero if S1 is less than,
 * equal to or greater than S2 (for more info, see the texinfo doc).
 */
extern int strverscmp(const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;

	/* Symbol(s)    0       [1-9]   others
	 * Transition   (10) 0  (01) d  (00) x   */

	static const uint8_t next_state[] =
	{
		/* state    x    d    0  */
		/* S_N */  S_N, S_I, S_Z,
		/* S_I */  S_N, S_I, S_I,
		/* S_F */  S_N, S_F, S_F,
		/* S_Z */  S_N, S_F, S_Z
	};

	static const int8_t result_type[] =
	{
		/* state   x/x  x/d  x/0  d/x  d/d  d/0  0/x  0/d  0/0  */
		/* S_N */  CMP, CMP, CMP, CMP, LEN, CMP, CMP, CMP, CMP,
		/* S_I */  CMP, -1,  -1,  +1,  LEN, LEN, +1,  LEN, LEN,
		/* S_F */  CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
		/* S_Z */  CMP, +1,  +1,  -1,  CMP, CMP, -1,  CMP, CMP
	};

	if (p1 == p2)
		return 0;

	unsigned char c1 = *p1++;
	unsigned char c2 = *p2++;
	/* Hint: '0' is a digit too.  */
	int state = S_N + ((c1 == '0') + (isdigit (c1) != 0));

	int diff;
	while ((diff = c1 - c2) == 0)
	{
		if (c1 == '\0')
			return diff;

		state = next_state[state];
		c1 = *p1++;
		c2 = *p2++;
		state += (c1 == '0') + (isdigit (c1) != 0);
	}

	state = result_type[state * 3 + (((c2 == '0') + (isdigit (c2) != 0)))];

	switch (state)
	{
		case CMP:
			return diff;

		case LEN:
			while (isdigit (*p1++))
				if (!isdigit (*p2++))
					return 1;

			return isdigit (*p2) ? -1 : diff;

		default:
			return state;
	}
}

#endif /* __APPLE__ || _WIN32 || __FreeBSD__ || __sun */


#ifdef _WIN32

#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>

#include <stdint.h>
#include <stdbool.h>

#include <dirent.h>

char *program_invocation_short_name = NULL;

extern ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	bool e = false;
	ssize_t r = 0;
	int32_t step = 0xFF;
	char *buffer = malloc(step);
	if (!buffer)
		die(_("out of memory @ %s:%d:%s [%d]"), __FILE__, __LINE__, __func__, step);
	for (r = 0; ; r++)
	{
		int c = fgetc(stream);
		if (c == EOF)
		{
			e = true;
			break;
		}
		buffer[r] = c;
		if (c == '\n')
			break;
		if (r >= step - 0x10)
		{
			step += 0xFF;
			if (!(buffer = realloc(buffer, step)))
				die(_("out of memory @ %s:%d:%s [%d]"), __FILE__, __LINE__, __func__, step);
		}
	}
	buffer[r + 1] = 0x00;
	if (*lineptr)
		free(*lineptr);
	*lineptr = buffer;
	*n = r;
	return e ? -1 : r;
}

extern char *strndup(const char *s, size_t l)
{
	size_t z = strlen(s);
	if (z > l)
		z = l;
	z++;
	char *r = malloc(z);
	memmove(r, s, z);
	r[z - 1] = '\0';
	return r;
}

/*
 * Copyright © 2005-2012 Rich Felker, http://www.musl-libc.org/
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
extern int scandir(const char *path, struct dirent ***res, int (*sel)(const struct dirent *), int (*cmp)(const struct dirent **, const struct dirent **))
{
	DIR *d = opendir(path);
	struct dirent *de, **names = 0, **tmp;
	size_t cnt = 0, len = 0;
	int old_errno = errno;

	if (!d)
		return -1;

	while ((errno = 0), (de = readdir(d)))
	{
		if (sel && !sel(de))
			continue;

		if (cnt >= len)
		{
			len = 2 * len + 1;
			if (len > SIZE_MAX / sizeof *names)
				break;

			if (!(tmp = realloc(names, len * sizeof *names)))
				die(_("out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, len * sizeof *names);
			names = tmp;
		}

		if (!(names[cnt] = malloc(sizeof( struct dirent ))))
			die(_("out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof *names);

		memcpy(names[cnt++], de, sizeof( struct dirent ));
	}

	closedir(d);

	if (errno)
	{
		if (names)
			while (cnt-- > 0)
				free(names[cnt]);
		free(names);
		return -1;
	}
	errno = old_errno;

	if (cmp)
		qsort(names, cnt, sizeof *names, (int (*)(const void *, const void *))cmp);

	*res = names;
	return cnt;
}

extern int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}

extern FILE *temp_file(void)
{
	char p[256] = { 0x0 };
	GetTempPath(sizeof p - 1, p);
	int o = strlen(p);
	uint16_t r = (uint16_t)(lrand48() & 0x0000FFFF);
	snprintf(p + o, sizeof p - o - 1, "alr-%04x", r);
	FILE *t = fopen(p, "wb+");
	return t;
}

#include <VersionHelpers.h>

extern char *windows_version(void)
{
	static char version[32];

	if (IsWindows10OrGreater())
	{
		strcpy(version, "Windows 10");
	}
	else if (IsWindows8Point1OrGreater())
	{
		strcpy(version, "Windows 8.1");
	}
	else if (IsWindows8OrGreater())
	{
		strcpy(version, "Windows 8");
	}
	else if (IsWindows7SP1OrGreater())
	{
		strcpy(version, "Windows 7 SP1");
	}
	else if (IsWindows7OrGreater())
	{
		strcpy(version, "Windows 7");
	}
	else if (IsWindowsVistaSP2OrGreater())
	{
		strcpy(version, "Windows Vista SP2");
	}
	else if (IsWindowsVistaSP1OrGreater())
	{
		strcpy(version, "Windows Vista SP1");
	}
	else if (IsWindowsVistaOrGreater())
	{
		strcpy(version, "Windows Vista");
	}
	else if (IsWindowsXPSP3OrGreater())
	{
		strcpy(version, "Windows XP SP3");
	}
	else if (IsWindowsXPSP2OrGreater())
	{
		strcpy(version, "Windows XP SP2");
	}
	else if (IsWindowsXPSP1OrGreater())
	{
		strcpy(version, "Windows XP SP1");
	}
	else if (IsWindowsXPOrGreater())
	{
		strcpy(version, "Windows XP");
	}

	if (IsWindowsServer())
	{
		strcat(version, " Server");
	}
	else
	{
		strcat(version, " Client");
	}
	return version;
}

/* Print out on stderr a line consisting of the test in S, a colon, a space,
   a message describing the meaning of the signal number SIG and a newline.
   If S is NULL or "", the colon and space are omitted.  */
extern void psignal(int sig, const char *s)
{
	const char *colon, *desc;
	if (s == NULL || *s == '\0')
		s = colon = "";
	else
		colon = ": ";
	switch (sig)
	{
		case SIGABRT:
			desc = _("Aborted");
			break;
		case SIGFPE:
			desc = _("Floating point exception");
			break;
		case SIGILL:
			desc = _("Illegal instruction");
			break;
		case SIGINT:
			desc = _("Interrupt");
			break;
		case SIGSEGV:
			desc = _("Segmentation fault");
			break;
		case SIGTERM:
			desc = _("Terminated");
			break;
		default:
			desc = _("Unknown signal");
			break;
	}
	fprintf(stderr, "%s%s%s\n", s, colon, desc);
	return;
}

#endif /* _WIN32 */
