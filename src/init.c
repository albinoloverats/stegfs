/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2015, albinoloverats ~ Software Development
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include "common/common.h"
#include "common/error.h"
#include "common/ccrypt.h"

#include "stegfs.h"
#include "init.h"

static void print_version(void);
static void print_usage(void);
static void print_help(void);

static char *extract_long_option(char *);

extern args_t init(int argc, char **argv, char **fuse)
{
	args_t a = { NULL, NULL, DEFAULT_CIPHER, DEFAULT_HASH, DEFAULT_MODE, DEFAULT_COPIES, 0, false, false, false, false, false };
	/*
	 * parse commandline arguments
	 */
	for (int i = 1, j = 1; i < argc; i++)
	{
		if (!strcmp("--version", argv[i]) || !strcmp("-v", argv[i]))
			show_version();
		else if (!strcmp("--licence", argv[i]) || !strcmp("-l", argv[i]))
			show_licence();
		else if (!strcmp("--help", argv[i]) || !strcmp("-h", argv[i]))
		{
			if (is_stegfs())
				print_help();
			else
				show_help();
			a.help = true;
			if (fuse)
			{
				fuse[j] = "-h";
				j++;
			}
		}
		else if (!strcmp("--cipher", argv[i]) || !strcmp("-c", argv[i]))
		{
			if (argv[i][1] == '-')
			{
				char *x = extract_long_option(argv[i]);
				a.cipher = cipher_id_from_name(x);
				free(x);
			}
			else
				a.cipher = cipher_id_from_name(argv[(++i)]);
		}
		else if (!strcmp("--hash", argv[i]) || !strcmp("-h", argv[i]))
		{
			if (argv[i][1] == '-')
			{
				char *x = extract_long_option(argv[i]);
				a.hash = hash_id_from_name(x);
				free(x);
			}
			else
				a.hash = hash_id_from_name(argv[(++i)]);
		}
		else if (!strcmp("--mode", argv[i]) || !strcmp("-m", argv[i]))
		{
			if (argv[i][1] == '-')
			{
				char *x = extract_long_option(argv[i]);
				a.mode = mode_id_from_name(x);
				free(x);
			}
			else
				a.mode = mode_id_from_name(argv[(++i)]);
		}
		else if (!strcmp("--paranoid", argv[i]) || !strcmp("-p", argv[i]))
			a.paranoid = true;
		else if (!strcmp("--duplicates", argv[i]) || !strcmp("-x", argv[i]))
			a.duplicates = true;
		else if (!is_stegfs() && (!strcmp("--size", argv[i]) || !strcmp("-z", argv[i])))
		{
			char *s = NULL;
			if (argv[i][1] == '-')
				s = argv[i][0] + strchr(argv[i], '=') + sizeof(char);
			else
				s = argv[(++i)];
			/*
			 * parse the value for our file system size (if using a file on an
			 * existing file system) allowing for suffixes: MB, GB, TB, PB and
			 * EB - sizes less than 1MB or greater than an Exbibyte are not yet
			 * supported
			 */
			char *f;
			a.size = strtol(s, &f, 0);
			switch (toupper(f[0]))
			{
				case 'E':
					a.size *= KILOBYTE; /* Ratio between EB and PB (ditto below) */
				case 'P':
					a.size *= KILOBYTE;
				case 'T':
					a.size *= KILOBYTE;
				case 'G':
					a.size *= KILOBYTE;
				case 'M':
				case '\0':
					a.size *= MEGABYTE;
					break;
				default:
					die("unknown size suffix %c", f[0]);
			}
		}
		else if (!is_stegfs() && (!strcmp("--force", argv[i]) || !strcmp("-f", argv[i])))
			a.force = true;
		else if (!is_stegfs() && (!strcmp("--rewrite-sb", argv[i]) || !strcmp("-r", argv[i])))
			a.rewrite_sb = true;
		else if (!is_stegfs() && (!strcmp("--dry-run", argv[i]) || !strcmp("-d", argv[i])))
			a.dry_run = true;
		else
		{
			struct stat s;
			memset(&s, 0x00, sizeof s);
			stat(argv[i], &s);
			switch (s.st_mode & S_IFMT)
			{
				case S_IFBLK:
				case S_IFLNK:
				case S_IFREG:
					a.fs = strdup(argv[i]);
					if (is_stegfs())
						argv[i] = "-s";
					break;
				case S_IFDIR:
					a.mount = strdup(argv[i]);
					break;
				default:
					if (!is_stegfs())
						a.fs = strdup(argv[i]);
					else if (fuse)
					{
						fuse[j] = argv[i];
						j++;
					}
					break;
			}
		}
	}

	return a;
}

static void print_version(void)
{
	char *name = is_stegfs() ? STEGFS_NAME : MKFS_NAME;
	char *git = strndup(GIT_COMMIT, GIT_COMMIT_LENGTH);
	fprintf(stderr, _("%s version: %s\n%*s built on: %s %s\n%*s git commit: %s\n"), name, STEGFS_VERSION, (int)strlen(name) - 1, "", __DATE__, __TIME__, (int)strlen(name) - 3, "", git);
	free(git);
	return;
}

static void print_usage(void)
{
	char *name = is_stegfs() ? STEGFS_NAME : MKFS_NAME;
	char *usage = is_stegfs() ? STEGFS_USAGE : MKFS_USAGE;
	fprintf(stderr, _("Usage:\n  %s %s\n"), name, usage);
	return;
}

static void print_help(void)
{
	print_version();
	fprintf(stderr, "\n");
	print_usage();
	fprintf(stderr, "\n");
	fprintf(stderr, _("Options:\n"));
	fprintf(stderr, _("  -h, --help                 Display this message\n"));
	fprintf(stderr, _("  -l, --licence              Display GNU GPL v3 licence header\n"));
	fprintf(stderr, _("  -v, --version              Display application version\n"));
	fprintf(stderr, _("  -c, --cipher=<algorithm>   Algorithm to use to encrypt data\n"));
	fprintf(stderr, _("  -s, --hash=<algorithm>     Hash algorithm to generate key\n"));
	fprintf(stderr, _("  -m, --mode=<mode>          The encryption mode to use\n"));
	fprintf(stderr, _("  -p, --paranoid             Enable paranoia mode\n"));
	fprintf(stderr, _("  -x, --duplicates   Number of times each file should be duplicated\n"));
	if (!is_stegfs())
	{
		fprintf(stderr, _("  -z, --size=<size>  Desired file system size, required when creating\n"));
		fprintf(stderr, _("                     a file system in a normal file\n"));
		fprintf(stderr, _("  -f, --force        Force overwrite existing file, required when\n"));
		fprintf(stderr, _("                     overwriting a file system in a normal file\n"));
		fprintf(stderr, _("  -r, --rewrite-sb   Rewrite the superblock (perhaps it became corrupt)\n"));
		fprintf(stderr, _("  -d, --dry-run      Dry run - print details about the file system that\n"));
		fprintf(stderr, _("                     would have been created\n"));
	}
	fprintf(stderr, _("\nNotes:\n"));
	if (is_stegfs())
	{
		fprintf(stderr, _("  • It doesn't matter which order the file system and mount point are specified\n"));
		fprintf(stderr, _("    as stegfs will figure that out. All other options are passed to FUSE.\n"));
	}
	fprintf(stderr, _("  • If you’re feeling extra paranoid you can now disable to stegfs file\n"));
	fprintf(stderr, _("    system header. This will also disable the checks when mounting and thus\n"));
	fprintf(stderr, _("    anything could happen ;-)\n"));
	if (is_stegfs())
		fprintf(stderr, "\n");
	return;
}

extern void show_help(void)
{
	print_help();
	exit(EXIT_SUCCESS);
}

extern void show_licence(void)
{
	fprintf(stderr, _(TEXT_LICENCE));
	exit(EXIT_SUCCESS);
}

extern void show_usage(void)
{
	print_usage();
	exit(EXIT_SUCCESS);
}

extern void show_version(void)
{
	print_version();
	exit(EXIT_SUCCESS);
}

static char *extract_long_option(char *arg)
{
	char *e = strchr(arg, '=');
	if (!e)
		return NULL;
	e++;
	return strdup(e);
}
