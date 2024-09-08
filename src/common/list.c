/*
 * Common code for dealing with linked lists.
 * Copyright © 2021-2024, albinoloverats ~ Software Development
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
#include "non-gnu.h"
#include "list.h"
#include "error.h"

typedef struct _item_t
{
	struct _item_t *next;
	const void *data;
}
item_t;

typedef struct
{
	item_t *head;                               /*!< The first item in the list */
	item_t *tail;                               /*!< The last item in the list - allows easy appending of items */
	size_t  size;                               /*!< The number of items in the list - gives constant lookup, so no need to count each time */
	int (*compare)(const void *, const void *); /*!< How to compare items in the list */
	bool duplicates:1;                          /*!< Whether to allow duplicate items in the list */
	bool sorted:1;                              /*!< Whether the items should be sorted */
}
list_t;

typedef struct
{
	item_t *next;
}
iterator_t;

extern LIST list_init(int comparison_fn_t(const void *, const void *), bool dupes, bool sorted)
{
	list_t *list = calloc(sizeof( list_t ), sizeof( byte_t ));
	if (!list)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( list_t ));
	list->size = 0;
	list->compare = comparison_fn_t;
	list->duplicates = dupes;
	list->sorted = sorted;
	return list;
}

extern void list_deinit_aux(LIST ptr, void f(void *))
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return;
	item_t *item = list_ptr->head;
	while (item)
	{
		if (f)
			f((void *)item->data);
		item_t *next = item->next;
		free(item);
		item = next;
	}
	free(list_ptr);
	return;
}

extern LIST list_copy(LIST ptr, void *c(const void *))
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	LIST copy = list_init(list_ptr->compare, list_ptr->duplicates, list_ptr->sorted);
	if (!list_ptr->size)
		return copy;
	item_t *item = list_ptr->head;
	do
	{
		list_append(copy, c(item->data));
		item = item->next;
	}
	while (item);
	return copy;
}

/*
 * TODO see whether this could be better as a macro
 */
extern size_t list_size(LIST ptr)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return 0;
	return list_ptr->size;
}

extern bool list_append(LIST ptr, const void *d)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return false;
	if (list_ptr->sorted)
		return list_add(ptr, d);
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return false;
	item_t *new = calloc(sizeof( item_t ), sizeof( byte_t ));
	if (!new)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( item_t ));
	new->data = d;
	item_t *end = list_ptr->tail;
	if (end)
		end->next = new;
	if (!list_ptr->head)
		list_ptr->head = new;
	list_ptr->tail = new;
	list_ptr->size++;
	return true;
}

extern bool list_insert(LIST ptr, size_t i, const void *d)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return false;
	if (list_ptr->sorted)
		return list_add(ptr, d);
	if (i >= list_ptr->size)
		return list_append(ptr, d);
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return false;
	item_t *new = calloc(sizeof( item_t ), sizeof( byte_t ));
	if (!new)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( item_t ));
	new->data = d;
	item_t *prev = list_ptr->head;
	if (i == 0)
	{
		new->next = prev;
		list_ptr->head = new;
	}
	else
	{
		for (size_t j = 0; j < i - 1; j++)
			prev = prev->next;
		item_t *next = prev->next;
		prev->next = new;
		new->next = next;
	}
	list_ptr->size++;
	return true;
}

extern bool list_add(LIST ptr, const void *d)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return false;
	if (!list_ptr->sorted)
		return list_append(ptr, d);
	if (!list_ptr->duplicates && list_contains(ptr, d))
		return false;
	item_t *new = calloc(sizeof( item_t ), sizeof( byte_t ));
	if (!new)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( item_t ));
	new->data = d;
	if (list_ptr->size == 0)
	{
		list_ptr->head = new;
		list_ptr->tail = new;
	}
	else if (list_ptr->size == 1)
	{
		item_t *prev = list_ptr->head;
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
		item_t *this = list_ptr->head;
		item_t *prev = NULL;
		bool added = false;
		do
		{
			if (list_ptr->compare(new->data, this->data) < 0)
			{
				new->next = this;
				if (this == list_ptr->head)
					list_ptr->head = new;
				else if (prev)
					prev->next = new;
				else
					die(_("We’ve reached an unreachable location in the code @ %s:%d:%s"), __FILE__, __LINE__, __func__);
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
	return true;
}

extern int list_add_all(LIST ptr, LIST otr, void *c(const void *))
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return -1;
	list_t *list_otr = (list_t *)otr;
	if (!list_otr)
		return -1;
	if (!list_otr->size)
		return 0;
	int r = 0;
	item_t *item = list_otr->head;
	do
	{
		if (list_append(list_ptr, c(item->data)))
			r++;
		item = item->next;
	}
	while (item);
	return r;
}

extern const void *list_get(LIST ptr, size_t i)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (i >= list_ptr->size)
		return NULL;
	item_t *item = list_ptr->head;
	for (size_t j = 0; j < i; j++)
		item = item->next;
	return item->data;
}

extern const void *list_contains(LIST ptr, const void *d)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (!list_ptr->size)
		return NULL;
	item_t *item = list_ptr->head;
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
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (!list_ptr->size)
		return NULL;
	void *data = NULL;
	item_t *item = list_ptr->head;
	item_t *prev = NULL;
	do
	{
		if (item->data == d || (list_ptr->compare && !list_ptr->compare(d, item->data)))
		{
			data = (void *)item->data;
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
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	if (i >= list_ptr->size)
		return NULL;
	item_t *item = list_ptr->head;
	if (i == 0)
		list_ptr->head = item->next;
	else
	{
		item_t *prev = item;
		for (size_t j = 0; j < i - 1; j++)
			prev = prev->next;
		item = prev->next;
		item_t *next = item->next;
		prev->next = next;
	}
	const void *data = item->data;
	list_ptr->size--;
	free(item);
	return data;
}

extern ITER list_iterator(LIST ptr)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return NULL;
	iterator_t *iter = malloc(sizeof (iterator_t));
	if (!iter)
		die(_("Out of memory @ %s:%d:%s [%zu]"), __FILE__, __LINE__, __func__, sizeof( iterator_t ));
	iter->next = list_ptr->head;
	return iter;
}

extern const void *list_get_next(ITER ptr)
{
	iterator_t *iter_ptr = (iterator_t *)ptr;
	if (!iter_ptr)
		return NULL;
	item_t *next = iter_ptr->next;
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

extern void list_for_each(LIST ptr, void f(const void *))
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return;
	if (!list_ptr->size)
		return;
	item_t *item = list_ptr->head;
	do
	{
		f(item->data);
		item = item->next;
	}
	while (item);
	return;
}

static void *list_sort_noop(const void *x)
{
	return (void *)x;
}

extern void list_sort(LIST ptr)
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return;
	if (!list_ptr->compare)
		return;
	if (!list_ptr->size)
		return;

	list_t *copy = list_init(list_ptr->compare, list_ptr->duplicates, true);
	list_add_all(copy, list_ptr, list_sort_noop);

	size_t z = list_ptr->size;
	for (size_t i = 0; i < z; i++)
		list_remove_index(list_ptr, 0);

	list_ptr->sorted = true;
	list_add_all(list_ptr, copy, list_sort_noop);

	list_deinit(copy);

	return;
}

extern void list_add_comparator(LIST ptr, int c(const void *, const void *))
{
	list_t *list_ptr = (list_t *)ptr;
	if (!list_ptr)
		return;
	list_ptr->compare = c;
	return;
}

extern int list_compare_integer(const void *a, const void *b)
{
	const int64_t *x = a;
	const int64_t *y = b;
	return *x - *y;
}

extern int list_compare_decimal(const void *a, const void *b)
{
	//const __float128 *x = a;
	//const __float128 *y = b;
	const long double *x = a;
	const long double *y = b;
	return *x - *y;
}
