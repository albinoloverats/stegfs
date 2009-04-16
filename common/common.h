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

  #ifdef __cplusplus
  extern "C"
  {
  #endif /* __cplusplus */

  #define NOTSET 0

  #include <ctype.h>
  #include <errno.h>
  #include <stdio.h>
  #include <signal.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include <string.h>
  #include <stdbool.h>
  #include <inttypes.h>

  #ifdef HAVE_CONFIG_H
    #include <config.h>
  #endif /* HAVE_CONFIG_H */

  #ifndef _WIN32
    #include <dlfcn.h>
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

  #define HEX_LINE_WIDTH 72
  #define HEX_LINE_WRAP  24

  int main(int, char **);
  void init(const char *, const char *);

  int64_t show_help(void); /* defined here, but should be declared in app */
  int64_t show_licence(void);
  int64_t show_usage(void);
  int64_t show_version(void);

  void hex(void *, uint64_t);
  void msg(const char *, ...);
  void die(const char *, ...);

  void sigint(int);

  #ifdef __cplusplus
  }
  #endif /* __cplusplus */

#endif /* _COMMON_LIB_H_ */
