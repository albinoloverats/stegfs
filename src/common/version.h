/*
 * Version checking functions (non-applications specific)
 * Copyright © 2005-2021, albinoloverats ~ Software Development
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

#ifndef _COMMON_VERSION_H_
#define _COMMON_VERSION_H_

#define NEW_VERSION_AVAILABLE "Version %s is available!"
#define NEW_VERSION_OF_AVAILABLE "Version %s of %s is available!"
#define NEW_VERSION_URL "Version %s of %s is available from\n%s\n"

extern void version_print(char *name, char *version, char *url);
extern void version_check_for_update(char *current_version, char *check_url, char *download_url) __attribute__((nonnull(1, 2)));

extern bool version_new_available;
extern bool version_is_checking;
extern char version_available[];
extern char new_version_url[];

#endif /* _COMMON_VERSION_H_ */
