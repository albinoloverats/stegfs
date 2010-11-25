/*
 * Common code shared between projects
 * Copyright (c) 2009-2010, albinoloverats ~ Software Development
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

#ifndef _COMMON_LIB_H_
    #define _COMMON_LIB_H_

    /*!
     * \file    common.h
     * \author  albinoloverats ~ Software Development
     * \date    2009-2010
     * \brief   Common code shared between projects
     *
     * Common code, mostly used for application initialisation. Such as
     * displaying usage messages and parsing configuration files
     */

    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <inttypes.h>
    #include <libintl.h>

    #define NOTSET 0 /*!< Value to use when nothing else is available */

    #ifndef _WIN32
        #ifndef O_BINARY
            #define O_BINARY NOTSET  /*!< Value is only relevant on MS systems (and is required), pretend it exists elsewhere */
        #endif
    #else
        #define srand48 srand    /*!< Quietly alias srand48 to be srand on Windows */
        #define lrand48 rand     /*!< Quietly alias lrand48 to be rand on Windows */
        #define F_RDLCK NOTSET   /*!< If value doesn't exist on Windows, ignore it */
        #define F_WRLCK NOTSET   /*!< If value doesn't exist on Windows, ignore it */
        #define O_FSYNC NOTSET   /*!< If value doesn't exist on Windows, ignore it */
        #ifndef SIGQUIT
            #define SIGQUIT SIGBREAK /*!< If value doesn't exist on Windows, use next closest match */
        #endif
    #endif

    #define _(s) gettext(s) /*!< Allow use of _() to refer to gettext() */

    /*! Brief overview of the GNU General Public License */
    #define TEXT_LICENCE \
        "This program is free software: you can redistribute it and/or modify\n"  \
        "it under the terms of the GNU General Public License as published by\n"  \
        "the Free Software Foundation, either version 3 of the License, or\n"     \
        "(at your option) any later version.\n\n"                                 \
        "This program is distributed in the hope that it will be useful,\n"       \
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"        \
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"         \
        "GNU General Public License for more details.\n\n"                        \
        "You should have received a copy of the GNU General Public License\n"     \
        "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"

    #define HEX_LINE_WIDTH 72 /*!< Maximum line width for hex() output */

    #define HEX_LINE_WRAP  24 /*!< Wrap hex() output after this many bytes (and spaces between words, etc) */

    #define ONE_MILLION 1000000 /*!< Integer value for 1 million */

    #define RANDOM_SEED_SIZE 3 /*!< Size of random seed value in bytes */

    /*!
     * \brief  Structure to hold configuration file options
     *
     * Store pair of configuration options from property files
     */
    typedef struct conf_t
    {
        char *option; /*!< Option name */
        char *value;  /*!< Option value */
    }
    conf_t;

    /*!
     * \brief            Main application entry point
     * \note             This function is declared here, but defined in the application source
     * \param[in]  argc  Number of command line options
     * \param[in]  argv  Array of command line options
     *
     * Main function; application entry point. Define MAIN_VOID to use main
     * function which takes no arguments
     */
    #ifndef MAIN_VOID
        int main(int argc, char **argv);
    #else
        int main(void);
    #endif

    /*!
     * \brief         Initialisation of signal handler and locale settings
     * \param[in]  a  The application name to use when displaying messages
     * \param[in]  v  The version of the application
     * \param[in]  f  A file name to log messages to; if NULL, stderr is used
     *
     * Application initialisation, such as log/error output, signal handling
     * and user displayed version details
     */
    extern void init(const char * const restrict a, const char * const restrict v, const char * const restrict f) __attribute__((nonnull(1, 2)));

    /*!
     * \brief         Parse configuration file for options
     * \param[in]  f  Configuration file to parse
     * \return        Pointer to array of parameters
     *
     * Configuration file parser. Will read through a config file and build a
     * array of option/value pairs which can then be easily dealt with by the
     * application
     */
    /*@null@*/extern conf_t **config(const char const * restrict f) __attribute__((nonnull(1)));

    /*!
     * \brief   Show list of command line options
     * \note    This function is declared here, but should be defined in the application source
     * \return  EXIT_SUCCESS
     *
     * Show list of command line options, and ways to invoke the application.
     * Usually when --help is given as a command line argument. Although the
     * function is declared here, it should be implemented as normal in the
     * main source of the application
     */
    extern int64_t show_help(void);

    /*!
      * \brief   Show brief GPL licence text
      * \return  EXIT_SUCCESS
      *
      * Display a brief overview of the GNU GPL v3 licence, such as when the
      * command line argument is --licence (or --license if you're American)
      */
    extern int64_t show_licence(void);

    /*!
     * \brief   Show simple usage instructions
     * \return  EXIT_SUCCESS
     *
     * Currently this is a 'do-nothing' function; it just prints out
     * app_name [arg] [value]
     */
    extern int64_t show_usage(void);

    /*!
     * \brief   Show application version
     * \return  EXIT_SUCCESS
     *
     * Display the version of the application, as setup in init()
     */
    extern int64_t show_version(void);

    /*!
     * \brief         Output binary data as hexadecimal values
     * \param[in]  v  Data to display
     * \param[in]  l  Length of data in bytes
     *
     * Trace out the bytes (displayed as hexadecimal values) in the byte array
     * of the given length. Output is to either a logfile or stderr
     */
    extern void hex(void *v, uint64_t l) __attribute__((nonnull(1)));

    /*!
     * \brief         Display messages to the user on STDERR
     * \param[in]  s  String format, followed by optional additional variables
     *
     * Trace out a text message to either a logfile or stderr 
     */
    extern void msg(const char * const restrict s, ...) __attribute__((format(printf, 1, 2)));

    /*!
     * \brief         Display fatal error to user and quit application
     * \param[in]  s  String format, followed by optional additional variables
     *
     * Trace out a text message to either a logfile or stderr and then kill the
     * application
     */
    extern void die(const char * const restrict s, ...) __attribute__((noreturn, format(printf, 1, 2)));

    /*!
     * \brief         Capture known signals and handle them accordingly
     * \param[in]  s  Signal value
     *
     * Generic function to catch signals and deal with them accordingly
     */
    extern void sigint(int s);

    /*!
     * \brief         Wait for the specified number of milliseconds
     * \param[in]  s  Number of milliseconds
     *
     * Pause application execution for the given number of milliseconds
     */
    extern void wait(uint32_t s);

	/*!
	 * \brief  Wrapper function for systems without getline (ie BSD)
	 */
    #ifndef _GUN_SOURCE
    	extern ssize_t getline(char **lineptr, size_t *n, FILE *stream);
    #endif

#endif
