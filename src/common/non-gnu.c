/*
 * Common code which is typically missing on MS Windows
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
char *strchrnul(const char *s, int c_in)
{
	/* Find the first occurrence of C in S or the final NUL byte.  */
	const unsigned char *char_ptr;
	const unsigned long int *longword_ptr;
	unsigned long int longword, magic_bits, charmask;
	unsigned char c;

	c = (unsigned char)c_in;

	/* Handle the first few characters by reading one character at a time.
	   Do this until CHAR_PTR is aligned on a longword boundary.  */
	for (char_ptr = (const unsigned char *)s;
		((unsigned long int)char_ptr & (sizeof (longword) - 1)) != 0;
		++char_ptr)
		if (*char_ptr == c || *char_ptr == '\0')
			return (void *)char_ptr;

	/* All these elucidatory comments refer to 4-byte longwords,
	   but the theory applies equally well to 8-byte longwords.  */

	longword_ptr = (unsigned long int *)char_ptr;

	/* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
	   the "holes."  Note that there is a hole just to the left of
	   each byte, with an extra at the end:

	   bits:  01111110 11111110 11111110 11111111
	   bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

	   The 1-bits make sure that carries propagate to the next 0-bit.
	   The 0-bits provide holes for carries to fall into.  */
	switch (sizeof (longword))
	{
		case 4: magic_bits = 0x7efefeffL; break;
		case 8: magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL; break;
		default:
			die("unsupported size of unsigned long @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, sizeof (longword));
	}

	/* Set up a longword, each of whose bytes is C.  */
	charmask = c | (c << 8);
	charmask |= charmask << 16;
	if (sizeof (longword) > 4)
		/* Do the shift in two steps to avoid a warning if long has 32 bits.  */
		charmask |= (charmask << 16) << 16;
	if (sizeof (longword) > 8)
		die("unsupported size of unsigned long @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, sizeof (longword));

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
		die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, step);
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
				die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, step);
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
				die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, len * sizeof *names);
			names = tmp;
		}

		if (!(names[cnt] = malloc(sizeof( struct dirent ))))
			die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, sizeof *names);

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

#endif /* _WIN32 */
