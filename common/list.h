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

#ifndef _LIST_H_
#define _LIST_H_

/*!
 * \file
 * \author  albinoloverats ~ Software Development
 * \date    2009-2010
 * \brief   Common code for implementing a linked list
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /*!
     * \brief               A single list item object
     */
    typedef struct list_t
    {
        void *object;
        struct list_t *prev;
        struct list_t *next;
    }
    list_t;

    /*!
     * \brief               Provided the objects in the list have an ->id,
     *                      they can be compared using it; this structure
     *                      allows us to fake it :p
     */
    typedef struct compare_id_t
    {
        uintptr_t id;
    }
    compare_id_t;

    typedef list_t * LIST;

    /*!
     * \brief               Create a new list_t
     * \return              Newly created list_t structure
     */
    extern list_t *list_create(void);

    /*!
     * \brief               Delete a list_t
     * \param[in]   l       The list_t to delete and free
     */
    extern void list_delete(list_t **l);

    /*!
     * \brief               Add an object to the list
     * \param[in]   l       List to add object to
     * \param[in]   o       The object to add
     */
    extern void list_append(list_t **l, void *o);

    /*!
     * \brief               Remove i'th object from a list
     * \param[in]   l       Remove element from the list
     * \param[in]   i       The index of the object
     */
    extern void list_remove(list_t **l, uint64_t i);

    /*!
     * \brief               Get the i'th object from the list
     * \param[in]   l       The list to search through
     * \param[in]   i       The index of the object
     * \return              The object
     */
    extern list_t *list_get(list_t *l, uint64_t i);

    /*!
     * \brief               Return the number of elements in the list
     * \param[in]   l       The list
     * \return              The number of elements
     */
    extern uint64_t list_size(list_t *l);

    /*!
     * \brief               Join two lists toghether; both lists will then point to each other
     * \param[in]   l       The 1st list
     * \param[in]   m       The 2nd list
     */
    extern void list_join(list_t *l, list_t *m);

    /*!
     * \brief               Split the list into 2 lists each with 1/2 the elements in each
     * \param[in]   l       The list to split
     * \param[out]  l       1st 1/2
     * \param[out]  m       2nd 1/2
     */
    extern list_t *list_split(list_t *l);

    /*!
     * \brief               Sort the list
     * \param[in]   l       The list to sort
     * \param[out]  l       The sorted list
     * \return              The sorted list
     */
    extern list_t *list_sort(list_t **l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LIST_H_ */

#ifdef _IN_LIST_
    /*!
     * \brief               Perform the merge part of mergesort
     * \param[in]   l       1/2 the list to merge
     * \param[in]   r       1/2 the list to merge
     * \return              The merged result
     */
    static list_t *list_msort(list_t *l, list_t *r);

    /*!
     * \brief               Find the 1st element in the list
     * \param[in]   l       The list
     * \return              The 1st element
     */
    static list_t *list_find_first(list_t *l);

    /*!
     * \brief               Find the last element in the list
     * \param[in]   l       The list
     * \return              The last element
     */
    static list_t *list_find_last(list_t *l);

#endif /* _IN_LIST_ */
