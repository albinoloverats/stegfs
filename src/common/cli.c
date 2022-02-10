/*
 * Common code for providing a cmomand line progress bar
 * Copyright © 2005-2022, albinoloverats ~ Software Development
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include <signal.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <math.h>
#include <locale.h>

#ifndef _WIN32
	#include <termios.h>
	#include <sys/ioctl.h>
#endif

#include "common.h"
#include "cli.h"
#include "dir.h"
#include "non-gnu.h"

#define CLI_HELP_FORMAT_RIGHT_COLUMN 37

#define CLI_SMALL   62
#define CLI_DEFAULT 80 /* Expected default terminal width */
#define CLI_LARGE   75

static int cli_width = CLI_DEFAULT;
static int cli_size_width = 0;

static void cli_display_bars(cli_progress_t *, cli_progress_t *, cli_bps_t *);
#ifndef _WIN32
static void cli_sigwinch(int);
#endif

extern void on_quit(int) __attribute__((noreturn));

static void cli_init(void);
static int cli_inited = -1; // -1 (no), 0 (failed), 1 (okay)

static int cli_bps_sort(const void *, const void *);
static int cli_print(FILE *, char *);

extern int cli_printf(const char * const restrict s, ...)
{
	cli_init();
	va_list ap;
	va_start(ap, s);
	char *d = NULL;
	vasprintf(&d, s, ap);
	int x = cli_print(stdout, d);
	va_end(ap);
	free(d);
	return x;
}

extern int cli_eprintf(const char * const restrict s, ...)
{
	cli_init();
	va_list ap;
	va_start(ap, s);
	char *d = NULL;
	vasprintf(&d, s, ap);
	int x = cli_print(stderr, d);
	va_end(ap);
	free(d);
	return x;
}

extern int cli_fprintf(FILE *f, const char * const restrict s, ...)
{
	cli_init();
	va_list ap;
	va_start(ap, s);
	char *d = NULL;
	vasprintf(&d, s, ap);
	int x = cli_print(f, d);
	va_end(ap);
	free(d);
	return x;
}

extern int cli_printx(const uint8_t * const restrict x, size_t z)
{
	cli_init();
	return cli_fprintx(stdout, x, z);
}

extern int cli_eprintx(const uint8_t * const restrict x, size_t z)
{
	cli_init();
	return cli_fprintx(stderr, x, z);
}

extern int cli_fprintx(FILE *f, const uint8_t * const restrict x, size_t z)
{
	cli_init();
#define CLI_PRINTX_W 16
	// TODO allow variable width lines
	int l = 0, o = 0;
	char r[CLI_PRINTX_W * 2 + CLI_PRINTX_W] = { 0x0 };
	for (size_t i = 0; i < z; i++)
	{
		char c[4] = { 0x0 };
		sprintf(c, "%02x", x[i]);
		strcat(r, c);
		if (i % 2 == 1)
			strcat(r, " ");
		if (i % CLI_PRINTX_W == (CLI_PRINTX_W - 1))
		{
			l += cli_fprintf(f, "%08x: %s\n", o, r);
			memset(r, 0x00, sizeof r);
			o += CLI_PRINTX_W;
		}
	}
	if (strlen(r))
		l += cli_fprintf(f, "%08x: %s\n", o, r);
	return l;
}

extern double cli_calc_bps(cli_bps_t *bps)
{
	cli_init();
	cli_bps_t *copy = calloc(BPS, sizeof( cli_bps_t ));
	for (int i = 0; i < BPS; i++)
	{
		copy[i].time = bps[i].time;
		copy[i].bytes = bps[i].bytes;
	}
	qsort(copy, BPS, sizeof( cli_bps_t ), cli_bps_sort);
	double avg[BPS - 1] = { 0.0f };
	for (int i = 0; i < BPS - 1; i++)
		/*
		 * requires scale factor of MILLION as time is in microseconds
		 * not seconds (millions of bytes / micros of seconds, so to
		 * speak)
		 */
		avg[i] = MILLION * (double)(copy[i + 1].bytes - copy[i].bytes) / (double)(copy[i + 1].time - copy[i].time);
	double val = 0.0;
	for (int i = 0; i < BPS - 1; i++)
		val += avg[i];
	free(copy);
	return val /= (BPS - 1);
}

//#ifndef _WIN32
extern void cli_display(cli_t *p)
{
	cli_init();
#ifndef _WIN32
	cli_sigwinch(SIGWINCH);
#endif
	cli_size_width = 0;

	cli_bps_t bps[BPS];
	memset(bps, 0x00, BPS * sizeof( cli_bps_t ));
	int b = 0;

	fprintf(stderr, "\e[?25l\n"); /* hide cursor */
	while (*p->status == CLI_INIT || *p->status == CLI_RUN)
	{
		struct timespec s = { 0, 10 * MILLION }; /* 10 milliseconds */
		nanosleep(&s, NULL);
		if (*p->status == CLI_INIT)
			continue;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		bps[b].time = tv.tv_sec * MILLION + tv.tv_usec;
		bps[b].bytes = p->current->offset;
		b++;
		if (b >= BPS)
			b = 0;
		cli_display_bars(p->total, p->current, bps);
	}
	if (*p->status == CLI_DONE)
	{
		p->total->offset = p->total->size;
		p->current->offset = p->current->size;
		cli_display_bars(p->total, p->current, bps);
	}
	fprintf(stderr, "\e[?25h\n"); /* restore cursor */

	return;
}

static void cli_display_bar(char *name, cli_progress_t *p, double percent)
{
	int name_width = CLI_TRUNCATED_DISPLAY_SHORT + strlen(CLI_TRUNCATED_ELLIPSE) + (cli_width > CLI_DEFAULT ? CLI_TRUNCATED_DISPLAY_SHORT : 0);
	fprintf(stderr, "\r%-*s : ", name_width, name);
	if (cli_width > CLI_SMALL)
		fprintf(stderr, "%'13" PRIu64 "/%-'13" PRIu64 " (", p->offset, p->size);
	fprintf(stderr, "%3.0f%%", isnan(percent) ? 0.0f : (percent > PERCENT ? PERCENT : percent));
	if (cli_width > CLI_SMALL)
		fprintf(stderr, ")");

	if (cli_width > CLI_DEFAULT)
	{
		int progress_bar_width = cli_width - CLI_LARGE;
		fprintf(stderr, " [");
		/*
		 * display overall progress bar
		 */
		for (int i = 0; i < progress_bar_width; i++)
			fprintf(stderr, "%c", i < progress_bar_width * percent  / PERCENT ? '=' : ' ');
		fprintf(stderr, "]");
	}
	return;
}

static void cli_display_bps(cli_bps_t *bps)
{
	/*
	 * calculate B/s
	 */
	double val = cli_calc_bps(bps);
	if (isnan(val) || val == 0.0f)
		fprintf(stderr, "    0.0 B/s");
	else
	{
		if (val < THOUSAND)
			fprintf(stderr, "  %5.1f B/s", val);
		else if (val < MILLION)
			fprintf(stderr, " %5.1f KB/s", val / KILOBYTE);
		else if (val < THOUSAND_MILLION)
			fprintf(stderr, " %5.1f MB/s", val / MEGABYTE);
		else if (val < BILLION)
			fprintf(stderr, " %5.1f GB/s", val / GIGABYTE);
#if 1           /* if you’re getting these kinds of speeds please, please can I have your machine ;-) */
		else if (val < THOUSAND_BILLION)
			fprintf(stderr, " %5.1f TB/s", val / TERABYTE);
		else if (val < TRILLION)
			fprintf(stderr, " %5.1f PB/s", val / PETABYTE);
//		else // if (val < THOUSAND_TRILLION) // even uint64_t is too small now!
//			fprintf(progress_bar + strlen(progress_bar), " %5.1f EB/s", val / EXABYTE);
//		else if (val < QUADRILLION)
//			fprintf(progress_bar + strlen(progress_bar), " %5.1f ZB/s", val / ZETTABYTE);
//		else
//			fprintf(progress_bar + strlen(progress_bar), " %5.1f YB/s", val / YOTTABYTE);
#endif
		else
			fprintf(stderr, "  ---.- B/s");
	}
	return;
}

static void cli_display_bars(cli_progress_t *t, cli_progress_t *c, cli_bps_t *bps)
{
	double total   = PERCENT * (t->offset + (1.0f * c->offset / c->size)) / t->size;
	double current = PERCENT * c->offset / c->size;
	bool single = t->size == 1;

	if (!single)
	{
		char name[CLI_TRUNCATED_DISPLAY_LONG] = { 0x0 };
		char *nm = c->display ? /*dir_get_name(c->display, '.')*/ : CLI_UNKNOWN;
		if (strlen(nm) < CLI_TRUNCATED_DISPLAY_LONG)
			strcpy(name, nm);
		else
		{
			strncpy(name, nm, CLI_TRUNCATED_DISPLAY_SHORT);
			strcat(name, CLI_TRUNCATED_ELLIPSE);
			if (cli_width > CLI_DEFAULT)
				strcat(name, nm + (strlen(nm) - CLI_TRUNCATED_DISPLAY_SHORT));
		}
		fprintf(stderr, "\x1B[1F"); // go up 1 line
		cli_display_bar(name, c, current);
		cli_display_bps(bps);
		fprintf(stderr, "\n");
	}
	cli_display_bar("Total", single ? c : t, total);

	return;
}

#ifndef _WIN32
static void cli_sigwinch(int s)
{
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	cli_width = ws.ws_col;
	signal(SIGWINCH, cli_sigwinch);
	(void)s;
}
#endif

static int cli_bps_sort(const void *a, const void *b)
{
	const cli_bps_t *ba = (const cli_bps_t *)a;
	const cli_bps_t *bb = (const cli_bps_t *)b;
	return (ba->time > bb->time) - (ba->time < bb->time);
}

static int cli_print(FILE *stream, char *text)
{
	size_t l = strlen(text);
	char *copy = calloc(1, l + 1);
#ifndef _WIN32
	bool strip = !((stream == stdout && isatty(STDOUT_FILENO)) || (stream == stderr && isatty(STDERR_FILENO)));
#else
	#ifdef __DEBUG__
	bool strip = cli_inited != 1;
	#else
	bool strip = false;
	#endif
#endif
	if (strip)
	{
		char *ptr = text;
		for (size_t i = 0, j = 0; i < l; i++)
		{
			char *e = strstr(ptr, "\x1b[");
			if (!e)
				break;
			memcpy(copy + j, ptr, e - ptr);
			j += e - ptr;
			ptr = strchr(e, 'm') + 1;
		}
		strcat(copy, ptr);
	}
	else
		strcpy(copy, text);
	int x = fprintf(stream, "%s", copy);
	free(copy);
	return x;
}

extern void on_quit(int s)
{
	fprintf(stderr, "\e[?25h\n"); /* restore cursor */
	signal(s, SIG_DFL);
	raise(s);
	exit(EXIT_FAILURE); // this shouldn't happen as the raise above will handle things
}

static void cli_init(void)
{
	if (cli_inited >= 0)
		return;
	signal(SIGTERM, on_quit);
	signal(SIGINT,  on_quit);
	signal(SIGQUIT, on_quit);

	setlocale(LC_NUMERIC, "");
#ifdef _WIN32
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		cli_inited = 0;
		return;
	}
	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
	{
		cli_inited = 1;
		return; // MSYS2 ends up here; so take a chance that everything's going to be okay
	}
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
	{
		cli_inited = 0;
		return; // cmd on Win7 ends up here; looks like the feature is not available until Win10
	}
#endif
	cli_inited = 1;
	return;
}
