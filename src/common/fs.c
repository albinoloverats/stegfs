/*
 * Common code for dealing with file system functions
 * Copyright © 2009-2013, albinoloverats ~ Software Development
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef __APPLE__
#include "common/common.h"
#include "common/logging.h"
#else
#include "common.h"
#include "logging.h"
#endif

#ifdef _WIN32
    #include "common/win32_ext.h"
#endif

/*
 * Taken from http://nion.modprobe.de/tmp/mkdir.c
 */
extern void recursive_mkdir(const char *path, mode_t mode)
{
    char *opath = strdup(path);
    size_t len = strlen(opath);
    if (opath[len - 1] == '/')
        opath[len - 1] = '\0';
    for (char *p = opath; *p; p++)
        if (*p == '/')
        {
            *p = '\0';
            if (access(opath, F_OK))
            {
                log_message(LOG_EVERYTHING, _("Creating directory : %s"), opath);
                mkdir(opath, mode);
            }
            *p = '/';
        }
    if (access(opath, F_OK)) /* if path is not terminated with / */
    {
        log_message(LOG_EVERYTHING, _("Creating directory : %s"), opath);
        mkdir(opath, mode);
    }
    free(opath);
    return;
}
