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

void init(const char *app, const char *ver)
{
    errno = 0;
    if (c_app)
        return;
    c_app = strdup(app);
    c_ver = strdup(ver);
    if ((signal(SIGTERM, sigint) == SIG_ERR) || (signal(SIGINT, sigint) == SIG_ERR) || (signal(SIGQUIT, sigint) == SIG_ERR))
        die("could not set signal handler");
}

int64_t show_licence(void)
{
    fprintf(stderr, "This program is free software: you can redistribute it and/or modify\n");
    fprintf(stderr, "it under the terms of the GNU General Public License as published by\n");
    fprintf(stderr, "the Free Software Foundation, either version 3 of the License, or\n");
    fprintf(stderr, "(at your option) any later version.\n\n");
    fprintf(stderr, "This program is distributed in the hope that it will be useful,\n");
    fprintf(stderr, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    fprintf(stderr, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    fprintf(stderr, "GNU General Public License for more details.\n\n");
    fprintf(stderr, "You should have received a copy of the GNU General Public License\n");
    fprintf(stderr, "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
    return EXIT_SUCCESS;
}

int64_t show_usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [OPTION] [ARGUMENT] ...\n", c_app);
    return EXIT_SUCCESS;
}

int64_t show_version(void)
{
    fprintf(stderr, "%s version : %s\n%*s built on: %s %s\n", c_app, c_ver, (int)strlen(c_app), "", __DATE__, __TIME__);
    return EXIT_SUCCESS;
}

void hex(void *v, uint64_t l)
{
    uint8_t *s = v;
    char b[72] = { 0x00 };
    uint8_t c = 1;
    for (uint64_t i = 0; i < l; i++, c++)
    {
        if (c > 24)
        {
            c = 1;
            msg(b);
            memset(b, 0x00, sizeof( b ));
        }
        sprintf(b, "%s%02X%s", b, s[i], (c % 4) ? "" : " ");
    }
    msg(b);
}

void msg(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    fprintf(stderr, "\r%s: ", c_app);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
	fflush(stderr);
    va_end(ap);
}

void die(const char *s, ...)
{
    if (s)
        msg(s);
    if (errno)
    {
        char *e = strdup(strerror(errno));
        for (uint8_t i = 0; i < strlen(e); i++)
            e[i] = tolower(e[i]);
        msg("%s", e);
        free(e);
    }
    exit(errno);
}

void sigint(int s)
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
    }
    msg("caught and ignoring signal %s", ss);
    msg("try again once more to force quit");
    c_sig = true;
}

