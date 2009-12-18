/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
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

#define MAX_FILES 16

#include <ncurses.h>
#include <pthread.h>

#include "common/common.h"

#include "src/fuse-stegfs.h"
#include "src/lib-stegfs.h"

static void *tmain(void *);

static pthread_mutex_t tmutex;
static const struct timespec slp = { 0, 5000000 };

int main(int argc, char **argv)
{
    init("test" APP, VER, NULL);
    int16_t max_files = MAX_FILES;

    /*
     * TODO tidy this up
     */
    if (argc != 2 && argc != 3)
        die("missing option(s)");

    char *path = strdup(argv[1]);
    if (chdir(path) < 0)
        die(NULL);
    free(path);

    if (argc == 3)
        max_files = strtol(argv[2], 0, 0);

    srand48(time(NULL));

    int8_t *tid[max_files];

    initscr();
    cbreak();
    noecho();
    intrflush(stdscr, false);
    keypad(stdscr, true);
    timeout(60000);

    mvprintw(0, 0, _("%s Concurrent File Test"), APP);
    for (int8_t i = 0; i < max_files; i++)
        mvprintw(i + 2, 2, _("File # %2i:   0%%"), i);

    pthread_t threads[max_files];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for (int8_t i = 0; i < max_files; i++)
    {
        tid[i] = malloc(sizeof( int8_t ));
        memcpy(tid[i], &i, sizeof( int8_t ));
        pthread_create(&threads[i], &attr, tmain, tid[i]);
        nanosleep(&slp, NULL);
    }

    pthread_attr_destroy(&attr);

    for (int8_t i = 0; i < max_files; i++)
    {
        pthread_join(threads[i], NULL);
        free(tid[i]);
    }

    mvprintw(LINES - 2, 0, _("Press any key to quit..."));
    move(0, 0);
    while (getch() == ERR);

    refresh();
    move(0, 0);
    endwin();

    return EXIT_SUCCESS;
}

static void *tmain(void *arg)
{
    int8_t tid = *(int8_t *)arg;
    int16_t max = lrand48() % 0x0400;

    int8_t blk[0x0400];
    memset(blk, tid, sizeof( blk ));

    char *fname = NULL;
    asprintf(&fname, "file_%02i", tid);
    FILE *file = fopen(fname, "w");

    bool failed = false;

    for (int16_t i = 0; i < max; i++)
    {
        if (fwrite(blk, sizeof( int8_t ), sizeof( blk ), file) != sizeof( blk ))
        {
            failed = true;
            break;
        }
        pthread_mutex_lock(&tmutex);
        mvprintw(tid + 2, 2, _("File # %2i: %3i%% (%i/%i)"), tid, 100 * i / max, i, max);
        move(0, 0);
        refresh();
        pthread_mutex_unlock(&tmutex);
        nanosleep(&slp, NULL);
    }

    free(fname);
    if (fflush(file) != 0)
        failed = true;
    if (fclose(file) != 0)
        failed = true;

    pthread_mutex_lock(&tmutex);
    if (errno)
        failed = true;
    mvprintw(tid + 2, 2, _("File # %2i: %s          "), tid, failed ? _("FAILED") : _("PASSED"));
    move(0, 0);
    refresh();
    pthread_mutex_unlock(&tmutex);
    nanosleep(&slp, NULL);

    pthread_exit(NULL);
    return NULL;
}
