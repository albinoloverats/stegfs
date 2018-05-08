/*
 * Common code for error reporting
 * Copyright © 2009-2018, albinoloverats ~ Software Development
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
			{
				fprintf(stderr, "%s\n", sym[i]);
				free(sym[i]);
			}
			free(sym);
		}
#endif
	}
	exit(ex);
}

#ifdef BUILD_GUI
extern void error_gui_init(GtkWidget *w, GtkWidget *m)
{
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
