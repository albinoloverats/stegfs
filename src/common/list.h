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

#ifndef _COMMON_LIST_H_
#define _COMMON_LIST_H_

/*!
 * \file    list.h
 * \author  albinoloverats ~ Software Development
 * \date    2021-2022
 * \brief   Common linked list code shared between projects
 *
 * Common linked list implementation.
 */

#include <stddef.h>
#include <stdbool.h>

#include "common.h"

typedef void * LIST; /*!< The user visible LIST type */

typedef void * ITER;

#if 0
#define LIST_INIT_COUNT(...) LIST_INIT_COUNT2(__VA_ARGS__, 3, 2, 1) /*!< Function overloading argument count (part 1) */
#define LIST_INIT_COUNT2(_1, _2, _3, _, ...) _                      /*!< Function overloading argument count (part 2) */
// doesn't (yet) work with zero arguments
#define list_init_0()         list_init_aux(NULL, true, false) /*<! Call list_init_aux with NULL and true for parameters    */
#define list_init_1(A)        list_init_aux(A, true, false)    /*<! Call list_init_aux with true for second parameter       */
#define list_init_2(A, B)     list_init_aux(A, B, false)       /*<! Call list_init_aux with both user supplied parameters   */
#define list_init_3(A, B, C)  list_init_aux(A, B, C)           /*<! Call list_init_aux with both user supplied parameters   */
#define list_init(...) CONCAT(list_init_, LIST_INIT_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Decide how to call list_init */
#endif

/*!
 * \brief         Create a new linked list
 * \return        A new linked list
 *
 * Create a new linked list instance; all further operations are then
 * performed against this handle. Returns NULL on error.
 */
#define list_default() list_init(NULL, true, false)

/*!
 * \brief         Create a new linked list
 * \return        A new linked list
 *
 * Create a new linked list instance that is intended to contain string
 * values. It allows duplicates and will sort the list according to
 * strcmp. Returns NULL on error.
 */
#define list_string() list_init((void *)strcmp, true, true)

/*!
 * \brief         Create a new linked list
 * \param[in]  c  A function to compare items within the list (can be NULL)
 * \param[in]  d  Whether to allow duplicates in the list
 * \param[in]  s  Whether items in the list should be sorted
 * \return        A new linked list
 *
 * Create a new linked list instance; all further operations are then
 * performed against this handle. Returns NULL on error. The comparator
 * is used when searching and removing items to check if the correct
 * item has been found or whether the item can be deleted. Use
 * list_default() if you are happy with no comparator and allowing
 * duplicates in your list.
 */
extern LIST list_init(int c(const void *, const void *), bool d, bool s) __attribute__((malloc));

#define LIST_DEINIT_ARGS_COUNT(...) LIST_DEINIT_ARGS_COUNT2(__VA_ARGS__, 2, 1) /*!< Function overloading argument count (part 1) */
#define LIST_DEINIT_ARGS_COUNT2(_1, _2, _, ...) _                              /*!< Function overloading argument count (part 2) */

#define list_deinit_1(A)     list_deinit_aux(A, NULL)  /*<! Call list_deinit_aux with false for second parameter */
#define list_deinit_2(A, B)  list_deinit_aux(A, B)     /*<! Call list_deinit_aux with both user supplied parameters */
#define list_deinit(...) CONCAT(list_deinit_, LIST_DEINIT_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Decide how to call list_deinit */

/*!
 * \brief         Destroy a linked list
 * \param[in]  h  A pointer to a linked list to destroy
 * \param[in]  f  A function to free data items within the list
 *
 * Destroy a previously created linked list when it is no longer needed.
 * Free the memory and sets h to NULL so all subsequent calls to LIST
 * functions will not result in undefined behaviour. If called with a
 * NULL function f then data items will not be freed.
 *
 */
extern void list_deinit_aux(LIST h, void f(void *)) __attribute__((nonnull(1)));

/*!
 * \brief         Get the number of items in the list
 * \param[in]  h  A pointer to the list
 * \return        The number of items in the list
 *
 * Get the number of item in the list. This is a constant time lookup as
 * the number of items is kept as metadata within the LIST.
 */
extern size_t list_size(LIST h) __attribute__((nonnull(1)));

//#define LIST_SIZE(l) (((ptrdiff_t)l)+4)

/*!
 * \brief         Add an item to the end of the list
 * \param[in]  h  A pointer to the list
 * \param[in]  d  The item to add to the list
 *
 * Add a new item to the end of the list. If the list is sorted then
 * this just calls list_add().
 */
extern void list_append(LIST h, const void *d) __attribute__((nonnull(1, 2)));

/*!
 * \brief         Insert an item into the list
 * \param[in]  h  A pointer to the list
 * \param[in]  i  The index where to insert the item
 * \param[in]  d  The item to be inserted
 *
 * Insert an item into the middle of the list, at the given index. If
 * this list is sorted then this just call list_add().
 */
extern void list_insert(LIST h, size_t i, const void *d) __attribute__((nonnull(1, 3)));

/*!
 * \brief         Add an item to the sorted list
 * \param[in]  h  A pointer to the list
 * \param[in]  d  The item to add to the list
 *
 * Add a new item to the list in sorted order. If the list is not sorted
 * then this just calls list_append().
 */
extern void list_add(LIST h, const void *d) __attribute__((nonnull(1, 2)));

/*!
 * \brief         Check if the list contains the item
 * \param[in]  h  A pointer to the list
 * \param[in]  d  The item to check for
 * \return        A pointer to the item already in the list, NULL
 *                otherwise
 *
 * Check whether the list contains the item. If a comparator was set
 * during initialisation then that is used instead of just comparing the
 * pointer. Returns a pointer to the existing item or NULL if it doesn't
 * exist in the list.
 */
extern const void *list_contains(LIST h, const void *d) __attribute__((nonnull(1, 2)));

/*!
 * \brief         Remove an item from within the list
 * \param[in]  h  A pointer to the list
 * \param[in]  d  The item to remove
 * \return        The item that was removed, or NULL
 *
 * Remove the given item from the list. The item is returned to allow
 * the user to free it if necessary. If the item is not in the list then
 * the function returns NULL. If a comparator was set during
 * initialisation then that is used instead of just comparing the
 * pointer.
 */
extern const void *list_remove_item(LIST h, const void *d) __attribute__((nonnull(1, 2)));

/*!
 * \brief         Remove an item from within the list
 * \param[in]  h  A pointer to the list
 * \param[in]  i  The index of the item to remove
 * \return        The item that was removed
 *
 * Remove the item at the given index from the list. The item is
 * returned to allow the user to free it if necessary.
 */
extern const void *list_remove_index(LIST h, size_t i) __attribute__((nonnull(1)));

/*!
 * \brief         Get an item from the list
 * \param[in]  h  A pointer to the list
 * \param[in]  i  The index of the item to get
 * \return        The item at the give index
 *
 * Retrieve the item at the given index within the list.
 */
extern const void *list_get(LIST h, size_t i) __attribute__((nonnull(1)));

/*!
 * \brief         Set the list up for iterating
 * \param[in]  h  A pointer to the list
 * \return        An iterator for the list
 *
 * Set the list up to be iterated over. The iterator should be freed
 * after use.
 */
extern ITER list_iterator(LIST h) __attribute__((nonnull(1)));

/*!
 * \brief         Get the next item in the list
 * \param[in]  h  A pointer to the iterator
 * \return        The next item in the list
 *
 * Allow iterating through the list, this returns the next item.
 */
extern const void *list_get_next(ITER h) __attribute__((nonnull(1)));

/*!
 * \brief         Indicates if there is another item in the list
 * \param[in]  h  A pointer to the iterator
 * \return        Returns true if there is another item
 *
 * Allow iterating through the list, this returns whether there is
 * another item.
 */
extern bool list_has_next(ITER h) __attribute__((nonnull(1)));

/*!
 * \brief         Add comparator to the list
 * \param[in]  h  A pointer to the list
 * \param[in]  c  The comparator to add
 *
 * Add a comparator to the list so that items can be compared.
 */
extern void list_add_comparator(LIST h, int c(const void *, const void *)) __attribute__((nonnull(1, 2)));

#endif
