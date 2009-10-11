/*
 * Common code shared between projects
 * Copyright (c) 2009, albinoloverats ~ Software Development
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

#include "common/common.h"

static bool  c_sig = false;
static char *c_app = NULL;
static char *c_ver = NULL;

extern void init(const char * const restrict a, const char * const restrict v)
{
    errno = 0;
    if (c_app)
        return;
    c_app = strdup(a);
    c_ver = strdup(v);
    if ((signal(SIGTERM, sigint) == SIG_ERR) || (signal(SIGINT, sigint) == SIG_ERR) || (signal(SIGQUIT, sigint) == SIG_ERR))
        die(_("could not set signal handler"));
    /*
     * set locale
     */
#if 0
    setlocale(LC_ALL, "");
    bindtextdomain(c_app, "/usr/share/locale");
    textdomain(c_app);
#endif
}

extern conf_t **config(const char * const restrict f)
{
    FILE *file = fopen(f, "r");
    if (!file)
    {
        msg(_("could not open configuration file %s"), f);
        return NULL;
    }
    conf_t **x  = malloc(sizeof(conf_t *));
    x[0] = NULL;
    size_t i = 0;

    char  *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) != -1)
    {
        if (line[0] != '#' && line[0] != '\n')
        {
            char *l = strdup(line);
            char *o = strtok(l, " \t\n\r#");
            char *v = strtok(NULL, "\t\n\r#"); /* don't delimit by space this time */

            if (o && v && strlen(o) && strlen(v))
            {
                if (!(x = realloc(x, (i + 2) * sizeof( conf_t * ))))
                    die(_("out of memory @ %s:%i"), __FILE__, __LINE__ - 1);
                if (!(x[i] = malloc(sizeof( conf_t ))))
                    die(_("out of memory @ %s:%i"), __FILE__, __LINE__ - 1);

                if (!(x[i]->option = strdup(o)))
                    die(_("out of memory @ %s:%i"), __FILE__, __LINE__ - 1);
                if (!(x[i]->value  = strdup(v)))
                    die(_("out of memory @ %s:%i"), __FILE__, __LINE__ - 1);

                x[++i] = NULL;
            }
            free(l);
        }

        free(line);
        line = NULL;
        size = 0;
    }
    fclose(file);

    return x;
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
    uint8_t *s = v;
    char b[HEX_LINE_WIDTH] = { 0x00 };
    uint8_t c = 1;
    for (uint64_t i = 0; i < l; i++, c++)
    {
        if (c > HEX_LINE_WRAP)
        {
            c = 1;
            msg(b);
            memset(b, 0x00, sizeof( b ));
        }
        sprintf(b, "%s%02X%s", b, s[i], (c % sizeof( uint32_t )) ? "" : " ");
    }
    msg(b);
}

extern void msg(const char *s, ...)
{
    if (!s)
        return;
    va_list ap;
    va_start(ap, s);
    fprintf(stderr, "\r%s: ", c_app);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

extern void die(const char *s, ...)
{
    if (s)
        msg(s);
    if (errno)
    {
        char *e = strdup(strerror(errno));
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
    char *ss = NULL;
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

extern void wait(uint32_t s)
{
    div_t a = div(s, 1000);
    struct timespec t = { a.quot, a.rem * 10 };
    struct timespec r = { 0, 0 };
    nanosleep(&t, &r);
}

extern ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    ssize_t r = 0;
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

    return r;
}
