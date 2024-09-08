/*
 * Version checking functions (non-applications specific)
 * Copyright © 2005-2024, albinoloverats ~ Software Development
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>

#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include <gcrypt.h> // figure out how to get the gcrypt info without including the header
#include <curl/curl.h>
#include <pthread.h>

#if defined __FreeBSD__ || defined __sun
	#include <libgen.h>
#endif

#ifndef _WIN32
	#include <sys/stat.h>
	#include <sys/wait.h>

	#include <sys/utsname.h>
	#include <sys/ioctl.h>
	#ifdef __sun
		#include <sys/tty.h>
	#endif
#else
	#include <windows.h>
	#include <shellapi.h>
	#include <Shlobj.h>
	extern char *program_invocation_short_name;

	#include "non-gnu.h"
#endif

#include "common.h"
#include "version.h"
#include "error.h"
#include "cli.h"
#ifdef USE_GCRYPT
	#include "ccrypt.h"
#endif

#if __has_include("misc.h")
	#include "misc.h"
#else
	#define ALL_CFLAGS   "(unknown)"
	#define ALL_CPPFLAGS "(unknown)"
#endif

#define TIMEOUT 10

static void version_format(int indent, char *id, char *value) __attribute__((nonnull(2, 3)));
static void version_format_line(char **buffer, int max_width, int indent, char *id, char *value) __attribute__((nonnull(1, 4, 5)));
static void version_download_latest(char *) __attribute__((nonnull(1)));
static void version_install_latest(char *) __attribute__((nonnull(1)));
static void *version_check(void *) __attribute__((nonnull(1)));
static size_t version_verify(void *, size_t, size_t, void *) __attribute__((nonnull(1, 4)));

bool version_new_available = false;
bool version_is_checking = false;
char version_available[0x10] = { 0x0 };
char new_version_url[0xFF] = { 0x0 };

static char *update = NULL;

typedef struct
{
	char *current;
	char *check_url;
	char *update_url;
}
version_check_t;

extern void version_print(char *name, char *version, char *url)
{
	int indent = strlen(name) + 8;
	char *av = NULL;
	asprintf(&av, _("%s version"), name);
	char *runtime = NULL;
#ifndef _WIN32
	struct utsname un;
	uname(&un);
	asprintf(&runtime, "%s %s %s %s", un.sysname, un.release, un.version, un.machine);
#else
	asprintf(&runtime, "%s", windows_version());
#endif
	version_format(indent, av,              version);
	version_format(indent, _("built on"),   __DATE__ " " __TIME__);
	version_format(indent, _("git commit"), GIT_COMMIT);
	version_format(indent, _("build os"),   BUILD_OS);
	version_format(indent, _("compiler"),   COMPILER);
	version_format(indent, _("cflags"),     ALL_CFLAGS);
	version_format(indent, _("cppflags"),   ALL_CPPFLAGS);
	version_format(indent, _("runtime"),    runtime);
#ifdef USE_GCRYPT
	char *gcv = NULL;
	asprintf(&gcv, "%s (compiled) %s (runtime) %s (required)", GCRYPT_VERSION, gcry_check_version(NULL), NEED_LIBGCRYPT_VERSION);
	version_format(indent, _("libgcrypt"), gcv);
	free(gcv);
#endif
	free(av);
	free(runtime);
	struct timespec vc = { 0, MILLION }; /* 1ms == 1,000,000ns*/
	while (version_is_checking)
		nanosleep(&vc, NULL);
	if (version_new_available)
	{
		cli_fprintf(stderr, "\n");
		cli_fprintf(stderr, _(NEW_VERSION_URL), version_available, program_invocation_short_name, strlen(new_version_url) ? new_version_url : url);
	}
	return;
}

extern char *version_build_info(void)
{
	char *info = NULL;

	char *runtime = NULL;
#ifndef _WIN32
	struct utsname un;
	uname(&un);
	asprintf(&runtime, "%s %s %s %s", un.sysname, un.release, un.version, un.machine);
#else
	asprintf(&runtime, "%s", windows_version());
#endif

#define AA_GW 80
#define AA_GI 10

	version_format_line(&info, AA_GW, AA_GI, _("built on"),   __DATE__ " " __TIME__);
	version_format_line(&info, AA_GW, AA_GI, _("git commit"), GIT_COMMIT);
	version_format_line(&info, AA_GW, AA_GI, _("build os"),   BUILD_OS);
	version_format_line(&info, AA_GW, AA_GI, _("compiler"),   COMPILER);
	version_format_line(&info, AA_GW, AA_GI, _("cflags"),     ALL_CFLAGS);
	version_format_line(&info, AA_GW, AA_GI, _("cppflags"),   ALL_CPPFLAGS);
	version_format_line(&info, AA_GW, AA_GI, _("runtime"),    runtime);

#ifdef USE_GCRYPT
	char *gcv = NULL;
	asprintf(&gcv, "%s (compiled) %s (runtime)", GCRYPT_VERSION, gcry_check_version(NULL));
	//asprintf(&info, "%s%s: %s\n", info, _("libgcrypt"), gcv);
	version_format_line(&info, AA_GW, AA_GI, _("libgcrypt"), gcv);
	free(gcv);
#endif

	free(runtime);

	return info;
}

extern void version_check_for_update(char *current_version, char *check_url, char *download_url)
{
	if (version_is_checking)
		return;
	version_is_checking = true;

	version_check_t *info = malloc(sizeof( version_check_t ));
	if (!info)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( version_check_t ));
	info->current    = strdup(current_version);
	info->check_url  = strdup(check_url);
	info->update_url = download_url ? strdup(download_url) : NULL;
//#ifndef __DEBUG__
	pthread_t vt;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	pthread_create(&vt, &a, version_check, info);
	pthread_attr_destroy(&a);
//#else
	//version_check(info);
//#endif
	return;
}

static void *version_check(void *n)
{
	struct timespec vc = { 1, 0 };
	nanosleep(&vc, NULL);

	version_check_t *info = n;
	curl_global_init(CURL_GLOBAL_ALL);
	CURL *ccheck = curl_easy_init();
	curl_easy_setopt(ccheck, CURLOPT_URL, info->check_url);
#ifdef WIN32
	curl_easy_setopt(ccheck, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
	curl_easy_setopt(ccheck, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(ccheck, CURLOPT_CONNECTTIMEOUT, TIMEOUT);
	//curl_easy_setopt(ccheck, CURLOPT_TIMEOUT, TIMEOUT);
	curl_easy_setopt(ccheck, CURLOPT_WRITEDATA, info->current);
	curl_easy_setopt(ccheck, CURLOPT_WRITEFUNCTION, version_verify);
	curl_easy_perform(ccheck);
	curl_easy_cleanup(ccheck);
	if (version_new_available && info->update_url)
		version_download_latest(info->update_url);

	if (info->update_url)
		free(info->update_url);
	free(info->check_url);
	free(info->current);
	free(info);
	version_is_checking = false;
//#ifndef __DEBUG__
	pthread_exit(NULL);
	#ifdef _WIN32
	return NULL;
	#endif
//#else
	//return NULL;
//#endif
}

static void version_download_latest(char *update_url)
{
	/* download new version */
	CURL *cupdate = curl_easy_init();
	/*
	 * default template for our projects download url is /downloads/project/version/project-version
	 * and as the project knows and can set everything except the new version number this is sufficient
	 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	snprintf(new_version_url, sizeof new_version_url - 1, update_url, version_available, version_available);
#pragma GCC diagnostic pop
	curl_easy_setopt(cupdate, CURLOPT_URL, new_version_url);
#ifdef WIN32
	curl_easy_setopt(cupdate, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
	curl_easy_setopt(cupdate, CURLOPT_NOPROGRESS, 1L);
#ifndef _WIN32
	asprintf(&update, "%s/update-%s-XXXXXX", P_tmpdir ,version_available);
	int64_t fd = mkstemp(update);
	fchmod(fd, S_IRUSR | S_IWUSR | S_IXUSR);
#else
	char p[MAX_PATH] = { 0x0 };
	GetTempPath(sizeof p, p);
	asprintf(&update, "%supdate-%s.exe", p, version_available);
	int64_t fd = open(update, O_CREAT | O_WRONLY | O_BINARY);
#endif
	if (fd > 0)
	{
		FILE *fh = fdopen(fd, "wb");
		curl_easy_setopt(cupdate, CURLOPT_WRITEDATA, fh);
		curl_easy_perform(cupdate);
		curl_easy_cleanup(cupdate);
		fclose(fh);
		close(fd);

		version_install_latest(update);
	}
	free(update);
}

static void version_install_latest(char *u)
{
#if !defined VERSION_NO_INSTALL
	if (!version_new_available)
		return;
#if !defined __APPLE__ && !defined _WIN32
	char *u2 = strdup(u);
	if (!u2)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, strlen(u) + 1);

	pid_t pid = fork();
	if (pid == 0)
	{
		execl(u2, basename(u2), NULL);
		unlink(u2);
		free(u2);
		_exit(EXIT_FAILURE);
	}
	else if (pid > 0)
		waitpid(pid, NULL, 0);
	unlink(u2);
	free(u2);
#elif defined __APPLE__
	char *dmg = NULL;
	asprintf(&dmg, "%s.dmg", u);
	rename(u, dmg);
	execl("/usr/bin/open", dmg, NULL);
	unlink(dmg);
	free(dmg);
#elif defined _WIN32
	ShellExecute(NULL, "open", u, NULL, NULL, SW_SHOWNORMAL);
#endif
#else
	(void)u;
#endif
	return;
}

static size_t version_verify(void *p, size_t s, size_t n, void *v)
{
	char *b = calloc(s + 1, n);
	if (!b)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, (s + 1) * n);
	memcpy(b, p, s * n);
	char *l = strrchr(b, '\n');
	if (l)
		*l = '\0';
	if (strcmp(b, (char *)v) > 0)
	{
		version_new_available = true;
		snprintf(version_available, sizeof version_available - 1, "%s", b);
	}
	free(b);
	return s * n;
}

static void version_format(int indent, char *id, char *value)
{
	cli_fprintf(stderr, ANSI_COLOUR_GREEN "%*s" ANSI_COLOUR_RESET ": " ANSI_COLOUR_MAGENTA, indent, id);
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
		max_width = 77; // needed for MSYS2, but also sensible default if output is bein redirected
	int width = max_width - indent;
	for (; isspace(*value); value++)
		;
	int l = strlen(value);
	if (l < width)
		cli_fprintf(stderr, "%s", value);
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
					if (isspace(value[e]))
					{
						too_long = false;
						break;
					}
			if (too_long)
			{
				for (int e2 = s; e2 < s + max_width; e2++)
					if (isspace(value[e2]) || value[e2] == 0x00)
						e = e2;
				cli_fprintf(stderr, "\n  ");
			}
			else if (s)
				cli_fprintf(stderr, "\n%*s  ", indent, " ");
			cli_fprintf(stderr, "%.*s", e - s, value + s);
			s = e + 1;
		}
		while (s < l);
	}

	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

static void version_format_line(char **buffer, int max_width, int indent, char *id, char *value)
{
	asprintf(buffer, "%s%*s: ", *buffer ? *buffer : "", indent, id);
	for (; isspace(*value); value++)
		;
	int l = strlen(value);
	if (l < max_width)
		asprintf(buffer, "%s%s", *buffer, value);
	else
	{
		int s = 0;
		do
		{
			int e = s + max_width;
			if (e > l)
				e = l;
			else
				for (; e > s; e--)
					if (isspace(value[e]))
						break;
			if (s)
				asprintf(buffer, "%s\n%*s  ", *buffer, indent, " ");
			asprintf(buffer, "%s%.*s", *buffer, e - s, value + s);
			s = e + 1;
		}
		while (s < l);
	}
	asprintf(buffer, "%s\n", *buffer);
	return;
}
