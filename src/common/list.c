/*
 * Common code for dealing with linked lists.
 * Copyright © 2021-2022, albinoloverats ~ Software Development
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

#include <stdbool.h>

#include "common.h"
#include "list.h"

typedef struct _list_t
{
	struct _list_t *next;
	const void *data;
}
list_t;

typedef struct
{
	list_t *head;
	list_t *next;
	list_t *tail;
	size_t  size;
	int (*compare)(const void *, const void *);
	bool duplicates:1;
	bool sorted:1;
}
list_private_t;

typedef struct
{
	list_private_t *list; // this might not be necessary
	list_t *next;
}
iterator_t;

extern LIST list_init(int comparison_fn_t(const void *, const void *), bool dupes, bool sorted)
{
	list_private_t *list = calloc(sizeof( list_private_t ), sizeof( byte_t ));
	list->size = 0;
	list->compare = comparison_fn_t;
	list->duplicates = dupes;
	list->sorted = sorted;
	return list;
}

extern void list_deinit_aux(LIST ptr, void f(void *))
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return;
	list_t *item = list_ptr->head;
	while (item)
	{
		if (f)
			f((void *)item->data);
		list_t *next = item->next;
		free(item);
		item = next;
	}
	free(list_ptr);
	return;
}

/*
 * TODO see whether this could be better as a macro
 */
extern size_t list_size(LIST ptr)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return 0;
	return list_ptr->size;
}

extern void list_append(LIST ptr, const void *d)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return;
	if (list_ptr->sorted)
	{
		list_add(ptr, d);
		return;
	}
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return;
	list_t *new = calloc(sizeof( list_t ), sizeof( byte_t ));
	new->data = d;
	list_t *end = list_ptr->tail;
	if (end)
		end->next = new;
	if (!list_ptr->head)
		list_ptr->head = new;
	list_ptr->tail = new;
	list_ptr->size++;
	return;
}

extern void list_insert(LIST ptr, size_t i, const void *d)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return;
	if (list_ptr->sorted)
	{
		list_add(ptr, d);
		return;
	}
	if (i >= list_ptr->size)
	{
		list_append(ptr, d);
		return;
	}
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return;
	list_t *new = calloc(sizeof( list_t ), sizeof( byte_t ));
	new->data = d;
	list_t *prev = list_ptr->head;
	if (i == 0)
	{
		new->next = prev;
		list_ptr->head = new;
	}
	else
	{
		for (size_t j = 0; j < i - 1; j++)
			prev = prev->next;
		list_t *next = prev->next;
		prev->next = new;
		new->next = next;
	}
	list_ptr->size++;
	return;
}

extern void list_add(LIST ptr, const void *d)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return;
	if (!list_ptr->sorted)
	{
		list_append(ptr, d);
		return;
	}
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return;
	list_t *new = calloc(sizeof( list_t ), sizeof( byte_t ));
	new->data = d;
	if (list_ptr->size == 0)
	{
		list_ptr->head = new;
		list_ptr->tail = new;
	}
	else if (list_ptr->size == 1)
	{
		list_t *prev = list_ptr->head;
		if (list_ptr->compare(new->data, prev->data) <= 0)
		{
			list_ptr->head = new;
			new->next = prev;
		}
		else
		{
			prev->next = new;
			list_ptr->tail = new;
		}
	}
	else
	{
		list_t *this = list_ptr->head;
		list_t *prev = NULL;
		bool added = false;
		do
		{
			int p = list_ptr->compare(new->data, this->data);
			if (p < 0)
			{
				new->next = this;
				if (this == list_ptr->head)
					list_ptr->head = new;
				else if (prev)
					prev->next = new;
				added = true;
				break;
			}
			prev = this;
			this = this->next;
		}
		while (this);
		if (!added)
		{
			prev->next = new;
			list_ptr->tail = new;
		}
	}
	list_ptr->size++;
	return;
}

extern const void *list_get(LIST ptr, size_t i)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (i >= list_ptr->size)
		return NULL;
	list_t *item = list_ptr->head;
	for (size_t j = 0; j < i; j++)
		item = item->next;
	return item->data;
}

extern const void *list_contains(LIST ptr, const void *d)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (!list_ptr->size)
		return NULL;
	list_t *item = list_ptr->head;
	do
	{
		if (item->data == d || (list_ptr->compare && !list_ptr->compare(d, item->data)))
			return item->data;
		item = item->next;
	}
	while (item);
	return NULL;
}

extern const void *list_remove_item(LIST ptr, const void *d)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (!list_ptr->size)
		return NULL;
	const void *data = NULL;
	list_t *item = list_ptr->head;
	list_t *prev = NULL;
	do
	{
		if (item->data == d || (list_ptr->compare && !list_ptr->compare(d, item->data)))
		{
			data = item->data;
			if (prev)
				prev->next = item->next;
			else
				list_ptr->head = item->next;
			list_ptr->size--;
			free(item);
			item = prev ? prev->next : list_ptr->head;
		}
		else
		{
			prev = item;
			item = item->next;
		}
	}
	while (item);
	return data;
}

extern const void *list_remove_index(LIST ptr, size_t i)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (i >= list_ptr->size)
		return NULL;
	list_t *item = list_ptr->head;
	if (i == 0)
		list_ptr->head = item->next;
	else
	{
		list_t *prev = item;
		for (size_t j = 0; j < i - 1; j++)
			prev = prev->next;
		item = prev->next;
		list_t *next = item->next;
		prev->next = next;
	}
	const void *data = item->data;
	list_ptr->size--;
	free(item);
	return data;
}

extern ITER list_iterator(LIST ptr)
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return NULL;
	iterator_t *iter = malloc(sizeof (iterator_t));
	iter->list = list_ptr;
	iter->next = list_ptr->head;
	return iter;
}

extern const void *list_get_next(ITER ptr)
{
	iterator_t *iter_ptr = (iterator_t *)ptr;
	if (!iter_ptr)
		return NULL;
	list_t *next = iter_ptr->next;
	if (!next)
		return NULL;
	iter_ptr->next = next->next;
	return next->data;
}

extern bool list_has_next(ITER ptr)
{
	iterator_t *iter_ptr = (iterator_t *)ptr;
	if (!iter_ptr)
		return false;
	return iter_ptr->next;
}

extern void list_add_comparator(LIST ptr, int c(const void *, const void *))
{
	list_private_t *list_ptr = (list_private_t *)ptr;
	if (!list_ptr)
		return;
	list_ptr->compare = c;
	return;
}
