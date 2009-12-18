/*
 * Common code for implementing a linked list
 * Copyright (c) 2009-2010, albinoloverats ~ Software Development
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

#include "common/common.h"
#define _IN_LIST_
#include "common/list.h"
#undef _IN_LIST

int compare_object(void *a, void *b)
{
    /*
     * provided the objects in the list have an ->id, they can be compared using it
     */
    return ((compare_id_t *)a)->id - ((compare_id_t *)b)->id;
}

extern list_t *list_create(void)
{
    return (void *)-1;
}

extern void list_delete(list_t **l)
{
    list_t *x = list_find_first(*l);
    while (true)
    {
        list_t *y = x->next;
        x->object = NULL;
        x->prev = NULL;
        x->next = NULL;
        free(x);
        if (!y)
            break;
        x = y;
    }
    free(*l);
    l = NULL;
    return;
}

extern void list_append(list_t **l, void *o)
{
    if (*l == (void *)-1)
    {
        *l = calloc(1, sizeof( list_t ));
        (*l)->object = o;
        return;
    }
    list_t *x = list_find_last(*l);
    list_t *n = calloc(1, sizeof( list_t ));
    n->prev = x;
    n->object = o;
    x->next = n;
    list_sort(l);
    return;
}

extern void list_remove(list_t **l, uint64_t i)
{
    uint64_t s = list_size(*l);
    if (i >= s)
        return;
    list_t *x = list_get(*l, i);
    if (x->prev)
        x->prev->next = x->next;
    else
        *l = (*l)->next;
    if (x->next)
        x->next->prev = x->prev;
    x->object = NULL;
    x->prev = NULL;
    x->next = NULL;
    free(x);
    x = NULL;
    list_sort(l);
    return;
}

extern list_t *list_get(list_t *l, uint64_t i)
{
    if (i >= list_size(l))
        return NULL;
    list_t *x = list_find_first(l);
    for (uint64_t j = 0; j < i; j++)
        x = x->next;
    return x;
}

extern uint64_t list_size(list_t *l)
{
    if (!l || (l == (void *)-1))
        return 0;
    list_t *x = list_find_first(l);
    uint64_t s = 1;
    while (true)
    {
        if (!x->next)
            break;
        x = x->next;
        s++;
    }
    return s;
}

extern void list_join(list_t *l, list_t *m)
{
    list_t *x = list_find_last(l);
    list_t *y = list_find_first(m);
    x->next = y;
    y->prev = x;
    return;
}

extern list_t *list_split(list_t *l)
{
    list_t *m = list_get(l, list_size(l) / 2);
    m->prev->next = NULL;
    m->prev = NULL;
    return m;
}

extern list_t *list_sort(list_t **l)
{
    uint64_t s = list_size(*l);
    if (s <= 1)
        return *l;
    list_t *r = list_split(*l);
    *l = list_sort(l);
    r = list_sort(&r);
    if (compare_object(list_find_last(*l)->object, list_find_first(r)->object) > 0)
        *l = list_msort(*l, r);
    else
        list_join(*l, r);
    return *l;
}

static list_t *list_msort(list_t *l, list_t *r)
{
    int64_t x = list_size(l);
    int64_t y = list_size(r);
    list_t *a = list_create();
    while (x && y)
    {
        l = list_find_first(l);
        r = list_find_first(r);
        if (compare_object(l->object, r->object) <= 0)
        {
            list_append(&a, l->object);
            list_remove(&l, 0);
            x--;
        }
        else
        {
            list_append(&a, r->object);
            list_remove(&r, 0);
            y--;
        }
    }
    if (x)
        list_join(a, l);
    else
        list_join(a, r);
    return a;
}


static list_t *list_find_first(list_t *l)
{
    list_t *x = l;
    while (true)
        if (!x->prev)
            break;
        else
            x = x->prev;
    return x;
}

static list_t *list_find_last(list_t *l)
{
    list_t *x = l;
    while (true)
        if (!x->next)
            break;
        else
            x = x->next;
    return x;
}
