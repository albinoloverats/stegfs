/*
 * Version checking functions (non-applications specific)
 * Copyright © 2005-2013, albinoloverats ~ Software Development
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

#define NEW_VERSION_AVAILABLE "A new version is available!"
#define NEW_VERSION_OF_AVAILABLE "A new version of %s is available!"
#define NEW_VERSION_OF_AVAILABLE_LINE NEW_VERSION_OF_AVAILABLE "\n"

extern void version_check_for_update(char *, char *) __attribute__((nonnull(1, 2)));

extern bool new_version_available;

#endif /* _COMMON_VERSION_H_ */
