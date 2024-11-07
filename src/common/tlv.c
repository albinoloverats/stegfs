/*
 * Common code for dealing with tag, length, value arrays.
 * Copyright © 2009-2024, albinoloverats ~ Software Development
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
#include "mem.h"
#include "non-gnu.h"
#include "tlv.h"
#include "list.h"
#include "error.h"

/*
 * TODO this could be simplified to just a LIST
 */
typedef struct
{
	list_t  tags;
	byte_t *export;
}
tlv_private_s;

static int tlv_compare(const void *a, const void *b);
static void tlv_free(void *tlv);

extern tlv_t tlv_init(void)
{
	tlv_private_s *t = m_calloc(sizeof( tlv_private_s ), sizeof( byte_t ));
	t->tags = list_init(tlv_compare, false, false);
	return t;
}

extern void tlv_deinit(tlv_t ptr)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return;
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	list_deinit(tlv_ptr->tags, tlv_free);
	free(tlv_ptr);
	return;
}

extern bool tlv_append(tlv_t ptr, tlv_entry_s tlv)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return false;
	tlv_entry_s *t  = m_malloc(sizeof tlv);
	t->tag    = tlv.tag;
	t->length = tlv.length;
	t->value  = m_malloc(t->length);
	memcpy(t->value, tlv.value, t->length);
	bool r = list_append(tlv_ptr->tags, t);
	if (!r)
	{
		free(t->value);
		free(t);
	}
	return r;
}

extern const tlv_entry_s *tlv_remove(tlv_t ptr, tlv_entry_s tlv)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return NULL;
	return list_remove_item(tlv_ptr->tags, &tlv);
}

extern const tlv_entry_s *tlv_remove_tag(tlv_t ptr, uint8_t tag)
{
	return tlv_remove(ptr, (tlv_entry_s){ tag, 0, NULL });
}

extern const tlv_entry_s *tlv_get(tlv_t ptr, uint8_t tag)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return NULL;
	return list_contains(tlv_ptr->tags, &((tlv_entry_s){ tag, 0, NULL }));
}

extern bool tlv_has_tag(tlv_t ptr, uint8_t tag)
{
	return tlv_get(ptr, tag);
}

extern byte_t *tlv_value_of_aux(tlv_t ptr, uint8_t tag, uint8_t *def)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return NULL;
	const tlv_entry_s *t = tlv_get(ptr, tag);
	return t ? t->value : def;
}

extern uint16_t tlv_length_of(tlv_t ptr, uint8_t tag)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return 0;
	const tlv_entry_s *t = tlv_get(ptr, tag);
	return t ? t->length : 0;
}

extern byte_t *tlv_export_aux(tlv_t ptr, bool nbo)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return NULL;
	size_t size = tlv_length(tlv_ptr);
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	tlv_ptr->export = m_malloc(size);
	size_t off = 0;
	iter_t iter = list_iterator(tlv_ptr->tags);
	while (list_has_next(iter))
	{
		const tlv_entry_s *tlv = list_get_next(iter);
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

extern uint16_t tlv_size(tlv_t ptr)
{
	return list_size(((tlv_private_s *)ptr)->tags);
}

extern size_t tlv_length(tlv_t ptr)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return 0;
	size_t size = 0;
	iter_t iter = list_iterator(tlv_ptr->tags);
	while (list_has_next(iter))
		size += sizeof( uint8_t ) + sizeof( uint16_t ) + ((tlv_entry_s *)list_get_next(iter))->length;
	return size;
}

extern iter_t tlv_iterator(tlv_t ptr)
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return NULL;
	return list_iterator(tlv_ptr->tags);
}

extern const tlv_entry_s *tlv_get_next(iter_t ptr)
{
	return list_get_next(ptr);
}

extern bool tlv_has_next(iter_t ptr)
{
	return list_has_next(ptr);
}

extern void tlv_for_each(tlv_t ptr, void f(uint8_t , uint16_t, const void *))
{
	tlv_private_s *tlv_ptr = (tlv_private_s *)ptr;
	if (!tlv_ptr)
		return;
	if (!list_size(tlv_ptr->tags))
		return;
	iter_t iter = list_iterator(tlv_ptr->tags);
	do
	{
		const tlv_entry_s *entry = (tlv_entry_s *)list_get_next(iter);
		f(entry->tag, entry->length, entry->value);
	}
	while (list_has_next(iter));
	free(iter);
	return;
}

static int tlv_compare(const void *a, const void *b)
{
	const tlv_entry_s *x = a;
	const tlv_entry_s *y = b;
	/*
	 * cast to int allows us to overlook that tags are uint8_t's and
	 * get a negative result if applicable
	 */
	return (int)x->tag - (int)y->tag;
}

static void tlv_free(void *tlv)
{
	free(((tlv_entry_s *)tlv)->value);
	free(tlv);
	return;
}
