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

#include "common/common.h"

#include "src/dir.h"

/*
 * these functions all return newly allocated strings (which the user is
 * required to free once finished with them)
 */

/*
 * return the file part of the path (everything after the final /)
 */
extern char *dir_get_file(const char *path)
{
    char *p = strdupa(path);
    if (!p)
        return NULL;
    char *f = strrchr(p, ':');
    char *e = strchr(p, ':') ? : p + strlen(p);
    char *r = NULL;
    *e = 0x00;
    if (!strrchr(path, '/'))
        r = strndup(path, f - p);
    else
        r = strndup(strrchr(p, '/') + 1, f - strrchr(p, '/'));
    return r;
}

/*
 * calculate how deep in the hierarchy we are (count number of '/')
 */
extern uint16_t dir_get_deep(const char *path)
{
    if (!strlen(path))
        return 0;
    uint16_t c = 0;
    if (path[0] != '/')
        c++;
    char *p = strdupa(path);
    if (!p)
        return 0;
    char *e = strchr(p, ':') ? : p + strlen(p);
    *e = 0x00;
    while ((p = strchr(p, '/')))
        if (++p >= e) /* if we run past the end of the string break */
            break;
        else
            c++;
    return ++c;
}

/*
 * return a particular directory name from with the hierarchy
 */
extern char *dir_get_part(const char *path, uint16_t part)
{
    if (part > dir_get_deep(path))
        return NULL;
    char *p = strdupa(path);
    if (!p)
        return NULL;
    char *s = NULL;
    for (uint16_t i = 0; i <= part; i++)
        s = strsep(&p, "/");
    return s;
}

/*
 * return the password associated with the file
 */
extern char *dir_get_pass(const char *path)
{
    if (!strrchr(path, ':'))
        return strdup("");
    return strdup(strrchr(path, ':') + 1);
}

/*
 * return the directory part of the path (everything up to the final /)
 */
extern char *dir_get_path(const char *path)
{
    return strndup(path, strrchr(path, '/') - path);
}
