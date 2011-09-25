/*
 * Common TLV code
 * Copyright (c) 2011, albinoloverats ~ Software Development
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
#include <inttypes.h>

#include "tlv.h"
#include "common.h"
#include "list.h"

extern tlv_t tlv_combine(uint8_t t, uint16_t l, void *v)
{
    tlv_t x = { 0, 0, NULL };
    x.tag = t;
    x.length = l;
    if (!(x.value = malloc(l)))
        die("out of memory @ %s:%d:%s", __FILE__, __LINE__, __func__);
    memcpy(x.value, v, l);
    return x;
}

extern uint64_t tlv_build(uint8_t **b, list_t *l)
{
    uint64_t z = 0;
    uint64_t j = list_size(l);
    for (uint64_t i = 0; i < j; i++)
    {
        tlv_t *t = list_get(l, i);
        uint64_t c = z;
        z += sizeof( uint8_t ) + sizeof( uint16_t ) + t->length;
        uint8_t *x = realloc(*b, z);
        if (!x)
            die("out of memory @ %s:%d:%s [%ju]", __FILE__, __LINE__, __func__, z);
        memcpy(x + c, &(t->tag), sizeof( uint8_t ));
        memcpy(x + c + sizeof( uint8_t ), &(t->length), sizeof( uint16_t ));
        memcpy(x + c + sizeof( uint8_t ) + sizeof( uint16_t ), t->value, t->length);
        *b = x;
    }
    return z;
}
