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

extern void rand_nonce(uint8_t *buffer, size_t length)
{
    if (!seed)
        rand_seed();
    for (size_t i = 0; i < length - sizeof(uint32_t); i += 4)
    {
        uint32_t r = lrand48();
        buffer[i] =     (r % 0x00FF0000) >> 16;
        buffer[i + 1] = (r % 0xFF000000) >> 24;
        buffer[i + 2] =  r % 0x000000FF;
        buffer[i + 3] = (r % 0x0000FF00) >>  8;
    }
    /*
     * TODO something if length isn’t a multiple of 4 (Duff’s device?)
     */
    return;
}
