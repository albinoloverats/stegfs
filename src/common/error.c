/*
 * Common code for error reporting
 * Copyright © 2009-2022, albinoloverats ~ Software Development
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

#include <errno.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#if !defined _WIN32 && !defined __CYGWIN__ && !defined __FreeBSD__
	#include <execinfo.h>
#endif

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"
#include "non-gnu.h"

#ifdef BUILD_GUI
static void error_gui_alert(const char * const restrict);

static GtkWidget *error_gui_window;
static GtkWidget *error_gui_message;
#endif

extern void on_error(int) __attribute__((noreturn));

static bool error_inited = false;

volatile sig_atomic_t fatal_error_in_progress = 0;

extern void on_error(int s)
{
	if (fatal_error_in_progress)
		raise(s);
	fatal_error_in_progress = 1;

	fprintf(stderr, "\e[?25h\n"); /* restore cursor */

	char m[32] = { 0x0 };
	snprintf(m, sizeof m, "Received fatal signal [%d] ", s);
	psignal(s, m);

#ifdef BUILD_GUI
	error_gui_alert(m);
#endif

#if !defined _WIN32 && !defined __CYGWIN__ && !defined __FreeBSD__
	void *bt[BACKTRACE_BUFFER_LIMIT];
	int c = backtrace(bt, BACKTRACE_BUFFER_LIMIT);
	char **sym = backtrace_symbols(bt, c);
	if (sym)
	{
		for (int i = 0; i < c; i++)
			fprintf(stderr, "%s\n", sym[i]);
		free(sym);
	}
#endif

#ifndef __APPLE__
	signal(s, SIG_DFL);
	raise(s);
	__builtin_unreachable();
#else
	exit(errno);
#endif
}

extern void error_init(void)
{
	if (error_inited)
		return;

	signal(SIGILL,  on_error);
	signal(SIGSEGV, on_error);
	signal(SIGBUS,  on_error);
	signal(SIGABRT, on_error);
	signal(SIGSYS,  on_error);

	error_inited = true;

	return;
}

extern void die(const char * const restrict s, ...)
{
	int ex = errno;
	char *d = NULL;
	va_list ap;
	va_start(ap, s);
#ifndef _WIN32
	vasprintf(&d, s, ap);
#else
	uint8_t l = 0xFF;
	if ((d = calloc(l, sizeof l)))
		vsnprintf(d, l - 1, s, ap);
#endif
	fprintf(stderr, "%s", d);
	va_end(ap);
	fprintf(stderr, "\n");
#ifdef BUILD_GUI
	error_gui_alert(d);
#endif
	free(d);
	if (ex)
	{
		char * const restrict e = strdup(strerror(ex));
		for (uint32_t i = 0; i < strlen(e); i++)
			e[i] = tolower((unsigned char)e[i]);
		fprintf(stderr, "%s\n", e);
		free(e);
#if !defined _WIN32 && !defined __CYGWIN__ && !defined __FreeBSD__
		void *bt[BACKTRACE_BUFFER_LIMIT];
		int c = backtrace(bt, BACKTRACE_BUFFER_LIMIT);
		char **sym = backtrace_symbols(bt, c);
		if (sym)
		{
			for (int i = 0; i < c; i++)
				fprintf(stderr, "%s\n", sym[i]);
			free(sym);
		}
#endif
	}
	exit(ex);
}

#ifdef BUILD_GUI
extern void error_gui_init(GtkWidget *w, GtkWidget *m)
{
	error_init();
	error_gui_window = w;
	error_gui_message = m;
}

G_MODULE_EXPORT gboolean error_gui_close(void *w, void *d)
{
	(void)w;
	(void)d;
	gtk_widget_hide(error_gui_window);
	return TRUE;
}

static void error_gui_alert(const char * const restrict msg)
{
	if (error_gui_window)
	{
		gtk_label_set_text((GtkLabel *)error_gui_message, msg);
		gtk_dialog_run((GtkDialog *)error_gui_window);
	}
	return;
}
#endif
