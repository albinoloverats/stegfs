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

#ifndef _COMMON_TLV_H_
#define _COMMON_TLV_H_

/*!
 * \file    tlv.h
 * \author  albinoloverats ~ Software Development
 * \date    2009-2022
 * \brief   Common TLV code shared between projects
 *
 * Common tag/length/value code, for creating, importing and exporting
 * TLV values.
 */

#include <stdint.h> /*!< Necessary include as c99 standard integer types are referenced in this header */
#include <stdbool.h> /*!< Necessary include as c99 standard boolean type is referenced in this header */

typedef void * TLV; /*<! Handle type for TLV functions */

/*!
 * \brief  A TLV structure
 *
 * The standard tag/length/value triple. Used to add a new TLV to the
 * array.
 */
typedef struct
{
	uint8_t  tag;    /*!< The tag value */
	uint16_t length; /*!< The length of the value */
	void    *value;  /*!< The actual data */
}
tlv_t;

/*!
 * \brief         Create a new TLV array
 * \return        A new TLV array
 *
 * Create a new TLV array instance; all further operations are then
 * performed against this handle. Returns NULL on error.
 */
extern TLV tlv_init(void) __attribute__((malloc));

/*!
 * \brief         Destroy a TLV array
 * \param[in]  h  A pointer to a TLV array to destroy
 * \param[in]  f  Whether to free the values within the TLV
 *
 * Destroy a previously created TLV array when it is no longer needed.
 * Free the memory and sets h to NULL so all subsequent calls to TLV
 * functions will not result in undefined behaviour.
 */
extern void tlv_deinit(TLV *h) __attribute__((nonnull(1)));

/*!
 * \brief         Append a new TLV triple to an array
 * \param[in]  h  A pointer to the TLV
 * \param[in]  t  The TLV triple to add
 *
 * Add a new TLV triple to the end of an existing TLV array. Does not
 * (currently) check for duplicates. Potentially modifies the address of
 * the TLV in accordance with the rules of realloc().
 */
extern void tlv_append(TLV h, tlv_t t) __attribute__((nonnull(1)));

/*!
 * \brief         Get TLV structure for tag
 * \param[in]  h  The TLV array to check
 * \param[in]  t  The tag value to look for
 * \return        The individual TLV structure
 *
 * Return the TLV structure for the given tag value. NB Do not free the
 * returned TLV pointer: bad things will happen.
 */
extern const tlv_t *tlv_get(TLV h, uint8_t t) __attribute__((nonnull(1)));

/*!
 * \brief         Check whether a TLV array has a particular tag
 * \param[in]  h  The TLV array to check
 * \param[in]  t  The tag value to look for
 * \return        Whether the tag is used
 *
 * Check whether the TLV array has the tag given value.
 */
extern bool tlv_has_tag(TLV h, uint8_t t) __attribute__((nonnull(1)));

#define TLV_VALUE_OF_ARGS_COUNT(...) TLV_VALUE_OF_ARGS_COUNT2(__VA_ARGS__, 3, 2, 1) /*!< Function overloading argument count (part 1) */
#define TLV_VALUE_OF_ARGS_COUNT2(_1, _2, _3, _, ...) _                              /*!< Function overloading argument count (part 2) */

#define tlv_value_of_2(A, B)     tlv_value_of_aux(A, B, NULL) /*<! Call tlv_value_of_aux with NULL for third parameter */
#define tlv_value_of_3(A, B, C)  tlv_value_of_aux(A, B, C)    /*<! Call tlv_value_of_aux with both user supplied parameters */
#define tlv_value_of(...) CONCAT(tlv_value_of_, TLV_VALUE_OF_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Decide how to call tlv_value_of */

/*!
 * \brief         Get value for tag
 * \param[in]  h  The TLV array to check
 * \param[in]  t  The tag value to get
 * \param[in]  d  The default to use if the tag doesn't exist
 * \return        The value of the tag
 *
 * Get the value for the given tag. If there are duplicates in the array
 * this will return the value of the first.
 */
extern byte_t *tlv_value_of_aux(TLV h, uint8_t t, uint8_t *d) __attribute__((nonnull(1)));

/*!
 * \brief         Get length for tag
 * \param[in]  h  The TLV array to check
 * \param[in]  t  The tag length to get
 * \return        The length for the given tag
 *
 * Retrieve the length for the TLV entry with the given tag value.
 */
extern uint16_t tlv_length_of(TLV ptr, uint8_t t) __attribute__((nonnull(1)));

#define TLV_EXPORT_ARGS_COUNT(...) TLV_EXPORT_ARGS_COUNT2(__VA_ARGS__, 2, 1) /*!< Function overloading argument count (part 1) */
#define TLV_EXPORT_ARGS_COUNT2(_1, _2, _, ...) _                             /*!< Function overloading argument count (part 2) */

#define tlv_export_1(A)       tlv_export_aux(A, true) /*<! Call tlv_export_aux with value of true for second parameter */
#define tlv_export_2(A, B)    tlv_export_aux(A, B)    /*<! Call tlv_export_aux with both user supplied parameters */
#define tlv_export(...) CONCAT(tlv_export_, TLV_EXPORT_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__) /*!< Decide how to call tlv_export */

/*!
 * \brief         Export the TLV array
 * \param[in]  h  The TLV array to export
 * \param[in]  e  (Optional) Endianness of length value
 * \return        The TLV array as a byte array
 *
 * Export the TLV array as an unsigned byte array. The optional boolean
 * (defaults to true) specifies wherther to use network byte order for
 * the length value.
 */
extern byte_t *tlv_export_aux(TLV h, bool e) __attribute__((nonnull(1)));

/*!
 * \brief         The number of TLV items in the array
 * \param[in]  h  The TLV array to count
 * \return        The number of TLV items
 *
 * Count the number of TLV elements in the array.
 */
extern uint16_t tlv_count(TLV h) __attribute__((nonnull(1)));

/*!
 * \brief         The size of TLV array
 * \param[in]  h  The TLV array to size
 * \return        The size of the TLV array
 *
 * Return the total size of the TLV array in bytes; useful when calling
 * tlv_export().
 */
extern size_t tlv_size(TLV h) __attribute__((nonnull(1)));

#endif /* _COMMON_TLV_H_ */
