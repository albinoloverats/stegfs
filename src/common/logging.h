/*
 * Common code for logging messages
 * Copyright © 2009-2013, albinoloverats ~ Software Development
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

#ifndef _COMMON_LOG_H_
#define _COMMON_LOG_H_

/*!
 * \file    log.h
 * \author  albinoloverats ~ Software Development
 * \date    2009-2013
 * \brief   Common logging code shared between projects
 *
 * Common logging code; for outputing warnings and errors to the user or
 * other log file.
 */

#include <stdint.h> /*!< Necessary include as c99 standard integer types are referenced in this header */

#ifdef _WIN32
    #define flockfile(f)   (void)f /*!< Function doesn't exist on Windows - ginore it */
    #define funlockfile(f) (void)f /*!< Function doesn't exist on Windows - ginore it */
#endif

#ifndef LOG_DEFAULT
    #ifdef __DEBUG__
        #define LOG_DEFAULT LOG_DEBUG /*!< The default log level, if not already defined and __DEBUG__ is defined */
    #else
        #define LOG_DEFAULT LOG_INFO /*!< The default log level, if not already defined */
    #endif
#endif

#define LOG_BINARY_LINE_WIDTH 72 /*!< Maximum line width for log_binary() output */

#define LOG_BINARY_LINE_WRAP  16 /*!< Wrap log_binary() output after this many bytes (and spaces between words, etc) */


/*!
 * \brief  Enumeration of log level values
 *
 * Log level values to use when calling log_message().
 */
typedef enum
{
    LOG_EVERYTHING, /*!< Log out everything (use with caution) */
    LOG_VERBOSE,    /*!< Really verbose logging */
    LOG_DEBUG,      /*!< Minimum level with debugging turned on */
    LOG_INFO,       /*!< Default minimum log level */
    LOG_WARNING,    /*!< Warning level of logging */
    LOG_ERROR,      /*!< Error log level */
    LOG_FATAL,      /*!< Fatal event occured, used by die() */
    LOG_LEVEL_COUNT /*!< The number of available log levels */
} __attribute__((packed))
log_e;

/*!
 * \brief         Redirect STDERR
 * \param[in]  f  A file name to log messages to; uses stderr if NULL
 *
 * Useful for redirecting error messages to a log file after the
 * application has started if running as a daemon.
 */
extern void log_redirect(const char * const restrict f) __attribute__((nonnull(1)));

/*!
 * \brief         Update the log level
 * \param[in]  l  The new minimum log level
 *
 * Update the minimum level of logging; all messages which are the given
 * level or higher will be displaayed.
 */
extern void log_relevel(log_e l);

/*!
 * \brief         Parse the string to get the desired log level
 * \param[in]  l  The string name of the log level
 *
 * Get the log_e value for the given string.
 */
extern log_e log_parse_level(const char * const restrict l) __attribute__((nonnull(1)));

/*!
 * \brief         Output binary data as hexadecimal values
 * \param[in]  l  Log level of this message
 * \param[in]  v  Data to display
 * \param[in]  s  Length of data in bytes
 *
 * Trace out the bytes (displayed as hexadecimal values) in the byte
 * array of the given length. Output is to either a logfile or stderr.
 */
extern void log_binary(log_e l, const void * const restrict v, uint64_t s) __attribute__((nonnull(2)));

/*!
 * \brief         Display messages to the user on STDERR
 * \param[in]  l  Log level of this message
 * \param[in]  s  String format followed by additional variables
 *
 * Trace out a text message to either a logfile or stderr, using the
 * given log level value.
 */
extern void log_message(log_e l, const char * const restrict s, ...) __attribute__((nonnull(2), format(printf, 2, 3)));

#endif /* _COMMON_LOG_H_ */
