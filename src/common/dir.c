/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2022, albinoloverats ~ Software Development
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "non-gnu.h"
#include "common.h"
#include "error.h"
#include "dir.h"
#include "list.h"

extern char *dir_get_name_aux(const char * const path, const char ext)
{
	const char *file = strrchr(path, DIR_SEPARATOR_CHAR);
	if (file)
		file++;
	else
		file = path;
	const char *pass = strrchr(path, ext);
	if (!pass)
		pass = strchr(file, '\0');
	return strndup(file, pass - file);
}

extern uint16_t dir_get_deep(const char * const path)
{
	uint16_t d = 0;
	char *ptr = (char *)path;
	while ((ptr = strchr(ptr, DIR_SEPARATOR_CHAR)))
	{
		d++;
		ptr++;
	}
	return d;
}

extern char *dir_get_part(const char * const path, const uint16_t index)
{
	if (!index)
		return strdup(DIR_SEPARATOR);
	char *ptr = (char *)path;
	for (uint16_t i = 0; i < index; i++)
	{
		ptr = strchr(ptr, DIR_SEPARATOR_CHAR);
		if (!ptr)
			break;
		ptr++;
	}
	return strndup(ptr, strchrnul(ptr, DIR_SEPARATOR_CHAR) - ptr);
}

extern char *dir_get_pass(const char * const restrict path)
{
	if (!strrchr(path, ':'))
		return strdup("");
	return strdup(strrchr(path, ':') + 1);
}

extern char *dir_get_path(const char * const restrict path)
{
	char *s = strrchr(path, DIR_SEPARATOR_CHAR);
	if (!s)
		return strdup("");
	char *p = strndup(path, s - path);
	if (strlen(p))
		return p;
	free(p);
	return strdup(DIR_SEPARATOR);
}

/*
 * Taken from http://nion.modprobe.de/tmp/mkdir.c
 */
extern void dir_mk_recursive(const char *path, mode_t mode)
{
#ifdef _WIN32
	(void)mode;
#endif
	char *opath = strdup(path);
	size_t len = strlen(opath);
	if (opath[len - 1] == '/')
		opath[len - 1] = '\0';
	for (char *p = opath; *p; p++)
		if (*p == '/')
		{
			*p = '\0';
			if (access(opath, F_OK))
				mkdir(opath, mode);
			*p = '/';
		}
	if (access(opath, F_OK)) /* if path is not terminated with / */
		mkdir(opath, mode);
	free(opath);
	return;
}

static void get_tree(LIST l, const char *path, dir_type_e type)
{
	struct dirent **eps = NULL;
	int n = 0;
	if ((n = scandir(path, &eps, NULL, alphasort)))
		for (int i = 0; i < n; i++)
		{
			if (!strcmp(".", eps[i]->d_name) || !strcmp("..", eps[i]->d_name))
				continue;
			char *full_path = NULL;
			if (!asprintf(&full_path, "%s/%s", path, eps[i]->d_name))
				die(_("Out of memory @ %s:%d:%s [%" PRIu64 "]"), __FILE__, __LINE__, __func__, strlen(path) + strlen(eps[i]->d_name) + 2);
			bool add = false;
			bool dir = false;
			/*
			 * eps[i]->d_type doesn't always give a proper
			 * answer, so we've gotta stat
			 */
			struct stat s;
			lstat(full_path, &s);
			switch (s.st_mode & S_IFMT)
			{
				case S_IFDIR:
					add = type & DIR_FOLDER;
					dir = true;
					break;
				case S_IFCHR:
					add = type & DIR_CHAR;
					break;
				case S_IFBLK:
					add = type & DIR_BLOCK;
					break;
				case S_IFREG:
					add = type & DIR_FILE;
					break;
				case S_IFLNK:
					add = type & DIR_LINK;
					break;
				case S_IFSOCK:
					add = type & DIR_SOCKET;
					break;
				case S_IFIFO:
					add = type & DIR_PIPE;
					break;
			}
			if (add)
				list_add(l, strdup(full_path));
			if (dir)
				get_tree(l, full_path, type);
			free(full_path);
		}
	for (int i = 0; i < n; i++)
		free(eps[i]);
	free(eps);
	return;
}

extern LIST dir_get_tree(const char *path, dir_type_e type)
{
	LIST l = list_string();
	get_tree(l, path, type);
	return l;
}
