/*
 * Common code shared between projects
 * Copyright (c) 2009-2011, albinoloverats ~ Software Development
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

#include "common.h"

#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

static bool c_sig = false;
/*@null@*/static char *c_app = NULL;
/*@null@*/static char *c_ver = NULL;
/*@null@*/static FILE *LOG_FILE = NULL;

extern void init3(const char * const restrict a, const char * const restrict v, const char * const restrict f)
{
    errno = 0;
    if (c_app)
        return;
    /*@i1@*/c_app = strdup(a);
    /*@i1@*/c_ver = strdup(v);
    if ((signal(SIGTERM, sigint) == SIG_ERR) || (signal(SIGINT, sigint) == SIG_ERR) || (signal(SIGQUIT, sigint) == SIG_ERR))
        /*@i1@*/die(_("could not set signal handler"));
#if 0
    setlocale(LC_ALL, "");
    bindtextdomain(c_app, "/usr/share/locale");
    textdomain(c_app);
#endif
    /*
     * log to a file if we can, else revert to stderr
     */
    if (f)
        redirect_log(f);
    if (!LOG_FILE)
        LOG_FILE = stderr;
    return;
}

extern void redirect_log(const char * const restrict f)
{
    /*
     * log to a file if we can, else revert to stderr
     */
    if (f)
        LOG_FILE = fopen(f, "a");
    if (!LOG_FILE)
        LOG_FILE = stderr;
}

/*@null@*/extern list_t *config(const char * const restrict f)
{
    FILE *file = fopen(f, "r");
    if (!file)
    {
        msg(_("could not open configuration file %s"), f);
        return NULL;
    }
 
    list_t *list = list_create(NULL);
    char  *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) >= 0)
    {
        if (line[0] != '#' && line[0] != '\n')
        {
            conf_t *c = calloc(1, sizeof( conf_t * ));
            char *l = strdup(line);
            c->option = strdup(strsep(&l, " \t\n\r#"));
            c->value = strdup(strsep(&l, "\n\r#")); /* don't delimit by space this time, only end of line or start of comment */
            list_append(&list, c);
        }
    }
    free(line);
    fclose(file);
    return list;
}

extern int64_t show_licence(void)
{
    fprintf(stderr, _(TEXT_LICENCE));
    return EXIT_SUCCESS;
}

extern int64_t show_usage(void)
{
    fprintf(stderr, _("Usage:\n"));
    fprintf(stderr, _("  %s [OPTION] [ARGUMENT] ...\n"), c_app);
    return EXIT_SUCCESS;
}

extern int64_t show_version(void)
{
    fprintf(stderr, _("%s version : %s\n%*s built on: %s %s\n"), c_app, c_ver, (int)strlen(c_app), "", __DATE__, __TIME__);
    return EXIT_SUCCESS;
}

extern void hex(void *v, uint64_t l)
{
    const uint8_t * const s = v;
    char b[HEX_LINE_WIDTH] = { 0x00 };
    uint8_t c = 1;
    for (uint64_t i = 0; i < l; i++, c++)
    {
        if (c > HEX_LINE_WRAP)
        {
            c = 1;
            msg("%s", b);
            memset(b, 0x00, sizeof( b ));
        }
        sprintf(b, "%s%02X%s", b, s[i], (c % sizeof( uint32_t )) ? "" : " ");
    }
    msg("%s", b);
}

extern void msg(const char * const restrict s, ...)
{
    if (!s)
    {
        if (errno)
        {
            char * const restrict e = strdup(strerror(errno));
            for (uint32_t i = 0; i < strlen(e); i++)
                e[i] = tolower(e[i]);
            msg("%s", e);
            free(e);
        }
        return;
    }
    va_list ap;
    va_start(ap, s);
    flockfile(LOG_FILE);
    fprintf(LOG_FILE, "\r%s: ", c_app);
    vfprintf(LOG_FILE, s, ap);
    fprintf(LOG_FILE, "\n");
    fflush(LOG_FILE);
    funlockfile(LOG_FILE);
    va_end(ap);
}

extern void die(const char * const restrict s, ...)
{
    if (s)
        msg("%s", s);
    if (errno)
    {
        char * const restrict e = strdup(strerror(errno));
        for (uint32_t i = 0; i < strlen(e); i++)
            e[i] = tolower(e[i]);
        msg("%s", e);
        free(e);
    }
    exit(errno);
}

extern void sigint(int s)
{
    if (c_sig)
    {
        errno = ECANCELED;
        die(NULL);
    }
    char * restrict ss = NULL;
    switch (s)
    {
        case SIGTERM:
            ss = strdup("SIGTERM");
            break;
        case SIGINT:
            ss = strdup("SIGINT");
            break;
        case SIGQUIT:
            ss = strdup("SIGQUIT");
            break;
        default:
            ss = strdup(_("UNKNOWN"));
    }
    msg(_("caught and ignoring %s signal"), ss);
    msg(_("try again once more to force quit"));
    free(ss);
    c_sig = true;
}

extern void chill(uint32_t s)
{
    div_t a = div(s, 1000);
    struct timespec t = { a.quot, a.rem * 10 };
    struct timespec r = { 0, 0 };
    nanosleep(&t, &r);
}

extern ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    size_t r = 0;
    uint32_t step = 0xFF;
    char *buffer = malloc(step);
    for (r = 0; ; r++)
    {
        int c = fgetc(stream);
        if (c == EOF)
            break;
        buffer[r] = c;
        if (c == '\n')
            break;
        if (r >= step - 0x10)
        {
            step += 0xFF;
            buffer = realloc(buffer, step);
        }
    }
    if (*lineptr)
        free(*lineptr);
    *lineptr = buffer;
    *n = r;
    return r;
}
