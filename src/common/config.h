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

#define APP_USAGE "[source] [destination] [-c algorithm] [-s algorithm] [-m mode]\n           [-i iterations] [-k key/-p password] [-x] [-f] [-g] [-b version]"
#define ALT_USAGE "[-k key/-p password] [input] [output]"

#define CONF_COMPRESS       "compress"
#define CONF_FOLLOW         "follow"
#define CONF_KDF_ITERATIONS "kdf-iterations"
#define CONF_KEY            "key"
#define CONF_CIPHER         "cipher"
#define CONF_HASH           "hash"
#define CONF_MODE           "mode"
#define CONF_MAC            "mac"
#define CONF_VERSION        "version"
#define CONF_SKIP_HEADER    "raw"

#define CONF_TRUE     "true"
#define CONF_ON       "on"
#define CONF_ENABLED  "enabled"
#define CONF_FALSE    "false"
#define CONF_OFF      "off"
#define CONF_DISABLED "disabled"


#define CONFIG_ARG_REQUIRED 0x80000000

typedef enum
{
	CONFIG_ARG_OPT_BOOLEAN,
	CONFIG_ARG_OPT_NUMBER,
	CONFIG_ARG_OPT_STRING,

	CONFIG_ARG_REQ_BOOLEAN = (CONFIG_ARG_OPT_BOOLEAN | CONFIG_ARG_REQUIRED),
	CONFIG_ARG_REQ_NUMBER  = (CONFIG_ARG_OPT_NUMBER  | CONFIG_ARG_REQUIRED),
	CONFIG_ARG_REQ_STRING  = (CONFIG_ARG_OPT_STRING  | CONFIG_ARG_REQUIRED)
		// handle option/mandatory options
}
config_arg_e;

typedef union
{
	bool boolean;
	uint64_t number;
	char *string;
}
config_arg_u;

typedef struct
{
	char short_option;
	char *long_option;
	char *option_type;
	char *description;
	bool required;
	bool advanced;
	bool hidden;
	config_arg_e response_type;
	config_arg_u response_value;
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
