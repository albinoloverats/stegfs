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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

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

#ifdef _WIN32
	#include <Shlobj.h>
	extern char *program_invocation_short_name;
#endif



static void show_version(void);
static void show_help(config_arg_t *args, char **about, config_extra_t *extra);
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

extern int config_parse_aux(int argc, char **argv, config_arg_t *args, config_extra_t *extra, char **notes, bool warn)
{
	if (!init)
	{
		fprintf(stderr, _("Call config_init() first\n"));
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
				for (int i = 0; args[i].short_option; i++)
					if (args[i].long_option && !strncmp(args[i].long_option, line, strlen(args[i].long_option)) && isspace((unsigned char)line[strlen(args[i].long_option)]))
						switch (args[i].response_type)
						{
							case CONFIG_ARG_OPT_BOOLEAN:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_BOOLEAN:
								args[i].response_value.boolean = parse_config_boolean(args[i].long_option, line, args[i].response_value.boolean);
								break;
							case CONFIG_ARG_OPT_NUMBER:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_NUMBER:
								args[i].response_value.number = parse_config_number(args[i].long_option, line, args[i].response_value.number);
								break;
							case CONFIG_ARG_OPT_STRING:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_STRING:
								args[i].response_value.string = parse_config_string(args[i].long_option, line, args[i].response_value.string);
								break;

							case CONFIG_ARG_PAIR_BOOLEAN:
								{
									config_pair_boolean_t *pair = parse_config_pair_boolean(args[i].long_option, line);
									args[i].response_value.pair.boolean.b1 = pair->b1;
									args[i].response_value.pair.boolean.b2 = pair->b2;
									free(pair);
								}
								break;

							case CONFIG_ARG_PAIR_NUMBER:
								{
									config_pair_number_t *pair = parse_config_pair_number(args[i].long_option, line);
									args[i].response_value.pair.number.n1 = pair->n1;
									args[i].response_value.pair.number.n2 = pair->n2;
									free(pair);
								}
								break;

							case CONFIG_ARG_PAIR_STRING:
								{
									config_pair_string_t *pair = parse_config_pair_string(args[i].long_option, line);
									args[i].response_value.pair.string.s1 = pair->s1;
									args[i].response_value.pair.string.s2 = pair->s2;
									free(pair);
								}
								break;

							case CONFIG_ARG_LIST_STRING:
								args[i].response_value.list.count++;
								args[i].response_value.list.items = realloc(args[i].response_value.list.items, args[i].response_value.list.count * sizeof (config_list_u));
								args[i].response_value.list.items[args[i].response_value.list.count - 1].string = parse_config_string(args[i].long_option, line, NULL);
								break;

							case CONFIG_ARG_LIST_PAIR_STRING:
								{
									args[i].response_value.list.count++;
									args[i].response_value.list.items = realloc(args[i].response_value.list.items, args[i].response_value.list.count * sizeof (config_list_u));
									config_pair_string_t *pair = parse_config_pair_string(args[i].long_option, line);
									args[i].response_value.list.items[args[i].response_value.list.count - 1].pair.string.s1 = pair->s1;
									args[i].response_value.list.items[args[i].response_value.list.count - 1].pair.string.s2 = pair->s2;
									free(pair);
								}
								break;
						}
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
		int optlen = 4;
		for (int i = 0; args[i].short_option; i++, optlen += 1)
			;
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

		for (int i = 0; args[i].short_option; i++)
		{
			char S[] = "X";
			S[0] = args[i].short_option;
			strcat(short_options, S);
			if (args[i].response_type != CONFIG_ARG_REQ_BOOLEAN && args[i].response_type != CONFIG_ARG_OPT_BOOLEAN)
				strcat(short_options, ":");
			long_options[i + 3].name = args[i].long_option;

			if (args[i].response_type == CONFIG_ARG_REQ_BOOLEAN || args[i].response_type == CONFIG_ARG_OPT_BOOLEAN)
				long_options[i + 3].has_arg = no_argument;
			else if (args[i].response_type & CONFIG_ARG_REQUIRED)
				long_options[i + 3].has_arg = required_argument;
			else
				long_options[i + 3].has_arg = optional_argument;
			long_options[i + 3].flag = NULL;
			long_options[i + 3].val  = args[i].short_option;
		}

		/*
		 * parse command line options
		 */
		while (argc && argv)
		{
			int index = 0;
			int c = getopt_long(argc, argv, short_options, long_options, &index);
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
				for (int i = 0; args[i].short_option; i++)
					if (c == args[i].short_option)
					{
						unknown = false;
						switch (args[i].response_type)
						{
							case CONFIG_ARG_OPT_NUMBER:
								if (!optarg)
									break;
								__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
							case CONFIG_ARG_REQ_NUMBER:
								args[i].response_value.number = strtoull(optarg, NULL, 0);
								break;
							case CONFIG_ARG_OPT_STRING:
								if (!optarg)
									break;
								__attribute__((fallthrough)); /* allow fall-through; argument was seen and has a value */
							case CONFIG_ARG_REQ_STRING:
								if (args[i].response_value.string)
									free(args[i].response_value.string);
								args[i].response_value.string = strdup(optarg);
								break;
							case CONFIG_ARG_OPT_BOOLEAN:
								(void)0; // for Slackware's older GCC
								__attribute__((fallthrough)); /* allow fall-through; argument was seen */
							case CONFIG_ARG_REQ_BOOLEAN:
								args[i].response_value.boolean = !args[i].response_value.boolean;
								break;

							/* TODO extend handling of lists and pairs and list of pairs... */

							case CONFIG_ARG_LIST_STRING:
								args[i].response_value.list.count++;
								args[i].response_value.list.items = realloc(args[i].response_value.list.items, args[i].response_value.list.count * sizeof (config_list_u));
								if (strchr(optarg, ','))
								{
									char *s = strdup(optarg);
									args[i].response_value.list.items[args[i].response_value.list.count - 1].string = strdup(strtok(s, ","));
									char *t = NULL;
									while ((t = strtok(NULL, ",")))
									{
										args[i].response_value.list.count++;
										args[i].response_value.list.items = realloc(args[i].response_value.list.items, args[i].response_value.list.count * sizeof (config_list_u));
										args[i].response_value.list.items[args[i].response_value.list.count - 1].string = strdup(t);
									}
									free(s);
								}
								else
									args[i].response_value.list.items[args[i].response_value.list.count - 1].string = strdup(optarg);
								break;

							default:
								args[i].response_value.boolean = !args[i].response_value.boolean;
								break;
						}
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
		for (int i = 0; extra[i].description && optind < argc; i++, optind++)
		{
			extra[i].seen = true;
			switch (extra[i].response_type)
			{
				case CONFIG_ARG_STRING:
					if (!(extra[i].response_value.string = strdup(argv[optind])))
						die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(argv[optind]));
					break;
				case CONFIG_ARG_NUMBER:
					extra[i].response_value.number = strtoull(argv[optind], NULL, 0);
					break;
				case CONFIG_ARG_BOOLEAN:
					(void)0; // for Slackware's older GCC
					__attribute__((fallthrough)); /* allow fall-through; argument was seen */
				default:
					extra[i].response_value.boolean = true;
					break;
			}
		}
		r = argc - optind;
		for (int i = 0; extra[i].description; i++)
			if (extra[i].required && !extra[i].seen && warn)
			{
				cli_fprintf(stderr, "Missing required argument \"%s\"\n", extra[i].description);
				config_show_usage(args, extra);
			}
	}
	return r;
}

inline static void format_section(char *s)
{
	cli_fprintf(stderr, ANSI_COLOUR_CYAN "%s" ANSI_COLOUR_RESET ":\n", s);
	return;
}

static void show_version(void)
{
	version_print(about.name, about.version, about.url);
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

inline static void print_usage(config_arg_t *args, config_extra_t *extra)
{
#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	size_t x = ws.ws_col - strlen(about.name) - 2;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int w = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - strlen(about.name) - 2;
	if (w <= 0)
		w = 77 - strlen(about.name); // needed for MSYS2
	size_t x = (size_t)w;
#endif
	format_section(_("Usage"));
	cli_fprintf(stderr, "  " ANSI_COLOUR_GREEN "%s", about.name);
	if (extra)
	{
		for (int i = 0; extra[i].description; i++)
			if (extra[i].required)
				cli_fprintf(stderr, ANSI_COLOUR_RED " <%s>" ANSI_COLOUR_RESET, extra[i].description);
			else
				cli_fprintf(stderr, ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, extra[i].description);
	}
	if (args)
		for (int i = 0, j = 0; args[i].short_option; i++)
		{
			if (!args[i].hidden)
			{
				if (j + 4 + (args[i].option_type ? strlen(args[i].option_type) : 0) > x)
				{
					cli_fprintf(stderr, "\n%*s  ", (int)strlen(about.name), " ");
					j = 2;
				}
				if (args[i].required)
					j += cli_fprintf(stderr, ANSI_COLOUR_RED " <-%c", args[i].short_option);
				else
					j += cli_fprintf(stderr, ANSI_COLOUR_YELLOW " [-%c", args[i].short_option);
				if (args[i].option_type)
					j += cli_fprintf(stderr, " %s", args[i].option_type);
				j += cli_fprintf(stderr, "%c" ANSI_COLOUR_RESET, args[i].required ? '>' : ']');
			}
		}
	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

extern void config_show_usage(config_arg_t *args, config_extra_t *extra)
{
	print_usage(args, extra);
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

static void print_option(int width, char sopt, char *lopt, char *type, bool req, char *desc)
{
	size_t z = width - 8;
	if (lopt)
		z -= strlen(lopt);
	else
		z += 4;
	cli_fprintf(stderr, "  " ANSI_COLOUR_WHITE "-%c" ANSI_COLOUR_RESET, sopt);
	if (lopt)
		cli_fprintf(stderr, ", " ANSI_COLOUR_WHITE "--%s" ANSI_COLOUR_RESET, lopt);
	if (type)
	{
		if (req)
		{
			if (lopt)
				cli_fprintf(stderr, ANSI_COLOUR_WHITE "=" ANSI_COLOUR_RED "<%s>" ANSI_COLOUR_RESET, type);
			else
				cli_fprintf(stderr, " " ANSI_COLOUR_RED "<%s>" ANSI_COLOUR_RESET, type);
		}
		else
		{
			if (lopt)
				cli_fprintf(stderr, ANSI_COLOUR_YELLOW "[=%s]" ANSI_COLOUR_RESET, type);
			else
				cli_fprintf(stderr, ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, type);
		}
		z -= 3 + strlen(type);
	}
	fprintf(stderr, "%*s", (int)z, " ");

#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	int x = ws.ws_col - width - 2;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int x = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - width - 2;
	if (x <= 0)
		x = 77 - width; // needed for MSYS2
#endif
	for (; isspace(*desc); desc++)
		;
	int l = strlen(desc);
	cli_fprintf(stderr, ANSI_COLOUR_BLUE);
	if (l < x)
		cli_fprintf(stderr, "%s", desc);
	else
	{
		int s = 0;
		do
		{
			int e = s + x;
			if (e > l)
				e = l;
			else
				for (; e > s; e--)
					if (isspace(desc[e]))
						break;
			if (s)
				cli_fprintf(stderr, "\n%*s", width, " ");
			cli_fprintf(stderr, "%.*s", e - s, desc + s);
			s = e + 1;
		}
		while (s < l);
	}
	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

static void print_notes(char *line)
{
#ifndef _WIN32
	cli_fprintf(stderr, "  • ");
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	int x = ws.ws_col - 5;
#else
	cli_fprintf(stderr, "  * ");
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int x = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - 5;
	if (x <= 0)
		x = 72; // needed for MSYS2
#endif
	for (; isspace(*line); line++)
		;
	int l = strlen(line);
	if (l < x)
		cli_fprintf(stderr, "%s", line);
	else
	{
		int s = 0;
		do
		{
			int e = s + x;
			if (e > l)
				e = l;
			else
				for (; e > s; e--)
					if (isspace(line[e]))
						break;
			if (s)
				cli_fprintf(stderr, "\n%*s", 4, " ");
			cli_fprintf(stderr, "%.*s", e - s, line + s);
			s = e + 1;
		}
		while (s < l);
	}
	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

static void show_help(config_arg_t *args, char **notes, config_extra_t *extra)
{
	version_print(about.name, about.version, about.url);
	cli_fprintf(stderr, "\n");
	print_usage(args, extra);

	int width = 10;
	bool has_advanced = false;
	for (int i = 0; args[i].short_option; i++)
	{
		int w = 10 + (args[i].long_option ? strlen(args[i].long_option) : 0);
		if (args[i].option_type)
			w += 3 + strlen(args[i].option_type);
		width = width > w ? width : w;
		if (args[i].advanced && !args[i].hidden)
			has_advanced = true;
	}

	cli_fprintf(stderr, "\n");
	format_section(_("Options"));
	print_option(width, 'h', "help",    NULL, false, "Display this message");
	print_option(width, 'l', "licence", NULL, false, "Display GNU GPL v3 licence header");
	print_option(width, 'v', "version", NULL, false, "Display application version");
	for (int i = 0; args[i].short_option; i++)
		if (!args[i].hidden && !args[i].advanced)
			print_option(width, args[i].short_option, args[i].long_option, args[i].option_type ? : NULL, args[i].response_type & CONFIG_ARG_REQUIRED, args[i].description);
	if (has_advanced)
	{
		cli_fprintf(stderr, "\n");
		format_section(_("Advnaced Options"));
		for (int i = 0; args[i].short_option; i++)
			if (!args[i].hidden && args[i].advanced)
				print_option(width, args[i].short_option, args[i].long_option, args[i].option_type ? : NULL, args[i].response_type & CONFIG_ARG_REQUIRED, args[i].description);
	}
	if (notes)
	{
		cli_fprintf(stderr, "\n");
		format_section(_("Notes"));
		for (int i = 0; notes[i] ; i++)
			print_notes(notes[i]);
	}
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

static void show_licence(void)
{
	fprintf(stderr, _(TEXT_LICENCE));
	while (version_is_checking)
		sleep(1);
	exit(EXIT_SUCCESS);
}

extern void update_config(const char * const restrict o, const char * const restrict v)
{
	if (!init)
	{
		fprintf(stderr, _("Call config_init() first\n"));
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
				fprintf(t, "%s", line);

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
			fprintf(f, "\n%s %s\n", o, v);
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
