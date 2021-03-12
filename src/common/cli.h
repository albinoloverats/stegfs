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

#ifndef _COMMON_CLI_H_
#define _COMMON_CLI_H_

#include <stdint.h>
#include <stdbool.h>

#define BPS 128

#define ANSI_COLOUR_RESET          "\x1b[0m"
#if 0
	#define ANSI_COLOUR_BLACK          "\x1b[30m"
	#define ANSI_COLOUR_RED            "\x1b[31m"
	#define ANSI_COLOUR_GREEN          "\x1b[32m"
	#define ANSI_COLOUR_YELLOW         "\x1b[33m"
	#define ANSI_COLOUR_BLUE           "\x1b[34m"
	#define ANSI_COLOUR_MAGENTA        "\x1b[35m"
	#define ANSI_COLOUR_CYAN           "\x1b[36m"
	#define ANSI_COLOUR_WHITE          "\x1b[37m"
#else /* use bright colours */
	#define ANSI_COLOUR_BLACK   "\x1b[90m"
	#define ANSI_COLOUR_RED     "\x1b[91m"
	#define ANSI_COLOUR_GREEN   "\x1b[92m"
	#define ANSI_COLOUR_YELLOW  "\x1b[93m"
	#define ANSI_COLOUR_BLUE    "\x1b[94m"
	#define ANSI_COLOUR_MAGENTA "\x1b[95m"
	#define ANSI_COLOUR_CYAN    "\x1b[96m"
	#define ANSI_COLOUR_WHITE   "\x1b[97m"
#endif

typedef enum
{
	CLI_DONE,
	CLI_INIT,
	CLI_RUN
}
cli_status_e;

/*!
 * \brief  Current progress
 *
 * Provide the foreground thread a way to check on the progress. Thus a
 * percentage can be calculated using 100 * offset / size. Either the
 * number of bytes, or directory entries depending on what you’re taking
 * the progress of.
 */
typedef struct
{
	uint64_t offset; /*!< Progress */
	uint64_t size;   /*!< Maximum */
}
cli_progress_t;

typedef struct
{
	cli_status_e   *status;
	cli_progress_t *current;
	cli_progress_t *total;
}
cli_t;

typedef struct
{
	uint64_t time;
	uint64_t bytes;
}
cli_bps_t;

/*!
 * \brief         Display a line of --help text options/values
 * \param[in]  s  Short option
 * \param[in]  l  Long option
 * \param[in]  v  Option value
 * \param[in]  t  About text
 *
 * Display a line of --help text options/values.
 */
extern void cli_format_help(char s, char *l, char *v, char *t);

extern void cli_display(cli_t *) __attribute__((nonnull(1)));

extern double cli_calc_bps(cli_bps_t *) __attribute__((nonnull(1)));

extern int cli_printf(const char * const restrict s, ...) __attribute__((nonnull(1), format(printf, 1, 2)));

extern int cli_fprintf(FILE *f, const char * const restrict s, ...) __attribute__((nonnull(2), format(printf, 2, 3)));

extern int cli_printx(const uint8_t * const restrict x, size_t z) __attribute__((nonnull(1)));

extern int cli_fprintx(FILE *f, const uint8_t * const restrict x, size_t z) __attribute__((nonnull(2)));

#endif /* _COMMON_CLI_H_ */
