/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2014, albinoloverats ~ Software Development
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

#ifndef _HELP_H_

#define STEGFS_USAGE "<file system> <mount point> [FUSE options ...]"
#define MKFS_USAGE "<file system> [-s size] [-f] [-r]"

#define GIT_COMMIT_LENGTH 7

extern void show_help(void);
extern void show_licence(void);
extern void show_usage(void);
extern void show_version(void);

#endif
