/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2021, albinoloverats ~ Software Development
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
#include <unistd.h>

#include <string.h>

#include <sys/stat.h>
#include <dirent.h>

#include "common/common.h"
#include "common/error.h"
#include "common/dir.h"
#include "common/config.h"

#include "stegfs.h"

static void copy(const char *from_prefix, const char *from, const char *to_prefix, const char *to)
{
	struct dirent **eps = NULL;
	int n = 0;
	if ((n = scandir(from, &eps, NULL, NULL)))
	{
		for (int i = 0; i < n; ++i)
		{
			if (!strcmp(".", eps[i]->d_name) || !strcmp("..", eps[i]->d_name))
				continue;

			char *dir = NULL;
			if (!asprintf(&dir, "%s/%s", from, eps[i]->d_name))
				die(_("Out of memory @ %s:%d:%s"), __FILE__, __LINE__, __func__);

			struct stat s;
			lstat(dir, &s);
			if (S_ISDIR(s.st_mode))
			{
				printf("%s/%s --> %s/%s\n", from_prefix, dir, to, dir);
				char *new = NULL;
				asprintf(&new, "%s/%s/%s", to_prefix, to, dir);
				dir_mk_recursive(new, S_IRUSR | S_IWUSR | S_IXUSR);
				copy(from_prefix, dir, to_prefix, to);
			}
			free(dir);
		}
		/*
		 * no more files in this directory
		 */
	}
	for (int i = 0; i < n; ++i)
		free(eps[i]);
	free(eps);
	return;
}

int main(int argc, char **argv)
{
	char **extra = NULL;
	extra = calloc(2, sizeof (char *));
	extra[0] = strdup("+source");
	extra[0] = strdup("+destination");

	config_about_t about =
	{
		"stegfs-cp-tree",
		STEGFS_VERSION,
		PROJECT_URL,
		NULL
	};
	config_init(about);
	int e = config_parse(argc, argv, NULL, &extra, NULL);

	char *in = e > 0 ? extra[0] : NULL;
	char *to = e > 1 ? extra[1] : NULL;
	if (!in || !to)
	{
		char *x[] = { "+source", "+destination", NULL };
		config_show_usage(NULL, x);
		return EXIT_FAILURE;
	}

	char ps = in[strlen(in) - 1];
	if (ps == '/')
		ps = '\0';
	char *cwd = NULL;
	char *from = strrchr(in, '/');
	if (from)
	{
		*from = '\0';
		from++;
		cwd = getcwd(NULL, 0);
		chdir(in);
	}
	else
		from = in;

	copy(in, from, cwd, to);

	if (cwd)
		chdir(cwd);
	free(in);
	free(to);

	return EXIT_SUCCESS;
}
