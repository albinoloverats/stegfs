/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2008, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
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

char *dir_get_file(const char *);
char *dir_get_pass(const char *);
char *dir_get_path(const char *);
char *dir_get_part(const char *, u_int32_t);

u_int32_t dir_count_sub(const char *);

u_int32_t dir_is_file(const char *);

char *dir_strip_root(const char *);
char *dir_strip_tail(const char *);

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#endif /* _DIR_H_ */
