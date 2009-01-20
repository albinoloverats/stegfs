/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

//#include "vstegfs.h"
#include "dir.h"

/*
 * return the file part of the path (everything
 * after the final /)
 */
char *dir_get_file(const char *path)
{
    char *namepass = dir_get_part(path, dir_count_sub(path));
    return strtok(namepass, ":");
}

/*
 * return the password associated with the file
 */
char *dir_get_pass(const char *path)
{
    char *namepass = dir_get_part(path, dir_count_sub(path));
    strtok(namepass, ":");
    return strtok(NULL, ":");
}

/*
 * return the directory part of the path
 * (everthing upto the final /)
 */
char *dir_get_path(const char *path)
{
    char *ret = NULL;
    for (uint32_t i = 1; i < dir_count_sub(path); i++)
        asprintf(&ret, "%s/%s", ret ? ret : "", dir_get_part(path, i));
    return ret ? ret : "/";
}

/*
 * get a specific part of a path
 */
char *dir_get_part(const char *path, uint32_t entry)
{
    char *ret = NULL, *tmp = dir_strip_root(path);
    for (uint32_t i = 0; i < entry; i++)
        ret = strsep(&tmp, "/");
    return ret;
}

/*
 * count the number of sub directories
 * (including the file)
 */
uint32_t dir_count_sub(const char *path)
{
    char *tmp = dir_strip_root(path);
    for (uint32_t i = 0;; i++)
        if (!strsep(&tmp, "/"))
            return dir_is_file(path) ? i : --i;
}

/*
 * check for trailing / ; if it exists then all
 * we have is a path to a directory, not a file
 */
uint32_t dir_is_file(const char *path)
{
    if (path[strlen(path) - 1] != '/')
        return true;
    return false;
}

/*
 * remove leading /
 */
char *dir_strip_root(const char *path)
{
    if (path[0] != '/')
        return strdup(path);
    char *str = calloc(strlen(path), sizeof( char ));
    strncpy(str, path + 1, strlen(path) - 1);
    return str;
}

/*
 * remove trailing /
 */
char *dir_strip_tail(const char *path)
{
    if (path[strlen(path) - 1] != '/')
        return strdup(path);
    char *str = calloc(strlen(path), sizeof( char ));
    strncpy(str, path, strlen(path) - 1);
    return str;
}
