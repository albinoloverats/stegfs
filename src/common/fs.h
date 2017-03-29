/*
 * Common code for dealing with file system functions
 * Copyright © 2009-2017, albinoloverats ~ Software Development
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

#ifndef _COMMON_FS_H_
#define _COMMON_FS_H_

/*!
 * \file    fs.h
 * \author  albinoloverats ~ Software Development
 * \date    2009-2015
 * \brief   Common file system code, shared between projects
 *
 * Common file system code.
 */

/*!
 * \brief         Create a new directory recursively
 * \param[in]  p  Directory path to create
 * \param[in]  m  New directories mode
 *
 * Creates a new directory, recursively, for the given path.
 */
extern void recursive_mkdir(const char *p, mode_t m) __attribute__((nonnull(1)));

#endif /* _COMMON_FS_H_ */
