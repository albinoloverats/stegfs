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

#include <stdlib.h>
#include <string.h>

#include "common/apple.h"

#include "dir.h"

extern char *dir_get_file(const char * const restrict path)
{
    char *file = strrchr(path, '/') + 1;
    char *pass = strrchr(path, ':');
    if (!file)
        file = (char *)path;
    if (!pass)
        pass = strchr(file, '\0');
    return strndup(file, pass - file);
}

extern uint16_t dir_get_deep(const char * const restrict path)
{
    uint16_t d = 0;
    char *ptr = (char *)path;
    while ((ptr = strchr(ptr, '/')))
    {
        d++;
        ptr++;
    }
    return d;
}

extern char *dir_get_part(const char * const restrict path, const uint16_t index)
{
    char *ptr = (char *)path;
    for (uint16_t i = 0; i < index; i++)
    {
        ptr = strchr(ptr, '/');
        if (!ptr)
            break;
        ptr++;
    }
    return strndup(ptr, strchrnul(ptr, '/') - ptr);
}

extern char *dir_get_pass(const char * const restrict path)
{
    if (!strrchr(path, ':'))
        return strdup("");
    return strdup(strrchr(path, ':') + 1);
}

extern char *dir_get_path(const char * const restrict path)
{
    char *p = strndup(path, strrchr(path, '/') - path);
    if (strlen(p))
        return p;
    free(p);
    return strdup("/");
}
