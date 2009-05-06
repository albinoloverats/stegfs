/*
 * Common code shared between projects
 * Copyright (c) 2009, albinoloverats ~ Software Development
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
   * \file
   * \author  albinoloverats ~ Software Development
   * \date    2009
   * \brief   Common code shared between projects
   */

  #ifdef __cplusplus
  extern "C"
  {
  #endif /* __cplusplus */

  /*!
   * \brief  Value used for unset variables and enumerations
   */
  #define NOTSET 0

  #include <ctype.h>
  #include <errno.h>
  #include <stdio.h>
  #include <signal.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include <string.h>
  #include <libintl.h>
  #include <stdbool.h>
  #include <inttypes.h>

  #ifdef HAVE_CONFIG_H
    #include <config.h>
  #endif /* HAVE_CONFIG_H */

  #ifndef _WIN32
    #include <dlfcn.h>
    /*!
     * \brief  O_BINARY is only used on MS systems, we'll need to pretend it exists on Unix systems
     */
    #define O_BINARY NOTSET
  #else  /* ! _WIN32 */
    #include <windows.h>
    #define srand48 srand
    #define lrand48 rand
    #define F_RDLCK NOTSET
    #define F_WRLCK NOTSET
    #define O_FSYNC NOTSET
    #define SIGQUIT SIGBREAK
  #endif /*   _WIN32 */

  /*!
   * \brief  Allow us of _() to refer to gettext()
   */
  #define _(s) gettext(s)

  /*!
   * \brief  Brief overview of the GNU General Public License
   */
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

  /*!
   * \brief  Maximum line width for hex() output
   */
  #define HEX_LINE_WIDTH 72

  /*!
   * \brief  Wrap hex() output after this many bytes (and spaces between words, etc)
   */
  #define HEX_LINE_WRAP  24


  /*!
   * \brief            Main application entry point
   * \note             This function is declared here, but defined in the application source
   * \param[in]  argc  Number of command line options
   * \param[in]  argv  Array of command line options
   */
  int main(int argc, char **argv);

  /*!
   * \brief            Initialisation of signal handler and locale settings
   * \param[in]  a     The application name to use when displaying messages
   * \param[in]  v     The version of the application
   */
  extern void init(const char *a, const char *v);

  /*!
   * \brief            Show list of command line options
   * \return           EXIT_SUCCESS
   */
  extern int64_t show_help(void);

 /*!
   * \brief            Show brief GPL licence text
   * \note             This function is declared here, but defined in the application source
   * \return           EXIT_SUCCESS
   */
 extern int64_t show_licence(void);

  /*!
   * \brief            Show simple usage instructions
   * \return           EXIT_SUCCESS
   */
  extern int64_t show_usage(void);

  /*!
   * \brief            Show application version
   * \return           EXIT_SUCCESS
   */
  extern int64_t show_version(void);

  /*!
   * \brief            Output binary data as hexadecimal values
   * \param[in]  v     Data to display
   * \param[in]  l     Length of data in bytes
   */
  extern void hex(void *v, uint64_t l);

  /*!
   * \brief            Display messages to the user on STDERR
   * \param[in]  s     String format, followed by optional additional variables
   */
  extern void msg(const char *s, ...);

  /*!
   * \brief            Display fatal error to user and quit application
   * \param[in]  s     String format, followed by optional additional variables
   */
  extern void die(const char *s, ...);

  /*!
   * \brief            Capture known signals and handle them accordingly
   * \param[in]  s     Signal value
   */
  extern void sigint(int s);

  #ifdef __cplusplus
  }
  #endif /* __cplusplus */

#endif /* _COMMON_LIB_H_ */
