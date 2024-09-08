/*
 * encrypt ~ a simple, multi-OS encryption utility
 * Copyright © 2005-2024, albinoloverats ~ Software Development
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

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include <gcrypt.h>

#ifndef _WIN32
	#include <sys/utsname.h>
	#include <sys/ioctl.h>
	#ifdef __sun
		#include <sys/tty.h>
	#endif
#endif

#include "common.h"
#include "non-gnu.h"
#include "error.h"
#include "ccrypt.h"
#include "version.h"
#include "cli.h"
#include "config.h"
#include "pair.h"
#include "list.h"

#ifdef _WIN32
	#include <Shlobj.h>
	extern char *program_invocation_short_name;
#endif


static void show_version(LIST args, LIST largs, LIST notes, LIST extra) __attribute__((noreturn));
static void show_help(LIST args, LIST largs, LIST notes, LIST extra) __attribute__((noreturn));
static void show_licence(LIST args, LIST largs, LIST notes, LIST extra) __attribute__((noreturn));

inline static bool is_argument(char, const char *, const char *);
inline static void format_section(char *);
inline static void print_usage(LIST, LIST);

static int64_t parse_number_size_suffix(const char *s);

static bool  parse_boolean(const char *, const char *, bool      *);
static bool  parse_integer(const char *, const char *, int64_t   *);
//static bool  parse_decimal(const char *, const char *, __float128 *);
static bool  parse_decimal(const char *, const char *, long double *);
static char *parse_string (const char *, const char *, char      *);

static bool parse_pair_boolean(const char *c, const char *l, pair_boolean_t *);
static bool parse_pair_integer(const char *c, const char *l, pair_integer_t *);
static bool parse_pair_decimal(const char *c, const char *l, pair_decimal_t *);
static bool parse_pair_string (const char *c, const char *l, pair_string_t  *);

static pair_u *parse_pair(const char *c, const char *l);

static char *parse_tail(const char *, const char *);

static void parse_list_boolean(const char *, LIST list);
static void parse_list_integer(const char *, LIST list);
static void parse_list_decimal(const char *, LIST list);

static void parse_list(config_arg_e, const char *, LIST);

static bool init = false;
static config_about_t about = { 0x0 };


extern void config_init(config_about_t a)
{
	init = true;
	memcpy(&about, &a, sizeof about);
	return;
}

extern int config_named_compare(const void *a, const void *b)
{
	const config_named_t *x = a;
	const config_named_t *y = b;
	return x->short_option - y->short_option;
}

extern int config_unnamed_compare(const void *a, const void *b)
{
	const config_unnamed_t *x = a;
	const config_unnamed_t *y = b;
	return strcmp(x->description, y->description);
}

extern void config_named_free(void *f)
{
	config_named_t *x = (config_named_t *)f;
	if (x->response.type & CONFIG_ARG_LIST)
		list_deinit(x->response.value.list, free);
	else if (x->response.type & CONFIG_ARG_STRING)
		free(x->response.value.string);
	free(x);
	return;
}

extern void config_unnamed_free(void *f)
{
	config_unnamed_t *x = (config_unnamed_t *)f;
	if (x->response.type & CONFIG_ARG_STRING)
		free(x->response.value.string);
	free(x);
	return;
}

extern int config_parse_aux(int argc, char **argv, LIST args, LIST extra, LIST notes, bool warn)
{
	if (!init)
	{
		cli_eprintf(_("Call config_init() first\n"));
		return -1;
	}

	LIST largs = list_init((void *)strcmp, true, false);
	for (int i = 1; i < argc; i++) // from 1, skip invokation name
	{
		char *x = argv[i];
		if (x[0] == '-' && x[1] == '-')
		{
			// long option, check for '='
			char *o = strchr(x, '=');
			if (o)
			{
				list_append(largs, strndup(x, o - x));
				list_append(largs, strdup(o + 1));
			}
			else
				list_append(largs, strdup(argv[i]));
		}
		else if (x[0] == '-' && strlen(x) > 2)
		{
			// short option, check for anything after '-x...'
			list_append(largs, strndup(x, 2));
			list_append(largs, strdup(x + 2));
		}
		else
			list_append(largs, strdup(argv[i]));
	}
	/* handle help et al first */
	if (list_contains(largs, "-h") || list_contains(largs, "--help"))
		show_help(args, largs, notes, extra);
	if (list_contains(largs, "-v") || list_contains(largs, "--version"))
		show_version(args, largs, notes, extra);
	if (list_contains(largs, "-l") || list_contains(largs, "--licence"))
		show_licence(args, largs, notes, extra);

	/*
	 * check for options in rc file
	 */
	if (args && about.config != NULL)
	{
		char *rc = NULL;
#ifndef __DEBUG__
	#ifndef _WIN32
		if (about.config[0] == '/' || (about.config[0] == '.' && about.config[1] == '/'))
		{
			if (!(rc = strdup(about.config)))
				die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(about.config) + 1);
		}
		else if (!asprintf(&rc, "%s/%s", getenv("HOME") ? : ".", about.config))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(getenv("HOME")) + strlen(about.config) + 2);
	#else
		if (!(rc = calloc(MAX_PATH, sizeof( char ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, MAX_PATH);
		SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, rc);
		strcat(rc, "\\");
		strcat(rc, about.config);
	#endif
#else
		if (!(rc = strdup(about.config)))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(about.config) + 1);
#endif
		FILE *f = fopen(rc, "rb");
		if (f)
		{
			char *line = NULL;
			size_t len = 0;

			while (getline(&line, &len, f) >= 0)
			{
				if (len == 0 || line[0] == '#')
					goto end_line;

				// TODO handle unknown config file settings
				ITER iter = list_iterator(args);
				while (list_has_next(iter))
				{
					config_named_t *arg = (config_named_t *)list_get_next(iter);
					if (arg->long_option && !strncmp(arg->long_option, line, strlen(arg->long_option)) && isspace((unsigned char)line[strlen(arg->long_option)]))
						switch (arg->response.type)
						{
							/*
							 * optional values
							 */
							case CONFIG_ARG_OPT_BOOLEAN:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_BOOLEAN:
								arg->seen = parse_boolean(arg->long_option, line, &arg->response.value.boolean);
								break;

							case CONFIG_ARG_OPT_INTEGER:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_INTEGER:
								arg->seen = parse_integer(arg->long_option, line, &arg->response.value.integer);
								break;

							case CONFIG_ARG_OPT_DECIMAL:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_DECIMAL:
								arg->seen = parse_decimal(arg->long_option, line, &arg->response.value.decimal);
								break;

							case CONFIG_ARG_OPT_STRING:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_STRING:
								arg->seen = true;
								if (arg->response.value.string)
									free(arg->response.value.string);
								arg->response.value.string = parse_string(arg->long_option, line, arg->response.value.string);
								break;

							/*
							 * pairs of values
							 */
							case CONFIG_ARG_PAIR_BOOLEAN:
								arg->seen = parse_pair_boolean(arg->long_option, line, &arg->response.value.pair.boolean);
								break;

							case CONFIG_ARG_PAIR_INTEGER:
								arg->seen = parse_pair_integer(arg->long_option, line, &arg->response.value.pair.integer);
								break;

							case CONFIG_ARG_PAIR_DECIMAL:
								arg->seen = parse_pair_decimal(arg->long_option, line, &arg->response.value.pair.decimal);
								break;

							case CONFIG_ARG_PAIR_STRING:
								arg->seen = parse_pair_string(arg->long_option, line, &arg->response.value.pair.string);
								break;

							/*
							 * lists of values
							 */
							case CONFIG_ARG_LIST_BOOLEAN:
								if (!arg->seen && arg->response.value.list)
								{
									free(arg->response.value.list);
									arg->response.value.list = NULL;
								}
								arg->seen = true;
								if (!arg->response.value.list)
									arg->response.value.list = list_default();
								if (strchr(line, ','))
									parse_list(CONFIG_ARG_BOOLEAN, line + strlen(arg->long_option) + 1, arg->response.value.list);
								else
									parse_list_boolean(line + strlen(arg->long_option) + 1, arg->response.value.list);
								break;

							case CONFIG_ARG_LIST_INTEGER:
								if (!arg->seen && arg->response.value.list)
								{
									free(arg->response.value.list);
									arg->response.value.list = NULL;
								}
								arg->seen = true;
								if (!arg->response.value.list)
									arg->response.value.list = list_default();
								if (strchr(line, ','))
									parse_list(CONFIG_ARG_INTEGER, line + strlen(arg->long_option) + 1, arg->response.value.list);
								else
									parse_list_integer(line + strlen(arg->long_option) + 1, arg->response.value.list);
								break;

							case CONFIG_ARG_LIST_DECIMAL:
								if (!arg->seen && arg->response.value.list)
								{
									free(arg->response.value.list);
									arg->response.value.list = NULL;
								}
								arg->seen = true;
								if (!arg->response.value.list)
									arg->response.value.list = list_default();
								if (strchr(line, ','))
									parse_list(CONFIG_ARG_DECIMAL, line + strlen(arg->long_option) + 1, arg->response.value.list);
								else
									parse_list_decimal(line + strlen(arg->long_option) + 1, arg->response.value.list);
								break;

							case CONFIG_ARG_LIST_STRING:
								if (!arg->seen && arg->response.value.list)
								{
									free(arg->response.value.list);
									arg->response.value.list = NULL;
								}
								arg->seen = true;
								if (!arg->response.value.list)
									arg->response.value.list = list_default();
								if (strchr(line, ','))
									parse_list(CONFIG_ARG_STRING, line + strlen(arg->long_option) + 1, arg->response.value.list);
								else
									list_append(arg->response.value.list, parse_string(arg->long_option, line, NULL));
								break;

							/*
							 * lists of pairs
							 */
							case CONFIG_ARG_LIST_PAIR_STRING:
								{
									if (!arg->seen && arg->response.value.list)
									{
										free(arg->response.value.list);
										arg->response.value.list = NULL;
									}
									arg->seen = true;
									if (!arg->response.value.list)
										arg->response.value.list = list_default();
									// would this not be a map?
									pair_string_t *pair = malloc(sizeof( pair_string_t ));
									if (!pair)
										die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( pair_string_t ));
									if (parse_pair_string(arg->long_option, line, pair))
										if (!list_append(arg->response.value.list, pair))
											free(pair);
								}
								break;

							default:
								break;
						}
				}
				free(iter);
end_line:
				free(line);
				line = NULL;
				len = 0;
			}
			fclose(f);
			free(line);
		}
		free(rc);
	}

	for (size_t i = 0, j = 0; i < list_size(largs); i++)
	{
		bool unknown = true;
		const char *curr = list_get(largs, i);
		const char *next = list_get(largs, i + 1);

		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			config_named_t *arg = (config_named_t *)list_get_next(iter);
			if (is_argument(arg->short_option, arg->long_option, curr))
			{
				unknown = false;
				if (next && (arg->response.type & CONFIG_ARG_REQUIRED || next[0] != '-'))
					i++;
				else
					next = NULL;
				switch (arg->response.type)
				{
					case CONFIG_ARG_OPT_BOOLEAN:
						(void)0; // for Slackware's older GCC
						__attribute__((fallthrough)); /* allow fall-through; argument was seen */
					case CONFIG_ARG_REQ_BOOLEAN:
						arg->seen = true;
						if (next)
							parse_boolean(NULL, next, &arg->response.value.boolean);
						else
							arg->response.value.boolean = !arg->response.value.boolean;
						break;

					case CONFIG_ARG_OPT_INTEGER:
						arg->seen = true;
						if (!next)
							break;
						__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
					case CONFIG_ARG_REQ_INTEGER:
						arg->seen = parse_integer(NULL, next, &arg->response.value.integer);
						break;

					case CONFIG_ARG_OPT_DECIMAL:
						arg->seen = true;
						if (!next)
							break;
						__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
					case CONFIG_ARG_REQ_DECIMAL:
						arg->seen = parse_decimal(NULL, next, &arg->response.value.decimal);
						break;

					case CONFIG_ARG_OPT_STRING:
						arg->seen = true;
						if (!next)
							break;
						__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
					case CONFIG_ARG_REQ_STRING:
						arg->seen = true;
						if (arg->response.value.string)
							free(arg->response.value.string);
						if (!(arg->response.value.string = strdup(next)))
							die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(next) + 1);
						break;

					case CONFIG_ARG_LIST_BOOLEAN:
						if (!arg->seen && arg->response.value.list)
						{
							free(arg->response.value.list);
							arg->response.value.list = NULL;
						}
						arg->seen = true;
						if (!arg->response.value.list)
							arg->response.value.list = list_default();
						if (strchr(next, ','))
							parse_list(CONFIG_ARG_BOOLEAN, next, arg->response.value.list);
						else
							parse_list_boolean(next, arg->response.value.list);
						break;

					case CONFIG_ARG_LIST_INTEGER:
						if (!arg->seen && arg->response.value.list)
						{
							free(arg->response.value.list);
							arg->response.value.list = NULL;
						}
						arg->seen = true;
						if (!arg->response.value.list)
							arg->response.value.list = list_default();
						if (strchr(next, ','))
							parse_list(CONFIG_ARG_INTEGER, next, arg->response.value.list);
						else
							parse_list_integer(next, arg->response.value.list);
						break;

					case CONFIG_ARG_LIST_DECIMAL:
						if (!arg->seen && arg->response.value.list)
						{
							free(arg->response.value.list);
							arg->response.value.list = NULL;
						}
						arg->seen = true;
						if (!arg->response.value.list)
							arg->response.value.list = list_default();
						if (strchr(next, ','))
							parse_list(CONFIG_ARG_DECIMAL, next, arg->response.value.list);
						else
							parse_list_decimal(next, arg->response.value.list);
						break;

					case CONFIG_ARG_LIST_STRING:
						if (!arg->seen && arg->response.value.list)
						{
							free(arg->response.value.list);
							arg->response.value.list = NULL;
						}
						arg->seen = true;
						if (!arg->response.value.list)
							arg->response.value.list = list_default();
						if (strchr(next, ','))
						{
							char *s = strdup(next);
							if (!s)
								die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(next) + 1);
							char *u = strdup(strtok(s, ","));
							if (!list_append(arg->response.value.list, u))
								free(u);
							char *t = NULL;
							while ((t = strtok(NULL, ",")))
							{
								char *v = strdup(t);
								if (!list_append(arg->response.value.list, v))
									free(v);
							}
							free(s);
						}
						else
						{
							char *x = strdup(next);
							if (!list_append(arg->response.value.list, x))
								free(x);
						}
						break;

					/* TODO extend handling of pairs and list of pairs... */

					default:
						arg->response.value.boolean = !arg->response.value.boolean;
						break;
				}
				break;
			}
		}
		free(iter);

		if (unknown)
		{
			if (extra)
			{
				if (j >= list_size(extra))
				{
					config_unnamed_t *new = calloc(1, sizeof( config_unnamed_t ));
					new->response.type = CONFIG_ARG_STRING;
					list_append(extra, new);
				}
				config_unnamed_t *x = (config_unnamed_t *)list_get(extra, j);
				switch (x->response.type)
				{
					case CONFIG_ARG_BOOLEAN:
						x->seen = parse_boolean(NULL, curr, &x->response.value.boolean);
						break;
					case CONFIG_ARG_INTEGER:
						x->seen = parse_integer(NULL, curr, &x->response.value.integer);
						break;
					case CONFIG_ARG_DECIMAL:
						x->seen = parse_decimal(NULL, curr, &x->response.value.decimal);
						break;
					case CONFIG_ARG_STRING:
						if (!(x->response.value.string = strdup(curr)))
							die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(curr));
						break;
					default:
						break;
				}
				j++;
			}
			else if (warn)
				config_show_usage(args, extra);
		}
	}
	list_deinit(largs, free);

	int r = 0;
	ITER iter = list_iterator(args);
	while (list_has_next(iter))
	{
		config_named_t *arg = (config_named_t *)list_get_next(iter);
		if (arg->seen)
			r++;
		else if (arg->required && !arg->seen && warn)
		{
			cli_eprintf("Missing required argument \"%s\"\n", arg->description);
			config_show_usage(args, extra);
		}

	}
	free(iter);
	if (extra)
	{
		iter = list_iterator(extra);
		while (list_has_next(iter))
		{
			config_unnamed_t *arg = (config_unnamed_t *)list_get_next(iter);
			if (arg->seen)
				r++;
		}
		free(iter);
	}

	return r;
}

inline static bool is_argument(char s, const char *l, const char *a)
{
	if (strlen(a) == 2 && a[0] == '-' && a[1] == s)
		return true;
	if (strlen(a) > 2 && a[0] == '-' && a[1] == '-')
		return !strcmp(l, a + 2);
	return false;
}

inline static void format_section(char *s)
{
	cli_eprintf(ANSI_COLOUR_CYAN "%s" ANSI_COLOUR_RESET ":\n", s);
	return;
}

static void show_version(LIST args, LIST largs, LIST notes, LIST extra)
{
	version_print(about.name, about.version, about.url);
	while (version_is_checking)
		sleep(1);
	list_deinit(args);
	list_deinit(largs, free);
	list_deinit(notes);
	list_deinit(extra);
	exit(EXIT_SUCCESS);
}

inline static void print_usage(LIST args, LIST extra)
{
#ifndef _WIN32
	struct winsize ws;
	ioctl(STDERR_FILENO, TIOCGWINSZ, &ws);
	int max_width = ws.ws_col - strlen(about.name) - 2;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int max_width = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - strlen(about.name) - 2;
#endif
	if (max_width <= 0 || !isatty(STDERR_FILENO))
		max_width = CLI_MAX_WIDTH - strlen(about.name); // needed for MSYS2
	format_section(_("Usage"));
	int j = cli_eprintf("  " ANSI_COLOUR_GREEN "%s", about.name) - strlen(ANSI_COLOUR_GREEN) - 2;
	if (extra)
	{
		ITER iter = list_iterator(extra);
		while (list_has_next(iter))
		{
			const config_unnamed_t *x = (config_unnamed_t *)list_get_next(iter);
			if (x->required)
				j += cli_eprintf(ANSI_COLOUR_RED " <%s>" ANSI_COLOUR_RESET, x->description);
			else
				j+= cli_eprintf(ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, x->description);
		}
		free(iter);
		if (isatty(STDERR_FILENO))
			j -= (strlen(ANSI_COLOUR_RESET) + strlen(ANSI_COLOUR_WHITE));
	}
	if (args)
	{
		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_named_t *arg = list_get_next(iter);
			if (!arg->hidden)
			{
				if ((int)(j + 4 + (arg->option_type ? strlen(arg->option_type) : 0)) > max_width)
					j = cli_eprintf("\n%*s  ", (int)strlen(about.name), " ");
				if (arg->required)
					j += cli_eprintf(ANSI_COLOUR_RED " <-%c", arg->short_option);
				else
					j += cli_eprintf(ANSI_COLOUR_YELLOW " [-%c", arg->short_option);
				if (arg->option_type)
					j += cli_eprintf(" %s", arg->option_type);
				j += cli_eprintf("%c" ANSI_COLOUR_RESET, arg->required ? '>' : ']');
				if (isatty(STDERR_FILENO))
					j -= (strlen(ANSI_COLOUR_RESET) + strlen(ANSI_COLOUR_WHITE));
			}
		}
		free(iter);
	}
	cli_eprintf(ANSI_COLOUR_RESET "\n");
	return;
}

extern void config_show_usage(LIST args, LIST extra)
{
	print_usage(args, extra);
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

static void print_option(int indent, char sopt, char *lopt, char *type, char *def, bool req, char *desc)
{
	size_t z = indent - 8;
	if (lopt)
		z -= strlen(lopt);
	else
		z += 4;
	cli_eprintf("  " ANSI_COLOUR_WHITE "-%c" ANSI_COLOUR_RESET, sopt);
	if (lopt)
		cli_eprintf(", " ANSI_COLOUR_WHITE "--%s" ANSI_COLOUR_RESET, lopt);
	if (type)
	{
		if (req)
		{
			if (lopt)
				cli_eprintf(ANSI_COLOUR_WHITE "=" ANSI_COLOUR_RED "<%s>" ANSI_COLOUR_RESET, type);
			else
				cli_eprintf(" " ANSI_COLOUR_RED "<%s>" ANSI_COLOUR_RESET, type);
		}
		else
		{
			if (lopt)
				cli_eprintf(ANSI_COLOUR_YELLOW "[=%s]" ANSI_COLOUR_RESET, type);
			else
				cli_eprintf(ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, type);
		}
		z -= 3 + strlen(type);
	}
	cli_eprintf("%*s", (int)z, " ");

#ifndef _WIN32
	struct winsize ws;
	ioctl(STDERR_FILENO, TIOCGWINSZ, &ws);
	int max_width = ws.ws_col - 2;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int max_width = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - 2;
#endif
	if (max_width <= 0 || !isatty(STDERR_FILENO))
		max_width = CLI_MAX_WIDTH; // needed for MSYS2, but also sensible default if output is being redirected
	int width = max_width - indent;
	for (; isspace(*desc); desc++)
		;
	int l = strlen(desc);

#ifdef _WIN32
	char *tmp = calloc(l + 1, 1);
	if (!tmp)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, l + 1);
	for (int i = 0, j = 0; i < l; i++, j++)
		if (!strncmp(desc + i, "‘", strlen("‘")) || !strncmp(desc + i, "’", strlen("‘")))
		{
			tmp[j] = '\'';
			i += strlen("‘") - 1;
		}
		else
			tmp[j] = desc[i];
	desc = tmp;
#endif
	char *x_desc = NULL;
	if (def)
		asprintf(&x_desc, "%s" ANSI_COLOUR_WHITE " (default:" ANSI_COLOUR_GREEN " %s" ANSI_COLOUR_WHITE ")", desc, def);
	else
		x_desc = desc;
	l = strlen(x_desc);

	cli_eprintf(ANSI_COLOUR_BLUE);
	if (l < width)
		cli_eprintf("%s", x_desc);
	else if (width <= indent)
		cli_eprintf("\n  %s\n", x_desc);
	else
	{
		int s = 0;
		do
		{
			bool too_long = false;
			int e = s + width;
			if (e > l)
				e = l;
			else
				for (too_long = true; e > s; e--)
					if (isspace(x_desc[e]))
					{
						too_long = false;
						break;
					}
			if (too_long)
			{
				for (int e2 = s; e2 < s + max_width; e2++)
					if (isspace(x_desc[e2]) || x_desc[e2] == 0x00)
						e = e2;
				cli_eprintf("\n  ");
			}
			else if (s)
				cli_eprintf("\n%*s", indent, " ");
			cli_eprintf("%.*s", e - s, x_desc + s);
			s = e + 1;
		}
		while (s < l);
	}
#ifdef _WIN32
	free(tmp);
#endif
	if (def)
		free(x_desc);
	cli_eprintf(ANSI_COLOUR_RESET "\n");
	return;
}

static void print_notes(const char *line)
{
#ifndef _WIN32
	cli_eprintf("  • ");
	struct winsize ws;
	ioctl(STDERR_FILENO, TIOCGWINSZ, &ws);
	int max_width = ws.ws_col - 5;
#else
	cli_eprintf("  * ");
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int max_width = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - 5;
#endif
	if (max_width <= 0 || !isatty(STDERR_FILENO))
		max_width = 72; // needed for MSYS2, but also sensible default if output is being redirected
	for (; isspace(*line); line++)
		;
	int l = strlen(line);
	if (l < max_width)
		cli_eprintf("%s", line);
	else
	{
		int s = 0;
		do
		{
			bool too_long = false;
			int e = s + max_width;
			if (e > l)
				e = l;
			else
				for (too_long = true; e > s; e--)
					if (isspace(line[e]))
					{
						too_long = false;
						break;
					}
			if (too_long)
			{
				for (e = s; ; e++)
					if (isspace(line[e]) || line[e] == 0x00)
						break;
				cli_eprintf("\n  ");
			}
			else if (s)
				cli_eprintf("\n%*s", 4, " ");
			cli_eprintf("%.*s", e - s, line + s);
			s = e + 1;
		}
		while (s < l);
	}
	cli_eprintf(ANSI_COLOUR_RESET "\n");
	return;
}

static char *parse_default(config_arg_e type, config_arg_u value)
{
	char *d = NULL;
	switch (type)
	{
		case CONFIG_ARG_OPT_BOOLEAN:
			(void)0; // for Slackware's older GCC
			__attribute__((fallthrough)); /* allow fall-through */
		case CONFIG_ARG_REQ_BOOLEAN:
			d = strdup(value.boolean ? "true" : "false");
			break;
		case CONFIG_ARG_OPT_INTEGER:
			(void)0; // for Slackware's older GCC
			__attribute__((fallthrough)); /* allow fall-through */
		case CONFIG_ARG_REQ_INTEGER:
			asprintf(&d, "%'" PRIi64, (int64_t)value.integer);
			break;
		case CONFIG_ARG_OPT_DECIMAL:
			(void)0; // for Slackware's older GCC
			__attribute__((fallthrough)); /* allow fall-through */
		case CONFIG_ARG_REQ_DECIMAL:
			//{
			//	char buf[0xFF] = { 0x00 };
			//	strfromf128(buf, sizeof buf, "%'.9f", (__float128)value.decimal);
			//	asprintf(&d, "%s", buf);
			//}
			asprintf(&d, "%'Lf", (long double)value.decimal);
			break;
		case CONFIG_ARG_OPT_STRING:
			(void)0; // for Slackware's older GCC
			__attribute__((fallthrough)); /* allow fall-through */
		case CONFIG_ARG_REQ_STRING:
			d = strdup(value.string ? : "(null)");
			break;
		default: // all other defaults to be displayed should be string
			if (value.string)
				d = strdup((char *)value.string);
			break;
	}
	return d;
}

static void show_help(LIST args, LIST largs, LIST notes, LIST extra)
{
	version_print(about.name, about.version, about.url);
	cli_eprintf("\n");
	print_usage(args, extra);

	int indent = 10;
	bool has_advanced = false;
	if (args)
	{
		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_named_t *arg = list_get_next(iter);
			int w = 10 + (arg->long_option ? strlen(arg->long_option) : 0);
			if (arg->option_type)
				w += 3 + strlen(arg->option_type);
			indent = indent > w ? indent : w;
			if (arg->advanced && !arg->hidden)
				has_advanced = true;
		}
		free(iter);
	}
	else
		indent += 7;

	cli_eprintf("\n");
	format_section(_("Options"));
	print_option(indent, 'h', "help",    NULL, NULL, false, "Display this message");
	print_option(indent, 'l', "licence", NULL, NULL, false, "Display GNU GPL v3 licence header");
	print_option(indent, 'v', "version", NULL, NULL, false, "Display application version");
	if (args)
	{
		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_named_t *arg = list_get_next(iter);
			if (!arg->hidden && !arg->advanced)
			{
				char *def = parse_default(arg->response.type, arg->response.value);
				print_option(indent, arg->short_option, arg->long_option, arg->option_type ? : NULL, def, arg->response.type & CONFIG_ARG_REQUIRED, arg->description);
				if (def)
					free(def);
			}
		}
		free(iter);
	}
	if (has_advanced)
	{
		cli_eprintf("\n");
		format_section(_("Advanced Options"));
		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_named_t *arg = list_get_next(iter);
			if (!arg->hidden && arg->advanced)
			{
				char *def = parse_default(arg->response.type, arg->response.value);
				print_option(indent, arg->short_option, arg->long_option, arg->option_type ? : NULL, def, arg->response.type & CONFIG_ARG_REQUIRED, arg->description);
				if (def)
					free(def);
			}
		}
		free(iter);
	}
	if (notes)
	{
		cli_eprintf("\n");
		format_section(_("Notes"));
		ITER iter = list_iterator(notes);
		while (list_has_next(iter))
			print_notes(list_get_next(iter));
		free(iter);
	}
	while (version_is_checking)
		sleep(1);
	list_deinit(args);
	list_deinit(largs, free);
	list_deinit(notes);
	list_deinit(extra);
	exit(EXIT_SUCCESS);
}

static void show_licence(LIST args, LIST largs, LIST notes, LIST extra)
{
	cli_eprintf(_(TEXT_LICENCE));
	while (version_is_checking)
		sleep(1);
	list_deinit(args);
	list_deinit(largs, free);
	list_deinit(notes);
	list_deinit(extra);
	exit(EXIT_SUCCESS);
}

extern void update_config(const char * const restrict o, const char * const restrict v)
{
	if (!init)
	{
		cli_eprintf(_("Call config_init() first\n"));
		return;
	}
	char *rc = NULL;
#ifndef _WIN32
	if (!asprintf(&rc, "%s/%s", getenv("HOME") ? : ".", about.config))
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(getenv("HOME")) + strlen(about.config) + 2);
#else
	if (!(rc = calloc(MAX_PATH, sizeof( char ))))
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, MAX_PATH);
	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, rc);
	strcat(rc, "\\");
	strcat(rc, about.config);
#endif
	FILE *f = fopen(rc, "rb+");
	if (!f) /* file doesn’t exist, so create it */
		f = fopen(rc, "wb+");
	if (f)
	{
		FILE *t = tmpfile();
		char *line = NULL;
		size_t len = 0;
		bool found = false;

		for (int i = 0; i < 2; i++)
		{
			/*
			 * first iteration: read rc file and change the value second
			 * iteration: read from tmpfile back into rc file
			 */
			while (getline(&line, &len, f) >= 0)
			{
				if (!i && (!strncmp(o, line, strlen(o)) && isspace((unsigned char)line[strlen(o)])))
				{
					free(line);
					asprintf(&line, "%s %s\n", o, v);
					found = true;
				}
				cli_fprintf(t, "%s", line);
				free(line);
				line = NULL;
				len = 0;
			}
			fseek(f, 0, SEEK_SET);
			fseek(t, 0, SEEK_SET);
			if (!i)
				ftruncate(fileno(f), 0);
			FILE *z = f;
			f = t;
			t = z;
		}
		if (!found)
		{
			fseek(f, 0, SEEK_END);
			cli_fprintf(f, "\n%s %s\n", o, v);
		}
		fclose(f);
		free(line);
		fclose(t);
	}
	free(rc);
	return;
}

static bool parse_boolean(const char *c, const char *l, bool *v)
{
	bool r = false;
	char *n = parse_tail(c, l);
	if (n)
	{
		if (!strcasecmp(CONF_TRUE, n)
		 || !strcasecmp(CONF_ON, n)
		 || !strcasecmp(CONF_ENABLED, n)
		 || !strcasecmp(CONF_YES, n)
		 || !strcmp(CONF_ONE, n))
		{
			*v = true;
			r = true;
		}
		else if (!strcasecmp(CONF_FALSE, n) ||
			 !strcasecmp(CONF_OFF, n) ||
			 !strcasecmp(CONF_DISABLED, n) ||
			 !strcasecmp(CONF_NO, n) ||
			 !strcmp(CONF_ZERO, n))
		{
			*v = false;
			r = true;
		}
		free(n);
	}
	if (!r)
		cli_eprintf("invalid boolean value [%s]\n", l);
	return r;
}

static int64_t parse_number_size_suffix(const char *s)
{
	if (s[0] == ' ')
		return 1;
	int64_t r = 1;
	switch (s[0])
	{
		case 'E':
			r *= KILOBYTE;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'P':
			r *= KILOBYTE;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'T':
			r *= KILOBYTE;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'G':
			r *= KILOBYTE;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'M':
			r *= KILOBYTE;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'K':
			r *= KILOBYTE;
			break;
		case 'e':
			r *= THOUSAND;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'p':
			r *= THOUSAND;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 't':
			r *= THOUSAND;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'g':
			r *= THOUSAND;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'm':
			r *= THOUSAND;
			__attribute__((fallthrough)); /* allow fall-through; keep multiplying to get the correct size increase */
		case 'k':
			r *= THOUSAND;
			break;
		default:
			cli_eprintf("invalid size suffix [%c]\n", s[0]);
			break;
	}
	return r;
}

static bool parse_integer(const char *c, const char *l, int64_t *v)
{
	bool r = false;
	char *n = parse_tail(c, l);
	if (n)
	{
		char *e = NULL;
		*v = strtoull(n, &e, 0);
		if (e != n)
		{
			int64_t m = strlen(e) >= 1 ? parse_number_size_suffix(e) : 1;
			*v *= m;
			r = true;
		}
		free(n);
	}
	if (!r)
		cli_eprintf("invalid integer value [%s]\n", l);
	return r;
}

//static bool parse_decimal(const char *c, const char *l, __float128 *v)
static bool parse_decimal(const char *c, const char *l, long double *v)
{
	bool r = false;
	char *n = parse_tail(c, l);
	if (n)
	{
		char *e = NULL;
		*v = strtof128(n, &e);
		if (e != n)
		{
			int64_t m = strlen(e) >= 1 ? parse_number_size_suffix(e) : 1;
			*v *= m;
			r = true;
		}
		free(n);
	}
	if (!r)
		cli_eprintf("invalid decimal value [%s]\n", l);
	return r;
}

static char *parse_string(const char *c, const char *l, char *v)
{
	char *r = parse_tail(c, l);
	return r ? : v;
}

static char *parse_tail(const char *c, const char *l)
{
	if (!l)
		return NULL;
	char *x = strdup(l + (c ? strlen(c) : 0));
	if (!x)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(l) - strlen(c) + 1);
	size_t i = 0;
	for (i = 0; i < strlen(x) && isspace((unsigned char)x[i]); i++)
		;
	char *y = strdup(x + i);
	if (!y)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(x) - i + 1);
	free(x);
	for (i = strlen(y) - 1; i > 0 && isspace((unsigned char)y[i]); i--)
		;//y[i] = '\0';
	char *tail = strndup(y, i + 1);
	free(y);
	if (!tail)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, i + 1);
	return tail;
}

static bool parse_pair_boolean(const char *c, const char *l, pair_boolean_t *pair)
{
	pair_u *p = parse_pair(c, l);
	bool p1 = parse_boolean(NULL, p->string.s1, &pair->b1);
	bool p2 = parse_boolean(NULL, p->string.s2, &pair->b2);
	free(p->string.s1);
	free(p->string.s2);
	free(p);
	return p1 && p2;
}

static bool parse_pair_integer(const char *c, const char *l, pair_integer_t *pair)
{
	pair_u *p = parse_pair(c, l);
	bool p1 = parse_integer(NULL, p->string.s1, &pair->i1);
	bool p2 = parse_integer(NULL, p->string.s2, &pair->i2);
	free(p->string.s1);
	free(p->string.s2);
	free(p);
	return p1 && p2;
}

static bool parse_pair_decimal(const char *c, const char *l, pair_decimal_t *pair)
{
	pair_u *p = parse_pair(c, l);
	bool p1 = parse_decimal(NULL, p->string.s1, &pair->d1);
	bool p2 = parse_decimal(NULL, p->string.s2, &pair->d2);
	free(p->string.s1);
	free(p->string.s2);
	free(p);
	return p1 && p2;
}

static bool parse_pair_string(const char *c, const char *l, pair_string_t *pair)
{
	pair_u *p = parse_pair(c, l);
	pair->s1 = p->string.s1;
	pair->s2 = p->string.s2;
	free(p);
	return pair->s1 && pair->s2;
}

static pair_u *parse_pair(const char *c, const char *l)
{
	pair_u *pair = calloc(1, sizeof (pair_u));
	if (!pair)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, sizeof (pair_u));
	/* get everything after the parameter name */
	char *x = strdup(l + strlen(c));
	if (!x)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, strlen(l) - strlen(c) + 1);
	size_t i = 0, j = 0;
	/* skip past all whitespace */
	for (i = 0; i < strlen(x) && isspace((unsigned char)x[i]); i++)
		;
	char *y = x + i;
	/* find the end of the first value */
	if (y[0] == '"')
		for (i = 1, j = 1; i < strlen(y) && y[i] != '"'; i++)
			;
	else
		for (; i < strlen(y) && !isspace(y[i]); i++)
			;
	if (!(pair->string.s1 = strndup(y + j, i - j)))
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, i - j);
	/* skip past all whitespace */
	for (; i < strlen(y) && isspace((unsigned char)y[i]); i++)
		;
	char *z = y + i + j;
	/* remove any trailing whitespace */
	for (i = strlen(z) - 1; i > 0 && isspace((unsigned char)z[i]); i--)
		;//y[i] = '\0';
	if (z[j] == '"')
		j++;
	if (!(pair->string.s2 = strndup(z + j, i + (j ? -j : 1))))
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, i + (j ? -j : 1));
	/* all done */
	free(x);
	return pair;
}

static void parse_list_boolean(const char *text, LIST list)
{
	bool *r = malloc(sizeof( bool ));
	if (!r)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( bool ));
	if (parse_boolean(NULL, text, r))
	{
		if (!list_append(list, r))
			free(r);
	}
	else
		free(r);
	return;
}

static void parse_list_integer(const char *text, LIST list)
{
	int64_t *r = malloc(sizeof( int64_t ));
	if (!r)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( int64_t ));
	if (parse_integer(NULL, text, r))
	{
		if (!list_append(list, r))
			free(r);
	}
	else
		free(r);
	return;
}

static void parse_list_decimal(const char *text, LIST list)
{
	//__float128 *r = malloc(sizeof( __float128 ));
	long double *r = malloc(sizeof( long double ));
	if (!r)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( long double ));
	//die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( __float128 ));
	if (parse_decimal(NULL, text, r))
	{
		if (!list_append(list, r))
			free(r);
	}
	else
		free(r);
	return;
}

static void parse_list(config_arg_e type, const char *text, LIST list)
{
	while (*text && isspace(*text))
		text++;
	char *s = strdup(text);
	if (!s)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(text) + 1);
	char *t = s;
	char *u = s;
	while ((t = strtok(u, ",")))
	{
		switch (type)
		{
			case CONFIG_ARG_BOOLEAN:
				parse_list_boolean(t, list);
				break;

			case CONFIG_ARG_INTEGER:
				parse_list_integer(t, list);
				break;

			case CONFIG_ARG_DECIMAL:
				parse_list_decimal(t, list);
				break;

			case CONFIG_ARG_STRING:
				{
					char *v = strdup(t);
					if (!list_append(list, v))
						free(v);
				}
				break;

			default:
				break;
		}
		u = NULL;
	}
	free(s);
	return;
}
