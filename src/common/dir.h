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

#ifndef _DIR_H_
#define _DIR_H_

/*!
 * \file    dir.h
 * \author  albinoloverats ~ Software Development
 * \date    2009-2015
 * \brief   Directory parsing functions
 * \note    These functions all return newly allocated strings
 *
 * Collection of functions to parse directory hierarchies and extract the
 * names of each leaf along the tree, from the root all the way to the file
 * name and password
 */

#include <inttypes.h>

#ifndef _WIN32
    #define DIR_SEPARATOR_STRING "/"
    #define DIR_SEPARATOR_CHAR '/'
#else
    #define DIR_SEPARATOR_STRING "\\"
    #define DIR_SEPARATOR_CHAR '\\'
#endif

#define path_equals(X, Y)       (X && Y && !strcmp(X, Y))
#define path_starts_with(X, Y)  (X && Y && !strncmp(X, Y, strlen(X)))

/*!
 * \brief         Extract the name part of the /path/file:password
 * \param[in]  p  Path: /path/file:password
 * \return        Newly allocated string: name
 *
 * Working backwards, cut off the password and leading directories
 */
extern char *dir_get_name(const char * const restrict p) __attribute__((nonnull(1)));

/*!
 * \brief         Determine how deep down the hierarchy path goes
 * \param[in]  p  Path: /path/file:password
 * \return        Number of subdirectories passed through
 *
 * Count the number of parent directories the given path includes
 */
extern uint16_t dir_get_deep(const char * const restrict p) __attribute__((nonnull(1)));

/*!
 * \brief         Extract the numbered part of the /path/file:password
 * \param[in]  p  Path: /path/file:password
 * \param[in]  x  Element from path to extract
 * \return        Newly allocated string: directory name
 *
 * Work down the given path and return a copy of the directory name at the
 * given index in the hierarchy
 */
extern char *dir_get_part(const char * const restrict p, const uint16_t x) __attribute__((nonnull(1)));

/*!
 * \brief         Extract the password part of the /path/file:password
 * \param[in]  p  Path: /path/file:password
 * \return        Newly allocated string: password
 *
 * Find and return a copy of the password part of the path
 */
extern char *dir_get_pass(const char * const restrict p) __attribute__((nonnull(1)));

/*!
 * \brief         Extract only the path part of the /path/file:password
 * \param[in]  p  Path: /path/file:password
 * \return        Newly allocated string: path
 *
 * Take the given path, copy it, and cut off the file name and password
 */
extern char *dir_get_path(const char * const restrict p) __attribute__((nonnull(1)));

#endif /* _DIR_H_ */
