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

#include "crypt.h"


static void show_version(void);
static void show_help(config_arg_t *args, char **about, char **extra);
static void show_licence(void);

static bool parse_config_boolean(const char *, const char *, bool);
static char *parse_config_tail(const char *, const char *);

static bool init = false;
static config_about_t about = { 0x0 };


extern void config_init(config_about_t a)
{
	init = true;
	memcpy(&about, &a, sizeof about);
	return;
}

extern int config_parse(int argc, char **argv, config_arg_t *args, char ***extra, char **notes)
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
		if (!asprintf(&rc, "%s/%s", getenv("HOME") ? : ".", about.config))
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

				for (int i = 0; args[i].short_option; i++)
					if (!strncmp(args[i].long_option, line, strlen(args[i].long_option)) && isspace((unsigned char)line[strlen(args[i].long_option)]))
						switch (args[i].response_type)
						{
							case CONFIG_ARG_OPT_BOOLEAN:
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_BOOLEAN:
								args[i].response_value.boolean = parse_config_boolean(args[i].long_option, line, args[i].response_value.boolean);
								break;
							case CONFIG_ARG_OPT_NUMBER:
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_NUMBER:
								{
									char *n = parse_config_tail(args[i].long_option, line);
									if (n)
									{
										args[i].response_value.number = strtoull(n, NULL, 0);
										free(n);
									}
								}
								break;
							case CONFIG_ARG_OPT_STRING:
								__attribute__((fallthrough)); /* allow fall-through */
							case CONFIG_ARG_REQ_STRING:
								args[i].response_value.string = parse_config_tail(args[i].long_option, line);
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
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, 4 * sizeof (char));
		struct option *long_options;
		if (!(long_options = calloc(optlen, sizeof (struct option))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, 4 * sizeof (struct option));

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
			char S[1] = "X";
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
		while (true)
		{
			int index = 0;
			int c = getopt_long(argc, argv, short_options, long_options, &index);
			if (c == -1)
				break;
			bool unknown = true;
			if (c == 'h')
				show_help(args, notes, *extra);
			else if (c == 'v')
				show_version();
			else if (c == 'l')
				show_licence();
			else if (c == '?')
				config_show_usage(args, *extra);
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
								__attribute__((fallthrough)); /* allow fall-through; argument was seen */
							case CONFIG_ARG_REQ_BOOLEAN:
								__attribute__((fallthrough)); /* allow fall-through; argument was seen */
							default:
								args[i].response_value.boolean = !args[i].response_value.boolean;
								break;
						}
					}
			if (unknown)
				config_show_usage(args, *extra);
		}
		free(short_options);
		free(long_options);
	}
	int x = 0;
	if (extra)
	{
		for (int i = 0; (*extra) && (*extra)[i]; i++)
			free((*extra)[i]);
		if (!(*extra = realloc(*extra, argc * sizeof (char *))))
			die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, argc * sizeof (char *));
		for (; optind < argc; x++, optind++)
			if (!((*extra)[x] = strdup(argv[optind])))
				die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(argv[optind]));
	}
	return x;
}

inline static void format_section(char *s)
{
	cli_fprintf(stderr, ANSI_COLOUR_CYAN "%s" ANSI_COLOUR_RESET ":\n", s);
	return;
}

static void show_version(void)
{
	version_print(about.name, about.version, about.url);
	exit(EXIT_SUCCESS);
}

inline static void print_usage(config_arg_t *args, char **extra)
{
#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	size_t x = ws.ws_col - strlen(about.name) - 2;
#else
	//CONSOLE_SCREEN_BUFFER_INFO csbi;
	//GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	size_t x = 77 - strlen(about.name);// (csbi.srWindow.Right - csbi.srWindow.Left + 1) - width - 2;
#endif
	format_section(_("Usage"));
	cli_fprintf(stderr, "  " ANSI_COLOUR_GREEN "%s", about.name);
	if (extra)
	{
		for (int i = 0; extra[i]; i++)
			if (extra[i][0] == '+')
				cli_fprintf(stderr, ANSI_COLOUR_RED " <%s>" ANSI_COLOUR_RESET, extra[i] + 1);
			else
			{
				int o = 0;
				if (extra[i][0] == '-')
					o++;
				cli_fprintf(stderr, ANSI_COLOUR_YELLOW " [%s]" ANSI_COLOUR_RESET, extra[i] + o);
			}
	}
	if (args)
		for (int i = 0, j = 0; args[i].short_option; i++)
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
	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

extern void config_show_usage(config_arg_t *args, char **extra)
{
	print_usage(args, extra);
	exit(EXIT_SUCCESS);
}

static void print_option(int width, char sopt, char *lopt, char *type, bool req, char *desc)
{
	size_t z = width - 8 - strlen(lopt);
	cli_fprintf(stderr, "  " ANSI_COLOUR_WHITE "-%c" ANSI_COLOUR_RESET ", " ANSI_COLOUR_WHITE "--%s" ANSI_COLOUR_RESET, sopt, lopt);
	if (type)
	{
		if (req)
			cli_fprintf(stderr, ANSI_COLOUR_WHITE "=" ANSI_COLOUR_RED "<%s>" ANSI_COLOUR_RESET, type);
		else
			cli_fprintf(stderr, ANSI_COLOUR_YELLOW "[=%s]" ANSI_COLOUR_RESET, type);
		z -= 3 + strlen(type);
	}
	fprintf(stderr, "%*s", (int)z, " ");

#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	int x = ws.ws_col - width - 2;
#else
	//CONSOLE_SCREEN_BUFFER_INFO csbi;
	//GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int x = 77 - width;// (csbi.srWindow.Right - csbi.srWindow.Left + 1) - width - 2;
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
	cli_fprintf(stderr, "  • ");
	//fprintf(stderr, "  • %s\n", notes[i]);
#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	int x = ws.ws_col - 5;
#else
	//CONSOLE_SCREEN_BUFFER_INFO csbi;
	//GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int x = 72;// (csbi.srWindow.Right - csbi.srWindow.Left + 1) - width - 2;
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

static void show_help(config_arg_t *args, char **notes, char **extra)
{
	version_print(about.name, about.version, about.url);
	cli_fprintf(stderr, "\n");
	print_usage(args, extra);

	int width = 10;
	bool has_advanced = false;
	for (int i = 0; args[i].short_option; i++)
	{
		int w = 10 + strlen(args[i].long_option);
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
	exit(EXIT_SUCCESS);
}

static void show_licence(void)
{
	fprintf(stderr, _(TEXT_LICENCE));
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
	if (!strcasecmp(CONF_TRUE, v) || !strcasecmp(CONF_ON, v) || !strcasecmp(CONF_ENABLED, v))
		r = true;
	else if (!strcasecmp(CONF_FALSE, v) || !strcasecmp(CONF_OFF, v) || !strcasecmp(CONF_DISABLED, v))
		r = false;
	free(v);
	return r;
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
