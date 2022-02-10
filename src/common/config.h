/*
 * encrypt ~ a simple, multi-OS encryption utility
 * Copyright © 2005-2022, albinoloverats ~ Software Development
 * email: encrypt@albinoloverats.net
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

#ifndef _COMMON_CONFIG_H_
#define _COMMON_CONFIG_H_

#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "list.h"


#define CONF_TRUE     "true"
#define CONF_ON       "on"
#define CONF_ENABLED  "enabled"
#define CONF_FALSE    "false"
#define CONF_OFF      "off"
#define CONF_DISABLED "disabled"


#define CONFIG_ARG_REQUIRED 0x80000000 // 1000
#define CONFIG_ARG_LIST     0x40000000 // 0100
#define CONFIG_ARG_PAIR     0xa0000000 // 0010

typedef enum
{
	CONFIG_ARG_BOOLEAN,
	CONFIG_ARG_NUMBER,
	CONFIG_ARG_STRING,
	// these are for semantic clarity
	CONFIG_ARG_OPT_BOOLEAN      = CONFIG_ARG_BOOLEAN,
	CONFIG_ARG_OPT_NUMBER       = CONFIG_ARG_NUMBER,
	CONFIG_ARG_OPT_STRING       = CONFIG_ARG_STRING,

	CONFIG_ARG_REQ_BOOLEAN      = (CONFIG_ARG_OPT_BOOLEAN | CONFIG_ARG_REQUIRED),
	CONFIG_ARG_REQ_NUMBER       = (CONFIG_ARG_OPT_NUMBER  | CONFIG_ARG_REQUIRED),
	CONFIG_ARG_REQ_STRING       = (CONFIG_ARG_OPT_STRING  | CONFIG_ARG_REQUIRED),

	CONFIG_ARG_PAIR_BOOLEAN     = (CONFIG_ARG_REQ_BOOLEAN | CONFIG_ARG_PAIR), // pairs of options are currently required
	CONFIG_ARG_PAIR_NUMBER      = (CONFIG_ARG_REQ_NUMBER  | CONFIG_ARG_PAIR),
	CONFIG_ARG_PAIR_STRING      = (CONFIG_ARG_REQ_STRING  | CONFIG_ARG_PAIR),

//	CONFIG_ARG_LIST_BOOLEAN     = (CONFIG_ARG_REQ_BOOLEAN | CONFIG_ARG_LIST), // list options are currently required
//	CONFIG_ARG_LIST_NUMBER      = (CONFIG_ARG_REQ_NUMBER  | CONFIG_ARG_LIST),
	CONFIG_ARG_LIST_STRING      = (CONFIG_ARG_REQ_STRING  | CONFIG_ARG_LIST),

	CONFIG_ARG_LIST_PAIR_STRING = (CONFIG_ARG_LIST_STRING | CONFIG_ARG_PAIR_STRING)
}
config_arg_e;

typedef struct
{
	bool b1;
	bool b2;
}
config_pair_boolean_t;

typedef struct
{
	int64_t n1;
	int64_t n2;
}
config_pair_number_t;

typedef struct
{
	char *s1;
	char *s2;
}
config_pair_string_t;

typedef union
{
	config_pair_boolean_t boolean;
	config_pair_number_t number;
	config_pair_string_t string;
}
config_pair_u;

typedef union
{
	bool boolean;
	uint64_t number;
	char *string;
	config_pair_u pair;
}
config_list_u;

typedef union
{
	bool boolean;
	int64_t number;
	char *string;
	config_pair_u pair;
	LIST *list;
}
config_arg_u;

/*!
 * \brief  A named command line parameter.
 *
 * A named command line parameter; for instance --help
 */
typedef struct
{
	char short_option;           /*!< The command line short option */
	char *long_option;           /*!< The command line long option */
	char *option_type;           /*!< What the expected parameter should be */
	char *description;           /*!< A description of the argument */
	config_arg_e response_type;  /*!< The expected response type */
	config_arg_u response_value; /*!< The response value */
	bool required:1;             /*!< Whether this option is required */
	bool advanced:1;             /*!< Whether this option is considered advanced */
	bool hidden:1;               /*!< Whether this option should be hidden */
}
config_arg_t;

/*!
 * \brief  An unnamed command line parameter.
 *
 * An unnamed command line parameter; for instance when passing a file
 * name to a program: vim config.c
 */
typedef struct
{
	char *description;           /*!< A description of the argument */
	config_arg_e response_type;  /*!< The expected response type */
	config_arg_u response_value; /*!< The response value */
	bool required:1;             /*!< Whether this option is required */
	bool seen:1;                 /*!< Whether this argument was detected */
}
config_extra_t;

typedef struct
{
	char *name;
	char *version;
	char *url;
	char *config;
}
config_about_t;


extern void config_init(config_about_t about);

extern void config_show_usage(LIST args, LIST extra);

#define CONFIG_PARSE_COUNT(...) CONFIG_PARSE_COUNT2(__VA_ARGS__, 6, 5, 4, 3, 2, 1)
#define CONFIG_PARSE_COUNT2(_1, _2, _3, _4, _5, _6, _, ...) _

#define config_parse_2(A, B)              config_parse_aux(A, B, NULL, NULL, NULL, true)
#define config_parse_3(A, B, C)           config_parse_aux(A, B, C, NULL, NULL, true)
#define config_parse_4(A, B, C, D)        config_parse_aux(A, B, C, D, NULL, true)
#define config_parse_5(A, B, C, D, E)     config_parse_aux(A, B, C, D, E, true)
#define config_parse_6(A, B, C, D, E, F)  config_parse_aux(A, B, C, D, E, F)
#define config_parse(...) CONCAT(config_parse_, CONFIG_PARSE_COUNT(__VA_ARGS__))(__VA_ARGS__)

/*!
 * \brief             Configuration reading/parsing
 * \param[in]      c  Number of command line arguments
 * \param[in]      v  Command line arguments
 * \param[in/out]  a  Expected command line arguments, config options, and their resulting values
 * \param[in/out]  x  Any arguments without flags (file names, etc)
 * \param[in]      t  Any additional text to display as notes
 * \param[in]      w  Whether to warn about unknown arguments
 *
 * Provide simple command line argument parsing, and pass back whatever
 * options where set. Removes a lot of the cruft from the legacy common
 * code that used to exist here.
 */
extern int config_parse_aux(int c, char **v, LIST a, LIST x, LIST t, bool w);

/*!
 * \brief         Update configuration file
 * \param[in]  o  Option to update
 * \param[out] v  New value
 *
 * Set or update the given configuration option with the given value.
 */
extern void update_config(const char * const restrict o, const char * const restrict v) __attribute__((nonnull(1, 2)));


/*!
 * \brief         Compare config_arg_t's
 * \param[in]  a  First config_arg_t
 * \param[in]  b  Second config_arg_t
 * \return        The difference between the two
 *
 * Compare two config_arg_t using the short_option. Used to ensure that
 * only one of each argument is in the LIST.
 */
extern int config_arg_comp(const void *a, const void *b);

#if 0
/*!
 * \brief         Show list of command line options
 *
 * Show list of command line options, and ways to invoke the application.
 * Usually when --help is given as a command line argument.
 */
extern void show_help(void) __attribute__((noreturn));

/*!
  * \brief        Show brief GPL licence text
  *
  * Display a brief overview of the GNU GPL v3 licence, such as when the
  * command line argument is --licence.
  */
extern void show_licence(void) __attribute__((noreturn));

/*!
 * \brief         Show simple usage instructions
 *
 * Display simple application usage instruction.
 */
extern void show_usage(void) __attribute__((noreturn));

/*!
 * \brief         Show application version
 *
 * Display the version of the application.
 */
extern void show_version(void) __attribute__((noreturn));
#endif

#endif /* ! _COMMON_CONFIG_H_ */
