/*
 * Common code for error reporting
 * Copyright © 2009-2015, albinoloverats ~ Software Development
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

#ifndef _COMMON_ERROR_H_
#define _COMMON_ERROR_H_

/*!
 * \file    error.h
 * \author  albinoloverats ~ Software Development
 * \date    2009-2015
 * \brief   Common logging code shared between projects
 *
 * Common error handling code, currently only fatal end of execution,
 * likely due to out of memory or similar.
 */

#ifdef BUILD_GUI
    #include <gtk/gtk.h>
#endif

#define BACKTRACE_BUFFER_LIMIT 1024 /*!< Maximum number of elements in the backtrace buffer */

/*!
 * \brief         Display fatal error to user and quit application
 * \param[in]  s  String format, followed by optional additional variables
 *
 * Display the given error message and bail out - mostly for “out of memory”
 * errors which cannot be recovered from.
 */
extern void die(const char * const restrict s, ...) __attribute__((noreturn, nonnull(1), format(printf, 1, 2)));

#ifdef BUILD_GUI
extern void error_gui_init(GtkWidget *w, GtkWidget *m) __attribute__((nonnull(1), nonnull(2)));

extern void *error_gui_close(void *, void *);
#endif

#endif /* _COMMON_ERROR_H_ */
