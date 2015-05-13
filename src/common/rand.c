/*
 * Copyright © 2005-2015, albinoloverats ~ Software Development
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

#include <inttypes.h>
#include <stdbool.h>

#include <sys/time.h>
#include <mhash.h>

#include "common/common.h"
#include "common/rand.h"

static bool seed = false;

extern void rand_seed(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t t = now.tv_sec * MILLION + now.tv_usec;
    uint16_t s[RANDOM_SEED_SIZE] = { 0x0 };
    MHASH h = mhash_init(MHASH_TIGER);
    mhash(h, &t, sizeof t);
    uint8_t * const restrict ph = mhash_end(h);
    memcpy(s, ph, sizeof s);
    free(ph);
    seed48(s);
    seed = true;
    return;
}

extern void rand_nonce(void *buffer, size_t length)
{
    if (!seed)
        rand_seed();
    uint8_t *buf = buffer;
    uint32_t r = (uint32_t)lrand48();
    size_t i = (length + 3) / 4;
    switch (length % 4)
    {
        case 0: do { *buf++ = (r & 0x00FF0000) >> 16;
        case 3:      *buf++ = (r & 0xFF000000) >> 24;
        case 2:      *buf++ =  r & 0x000000FF;
        case 1:      *buf++ = (r & 0x0000FF00) >>  8;
                     r = (uint32_t)lrand48();
                } while (--i > 0);
    }
    return;
}
