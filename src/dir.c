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
    msg("%s", __func__);
    if (!strrchr(path, '/'))
        return strndup(path, strrchr(path, ':') - path);
    return strndup(strrchr(path, '/') + 1, strrchr(path, ':') - strrchr(path, '/') - 1);
}

/*
 * return the password associated with the file
 */
extern char *dir_get_pass(const char *path)
{
    msg("%s", __func__);
    if (!strrchr(path, ':'))
        return strdup("");
    return strdup(strrchr(path, ':')) + 1;
}

/*
 * return the directory part of the path (everything up to the final /)
 */
extern char *dir_get_path(const char *path)
{
    msg("%s", __func__);
    return strndup(path, strrchr(path, '/') - path);
}

