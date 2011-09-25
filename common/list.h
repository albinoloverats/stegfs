/*
 * Common code for implementing a linked list
 * Copyright (c) 2009-2011, albinoloverats ~ Software Development
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

#ifndef _LIST_H_
    #define _LIST_H_

    /*!
     * \file    list.h
     * \author  albinoloverats ~ Software Development
     * \date    2009-2010
     * \brief   Common code for implementing a linked list
     *
     * Create and manage a linked list: add and remove items; search and sort
     * through the list, split lists and merge them back together
     */

    #include <inttypes.h>

    /*!
     * \brief  A single list item object
     *
     * Each item in the list is wrapped in one of these structures. It provides
     * a pointer to the object in question, as well as pointers to the previous
     * and next items (null if at either end of the list)
     */
    typedef struct list_t
    {
        const void *object;  /*!< Pointer to this item in the list */
        struct list_t *prev; /*!< Pointer to previous list item */
        struct list_t *next; /*!< Pointer to next list item */
    } __attribute__((aligned))
    list_t;

    /*!
     * \brief  Generic structure to allow items to be compared
     *
     * The easiest way to compare objects within the list is to use structures
     * which have an ->id element at the beginning. The comparison function can
     * then overlay this structure type and perform a simple greater/less than
     */
    typedef struct compare_id_t
    {
        uintptr_t id; /*!< ID used for object comparison */
    } __attribute__((aligned))
    compare_id_t;

    /*!
     * \brief  Constant for new and empty list object
     *
     * This constant is used to indicate a newly created list
     */
    #define NEW_LIST (void *)-1

    /*!
     * \brief  Number of iterations through the list when shuffling
     *
     * The shuffle function will work through the list this number of times
     * in an attempt to ensure that the list is shuffled sufficiently
     */
    #define SHUFFLE_FACTOR 4

    /*typedef list_t * LIST;*/

    /*
     * TODO find a way to make the function pointer optional; overload
     * the function with either 0 or 1 parameters
     */
    /*!
     * \brief          Create a new list_t
     * \param[in]  fn  Function pointer used to compare objects in the list
     * \return         Newly created list_t structure
     *
     * Create a new list which is ready to have objects inserted into it
     */
    extern list_t *list_create(int (*fn)(const void *, const void *));

    /*!
     * \brief         Delete a list_t
     * \param[in]  l  The list_t to delete and free
     *
     * Delete a list, removing any remaining elements and freeing associated
     * memory NB does not free objects within the list, only list elements
     */
    extern void list_delete(list_t **l) __attribute__((nonnull(1)));

    /*!
     * \brief         Add an object to the list
     * \param[in]  l  List to add object to
     * \param[in]  o  The object to add
     *
     * Append a new object onto the tail end of the list
     */
    extern void list_append(list_t **l, const void * const restrict o) __attribute__((nonnull(1, 2)));

    /*!
     * \brief         Remove i'th object from a list
     * \param[in]  l  Remove element from the list
     * \param[in]  i  The index of the object
     * \return        The data that was stored at this point in the list
     *
     * Remove the object (at the given position within the list) from the list
     */
    extern void *list_remove(list_t **l, const uint64_t i) __attribute__((nonnull(1)));

    /*!
     * \brief         Move to the i'th object from the list
     * \param[in]  l  The list to search through
     * \param[in]  i  The index of the object
     * \return        The list object at i
     *
     * Move along the list to the container around the object at the given index
     */
    extern list_t *list_move_to(list_t *l, const uint64_t i) __attribute__((nonnull(1)));

    /*!
     * \brief         Get the i'th object from the list
     * \param[in]  l  The list to search through
     * \param[in]  i  The index of the object
     * \return        A pointer to the object at i
     *
     * Get the object at the given position within the list
     */
    extern void *list_get(list_t *l, const uint64_t i) __attribute__((nonnull(1)));

    /*!
     * \brief         Return the number of elements in the list
     * \param[in]  l  The list
     * \return        The number of elements
     *
     * Count the number of elements within the list
     */
    extern uint64_t list_size(list_t *l) __attribute__((nonnull(1)));

    /*!
     * \brief         Join two lists together; both lists will then point to each other
     * \param[in,out]  l  The 1st list. One long list
     * \param[in,out]  m  The 2nd list. One long list
     *
     * Merge the two lists, giving us one long list (unsorted)
     */
    extern void list_join(list_t *l, list_t *m) __attribute__((nonnull(1, 2)));

    /*!
     * \brief             Split the list into 2 lists each with 1/2 the elements in each
     * \param[in,out]  l  The list to split. 1st 1/2 of split list
     * \return            2nd 1/2 of split list
     *
     * Split the list in half. It's unexpected this will be all that useful to
     * the average user, but is used when sorting lists
     */
    extern list_t *list_split(list_t *l) __attribute__((nonnull(1)));

    /*!
     * \brief             Sort the list
     * \param[in,out]  l  The list to sort. The sorted list
     * \return            The sorted list
     *
     * Sort the list using the comparison function provided when the list was created
     */
    extern list_t *list_sort(list_t **l) __attribute__((nonnull(1)));

    /*!
     * \brief             Randomly shuffle this list
     * \param[in,out]  l  The list of shuffle. The shuffled list
     * \return            The shuffled list
     *
     * Randomly shuffle the items in the list
     */
    extern list_t *list_shuffle(list_t **l) __attribute__((nonnull(1)));

    #ifdef DEBUGGING
        /*!
         * \brief         Print out the list structure
         * \param[in]  l  The list to print the structure of
         *
         * Print out the structure of the list: all the elements and their values as well as the
         * pointers to the previous/next elements
         */
        extern void list_debug(list_t *l);
    #endif
#endif

#ifdef __LIST__H__
    #ifndef _BEEN_IN_LIST_
        #define _BEEN_IN_LIST_

        /*!
         * \brief         Perform the merge part of mergesort
         * \param[in]  l  1/2 the list to merge
         * \param[in]  r  1/2 the list to merge
         * \return        The merged result
         *
         * Internal function to perform the merge in the merge sort algorithm
         */
        static list_t *list_msort(list_t *l, list_t *r) __attribute__((nonnull(1, 2)));

        /*!
         * \brief         Find the 1st element in the list
         * \param[in]  l  The list
         * \return        The 1st element
         *
         * Internal function to move to the first item in the list
         */
        static list_t *list_find_first(list_t * const restrict l) __attribute__((nonnull(1)));

        /*!
         * \brief         Find the last element in the list
         * \param[in]  l  The list
         * \return        The last element
         *
         * Internal function to move to the last item in the list
         */
        static list_t *list_find_last(list_t * const restrict l) __attribute__((nonnull(1)));

        /*!
         * \brief         A generic compare by ID
         * \param[in]  a  A pointer to an object
         * \param[in]  b  A pointer to an object
         * \return        Whether objects differ (see: strcmp)
         *
         * A generic compare function which checks the objects for an ->id (as in compare_id_t)
         */
        static int list_generic_compare(const void *a, const void *b) __attribute__((nonnull(1,2)));

        /*!
         * \brief         Function pointer for list comparison function
         * \param[in]  a  A pointer to an object
         * \param[in]  b  A pointer to an object
         * \return        Whether objects differ (see: strcmp)
         *
         * Internal function pointer which points to the function to be used for comparing
         * objects in the list
         */
        static int (*list_compare_function)(const void *a, const void *b);

    #endif /* _BEEN_IN_LIST_ */
    #undef __LIST__H__
#endif
