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

#ifndef _COMMON_CLI_H_
#define _COMMON_CLI_H_

/*!
 * \file    cli.h
 * \author  albinoloverats ~ Software Development
 * \date    2014-2022
 * \brief   Common console output functions
 *
 * Various functions to help with console output.
 */


#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#define ANSI_COLOUR_RESET           "\x1b[0m"
#if _WIN32
	#define ANSI_COLOUR_BLACK   "\x1b[30m"
	#define ANSI_COLOUR_RED     "\x1b[31m"
	#define ANSI_COLOUR_GREEN   "\x1b[32m"
	#define ANSI_COLOUR_YELLOW  "\x1b[33m"
	#define ANSI_COLOUR_BLUE    "\x1b[34m"
	#define ANSI_COLOUR_MAGENTA "\x1b[35m"
	#define ANSI_COLOUR_CYAN    "\x1b[36m"
	#define ANSI_COLOUR_WHITE   "\x1b[37m"
#else /* use bright colours (on MSYS2/Windows there's no difference AFAIK) */
	#define ANSI_COLOUR_BLACK   "\x1b[90m"
	#define ANSI_COLOUR_RED     "\x1b[91m"
	#define ANSI_COLOUR_GREEN   "\x1b[92m"
	#define ANSI_COLOUR_YELLOW  "\x1b[93m"
	#define ANSI_COLOUR_BLUE    "\x1b[94m"
	#define ANSI_COLOUR_MAGENTA "\x1b[95m"
	#define ANSI_COLOUR_CYAN    "\x1b[96m"
	#define ANSI_COLOUR_WHITE   "\x1b[97m"
#endif

#define CLI_TRUNCATED_DISPLAY_LONG  25
#define CLI_TRUNCATED_DISPLAY_SHORT 10
#define CLI_TRUNCATED_ELLIPSE   "...."
#define CLI_UNKNOWN "(unknown)"

/*!
 * \brief Initialisation status of CLI
 *
 * Indicates the stage of output of the progress bars.
 */
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
	uint64_t offset;  /*!< Progress */
	uint64_t size;    /*!< Maximum */
	char    *display; /*!< Extra text to display within the progress bar */
}
cli_progress_t;

/*!
 * \brief Progress bar structure
 *
 * Pointers for progress status and level of completion. It allows for
 * current item progress and overall progress. The status is largely
 * used to ensure that the progress bar shows as 100% when complete
 * instead of showing (maybe only) 99%.
 */
typedef struct
{
	cli_status_e   *status;  /*!< Progress bar status */
	cli_progress_t *current; /*!< Current item progress */
	cli_progress_t *total;   /*!< Overall progress */
}
cli_t;

#define BPS 128 /*!< Bytes per second history length */

/*!
 * \brief Bytes per second structure
 *
 * A count of how many bytes have been processed and at what time. Is
 * used to calculate transfer speed.
 */
typedef struct
{
	uint64_t time;  /*!< The time at which this reading was taken */
	uint64_t bytes; /*!< The number of bytes process */
}
cli_bps_t;

/*!
 * \brief         Display a progress bar
 * \param[in]  c  The CLI progress bar instance
 *
 * Display a progress bar on stderr with the given levels of progress.
 * If only one bar is needed then that is all that will be shown, but if
 * you need progress for current item and overall progress then you get
 * two.
 */
extern void cli_display(cli_t *c) __attribute__((nonnull(1)));

/*!
 * \brief         Calculate the number of bytes per second
 * \param[in]  c  The bytes per second history
 * \return        The number of bytes per second
 *
 * Calculate the speed of a transfer given a list of cli_bps_t readings.
 * Currently this is required to be 128.
 */
extern double cli_calc_bps(cli_bps_t *c) __attribute__((nonnull(1)));

/*!
 * \brief         Formatted output to stdout
 * \param[in]  s  The format template
 * \return        The number of bytes written
 *
 * Prints the optional arguments under the control of the template
 * string to the stream stdout. It returns the number of characters
 * printed, or a negative value if there was an output error.
 */
extern int cli_printf(const char * const restrict s, ...) __attribute__((nonnull(1), format(printf, 1, 2)));

/*!
 * \brief         Formatted output to stderr
 * \param[in]  s  The format template
 * \return        The number of bytes written
 *
 * Prints the optional arguments under the control of the template
 * string to the stream stderr. It returns the number of characters
 * printed, or a negative value if there was an output error.
 */
extern int cli_eprintf(const char * const restrict s, ...) __attribute__((nonnull(1), format(printf, 1, 2)));

/*!
 * \brief         Formatted output to the given stream
 * \param[in]  f  The output stream
 * \param[in]  s  The format template
 * \return        The number of bytes written
 *
 * Prints the optional arguments under the control of the template
 * string to the stream s. It returns the number of characters printed,
 * or a negative value if there was an output error.
 */
extern int cli_fprintf(FILE *f, const char * const restrict s, ...) __attribute__((nonnull(2), format(printf, 2, 3)));

/*!
 * \brief         Hexadecimal output to stdout
 * \param[in]  x  The binary data to print
 * \param[in]  z  The length of the binary data
 * \return        The number of bytes written
 *
 * Prints binary data in a hexadecimal fashion to stdout. Output is
 * currently wrapped to 16 bytes per line. It returns the number of
 * characters printed, or a negative value if there was an output error.
 */
extern int cli_printx(const uint8_t * const restrict x, size_t z) __attribute__((nonnull(1)));

/*!
 * \brief         Hexadecimal output to stderr
 * \param[in]  x  The binary data to print
 * \param[in]  z  The length of the binary data
 * \return        The number of bytes written
 *
 * Prints binary data in a hexadecimal fashion to stderr. Output is
 * currently wrapped to 16 bytes per line. It returns the number of
 * characters printed, or a negative value if there was an output error.
 */
extern int cli_eprintx(const uint8_t * const restrict x, size_t z) __attribute__((nonnull(1)));

/*!
 * \brief         Hexadecimal output to the given stream
 * \param[in]  f  The output stream
 * \param[in]  x  The binary data to print
 * \param[in]  z  The length of the binary data
 * \return        The number of bytes written
 *
 * Prints binary data in a hexadecimal fashion to the given stream.
 * Output is currently wrapped to 16 bytes per line. It returns the
 * number of characters printed, or a negative value if there was an
 * output error.
 */
extern int cli_fprintx(FILE *f, const uint8_t * const restrict x, size_t z) __attribute__((nonnull(2)));

#endif /* _COMMON_CLI_H_ */
