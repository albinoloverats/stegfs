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

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "common/error.h"
#include "common/fs.h"

static char *stegify_path(const char *path)
{
    char *fixed = strdup(path);
    char *ptr = fixed;
    size_t len = strlen(fixed);
    int c = 1;
    for (size_t i = 0; i < len; i++)
        if ((ptr = strchr(ptr, '/')))
            ptr += 1, c++;
        else
            break;
    len += c + 1;
    ptr = realloc(fixed, len);
    if (!ptr)
        return NULL;
    fixed = ptr;
    memset(fixed, 0x00, len);
    for (size_t i = 0, j = 0; i < strlen(path); i++, j++)
    {
        fixed[j] = path[i];
        if (path[i] == '/')
            fixed[(++j)] = '+';
    }
    return fixed;
}

static void copy_file(const char *source, const char *dest)
{
    int64_t f = open(source, O_RDONLY | F_RDLCK, S_IRUSR | S_IWUSR);
    if (f < 0)
    {
        perror(NULL);
        return;
    }
    int64_t g = open(dest, O_CREAT | O_TRUNC | F_WRLCK | O_WRONLY, S_IRUSR | S_IWUSR);
    if (g < 0)
    {
        close(f);
        perror(NULL);
        return;
    }
    int64_t r;
    char b[4098];
    while ((r = read(f, b, sizeof b)) > 0)
        write(g, b, r);
    close(g);
    close(f);
    return;
}

static void recurse_tree(const char *dir, const char *dest, const char **display)
{
    struct dirent **eps = NULL;
    int n = 0;
    if ((n = scandir(dir, &eps, NULL, NULL)))
    {
        for (int i = 0; i < n; ++i)
        {
            if (!strcmp(".", eps[i]->d_name) || !strcmp("..", eps[i]->d_name))
                continue;
            char *filename = NULL;
            if (!asprintf(&filename, "%s/%s", dir, eps[i]->d_name))
                die("Out of memory @ %s:%d:%s", __FILE__, __LINE__, __func__);
            char *replaced = stegify_path(dir);
            struct stat s;
            stat(filename, &s);
            printf("'%s/%s' -> '%s/+%s/%s%s'\n", display[0], filename, display[1], replaced, S_ISDIR(s.st_mode) ? "+" : "", eps[i]->d_name);
            char *new = NULL;
            if (!asprintf(&new, "%s/+%s/%s%s", dest, replaced, S_ISDIR(s.st_mode) ? "+" : "", eps[i]->d_name))
                die("Out of memory @ %s:%d:%s", __FILE__, __LINE__, __func__);
            switch (s.st_mode & S_IFMT)
            {
                case S_IFDIR:
                    recursive_mkdir(new, S_IWUSR | S_IRUSR | S_IXUSR);
                    recurse_tree(filename, dest, display);
                    break;
                case S_IFREG:
                    copy_file(filename, new);
                    break;
                default:
                    break;
            }
            free(replaced);
            free(filename);
            free(new);
        }
    }
    for (int i = 0; i < n; ++i)
        free(eps[i]);
    free(eps);
    return;
}



int main(int argc, char **argv)
{
    if (argc < 3 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
    {
        fprintf(stderr, "Usage: %s <source ...> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc - 1; i++)
    {
        char *cwd = NULL;
        printf("%c\n", argv[i][strlen(argv[i]) - 1]);
        if (argv[i][strlen(argv[i]) - 1] == '/')
            argv[i][strlen(argv[i]) - 1] = '\0';
        char *dir = strrchr(argv[i], '/');
        char *source = argv[i];
        if (dir)
        {
            *dir = '\0';
            dir++;
            cwd = getcwd(NULL, 0);
            chdir(argv[i]);
            source = NULL;
            if (!(asprintf(&source, "%s", dir)))
                die("Out of memory @ %s:%d:%s", __FILE__, __LINE__, __func__);
        }
        char *dest = NULL;
        asprintf(&dest, "%s%s%s", cwd ?: "", cwd ? "/" : "", argv[argc - 1]);
        const char *display[] = { argv[i], argv[argc - 1] };
        recurse_tree(source, dest, display);
        if (cwd)
        {
            chdir(cwd);
            free(cwd);
        }
        free(dest);
    }

    return EXIT_SUCCESS;
}
