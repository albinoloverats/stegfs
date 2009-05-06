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

extern void init(const char *a, const char *v)
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
    setlocale(LC_ALL, "");
    bindtextdomain(c_app, "/usr/share/locale");
    textdomain(c_app);
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
        for (uint8_t i = 0; i < strlen(e); i++)
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
