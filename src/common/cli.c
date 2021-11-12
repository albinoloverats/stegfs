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

#define CLI_DEFAULT 80 /* Expected default terminal width */
#define CLI_LARGE  160 /* I like a nice large terminal (so Tmux can split it into 4 regular sized if I so desire) */

#define CLI_SINGLE 19
#define CLI_DOUBLE 14

#define CLI_TRUNCATED_DISPLAY 10
#define CLI_TRUNCATED_ELLIPSE "...."

#ifndef _WIN32
static int cli_width = CLI_DEFAULT;
static int cli_size_width = 0;

static void cli_display_bar(cli_progress_t *, cli_progress_t *, cli_bps_t *);
static void cli_sigwinch(int);
#endif

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
	cli_init();
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
	cli_init();
	return cli_fprintx(stdout, x, z);
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

#ifndef _WIN32
extern void cli_display(cli_t *p)
{
	cli_sigwinch(SIGWINCH);
	cli_size_width = 0;

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
		struct timeval tv;
		gettimeofday(&tv, NULL);
		bps[b].time = tv.tv_sec * MILLION + tv.tv_usec;
		bps[b].bytes = p->current->offset;
		b++;
		if (b >= BPS)
			b = 0;
		cli_display_bar(p->total, p->current, bps);
	}
	if (*p->status == CLI_DONE)
	{
		p->total->offset = p->total->size;
		p->current->offset = p->current->size;
		cli_display_bar(p->total, p->current, bps);
	}
	fprintf(stderr, "\e[?25h\n"); /* restore cursor */

	return;
}

static int cli_display_bar_size(char *prog_bar, cli_progress_t *p, bool use_max)
{
	char tmp[0xFF] = { 0x0 };
	int x = sprintf(tmp, "%'" PRIu64, p->size);
	if (use_max && x > cli_size_width)
		cli_size_width = x;
	if (prog_bar)
		return sprintf(prog_bar + strlen(prog_bar), "(%'*" PRIu64 "/%-'*" PRIu64 ") ", use_max ? cli_size_width : x, p->offset, use_max ? cli_size_width : x, p->size);
	return sprintf(tmp, "(%'*" PRIu64 "/%-'*" PRIu64 ") ", use_max ? cli_size_width : x, p->offset, use_max ? cli_size_width : x, p->size);
}

static void cli_display_bar(cli_progress_t *t, cli_progress_t *c, cli_bps_t *bps)
{
	double total   = (PERCENT * t->offset + PERCENT * c->offset / c->size) / t->size;
	double current = PERCENT * c->offset / c->size;
	bool single = t->size == 1;

	int max_width = cli_width; // just in case there's a change...
	char *progress_bar = calloc(max_width + 1, sizeof( char ));
	if (!progress_bar)
		return; // TODO maybe die?
	sprintf(progress_bar, "%3.0f%% ", isnan(total) ? 0.0f : (total > PERCENT ? PERCENT : total));

	int progress_bar_width = single ? (max_width - CLI_SINGLE) : ((max_width / 2) - CLI_DOUBLE);

	if (max_width >= CLI_LARGE)
	{
		if (single)
			progress_bar_width -= cli_display_bar_size(progress_bar, single ? c : t, single);
		else
		{
			progress_bar_width -= cli_display_bar_size(progress_bar, single ? c : t, single) / 2;
			progress_bar_width -= cli_display_bar_size(NULL, c, true) / 2;
		}
	}

	strcat(progress_bar, "[");
	/*
	 * display overall progress bar
	 */
	for (int i = 0; i < progress_bar_width; i++)
		strcat(progress_bar, i < progress_bar_width * total / PERCENT ? "=" : " ");
	/*
	 * display current progress (if necessary)
	 */
	if (!single)
	{
		sprintf(progress_bar + strlen(progress_bar), "] %3.0f%% ", isnan(total) ? 0.0f : (current > PERCENT ? PERCENT : current));
		if (max_width >= CLI_LARGE)
			cli_display_bar_size(progress_bar, c, true);
		strcat(progress_bar, "[");
		off_t no = strlen(progress_bar);
		for (int i = 0; i < progress_bar_width; i++)
			strcat(progress_bar, i < progress_bar_width * current / PERCENT ? "=" : " ");
		if (c->display)
		{
			off_t eq = no + progress_bar_width * current / PERCENT;
			char *nm = dir_get_name(c->display, '.');
			if (strlen(nm) < (2 * CLI_TRUNCATED_DISPLAY + strlen(CLI_TRUNCATED_ELLIPSE)))
				memcpy(progress_bar + no, nm, strlen(nm));
			else
			{
				memcpy(progress_bar + no, nm, CLI_TRUNCATED_DISPLAY);
				memcpy(progress_bar + no + CLI_TRUNCATED_DISPLAY, CLI_TRUNCATED_ELLIPSE, strlen(CLI_TRUNCATED_ELLIPSE));
				memcpy(progress_bar + no + CLI_TRUNCATED_DISPLAY + strlen(CLI_TRUNCATED_ELLIPSE), nm + (strlen(nm) - CLI_TRUNCATED_DISPLAY), CLI_TRUNCATED_DISPLAY);
			}
			memset(progress_bar + eq, '=', 1);
			free(nm);
		}
	}
	strcat(progress_bar, "]");
	/*
	 * calculate B/s
	 */
	double val = cli_calc_bps(bps);
	if (isnan(val) || val == 0.0f)
		strcat(progress_bar, "  ---.- B/s");
	else
	{
		if (val < THOUSAND)
			sprintf(progress_bar + strlen(progress_bar), "  %5.1f B/s", val);
		else if (val < MILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f KB/s", val / KILOBYTE);
		else if (val < THOUSAND_MILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f MB/s", val / MEGABYTE);
		else if (val < BILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f GB/s", val / GIGABYTE);
#if 1           /* if you’re getting these kinds of speeds please, please can I have your machine ;-) */
		else if (val < THOUSAND_BILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f TB/s", val / TERABYTE);
		else if (val < TRILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f PB/s", val / PETABYTE);
		else //if (val < THOUSAND_TRILLION)
			sprintf(progress_bar + strlen(progress_bar), " %5.1f EB/s", val / EXABYTE);
//		else if (val < QUADRILLION)
//			sprintf(progress_bar + strlen(progress_bar), " %5.1f ZB/s", val / ZETTABYTE);
//		else
//			sprintf(progress_bar + strlen(progress_bar), " %5.1f YB/s", val / YOTTABYTE);
#else
		else
			strcat(progress_bar, "  ---.- B/s");
#endif
	}
	fprintf(stderr, "\r%s ", progress_bar);
	free(progress_bar);
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

static void cli_init(void)
{
	if (cli_inited >= 0)
		return;
#ifndef _WIN32
	setlocale(LC_NUMERIC, "");
#else
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
