/*
 * Common code for memory handling (in a do-or-die kind of way).
 * Copyright © 2024-2024, albinoloverats ~ Software Development
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
#include <stdlib.h>
#include <stdarg.h>

#include <string.h>

#include "mem.h"
#include "error.h"
//#include "non-gnu.h"

extern void *mem_mod(const char *file, int line, const char *func, size_t size)
{
	void *m = malloc(size);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, size);
	return m;
}

extern void *mem_cod(const char *file, int line, const char *func, size_t count, size_t size)
{
	void *m = calloc(count, size);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, size);
	return m;
}

extern void *mem_rod(const char *file, int line, const char *func, void *ptr, size_t newsize)
{
	void *m = realloc(ptr, newsize);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, newsize);
	return m;
}

extern char *mem_sod(const char *file, int line, const char *func, const char *str)
{
	size_t len = strlen(str);
	char *new = strdup(str);
	if (!new)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, len);
	return new;
}

extern char *mem_nod(const char *file, int line, const char *func, const char *str, size_t len)
{
	size_t olen = strlen(str);
	char *new = strndup(str, len);
	if (!new)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, olen < len ? len : olen);
	return new;
}

extern int mem_aod(const char *file, int line, const char *func, char **ptr, const char *template, ...)
{
	int r = 0;
	size_t l = strlen(template);
	va_list ap;
	va_start(ap, template);
#ifndef _WIN32
	r = vasprintf(ptr, template, ap);
#else
	*ptr = m_calloc(l, 8);
	r = vsnprintf(*ptr, (l * 8) - 1, template, ap);
#endif
	va_end(ap);
	if (r < 0)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, l);
	return r;
}

extern char *mem_fod(const char *file, int line, const char *func, const char *template, ...)
{
	int r = 0;
	size_t l = strlen(template);
	va_list ap;
	va_start(ap, template);
	char *ptr = NULL;
#ifndef _WIN32
	r = vasprintf(&ptr, template, ap);
#else
	ptr = m_calloc(l, 8);
	r = vsnprintf(*ptr, (l * 8) - 1, template, ap);
#endif
	va_end(ap);
	if (r < 0)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, l);
	return ptr;
}

#ifdef USE_GCRYPT

#include <gcrypt.h>

extern void *mem_sec_mod(const char *file, int line, const char *func, size_t size)
{
	void *m = gcry_malloc_secure(size);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, size);
	return m;
}

extern void *mem_sec_cod(const char *file, int line, const char *func, size_t count, size_t size)
{
	void *m = gcry_calloc_secure(count, size);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, size);
	return m;
}

extern void *mem_sec_rod(const char *file, int line, const char *func, void *ptr, size_t newsize)
{
	void *m = gcry_realloc(ptr, newsize);
	if (!m)
		die(_("Out of memory @ %s:%d:%s [%zu]"), file, line, func, newsize);
	return m;
}

#endif /* USE_GCRYPT */
