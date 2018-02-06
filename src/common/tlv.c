/*
 * Common code for dealing with tag, length, value arrays
 * Copyright © 2009-2018, albinoloverats ~ Software Development
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

typedef struct
{
	size_t  tags;
	tlv_t  *buffer;
	byte_t *export;
}
tlv_private_t;

extern TLV_HANDLE tlv_init(void)
{
	return calloc(sizeof( tlv_private_t ), sizeof( byte_t ));
}

extern void tlv_deinit(TLV_HANDLE *ptr)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)*ptr;
	if (!tlv_ptr)
		return;
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	for (unsigned i = 0; i < tlv_ptr->tags; i++)
	{
		tlv_t tlv = tlv_ptr->buffer[i];
		tlv.tag = 0;
		tlv.length = 0;
		free(tlv.value);
		tlv.value = NULL;
	}
	tlv_ptr->tags = 0;
	free(tlv_ptr->buffer);
	tlv_ptr->buffer = NULL;
	free(tlv_ptr);
	tlv_ptr = NULL;
	*ptr = NULL;
	return;
}

extern void tlv_append(TLV_HANDLE *ptr, tlv_t tlv)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)*ptr;
	if (!tlv_ptr)
		return;
	void *z = realloc(tlv_ptr->buffer, (tlv_ptr->tags + 1) * sizeof tlv);
	tlv_ptr->buffer = z;
	tlv_ptr->buffer[tlv_ptr->tags].tag = tlv.tag;
	tlv_ptr->buffer[tlv_ptr->tags].length = tlv.length;
	tlv_ptr->buffer[tlv_ptr->tags].value = malloc(tlv.length);
	memcpy(tlv_ptr->buffer[tlv_ptr->tags].value, tlv.value, tlv.length);
	tlv_ptr->tags++;
	return;
}

extern tlv_t *tlv_get(TLV_HANDLE ptr, uint8_t tag)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	for (unsigned i = 0; i < tlv_ptr->tags; i++)
		if (tlv_ptr->buffer[i].tag == tag)
			return &(tlv_ptr->buffer[i]);
	return NULL;
}

extern bool tlv_has_tag(TLV_HANDLE ptr, uint8_t tag)
{
	return tlv_get(ptr, tag) != NULL;
}

extern byte_t *tlv_value_of_aux(TLV_HANDLE ptr, uint8_t tag, uint8_t *def)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	tlv_t *t = tlv_get(ptr, tag);
	return t ? t->value : def;
}

extern uint16_t tlv_length_of(TLV_HANDLE ptr, uint8_t tag)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return 0;
	tlv_t *t = tlv_get(ptr, tag);
	return t ? t->length : 0;
}

extern byte_t *tlv_export_aux(TLV_HANDLE ptr, bool nbo)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return NULL;
	size_t size = tlv_size(tlv_ptr);
	if (tlv_ptr->export)
		free(tlv_ptr->export);
	tlv_ptr->export = malloc(size);
	size_t off = 0;
	for (unsigned i = 0; i < tlv_ptr->tags; i++)
	{
		tlv_t tlv = tlv_ptr->buffer[i];
		memcpy(tlv_ptr->export + off, &tlv.tag, sizeof tlv.tag);
		off += sizeof tlv.tag;
		uint16_t l = nbo ? htons(tlv.length) : tlv.length;
		memcpy(tlv_ptr->export + off, &l, sizeof tlv.length);
		off += sizeof tlv.length;
		memcpy(tlv_ptr->export + off, tlv.value, tlv.length);
		off += tlv.length;
	}
	return tlv_ptr->export;
}

extern uint16_t tlv_count(TLV_HANDLE ptr)
{
	return ((tlv_private_t *)ptr)->tags;
}

extern size_t tlv_size(TLV_HANDLE ptr)
{
	tlv_private_t *tlv_ptr = (tlv_private_t *)ptr;
	if (!tlv_ptr)
		return 0;
	size_t size = 0;
	for (unsigned i = 0; i < tlv_ptr->tags; i++)
		size += sizeof( uint8_t ) + sizeof( uint16_t ) + tlv_ptr->buffer[i].length;
	return size;
}
