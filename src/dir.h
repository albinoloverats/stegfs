/*
 * stegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
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

#ifdef __cplusplus
extern "C"
{
#endif

extern char    *dir_get_file(const char *);
extern uint16_t dir_get_deep(const char *);
extern char    *dir_get_part(const char *, uint16_t);
extern char    *dir_get_pass(const char *);
extern char    *dir_get_path(const char *);

#ifdef __cplusplus
}
#endif

#endif /* _DIR_H_ */
