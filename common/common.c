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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#ifdef WIN32
char *program_invocation_short_name = NULL;
#endif

static bool c_sig = false;
/*@null@*/static char *c_app = NULL;
/*@null@*/static char *c_ver = NULL;

extern list_t *init7(const char * const restrict a, const char * const restrict v, const char * const restrict u, char **g, const char * const restrict c, list_t *o, const char * const restrict m)
{
    errno = 0;
    if (c_app)
        return NULL;
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
     * TODO move command line and config file parsing into own component
     */
    list_t *z = NULL;
    if (g)
    {
        /*
         * setup some default command line arguments
         */
        args_t help     = {'h', "help",      false, false, NULL, "Show this help screen"};
        args_t version  = {'v', "version",   false, false, NULL, "Show application verison"};
        args_t licence  = {'l', "licence",   false, false, NULL, "Show GNU GPL v3 licence header"};
        args_t debug    = {'d', "debug",     false, true,  NULL, "Show debugging messages during execution"};
        args_t quiet    = {'q', "quiet",     false, false, NULL, "Hide standard messages during execution"};
        args_t conf     = {'c', "config",    false, true,  NULL, "Use given config file instead of default"};
        list_t *d = list_create(NULL);
        list_append(&d, &licence);
        list_append(&d, &version);
        list_append(&d, &help);
        list_append(&d, &debug);
        list_append(&d, &quiet);
        if (c)
            list_append(&d, &conf);
        if (o)
            list_join(d, o);
        /*
         * read config file before command line arguments - command line overrides file
         */
        z = parse_args(g, d);
        list_t *y = list_create(NULL);
        if (c)
        {
            y = parse_config(c, d);
            /*
             * reparse command line arguments as they should always override config file settings
             */
            parse_args(g, d);
        }
        list_join(z, y);
        if (licence.found)
            show_licence();
        if (version.found)
            show_version();
        if (help.found)
            show_help(u, o, m);
        if (debug.found && quiet.found)
            die("please use either debug or quiet not both");
        log_e l = LOG_DEFAULT;
        if (debug.found)
        {
            if (debug.option)
                l = log_parse_level(debug.option);
        }
        else if (quiet.found)
            l = LOG_ERROR;
        log_relevel(l);
    }
    return z;
}

extern list_t *parse_args(char **v, list_t *a)
{
    list_t *x = list_create(NULL);
    uint16_t expected = list_size(a);
    for (uint16_t i = 1; ; i++) 
    {
        if (!v[i])
            break;
        bool found = false;
        for (uint16_t j = 0; j < expected && !found; j++)
        {
            args_t *arg = list_get(a, j);
            if (strlen(v[i]) == 2)
            {
                if (*(v[i]) == '-' && *(v[i] + 1) == arg->short_option)
                    found = true;
            }
            else
            {
                if (!strncmp(v[i], "--", 2) && !strcmp(v[i] + 2, arg->long_option))
                    found = true;
            }
            if (found)
            {
                if (arg->has_option && v[i + 1] && strncmp(v[i + 1], "-", 1))
                {
                    i++;
                    asprintf(&arg->option, v[i]);
                }
                arg->found = true;
            }
        }
        if (!found)
            list_append(&x, v[i]);
    }
    return x;
}

/*@null@*/extern list_t *parse_config(const char * const restrict f, list_t *a)
{
    FILE *file = fopen(f, "r");
    if (!file)
    {
        log_message(LOG_WARNING, _("could not open configuration file %s"), f);
        return NULL;
    }
 
    uint16_t expected = list_size(a);
    list_t *x = list_create(NULL);
    char  *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) >= 0)
    {
        if (line[0] != '#' && line[0] != '\n')
        {
            char *l = strdup(line);
            char *p = l;
            char *o = strdup(strtok(l, " \t\n\r#"));
            char *v = strdup(strtok(NULL, "\n\r#")); /* don't delimit by space this time, only end of line or start of comment */

            for (uint16_t j = 0; j < expected; j++)
            {
                args_t *arg = list_get(a, j);
                if (!strcmp(arg->long_option, o))
                {
                    arg->found = true;
                    asprintf(&arg->option, v);
                    break;
                }
            }
            free(o);
            free(v);
            free(p);
        }
    }
    free(line);
    fclose(file);
    return x;
}

extern void show_licence(void)
{
    fprintf(stderr, _(TEXT_LICENCE));
    exit(EXIT_SUCCESS);
}

extern void show_usage(const char * const restrict u)
{
    fprintf(stderr, _("Usage:\n"));
    fprintf(stderr, _("  %s %s\n"), c_app, u ? : "[OPTION] ...");
    exit(EXIT_SUCCESS);
}

extern void show_version(void)
{
    fprintf(stderr, _("%s version : %s\n%*s built on: %s %s\n"), c_app, c_ver, (int)strlen(c_app), "", __DATE__, __TIME__);
    exit(EXIT_SUCCESS);
}

extern void show_help(const char * const restrict u, list_t *l, const char * const restrict m)
{
    fprintf(stderr, _("%s version : %s\n%*s built on: %s %s\n"), c_app, c_ver, (int)strlen(c_app), "", __DATE__, __TIME__);
    fprintf(stderr, _("Usage:\n"));
    fprintf(stderr, _("  %s %s...\n"), c_app, u ? : "[OPTION] ...");
    fprintf(stderr, "\nOptions:\n\n");
    int lm = list_size(l);
    int w = 0;
    for (int i = 0; i < lm; i++)
    {
        int n = strlen(((args_t *)list_get(l, i))->long_option);
        w = n > w ? n : w;
    }
    for (int i = 0; i < lm; i++)
    {
        args_t *arg = list_get(l, i);
        fprintf(stderr, "  -%c, --%-*s  %s  %s\n", arg->short_option, w, arg->long_option, arg->has_option ? "*" : " ", arg->message);
    }
    fprintf(stderr, "\n  * Denotes mandatory argument\n");
    if (m)
        fprintf(stderr, "\n%s\n", m);
    exit(EXIT_SUCCESS);
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
    log_message(LOG_WARNING, _("caught and ignoring %s signal"), ss);
    log_message(LOG_WARNING, _("try again once more to force quit"));
    free(ss);
    c_sig = true;
}

extern void die(const char * const restrict s, ...)
{
    if (s)
    {
        char *d = NULL;
        va_list ap;
        va_start(ap, s);
#ifndef _WIN32
        vasprintf(&d, s, ap);
        log_message(LOG_FATAL, d);
#else
        uint8_t l = 0xFF;
        d = calloc(l, sizeof( uint8_t ));
        if (d)
            vsnprintf(d, l - 1, s, ap);
        log_message(LOG_FATAL, d);
        if (d)
            free(d);
#endif
        va_end(ap);
    }
    if (errno)
    {
        char * const restrict e = strdup(strerror(errno));
        for (uint32_t i = 0; i < strlen(e); i++)
            e[i] = tolower(e[i]);
        log_message(LOG_FATAL, "%s", e);
        free(e);
    }
    /*
     * TODO if running a GUI don't necessarily exit with alerting the user first
     * Users seem to dislike applications just quitting for no apparent reason!
     */
    exit(errno);
}

extern void chill(uint32_t m)
{
#ifndef _WIN32
    div_t a = div(m, 1000);
    struct timespec t = {a.quot, a.rem * TEN_MILLION};
    struct timespec r = {0, 0};
    do
        nanosleep(&t, &r);
    while (r.tv_sec > 0 && r.tv_nsec > 0);
#endif
}

#if !defined(_GNU_SOURCE) || defined(_WIN32)
extern ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    size_t r = 0;
    uint32_t step = 0xFF;
    char *buffer = malloc(step);
    if (!buffer)
        die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, step);
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
            if (!(buffer = realloc(buffer, step)))
                die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, step);
        }
    }
    if (*lineptr)
        free(*lineptr);
    *lineptr = buffer;
    *n = r;
    return r;
}
#endif

#ifdef _WIN32
extern ssize_t pread(int filedes, void *buffer, size_t size, off_t offset)
{
    off_t o = lseek(filedes, 0, SEEK_CUR);
    lseek(filedes, offset, SEEK_SET);
    ssize_t s = read(filedes, buffer, size);
    lseek(filedes, o, SEEK_SET);
    return s;
}

extern ssize_t pwrite(int filedes, const void *buffer, size_t size, off_t offset)
{
    off_t o = lseek(filedes, 0, SEEK_CUR);
    lseek(filedes, offset, SEEK_SET);
    ssize_t s = write(filedes, buffer, size);
    lseek(filedes, o, SEEK_SET);
    return s;
}

int asprintf(char **buffer, char *fmt, ...)
{
    /* guess we need no more than 200 chars of space */
    int size = 200;
    int nchars;
    va_list ap;
    
    if (!(*buffer = (char *)malloc(size)))
        die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, size);
          
    va_start(ap, fmt);
    nchars = vsnprintf(*buffer, size, fmt, ap);
    va_end(ap);

    if (nchars >= size)
    {
        char *tmpbuff;
        size = nchars + 1;
        if (!(tmpbuff = (char *)realloc(*buffer, size)))
            die("out of memory @ %s:%d:%s [%d]", __FILE__, __LINE__, __func__, size);

        *buffer = tmpbuff;

        va_start(ap, fmt);
        nchars = vsnprintf(*buffer, size, fmt, ap);
        va_end(ap);
    }
    if (nchars < 0)
        return nchars;
    return size;
}
#endif
