/*
 * Common code for memory handling (in a do-or-die kind of way).
 * Copyright © 2024-2024, albinoloverats ~ Software Development
 * email: encrypt@albinoloverats.net
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

#ifndef _COMMON_MEMORY_H_
#define _COMMON_MEMORY_H_

#include "common.h"

#define m_malloc(S)            mem_mod(__FILE__, __LINE__, __func__, S)
#define m_calloc(C, S)         mem_cod(__FILE__, __LINE__, __func__, C, S)
#define m_realloc(P, S)        mem_rod(__FILE__, __LINE__, __func__, P, S)

#define m_strdup(S)            mem_sod(__FILE__, __LINE__, __func__, S)
#define m_strndup(S, L)        mem_nod(__FILE__, __LINE__, __func__, S, L)

#define m_asprintf(P, F, ...)  mem_aod(__FILE__, __LINE__, __func__, P, F, __VA_ARGS__)

#define m_strdupf(F, ...)      mem_fod(__FILE__, __LINE__, __func__, F, __VA_ARGS__)

extern void *mem_mod(const char *, int, const char *, size_t);
extern void *mem_cod(const char *, int, const char *, size_t, size_t);
extern void *mem_rod(const char *, int, const char *, void *, size_t);

extern char *mem_sod(const char *, int, const char *, const char *);
extern char *mem_nod(const char *, int, const char *, const char *, size_t);

extern int mem_aod(const char *, int, const char *, char **, const char *, ...) __attribute__((nonnull(5), format(printf, 5, 6)));

extern char *mem_fod(const char *, int, const char *, const char *, ...) __attribute__((nonnull(4), format(printf, 4, 5)));

#ifdef USE_GCRYPT

#define m_gcry_malloc_secure(S)     mem_sec_mod(__FILE__, __LINE__, __func__, S)
#define m_gcry_calloc_secure(C, S)  mem_sec_cod(__FILE__, __LINE__, __func__, C, S)
#define m_gcry_realloc(P, S)        mem_sec_rod(__FILE__, __LINE__, __func__, P, S)

extern void *mem_sec_mod(const char *, int, const char *, size_t);
extern void *mem_sec_cod(const char *, int, const char *, size_t, size_t);
extern void *mem_sec_rod(const char *, int, const char *, void *, size_t);

#endif /* USE_GCRYPT */

#endif /* _COMMON_MEMORY_H_ */
