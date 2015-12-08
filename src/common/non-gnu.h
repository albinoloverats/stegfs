/*
 * Common code which is typically missing on Apple OS X
 * Copyright © 2005-2015, albinoloverats ~ Software Development
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

#ifndef _NON_GNU_EXT_H_
#define _NON_GNU_EXT_H_

#if defined(__APPLE__) || defined(_WIN32)

extern char *strchrnul(const char *string, int c);

#endif

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <direct.h>

#define srand48 srand  /*!< Quietly alias srand48 to be srand on Windows */
#define lrand48 rand   /*!< Quietly alias lrand48 to be rand on Windows */
#define F_RDLCK NOTSET /*!< If value doesn’t exist on Windows, ignore it */
#define F_WRLCK NOTSET /*!< If value doesn’t exist on Windows, ignore it */
#define O_FSYNC NOTSET /*!< If value doesn’t exist on Windows, ignore it */

#ifdef S_IRUSR
	#undef S_IRUSR
	#define S_IRUSR 0
#endif
#ifdef S_IWUSR
	#undef S_IWUSR
	#define S_IWUSR 0
#endif
#ifdef S_IXUSR
	#undef S_IXUSR
	#define S_IXUSR 0
#endif

#ifndef SIGQUIT
	#define SIGQUIT SIGBREAK /*!< If value doesn’t exist on Windows, use next closest match */
#endif
#ifndef ECANCELED
	#define ECANCELED 125 /*!< Make sure the missing error code exists */
#endif
#define __LITTLE_ENDIAN 1234 /*!< Not defined in MinGW, so set here */
#define __BYTE_ORDER __LITTLE_ENDIAN /*!< Windows is almost always going to be LE */

#define __bswap_16(x) /*!< Define ourselves a 2-byte swap macro */ ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#define ntohs(x) __bswap_16(x) /*!< Make sure that network-to-host-short exists */
#define htons(x) __bswap_16(x) /*!< Make sure that host-to-network-short exists */

#define __bswap_32(x) /*!< Define ourselves a 4-byte swap macro */ ((((x) & 0xff000000ul) >> 24) | (((x) & 0x00ff0000ul) >>  8) | (((x) & 0x0000ff00ul) <<  8) | (((x) & 0x000000fful) << 24))

#define ntohl(x) __bswap_32(x) /*!< Make sure that network-to-host-long exists */
#define htonl(x) __bswap_32(x) /*!< Make sure that host-to-network-long exists */

#ifndef vsnprintf
	#define vsnprintf _vsnprintf
#endif

#define fsync(fd) _commit(fd)
#define mkdir(dir, attr) _mkdir(dir)
#define lstat(path, st) stat(path, st)
#define link(old, new) CopyFile(old, new, FALSE)

extern ssize_t getline(char **lineptr, size_t *n, FILE *stream) __attribute__((nonnull(3)));

extern char *strndup(const char *s, size_t l) __attribute__((nonnull(1)));

extern int scandir(const char *path, struct dirent ***res, int (*sel)(const struct dirent *), int (*cmp)(const struct dirent **, const struct dirent **)) __attribute__((nonnull(1, 2)));

#endif /* _WIN32 */

#endif /* _NON_GNU_EXT_H_ */
