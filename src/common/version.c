/*
 * Version checking functions (non-applications specific)
 * Copyright © 2005-2021, albinoloverats ~ Software Development
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

#ifdef __APPLE__
	#include "osx.h"
#endif

#include "common.h"
#include "version.h"
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

static void version_format(int i, char *id, char *value);
static void version_format_line(char **buffer, int x, int i, char *id, char *value);
static void version_download_latest(char *);
static void version_install_latest(char *);
static void *version_check(void *);
static size_t version_verify(void *, size_t, size_t, void *);

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
	int i = strlen(name) + 8;
	char *av = NULL;
	asprintf(&av, _("%s version"), name);
	char *git = strndup(GIT_COMMIT, GIT_COMMIT_LENGTH);
	char *runtime = NULL;
#ifndef _WIN32
	struct utsname un;
	uname(&un);
	asprintf(&runtime, "%s %s %s %s", un.sysname, un.release, un.version, un.machine);
#else
	asprintf(&runtime, "%s", windows_version());
#endif
	version_format(i, av,              version);
	version_format(i, _("built on"),   __DATE__ " " __TIME__);
	version_format(i, _("git commit"), git);
	version_format(i, _("build os"),   BUILD_OS);
	version_format(i, _("compiler"),   COMPILER);
	version_format(i, _("cflags"),     ALL_CFLAGS);
	version_format(i, _("cppflags"),   ALL_CPPFLAGS);
	version_format(i, _("runtime"),    runtime);
#ifdef USE_GCRYPT
	char *gcv = NULL;
	asprintf(&gcv, "%s (compiled) %s (runtime) %s (required)", GCRYPT_VERSION, gcry_check_version(NULL), NEED_LIBGCRYPT_VERSION);
	version_format(i, _("libgcrypt"), gcv);
	free(gcv);
#endif
	free(av);
	free(git);
	free(runtime);
	struct timespec vc = { 0, MILLION }; /* 1ms == 1,000,000ns*/
	while (version_is_checking)
		nanosleep(&vc, NULL);
	if (version_new_available)
	{
		fprintf(stderr, "\n");
		cli_fprintf(stderr, _(NEW_VERSION_URL), version_available, program_invocation_short_name, strlen(new_version_url) ? new_version_url : url);
	}
	return;
}

extern char *version_build_info(void)
{
	char *info = NULL;

	char *git = strndup(GIT_COMMIT, GIT_COMMIT_LENGTH);
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
	version_format_line(&info, AA_GW, AA_GI, _("git commit"), git);
	version_format_line(&info, AA_GW, AA_GI, _("build os"),   BUILD_OS);
	version_format_line(&info, AA_GW, AA_GI, _("compiler"),   COMPILER);
	version_format_line(&info, AA_GW, AA_GI, _("cflags"),     ALL_CFLAGS);
	version_format_line(&info, AA_GW, AA_GI, _("cppflags"),   ALL_CPPFLAGS);
	version_format_line(&info, AA_GW, AA_GI, _("runtime"),    runtime);

	//asprintf(&info, "%s: %s\n", _("built on"),   __DATE__ " " __TIME__);
	//asprintf(&info, "%s%s: %s\n", info, _("git commit"), git);
	//asprintf(&info, "%s%s: %s\n", info, _("build os"),   BUILD_OS);
	//asprintf(&info, "%s%s: %s\n", info, _("compiler"),   COMPILER);
	//asprintf(&info, "%s%s: %s\n", info, _("cflags"),     ALL_CFLAGS);
	//asprintf(&info, "%s%s: %s\n", info, _("cppflags"),   ALL_CPPFLAGS);
	//asprintf(&info, "%s%s: %s\n", info, _("runtime"),    runtime);

//#ifdef GCRYPT_VERSION
	char *gcv = NULL;
#ifdef USE_GCRYPT
	asprintf(&gcv, "%s (compiled) %s (runtime)", GCRYPT_VERSION, gcry_check_version(NULL));
#endif
	//asprintf(&info, "%s%s: %s\n", info, _("libgcrypt"), gcv);
	version_format_line(&info, AA_GW, AA_GI, _("libgcrypt"), gcv);
	free(gcv);
//#endif

	free(git);
	free(runtime);

	return info;
}

extern void version_check_for_update(char *current_version, char *check_url, char *download_url)
{
	if (version_is_checking)
		return;
	version_is_checking = true;

	version_check_t *info = malloc(sizeof( version_check_t ));
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
	if (!version_new_available || !u)
		return;
#if !defined __APPLE__ && !defined _WIN32
	char *u2 = strdup(u);
	pid_t pid = fork();
	if (pid == 0)
	{
		execl(u2, basename(u2), NULL);
		unlink(u2);
		free(u2);
		_exit(EXIT_FAILURE);
	}
	else if (pid > 0)
	{
		waitpid(pid, NULL, 0);
		unlink(u2);
		free(u2);
	}
#elif defined __APPLE__
	char *dmg = NULL;
	asprintf(&dmg, "%s.dmg", u);
	rename(u, dmg);
	//execl("/usr/bin/open", "open", dmg, NULL);
	osx_open_file(dmg);
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

static void version_format(int i, char *id, char *value)
{
	cli_fprintf(stderr, ANSI_COLOUR_GREEN "%*s" ANSI_COLOUR_RESET ": " ANSI_COLOUR_MAGENTA, i, id);
#ifndef _WIN32
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	int x = ws.ws_col - i - 2;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int x = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - i - 2;
	if (x <= 0)
		x = 77 - i; // needed for MSYS2
#endif
	for (; isspace(*value); value++)
		;
	int l = strlen(value);
	if (l < x)
		cli_fprintf(stderr, "%s", value);
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
					if (isspace(value[e]))
						break;
			if (s)
				cli_fprintf(stderr, "\n%*s  ", i, " ");
			cli_fprintf(stderr, "%.*s", e - s, value + s);
			s = e + 1;
		}
		while (s < l);
	}

	cli_fprintf(stderr, ANSI_COLOUR_RESET "\n");
	return;
}

static void version_format_line(char **buffer, int x, int i, char *id, char *value)
{
	asprintf(buffer, "%s%*s: ", *buffer ? *buffer : "", i, id);
	for (; isspace(*value); value++)
		;
	int l = strlen(value);
	if (l < x)
		asprintf(buffer, "%s%s", *buffer, value);
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
					if (isspace(value[e]))
						break;
			if (s)
				asprintf(buffer, "%s\n%*s  ", *buffer, i, " ");
			asprintf(buffer, "%s%.*s", *buffer, e - s, value + s);
			s = e + 1;
		}
		while (s < l);
	}
	asprintf(buffer, "%s\n", *buffer);
	return;
}
