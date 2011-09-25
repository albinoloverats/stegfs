/*
 * Common code shared between projects
 * Copyright (c) 2009-2011, albinoloverats ~ Software Development
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
     * \date    2009-2011
     * \brief   Common code shared between projects
     *
     * Common code, mostly used for application initialisation. Such as
     * displaying usage messages and parsing configuration files
     */

    #include <unistd.h>
    #include <inttypes.h>
    #include <stdbool.h>
    #include <libintl.h>
    #include <stdio.h>

    #define NOTSET 0 /*!< Value to use when nothing else is available */

    #ifndef _WIN32
        #ifndef O_BINARY
            #define O_BINARY NOTSET /*!< Value is only relevant on MS systems (and is required), pretend it exists elsewhere */
        #endif
    #else
        #define srand48 srand  /*!< Quietly alias srand48 to be srand on Windows */
        #define lrand48 rand   /*!< Quietly alias lrand48 to be rand on Windows */
        #define F_RDLCK NOTSET /*!< If value doesn't exist on Windows, ignore it */
        #define F_WRLCK NOTSET /*!< If value doesn't exist on Windows, ignore it */
        #define O_FSYNC NOTSET /*!< If value doesn't exist on Windows, ignore it */
        #ifndef SIGQUIT
            #define SIGQUIT SIGBREAK /*!< If value doesn't exist on Windows, use next closest match */
        #endif
        #ifndef ECANCELED
            #define ECANCELED 125 /*!< Make sure the missing error code exists */
        #endif
        #define __attribute__(x) /*!< MinGW cannot handle attributes correctly */
        #define __LITTLE_ENDIAN 1234         /*!< Not defined in MinGW, so set here */
        #define __BYTE_ORDER __LITTLE_ENDIAN /*!< Windows is almost always going to be LE */
        
        extern char *program_invocation_short_name; /*!< Again, not declared on Windows */

        #define __bswap_16(x) \
            ((((x) & 0xff00) >> 8)\
           | (((x) & 0x00ff) << 8))

        #define ntohs(x) __bswap_16(x)
        #define htons(x) __bswap_16(x)

        #define __bswap_32(x) \
            ((((x) & 0xff000000) >> 24) \
           | (((x) & 0x00ff0000) >>  8) \
           | (((x) & 0x0000ff00) <<  8) \
           | (((x) & 0x000000ff) << 24))

        #define ntohl(x) __bswap_32(x)
        #define htonl(x) __bswap_32(x)
    #endif

    #include "list.h"
    #include "logging.h"

    #ifndef __bswap_64
        #define __bswap_64(x) \
            ((((x) & 0xff00000000000000ull) >> 56) \
           | (((x) & 0x00ff000000000000ull) >> 40) \
           | (((x) & 0x0000ff0000000000ull) >> 24) \
           | (((x) & 0x000000ff00000000ull) >> 8)  \
           | (((x) & 0x00000000ff000000ull) << 8)  \
           | (((x) & 0x0000000000ff0000ull) << 24) \
           | (((x) & 0x000000000000ff00ull) << 40) \
           | (((x) & 0x00000000000000ffull) << 56))
    #endif

    #if __BYTE_ORDER == __BIG_ENDIAN
        #define ntohll(x) (x)
        #define htonll(x) (x)
    #else
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            #define ntohll(x) __bswap_64(x)
            #define htonll(x) __bswap_64(x)
        #endif
    #endif

    #ifndef _WIN32
        #define _(s) gettext(s) /*!< Allow use of _() to refer to gettext() */
    #else
        #define _(s) s
    #endif

    #define COMMON_CONCAT(A, B) COMMON_CONCAT2(A, B) /*!< Function overloading argument concatination (part 1) */
    #define COMMON_CONCAT2(A, B) A ## B              /*!< Function overloading argument concatination (part 2) */

    /*@ignore@*/
    #define COMMON_ARGS_COUNT(...) COMMON_ARGS_COUNT2(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1) /*!< Function overloading argument count (part 1) */
    #define COMMON_ARGS_COUNT2(_1, _2, _3, _4, _5, _6, _7, _, ...) _                    /*!< Function overloading argument count (part 2) */

    #define init0()                 init7("", "N/A", NULL, NULL, NULL, NULL, NULL) /*!< Silently call init7() as init0() with all parameters as NULL or otherwise set to their default values */
    #define init1(A)                init7(A, "N/A", NULL, NULL, NULL, NULL, NULL)  /*!< Silently call init7() as init1() with NULLs and default log level */
    #define init2(A, B)             init7(A, B, NULL, NULL, NULL, NULL, NULL)      /*!< Silently call init7() as init2() with NULL for the command line usage and arguments, config file, help screen message */
    #define init3(A, B, C)          init7(A, B, C, NULL, NULL, NULL, NULL)         /*!< Silently call init7() as init3() with NULL for the command line arguments, config file, help screen message */
    #define init4(A, B, C, D)       init7(A, B, C, D, NULL, NULL, NULL)            /*!< Silently call init7() as init4() with NULL for the expected arguments, config file and help screen message */
    #define init5(A, B, C, D, E)    init7(A, B, C, D, E, NULL, NULL)               /*!< Silently call init7() as init5() with NULL for config file, help screen message */
    #define init6(A, B, C, D, E, F) init7(A, B, C, D, E, F, NULL)                  /*!< Silently call init7() as init6() with NULL for the help screen message */

    #define init(...) COMMON_CONCAT(init, COMMON_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Allow init() to be called with either 2, 3, 4, 5 or 6 parameters */
    /*@end@*/

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

    #define ONE_MILLION 1000000 /*!< Integer value for 1 million */
    #define TEN_MILLION 1000000  /*!< Integer value for 10 million */

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
    } __attribute__((aligned))
    conf_t;

    /*!
     * \brief  Structure for parsing program arguments and returned values
     *
     * Each copy of this structure will define the short and long program
     * arguments and whether an option is expected; the fields found and
     * option will return whether th option was found and any the string
     * value for the option
     */
    typedef struct args_t
    {
        char short_option; /*!< Expected short argument */
        char *long_option; /*!< Expected long argument */
        bool found;        /*!< Whether the argument was found in argv */
        bool has_option;   /*!< Whether to expect an option */
        char *option;      /*!< String value of the option */
        char *message;     /*!< Text to display on program help screen */
    } __attribute__((aligned))
    args_t;

    /*!
     * \brief         Initialisation of signal handler and locale settings
     * \param[in]  a  The application name to use when displaying messages
     * \param[in]  v  The version of the application
     * \param[in]  u  (Optional) Command line usage
     * \param[in]  g  (Optional) Command line arguments from argv
     * \param[in]  c  (Optional) Configuration file
     * \param[in]  o  (Optional) List of expected arguments and options
     * \param[in]  m  (Optional) Additional text to display on help screen
     * \return        Pointer to list of additional command line arguments
     *
     * Application initialisation, such as log/error output, signal handling
     * and user displayed version details
     *
     * \fn extern void init(const char * const restrict a, const char * const restrict v, const char * const restrict f, log_e l) __attribute__((nonnull(1, 2)));
     */

    /*!
     * \brief         Initialisation of signal handler and locale settings
     * \param[in]  a  The application name to use when displaying messages
     * \param[in]  v  The version of the application
     * \param[in]  u  (Optional) Command line usage
     * \param[in]  g  (Optional) Command line arguments from argv
     * \param[in]  c  (Optional) Configuration file
     * \param[in]  o  (Optional) List of expected arguments and options
     * \param[in]  m  (Optional) Additional text to display on help screen
     * \return        Pointer to list of additional command line arguments
     *
     * Hidden initialisation function - called by init() after optional parameter is added
     * if needed - a crud form of overloading in C; NB: don't call this function, use init()
     * instead
     */
    extern list_t * init7(const char * const restrict a, const char * const restrict v, const char * const restrict u, char **g, const char * const restrict c, list_t *o, const char * const restrict m) __attribute__((nonnull(1, 2)));

    /*!
     * \brief             Parse command line arguments
     * \param[in]      v  Command line arguments; typically argv from main()
     * \param[in,out]  a  List of args_t which define arguments to look for
     * \return            A list of strings which are extra arguments found but not looked for
     *
     * A way of parsing command line arguments, looking for any values and returning
     * a list of arguments which were seen but not explicitly looked for
     */
    extern list_t *parse_args(char **v, list_t *a) __attribute__((nonnull(1, 2)));

    /*!
     * \brief             Parse configuration file for options
     * \param[in]      f  Configuration file to parse
     * \param[in,out]  a  List of args_t which indicate options to look for
     * \return            Pointer to a list_t of parameters which were found but not looked for
     *
     * Configuration file parser. Will read through a config file and build a
     * array of option/value pairs which can then be easily dealt with by the
     * application
     */
    /*@null@*/extern list_t *parse_config(const char * const restrict f, list_t *a) __attribute__((nonnull(1)));

    /*!
     * \brief         Show list of command line options
     * \param[in]  u  Brief command line usage instructions
     * \param[in]  l  Display simple help message for program usage, uses same list of arguments as parse_args()
     * \param[in]  m  Additional text to display after version details and command line arguments
     *
     * Show list of command line options, and ways to invoke the application.
     * Usually when --help is given as a command line argument. Although the
     * function is declared here, it should be implemented as normal in the
     * main source of the application
     */
    extern void show_help(const char * const restrict u, list_t *l, const char * const restrict m) __attribute__((noreturn, nonnull(1, 2)));

    /*!
      * \brief   Show brief GPL licence text
      *
      * Display a brief overview of the GNU GPL v3 licence, such as when the
      * command line argument is --licence (or --license if you're American)
      */
    extern void show_licence(void) __attribute__((noreturn));

    /*!
     * \brief         Show simple usage instructions
     * \param[in]  u  Brief command line usage instructions
     *
     * If NULL is passed for the valur u, this function just prints out
     * app_name [arg] [value], otherwise it prints out the string u
     */
    extern void show_usage(const char * const restrict u) __attribute__((noreturn));

    /*!
     * \brief   Show application version
     *
     * Display the version of the application, as setup in init()
     */
    extern void show_version(void) __attribute__((noreturn));

    /*!
     * \brief         Capture known signals and handle them accordingly
     * \param[in]  s  Signal value
     *
     * Generic function to catch signals and deal with them accordingly, all
     * messages displayed by caught signals are shown at INFO
     */
    extern void sigint(int s);

    /*!
     * \brief         Display fatal error to user and quit application
     * \param[in]  s  String format, followed by optional additional variables
     *
     * Trace out a text message to either a logfile or stderr and then kill the
     * application - No log level available as all calls to die() are FATAL
     */
    extern void die(const char * const restrict s, ...) __attribute__((noreturn, format(printf, 1, 2)));

    /*!
     * \brief         Wait (or chill out) for the specified number of milliseconds
     * \param[in]  m  Number of milliseconds
     *
     * Pause application execution for the given number of milliseconds - currently
     * this function does nothing in MS Windows
     */
    extern void chill(uint32_t m);

    #if !defined(_GNU_SOURCE) || defined(_WIN32)
        /*!
         * \brief  Wrapper function for systems without getline (ie BSD or MS Windows)
         */
        extern ssize_t getline(char **lineptr, size_t *n, FILE *stream);
    #endif
    
    #ifdef _WIN32
        /*!
         * \brief  Wrapper function for systems without pread (ie MS Windows)
         */
        extern ssize_t pread(int filedes, void *buffer, size_t size, off_t offset);
        
        /*!
         * \brief  Wrapper function for systems without pwrite (ie MS Windows)
         */
        extern ssize_t pwrite(int filedes, const void *buffer, size_t size, off_t offset);

        #ifndef vsnprintf
            /*!
             * \brief  Map the GNU name to the WIN32 name
             */
            #define vsnprintf _vsnprintf
        #endif

        /*!
         * \brief  Wrapper function for systems without asprintf (ie MS Windows)
         */
        int asprintf(char **buffer, char *fmt, ...);
    #endif

#endif
