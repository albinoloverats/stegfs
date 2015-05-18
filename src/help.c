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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "common/common.h"

#include "stegfs.h"
#include "help.h"

static bool is_stegfs(void)
{
    return true;
}

static void print_version(void)
{
    char *git = strndup(GIT_COMMIT, GIT_COMMIT_LENGTH);
    fprintf(stderr, _("%s version: %s\n%*s built on: %s %s\n%*s git commit: %s\n"), STEGFS_NAME, STEGFS_VERSION, (int)strlen(STEGFS_NAME) - 1, "", __DATE__, __TIME__, (int)strlen(STEGFS_NAME) - 3, "", git);
    free(git);
    return;
}

static void print_usage(void)
{
    char *app_usage = is_stegfs() ? STEGFS_USAGE : MKFS_USAGE;
    fprintf(stderr, _("Usage:\n  %s %s\n"), STEGFS_NAME, app_usage);
    return;
}

extern void show_help(void)
{
    print_version();
    fprintf(stderr, "\n");
    print_usage();
    fprintf(stderr, "\n");
    fprintf(stderr, _("Options:\n"));
    fprintf(stderr, _("  -h, --help                   Display this message\n"));
    fprintf(stderr, _("  -l, --licence                Display GNU GPL v3 licence header\n"));
    fprintf(stderr, _("  -v, --version                Display application version\n"));
    fprintf(stderr, _("  -x                           Enable extra paranoid node\n"));
    fprintf(stderr, _("\nNotes:\n  • It doesn't matter which order the file system and mount point are specified\n"));
    fprintf(stderr, _("    as stegfs will figure that out. All other options are passed to FUSE.\n"));
    fprintf(stderr, _("  • The FUSE option -s (to use a single thread) is (currently) forced by stegfs.\n"));
    fprintf(stderr, _("  • If you’re feeling extra paranoid you can now disable to stegfs file\n"));
    fprintf(stderr, _("    system header. This will also disable the checks when mounting and then\n"));
    fprintf(stderr, _("    anything could happen ;-)\n"));
    fprintf(stderr, "\n");
    return;
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
