/*
 * Version checking functions (non-applications specific)
 * Copyright © 2005-2013, albinoloverats ~ Software Development
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

#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <curl/curl.h>
#include <pthread.h>

#ifndef __APPLE__
    #include "common/version.h"
#else
    #include "version.h"
#endif

static void *version_check(void *);
static size_t version_verify(void *, size_t, size_t, void *);

bool new_version_available = false;

static char *current = NULL;

extern void version_check_for_update(char *c, char *url)
{
    if (!c || !url)
        return;
    current = c;
    pthread_t vt;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_create(&vt, &a, version_check, url);
    pthread_attr_destroy(&a);
    return;
}

static void *version_check(void *n)
{
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, (char *)n);
#ifdef WIN32
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, version_verify);
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);
    return n;
}

static size_t version_verify(void *p, size_t s, size_t n, void *x)
{
    (void)x;
    char *b = calloc(s + 1, n);
    memcpy(b, p, s * n);
    char *l = strrchr(b, '\n');
    if (l)
        *l = '\0';
    if (strcmp(b, current) > 0)
        new_version_available = true;
    free(b);
    return s * n;
}
