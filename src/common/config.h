/*
 * encrypt ~ a simple, multi-OS encryption utility
 * Copyright © 2005-2021, albinoloverats ~ Software Development
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
	CONFIG_ARG_OPT_BOOLEAN,
	CONFIG_ARG_OPT_NUMBER,
	CONFIG_ARG_OPT_STRING,

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

typedef struct
{
	size_t count;
	config_list_u *items;
}
config_list_t;

typedef union
{
	bool boolean;
	int64_t number;
	char *string;
	config_pair_u pair;
	config_list_t list;
}
config_arg_u;

typedef struct
{
	char short_option;
	char *long_option;
	char *option_type;
	char *description;
	config_arg_e response_type;
	config_arg_u response_value;
	bool required:1;
	bool advanced:1;
	bool hidden:1;
}
config_arg_t;

typedef struct
{
	char *name;
	char *version;
	char *url;
	char *config;
}
config_about_t;


extern void config_init(config_about_t about);

extern void config_show_usage(config_arg_t *args, char **extra);

/*!
 * \brief           Application init function
 * \param[in]  argc Number of command line arguments
 * \param[out] argv Command line arguments
 * \return          Any command line options that were set
 *
 * Provide simple command line argument parsing, and pass back whatever
 * options where set. Removes a lot of the cruft from the legacy common
 * code that used to exist here.
 */
extern int config_parse(int argc, char **argv, config_arg_t *args, char ***extra, char **about);

/*!
 * \brief         Update configuration file
 * \param[in]  o  Option to update
 * \param[out] v  New value
 *
 * Set or update the given configuration option with the given value.
 */
extern void update_config(const char * const restrict o, const char * const restrict v) __attribute__((nonnull(1, 2)));

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
