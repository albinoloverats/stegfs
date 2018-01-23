/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2018, albinoloverats ~ Software Development
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
#include "common/fs.h"

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
				recursive_mkdir(new, S_IRUSR | S_IWUSR | S_IXUSR);
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
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *in = strdup(argv[1]);
	char *to = strdup(argv[2]);

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
