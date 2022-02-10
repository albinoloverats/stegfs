/*
 * Common code for dealing with tag, length, value arrays
 * Copyright © 2009-2022, albinoloverats ~ Software Development
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
#include <stdint.h>
#include <stdbool.h>

#ifndef _WIN32
	#include <netinet/in.h>
#endif

#include "common.h"
#include "non-gnu.h"
#include "tlv.h"
#include "list.h"

typedef struct
{
	LIST    tags;
	byte_t *export;
}
tlv_private_t;

static int tlv_compare(const void *a, const void *b);
static void tlv_free(void *tlv);

extern TLV tlv_init(void)
{
	tlv_private_t *t = calloc(sizeof( tlv_private_t ), sizeof( byte_t ));
	t->tags = list_init(tlv_compare, false, false);
	return t;
}

extern void tlv_deinit(TLV *ptr)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)*ptr;
	if (!tlv_ptr)
		return;
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	list_deinit(tlv_ptr->tags, tlv_free);
	free(tlv_ptr);
	tlv_ptr = NULL;
	*ptr = NULL;
	return;
}

extern void tlv_append(TLV ptr, tlv_t tlv)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return;
	tlv_t *t  = malloc(sizeof tlv);
	t->tag    = tlv.tag;
	t->length = tlv.length;
	t->value  = malloc(t->length);
	memcpy(t->value, tlv.value, t->length);
	list_append(tlv_ptr->tags, t);
	return;
}

extern const tlv_t *tlv_get(TLV ptr, uint8_t tag)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	tlv_t t = { tag, 0, NULL };
	return list_contains(tlv_ptr->tags, &t);
}

extern bool tlv_has_tag(TLV ptr, uint8_t tag)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	return tlv_get(ptr, tag);
}

extern byte_t *tlv_value_of_aux(TLV ptr, uint8_t tag, uint8_t *def)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	const tlv_t *t = tlv_get(ptr, tag);
	return t ? t->value : def;
}

extern uint16_t tlv_length_of(TLV ptr, uint8_t tag)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return 0;
	const tlv_t *t = tlv_get(ptr, tag);
	return t ? t->length : 0;
}

extern byte_t *tlv_export_aux(TLV ptr, bool nbo)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	size_t size = tlv_size(tlv_ptr);
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	tlv_ptr->export = malloc(size);
	size_t off = 0;
	ITER iter = list_iterator(tlv_ptr->tags);
	while (list_has_next(iter))
	{
		const tlv_t *tlv = list_get_next(iter);
		memcpy(tlv_ptr->export + off, &tlv->tag, sizeof tlv->tag);
		off += sizeof tlv->tag;
		uint16_t l = nbo ? htons(tlv->length) : tlv->length;
		memcpy(tlv_ptr->export + off, &l, sizeof tlv->length);
		off += sizeof tlv->length;
		memcpy(tlv_ptr->export + off, tlv->value, tlv->length);
		off += tlv->length;
	}
	return tlv_ptr->export;
}

extern uint16_t tlv_count(TLV ptr)
{
	return list_size(((tlv_private_t *)ptr)->tags);
}

extern size_t tlv_size(TLV ptr)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return 0;
	size_t size = 0;
	ITER iter = list_iterator(tlv_ptr->tags);
	while (list_has_next(iter))
		size += sizeof( uint8_t ) + sizeof( uint16_t ) + ((tlv_t *)list_get_next(iter))->length;
	return size;
}

static int tlv_compare(const void *a, const void *b)
{
	const tlv_t *x = a;
	const tlv_t *y = b;
	// cast to int allows us to overlook that tags are uint8_t's
	return (int)x->tag - (int)y->tag;
}

static void tlv_free(void *tlv)
{
	free(((tlv_t *)tlv)->value);
	free(tlv);
}
