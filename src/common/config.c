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
#include "list.h"

#ifdef _WIN32
	#include <Shlobj.h>
	extern char *program_invocation_short_name;
#endif



static void show_version(void);
static void show_help(LIST args, LIST about, LIST extra);
static void show_licence(void);

static bool    parse_config_boolean(const char *, const char *, bool);
static int64_t parse_config_number(const char *, const char *, int64_t);
static char   *parse_config_string(const char *, const char *, char *);

static config_pair_boolean_t *parse_config_pair_boolean(const char *c, const char *l);
static config_pair_number_t  *parse_config_pair_number(const char *c, const char *l);
static config_pair_string_t  *parse_config_pair_string(const char *c, const char *l);

static config_pair_u *parse_config_pair(const char *c, const char *l);
static char *parse_config_tail(const char *, const char *);

static bool init = false;
static config_about_t about = { 0x0 };


extern void config_init(config_about_t a)
{
	init = true;
	memcpy(&about, &a, sizeof about);
	return;
}

extern int config_arg_comp(const void *a, const void *b)
{
	const config_arg_t *x = a;
	const config_arg_t *y = b;
	return x->short_option - y->short_option;
}

extern int config_parse_aux(int argc, char **argv, LIST args, LIST extra, LIST notes, bool warn)
{
	if (!init)
	{
		cli_eprintf(_("Call config_init() first\n"));
		return -1;
	}
	/*
	 * check for options in rc file
	 */
	if (args && about.config != NULL)
	{
		char *rc = NULL;
#ifndef _WIN32
		if (about.config[0] == '/')
			rc = strdup(about.config);
		else if (!asprintf(&rc, "%s/%s", getenv("HOME") ? : ".", about.config))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(getenv("HOME")) + strlen(about.config) + 2);
#else
		if (!(rc = calloc(MAX_PATH, sizeof( char ))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, MAX_PATH);
		SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, rc);
		strcat(rc, "\\");
		strcat(rc, about.config);
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
					config_arg_t *arg = (config_arg_t *)list_get_next(iter);
					if (arg->long_option && !strncmp(arg->long_option, line, strlen(arg->long_option)) && isspace((unsigned char)line[strlen(arg->long_option)]))
						switch (arg->response_type)
						{
							case CONFIG_ARG_OPT_BOOLEAN:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_BOOLEAN:
								arg->response_value.boolean = parse_config_boolean(arg->long_option, line, arg->response_value.boolean);
								break;
							case CONFIG_ARG_OPT_NUMBER:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_NUMBER:
								arg->response_value.number = parse_config_number(arg->long_option, line, arg->response_value.number);
								break;
							case CONFIG_ARG_OPT_STRING:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_STRING:
								arg->response_value.string = parse_config_string(arg->long_option, line, arg->response_value.string);
								break;

							case CONFIG_ARG_PAIR_BOOLEAN:
								{
									config_pair_boolean_t *pair = parse_config_pair_boolean(arg->long_option, line);
									arg->response_value.pair.boolean.b1 = pair->b1;
									arg->response_value.pair.boolean.b2 = pair->b2;
									free(pair);
								}
								break;

							case CONFIG_ARG_PAIR_NUMBER:
								{
									config_pair_number_t *pair = parse_config_pair_number(arg->long_option, line);
									arg->response_value.pair.number.n1 = pair->n1;
									arg->response_value.pair.number.n2 = pair->n2;
									free(pair);
								}
								break;

							case CONFIG_ARG_PAIR_STRING:
								{
									config_pair_string_t *pair = parse_config_pair_string(arg->long_option, line);
									arg->response_value.pair.string.s1 = pair->s1;
									arg->response_value.pair.string.s2 = pair->s2;
									free(pair);
								}
								break;

							case CONFIG_ARG_LIST_STRING:
								if (!arg->response_value.list)
									arg->response_value.list = list_default();
								list_append(arg->response_value.list, parse_config_string(arg->long_option, line, NULL));
								break;

							case CONFIG_ARG_LIST_PAIR_STRING:
								if (!arg->response_value.list)
									arg->response_value.list = list_default();
								list_append(arg->response_value.list, parse_config_pair_string(arg->long_option, line));
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

	if (args)
	{
		/*
		 * build and populate the getopt structure
		 */
		char *short_options;
		int optlen = 4 + list_size(args);
		if (!(short_options = calloc(optlen * 2, sizeof (char))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, optlen * 2 * sizeof (char));
		struct option *long_options;
		if (!(long_options = calloc(optlen + 1, sizeof (struct option))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, (optlen + 1) * sizeof (struct option));

		strcat(short_options, "h");
		long_options[0].name    = "help";
		long_options[0].has_arg = no_argument;
		long_options[0].flag    = NULL;
		long_options[0].val     = 'h';

		strcat(short_options, "v");
		long_options[1].name    = "version";
		long_options[1].has_arg = no_argument;
		long_options[1].flag    = NULL;
		long_options[1].val     = 'v';

		strcat(short_options, "l");
		long_options[2].name    = "licence";
		long_options[2].has_arg = no_argument;
		long_options[2].flag    = NULL;
		long_options[2].val     = 'l';

		for (size_t i = 0; i < list_size(args); i++)
		{
			const config_arg_t *arg = list_get(args, i);
			if (isalnum(arg->short_option))
			{
				char S[] = "X";
				S[0] = arg->short_option;
				strcat(short_options, S);
			}
			if (arg->response_type != CONFIG_ARG_REQ_BOOLEAN && arg->response_type != CONFIG_ARG_OPT_BOOLEAN)
				strcat(short_options, ":");
			long_options[i + 3].name = arg->long_option;

			if (arg->response_type == CONFIG_ARG_REQ_BOOLEAN || arg->response_type == CONFIG_ARG_OPT_BOOLEAN)
				long_options[i + 3].has_arg = no_argument;
			else if (arg->response_type & CONFIG_ARG_REQUIRED)
				long_options[i + 3].has_arg = required_argument;
			else
				long_options[i + 3].has_arg = optional_argument;
			long_options[i + 3].flag = NULL;
			long_options[i + 3].val  = arg->short_option;
		}

		/*
		 * parse command line options
		 */
		while (argc && argv)
		{
			int c = getopt_long(argc, argv, short_options, long_options, &((int){0}));
			if (c == -1)
				break;
			bool unknown = true;
			if (c == 'h')
				show_help(args, notes, extra);
			else if (c == 'v')
				show_version();
			else if (c == 'l')
				show_licence();
			else if (c == '?' && warn)
				config_show_usage(args, extra);
			else
			{
				ITER iter = list_iterator(args);
				while (list_has_next(iter))
				{
					config_arg_t *arg = (config_arg_t *)list_get_next(iter);
					if (c == arg->short_option)
					{
						unknown = false;
						switch (arg->response_type)
						{
							case CONFIG_ARG_OPT_NUMBER:
								if (!optarg)
									break;
								__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
							case CONFIG_ARG_REQ_NUMBER:
								arg->response_value.number = strtoull(optarg, NULL, 0);
								break;
							case CONFIG_ARG_OPT_STRING:
								if (!optarg)
									break;
								__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
							case CONFIG_ARG_REQ_STRING:
								if (arg->response_value.string)
									free(arg->response_value.string);
								arg->response_value.string = strdup(optarg);
								break;
							case CONFIG_ARG_OPT_BOOLEAN:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through; argument was seen */
							case CONFIG_ARG_REQ_BOOLEAN:
								arg->response_value.boolean = !arg->response_value.boolean;
								break;

							/* TODO extend handling of lists and pairs and list of pairs... */

							case CONFIG_ARG_LIST_STRING:

								if (!arg->response_value.list)
									arg->response_value.list = list_default();

								if (strchr(optarg, ','))
								{
									char *s = strdup(optarg);
									list_append(arg->response_value.list, strdup(strtok(s, ",")));
									char *t = NULL;
									while ((t = strtok(NULL, ",")))
										list_append(arg->response_value.list, strdup(t));
									free(s);
								}
								else
									list_append(arg->response_value.list, strdup(optarg));
								break;

							default:
								arg->response_value.boolean = !arg->response_value.boolean;
								break;
						}
					}
				}
				free(iter);
			}
			if (unknown && warn)
				config_show_usage(args, extra);
		}
		free(short_options);
		free(long_options);
	}
	int r = 0;
	if (extra)
	{
		ITER iter = list_iterator(extra);
		for (int i = 0; list_has_next(iter) && optind < argc; i++, optind++)
		{
			config_extra_t *x = (config_extra_t *)list_get_next(iter);
			x->seen = true;
			switch (x->response_type)
			{
				case CONFIG_ARG_STRING:
					if (!(x->response_value.string = strdup(argv[optind])))
						die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(argv[optind]));
					break;
				case CONFIG_ARG_NUMBER:
					x->response_value.number = strtoull(argv[optind], NULL, 0);
					break;
				case CONFIG_ARG_BOOLEAN:
					(void)0; // for Slackware's older GCC
					__attribute__((fallthrough)); /* allow fall-through; argument was seen */
				default:
					x->response_value.boolean = true;
					break;
			}
		}
		free(iter);
		r = argc - optind;
		iter = list_iterator(extra);
		while (list_has_next(iter))
		{
			const config_extra_t *x = (config_extra_t *)list_get_next(iter);
			if (x->required && !x->seen && warn)
			{
				cli_eprintf("Missing required argument \"%s\"\n", x->description);
				config_show_usage(args, extra);
			}
		}
	}
	return r;
}

inline static void format_section(char *s)
{
	cli_eprintf(ANSI_COLOUR_CYAN "%s" ANSI_COLOUR_RESET ":\n", s);
	return;
}

static void show_version(void)
{
	version_print(about.name, about.version, about.url);
	while (version_is_checking)
		sleep(1);
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
		max_width = 77 - strlen(about.name); // needed for MSYS2
	format_section(_("Usage"));
	int j = cli_eprintf("  " ANSI_COLOUR_GREEN "%s", about.name) - strlen(ANSI_COLOUR_GREEN) - 2;
	if (extra)
	{
		ITER iter = list_iterator(extra);
		while (list_has_next(iter))
		{
			const config_extra_t *x = (config_extra_t *)list_get_next(iter);
			if (x->required)
				j += cli_eprintf(ANSI_COLOUR_RED " <%s>" ANSI_COLOUR_RESET, x->description);
			else
				j+= cli_eprintf(ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, x->description);
		}
		if (isatty(STDERR_FILENO))
			j -= (strlen(ANSI_COLOUR_RESET) + strlen(ANSI_COLOUR_WHITE));
		free(iter);
	}
	if (args)
	{
		ITER iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_arg_t *arg = list_get_next(iter);
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

static void print_option(int indent, char sopt, char *lopt, char *type, bool req, char *desc)
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
		max_width = 77; // needed for MSYS2, but also sensible default if output is being redirected
	int width = max_width - indent;
	for (; isspace(*desc); desc++)
		;
	int l = strlen(desc);

#ifdef _WIN32
	char *tmp = calloc(l + 1, 1);
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

	cli_eprintf(ANSI_COLOUR_BLUE);
	if (l < width)
		cli_eprintf("%s", desc);
	else if (width <= indent)
		cli_eprintf("\n  %s\n", desc);
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
					if (isspace(desc[e]))
					{
						too_long = false;
						break;
					}
			if (too_long)
			{
				for (int e2 = s; e2 < s + max_width; e2++)
					if (isspace(desc[e2]) || desc[e2] == 0x00)
						e = e2;
				cli_eprintf("\n  ");
			}
			else if (s)
				cli_eprintf("\n%*s", indent, " ");
			cli_eprintf("%.*s", e - s, desc + s);
			s = e + 1;
		}
		while (s < l);
	}
#ifdef _WIN32
	free(tmp);
#endif
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

static void show_help(LIST args, LIST notes, LIST extra)
{
	version_print(about.name, about.version, about.url);
	cli_eprintf("\n");
	print_usage(args, extra);

	int indent = 10;
	bool has_advanced = false;
	ITER iter = list_iterator(args);
	while (list_has_next(iter))
	{
		const config_arg_t *arg = list_get_next(iter);
		int w = 10 + (arg->long_option ? strlen(arg->long_option) : 0);
		if (arg->option_type)
			w += 3 + strlen(arg->option_type);
		indent = indent > w ? indent : w;
		if (arg->advanced && !arg->hidden)
			has_advanced = true;
	}
	free(iter);

	cli_eprintf("\n");
	format_section(_("Options"));
	print_option(indent, 'h', "help",    NULL, false, "Display this message");
	print_option(indent, 'l', "licence", NULL, false, "Display GNU GPL v3 licence header");
	print_option(indent, 'v', "version", NULL, false, "Display application version");
	iter = list_iterator(args);
	while (list_has_next(iter))
	{
		const config_arg_t *arg = list_get_next(iter);
		if (!arg->hidden && !arg->advanced)
			print_option(indent, arg->short_option, arg->long_option, arg->option_type ? : NULL, arg->response_type & CONFIG_ARG_REQUIRED, arg->description);
	}
	free(iter);
	if (has_advanced)
	{
		cli_eprintf("\n");
		format_section(_("Advanced Options"));
		iter = list_iterator(args);
		while (list_has_next(iter))
		{
			const config_arg_t *arg = list_get_next(iter);
			if (!arg->hidden && arg->advanced)
				print_option(indent, arg->short_option, arg->long_option, arg->option_type ? : NULL, arg->response_type & CONFIG_ARG_REQUIRED, arg->description);
		}
		free(iter);
	}
	if (notes)
	{
		cli_eprintf("\n");
		format_section(_("Notes"));
		iter = list_iterator(notes);
		while (list_has_next(iter))
			print_notes(list_get_next(iter));
		free(iter);
	}
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

static void show_licence(void)
{
	cli_eprintf(_(TEXT_LICENCE));
	while (version_is_checking)
		sleep(1);
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

static bool parse_config_boolean(const char *c, const char *l, bool d)
{
	bool r = d;
	char *v = parse_config_tail(c, l);
	if (v)
	{
		if (!strcasecmp(CONF_TRUE, v) || !strcasecmp(CONF_ON, v) || !strcasecmp(CONF_ENABLED, v))
			r = true;
		else if (!strcasecmp(CONF_FALSE, v) || !strcasecmp(CONF_OFF, v) || !strcasecmp(CONF_DISABLED, v))
			r = false;
		free(v);
	}
	return r;
}

static int64_t parse_config_number(const char *c, const char *l, int64_t d)
{
	int64_t r = d;
	char *n = parse_config_tail(c, l);
	if (n)
	{
		r = strtoull(n, NULL, 0);
		free(n);
	}
	return r;
}

static char *parse_config_string(const char *c, const char *l, char *d)
{
	char *r = parse_config_tail(c, l);
	return r ? : d;
}

static char *parse_config_tail(const char *c, const char *l)
{
	char *x = strdup(l + strlen(c));
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
	return tail;
}

static config_pair_boolean_t *parse_config_pair_boolean(const char *c, const char *l)
{
	config_pair_boolean_t *pair = calloc(1, sizeof (config_pair_boolean_t));
	if (!pair)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, sizeof (config_pair_boolean_t));
	config_pair_u *p = parse_config_pair(c, l);
	/* FIXME this only verifies is a value is true, and doesn't default like above */
	pair->b1 = (!strcasecmp(CONF_TRUE, p->string.s1) || !strcasecmp(CONF_ON, p->string.s1) || !strcasecmp(CONF_ENABLED, p->string.s1));
	pair->b2 = (!strcasecmp(CONF_TRUE, p->string.s2) || !strcasecmp(CONF_ON, p->string.s2) || !strcasecmp(CONF_ENABLED, p->string.s2));
	free(p->string.s1);
	free(p->string.s2);
	free(p);
	return pair;
}

static config_pair_number_t *parse_config_pair_number(const char *c, const char *l)
{
	config_pair_number_t *pair = calloc(1, sizeof (config_pair_number_t));
	if (!pair)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, sizeof (config_pair_number_t));
	config_pair_u *p = parse_config_pair(c, l);
	pair->n1 = strtoull(p->string.s1, NULL, 0);
	pair->n2 = strtoull(p->string.s2, NULL, 0);
	free(p->string.s1);
	free(p->string.s2);
	free(p);
	return pair;
}

static config_pair_string_t *parse_config_pair_string(const char *c, const char *l)
{
	config_pair_string_t *pair = calloc(1, sizeof (config_pair_string_t));
	if (!pair)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, sizeof (config_pair_string_t));
	config_pair_u *p = parse_config_pair(c, l);
	pair->s1 = p->string.s1;
	pair->s2 = p->string.s2;
	free(p);
	return pair;
}

static config_pair_u *parse_config_pair(const char *c, const char *l)
{
	config_pair_u *pair = calloc(1, sizeof (config_pair_u));
	if (!pair)
		die("Out of memory @ %s:%d:%s [%zu]", __FILE__, __LINE__, __func__, sizeof (config_pair_u));
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
	pair->string.s1 = strndup(y + j, i - j);
	/* skip past all whitespace */
	for (; i < strlen(y) && isspace((unsigned char)y[i]); i++)
		;
	char *z = y + i + j;
	/* remove any trailing whitespace */
	for (i = strlen(z) - 1; i > 0 && isspace((unsigned char)z[i]); i--)
		;//y[i] = '\0';
	if (z[j] == '"')
		j++;
	pair->string.s2 = strndup(z + j, i + (j ? -j : 1));
	/* all done */
	free(x);
	return pair;
}
