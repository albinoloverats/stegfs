/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright (c) 2007-2011, albinoloverats ~ Software Development
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

#include "dir.h"

#include <stdlib.h>
#include <string.h>

extern char *dir_get_file(const char * const restrict p)
{
    char * const restrict z = strdup(p);
    if (!z)
        return NULL;
    const char * const restrict f = strrchr(z, ':');
    char * const restrict e = strchr(z, ':') ? : z + strlen(z);
    char *r = NULL;
    *e = 0x00;
    if (!strrchr(p, '/'))
        r = strndup(p, f - z);
    else
        r = strndup(strrchr(z, '/') + 1, f - strrchr(z, '/'));
    free(z);
    return r;
}

extern uint16_t dir_get_deep(const char * const restrict p)
{
    if (!strlen(p))
        return 0;
    uint16_t c = 0;
    if (p[0] != '/')
        c++;
    char * restrict z = strdup(p);
    if (!z)
        return 0;
    char * const restrict e = strchr(z, ':') ? : z + strlen(z);
    *e = 0x00;
    while ((z = strchr(z, '/')))
        if (++z >= e) /* if we run past the end of the string break */
            break;
        else
            c++;
    free(z);
    return ++c;
}

extern char *dir_get_part(const char * const restrict p, const uint16_t x)
{
    if (x > dir_get_deep(p))
        return NULL;
    char *z = strdup(p);
    if (!z)
        return NULL;
    char *s = NULL;
    for (uint16_t i = 0; i <= x; i++)
        s = strsep(&z, "/");
    return s;
}

extern char *dir_get_pass(const char * const restrict p)
{
    if (!strrchr(p, ':'))
        return strdup("");
    return strdup(strrchr(p, ':') + 1);
}

extern char *dir_get_path(const char * const restrict p)
{
    return strndup(p, strrchr(p, '/') - p);
}
