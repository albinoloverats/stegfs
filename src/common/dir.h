/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2022, albinoloverats ~ Software Development
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
 * \date    2009-2022
 * \brief   Directory parsing functions
 * \note    These functions all return newly allocated strings
 *
 * Collection of functions to parse directory hierarchies and extract the
 * names of each leaf along the tree, from the root all the way to the file
 * name and password
 */

#include <inttypes.h>

#include "common.h"
#include "list.h"

#ifndef _WIN32
	#define DIR_SEPARATOR "/"
	#define DIR_SEPARATOR_CHAR '/'
#else
	#define DIR_SEPARATOR "\\"
	#define DIR_SEPARATOR_CHAR '\\'
#endif

#define path_equals(X, Y)       (X && Y && !strcmp(X, Y))
#define path_starts_with(X, Y)  (X && Y && !strncmp(X, Y, strlen(X)))

#define DIR_GET_NAME_ARGS_COUNT(...) DIR_GET_NAME_ARGS_COUNT2(__VA_ARGS__, 2, 1)
#define DIR_GET_NAME_ARGS_COUNT2(_1, _2, _, ...) _

#define dir_get_name_1(A)     dir_get_name_aux(A, '\0')
#define dir_get_name_2(A, B)  dir_get_name_aux(A, B)
#define dir_get_name(...) CONCAT(dir_get_name_, DIR_GET_NAME_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__)

typedef enum
{
	DIR_NONE    = 0x0000,

	DIR_FOLDER  = 0x0001,
	DIR_FILE    = 0x0002,
	DIR_LINK    = 0x0004,
	DIR_BLOCK   = 0x0008,
	DIR_CHAR    = 0x0010,
	DIR_SOCKET  = 0x0020,
	DIR_PIPE    = 0x0040,

	DIR_ALL     = 0x00FF
}
dir_type_e;

/*!
 * \brief         Extract the name part of the /path/file:password
 * \param[in]  p  Path: /path/file.extension
 * \param[in]  e  Extension to ommit
 * \return        Newly allocated string: name
 *
 * Working backwards, cut off the password and leading directories
 */
extern char *dir_get_name_aux(const char * const p, const char e) __attribute__((nonnull(1)));

/*!
 * \brief         Determine how deep down the hierarchy path goes
 * \param[in]  p  Path: /path/file:password
 * \return        Number of subdirectories passed through
 *
 * Count the number of parent directories the given path includes
 */
extern uint16_t dir_get_deep(const char * const p) __attribute__((nonnull(1)));

/*!
 * \brief         Extract the numbered part of the /path/file:password
 * \param[in]  p  Path: /path/file:password
 * \param[in]  x  Element from path to extract
 * \return        Newly allocated string: directory name
 *
 * Work down the given path and return a copy of the directory name at the
 * given index in the hierarchy
 */
extern char *dir_get_part(const char * const p, const uint16_t x) __attribute__((nonnull(1)));

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


/*!
 * \brief         Create a new directory recursively
 * \param[in]  p  Directory path to create
 * \param[in]  m  New directories mode
 *
 * Creates a new directory, recursively, for the given path.
 */
extern void dir_mk_recursive(const char *p, mode_t m) __attribute__((nonnull(1)));

/*!
 * \brief         Get all entries in a directory tree
 * \param[in]  p  Directory path to scan
 * \param[in]  m  The type of files to find
 *
 * Return all found files (of a paticular type) in a given directory
 * tree.
 */
extern LIST dir_get_tree(const char *p, dir_type_e m) __attribute__((nonnull(1)));

#endif /* _DIR_H_ */
