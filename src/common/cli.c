/*
 * Common code for providing a cmomand line progress bar
 * Copyright © 2005-2021, albinoloverats ~ Software Development
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
#include <string.h>

#include <signal.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <math.h>

#ifndef _WIN32
	#include <termios.h>
	#include <sys/ioctl.h>
#endif

#include "common.h"
#include "cli.h"
#include "non-gnu.h"

#define CLI_HELP_FORMAT_RIGHT_COLUMN 37

#define CLI_DEFAULT 80
#define CLI_SINGLE 18
#define CLI_DOUBLE 13

#ifndef _WIN32
static int cli_width = CLI_DEFAULT;

static void cli_display_bar(double, double, bool, cli_bps_t *);
static void cli_sigwinch(int);
#else
static void cli_init(void);
static int cli_inited = -1; // -1 (no), 0 (failed), 1 (okay)
#endif

static int cli_bps_sort(const void *, const void *);
static int cli_print(FILE *, char *);

extern void cli_format_help(char s, char *l, char *v, char *t)
{
#ifdef _WIN32
	cli_init();
#endif
	size_t z = CLI_HELP_FORMAT_RIGHT_COLUMN - 8 - strlen(l);
	cli_fprintf(stderr, "  " ANSI_COLOUR_WHITE "-%c" ANSI_COLOUR_RESET ", " ANSI_COLOUR_WHITE "--%s" ANSI_COLOUR_RESET, s, l);
	if (v)
	{
		cli_fprintf(stderr, ANSI_COLOUR_WHITE "=" ANSI_COLOUR_YELLOW "<%s>" ANSI_COLOUR_RESET, v);
		z -= 3 + strlen(v);
	}
	fprintf(stderr, "%*s", (int)z, " ");

	cli_fprintf(stderr, ANSI_COLOUR_BLUE);
#ifndef _WIN32
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	if (w.ws_col)
	{
		size_t o = 0;
		while (true)
		{
			int l = w.ws_col - CLI_HELP_FORMAT_RIGHT_COLUMN - 1;
			while (isspace(t[o]))
				o++;
			/* FIXME wrap on word boundary and handle UTF-8 characters properly */
			o += fprintf(stderr, "%.*s", l, t + o);
			if (o >= strlen(t))
				break;
			if (!isspace(t[o - 1]) && !isspace(t[o]))
				fprintf(stderr, "-");
			fprintf(stderr, "\n%*s", CLI_HELP_FORMAT_RIGHT_COLUMN, " ");
		}
	}
	else
#endif /* ! _WIN32 */
		fprintf(stderr, "%s", t);
	cli_fprintf(stderr, ANSI_COLOUR_RESET);

	fprintf(stderr, "\n");
	return;
}

extern int cli_printf(const char * const restrict s, ...)
{
#ifdef _WIN32
	cli_init();
#endif
	va_list ap;
	va_start(ap, s);
	char *d = NULL;
#ifndef _WIN32
	vasprintf(&d, s, ap);
#else
	uint8_t l = 0xFF;
	if ((d = calloc(l, sizeof l)))
		vsnprintf(d, l - 1, s, ap);
#endif
	int x = cli_print(stdout, d);
	va_end(ap);
	free(d);
	return x;
}

extern int cli_fprintf(FILE *f, const char * const restrict s, ...)
{
#ifdef _WIN32
	cli_init();
#endif
	va_list ap;
	va_start(ap, s);
	char *d = NULL;
#ifndef _WIN32
	vasprintf(&d, s, ap);
#else
	uint8_t l = 0xFF;
	if ((d = calloc(l, sizeof l)))
		vsnprintf(d, l - 1, s, ap);
#endif
	int x = cli_print(f, d);
	va_end(ap);
	free(d);
	return x;
}

extern int cli_printx(const uint8_t * const restrict x, size_t z)
{
#ifdef _WIN32
	cli_init();
#endif
	return cli_fprintx(stdout, x, z);
}

extern int cli_fprintx(FILE *f, const uint8_t * const restrict x, size_t z)
{
#ifdef _WIN32
	cli_init();
#endif
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
#ifdef _WIN32
	cli_init();
#endif
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

#ifndef _WIN32
extern void cli_display(cli_t *p)
{
	cli_sigwinch(SIGWINCH);

	cli_bps_t bps[BPS];
	memset(bps, 0x00, BPS * sizeof( cli_bps_t ));
	int b = 0;

	fprintf(stderr, "\e[?25l"); /* hide cursor */
	while (*p->status == CLI_INIT || *p->status == CLI_RUN)
	{
		struct timespec s = { 0, 10 * MILLION }; /* 10 milliseconds */
		nanosleep(&s, NULL);

		if (*p->status == CLI_INIT)
			continue;

		double pc = (PERCENT * p->total->offset + PERCENT * p->current->offset / p->current->size) / p->total->size;
		if (p->total->offset == p->total->size)
			pc = PERCENT * p->total->offset / p->total->size;

		struct timeval tv;
		gettimeofday(&tv, NULL);
		bps[b].time = tv.tv_sec * MILLION + tv.tv_usec;
		bps[b].bytes = p->current->offset;
		b++;
		if (b >= BPS)
			b = 0;

		cli_display_bar(pc, PERCENT * p->current->offset / p->current->size, p->total->size == 1, bps);
	}
	if (*p->status == CLI_DONE)
		cli_display_bar(PERCENT, PERCENT, p->total->size == 1, bps);
	fprintf(stderr, "\e[?25h\n"); /* restore cursor */

	return;
}

static void cli_display_bar(double total, double current, bool single, cli_bps_t *bps)
{
	char *prog_bar = calloc(cli_width + 1, sizeof( char ));
	sprintf(prog_bar, "%3.0f%%", isnan(total) ? 0.0f : total);
	/*
	 * display progress bar
	 */
	strcat(prog_bar, " [");
	int pb = single ? cli_width - CLI_SINGLE : cli_width / 2 - CLI_DOUBLE;
	for (int i = 0; i < pb; i++)
		strcat(prog_bar, i < pb * total / PERCENT ? "=" : " ");
	/*
	 * current (if necessary)
	 */
	if (!single)
	{
		sprintf(prog_bar + strlen(prog_bar), "] %3.0f%% [", isnan(total) ? 0.0f : current);
		for (int i = 0; i < pb; i++)
			strcat(prog_bar, i < pb * current / PERCENT ? "=" : " ");
	}
	strcat(prog_bar, "]");
	/*
	 * calculate B/s
	 */
	double val = cli_calc_bps(bps);
	if (isnan(val) || val == 0.0f)
		strcat(prog_bar, "  ---.- B/s");
	else
	{
		if (val < THOUSAND)
			sprintf(prog_bar + strlen(prog_bar), "  %5.1f B/s", val);
		else if (val < MILLION)
			sprintf(prog_bar + strlen(prog_bar), " %5.1f KB/s", val / KILOBYTE);
		else if (val < THOUSAND_MILLION)
			sprintf(prog_bar + strlen(prog_bar), " %5.1f MB/s", val / MEGABYTE);
		else if (val < BILLION)
			sprintf(prog_bar + strlen(prog_bar), " %5.1f GB/s", val / GIGABYTE);
#if 0
		else /* if you’re getting these kinds of speeds please, please can I have your machine ;-) */
			sprintf(prog_bar + strlen(prog_bar), " %5.1f TB/s", val / TERABYTE);
#endif
	}

	fprintf(stderr, "\r%s", prog_bar);
	free(prog_bar);
	return;
}

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
	bool strip = cli_inited != 1;
	#ifndef __DEBUG__
		strip = false;
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

#ifdef _WIN32
static void cli_init(void)
{
	if (cli_inited >= 0)
		return;
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
	cli_inited = 1;
	return;
}
#endif
