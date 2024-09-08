/*
 * Common code for working with libgcrypt
 * Copyright © 2005-2024, albinoloverats ~ Software Development
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

#ifndef _COMMON_CRYPT_H_
#define _COMMON_CRYPT_H_

#include <gcrypt.h>

#include "list.h"

#define NEED_LIBGCRYPT_VERSION "1.7.10"

/*!
 * \brief          Initialise libgcrypt library
 *
 * Initialise the libgcrypt library. Subsequent calls to the function
 * are ignored.
 */
extern void init_crypto(void);

/*!
 * \brief         Get list of usable ciphers
 * \return        A list of char* cipher names
 *
 * Get a list of strings which lists the names of usable cipher
 * algorithms. NB: The list is allocated statically and SHOULD NOT be
 * free’d (or otherwise altered).
 */
extern LIST list_of_ciphers(void) __attribute__((pure));

/*!
 * \brief         Get list of usable hashes
 * \return        An array of char* of hash names
 *
 * Get an array of strings which lists the names of usable hash
 * algorithms. NB: The array is allocated statically and SHOULD NOT be
 * free’d (or otherwise altered).
 */
extern LIST list_of_hashes(void) __attribute__((pure));

/*!
 * \brief         Get list of available cipher modes
 * \return        An array of char* of mode names
 *
 * Get an array of strings which lists the names of available cipher
 * modes. NB: The array is allocated statically and SHOULD NOT be
 * free’d (or otherwise altered).
 */
extern LIST list_of_modes(void) __attribute__((pure));

/*!
 * \brief         Get list of available MAC algorithms
 * \return        An array of char* of MAC names
 *
 * Get an array of strings which lists the names of available MAC
 * algorithms. NB: The array is allocated statically and SHOULD NOT be
 * free’d (or otherwise altered).
 */
extern LIST list_of_macs(void) __attribute__((pure));

/*!
 * \brief         Get cipher ID, given its name
 * \param[in]  n  Cipher name
 * \return        The ID used by libgcrypt
 *
 * Get the ID used internally by libgcrypt for the given cipher name.
 */
extern enum gcry_cipher_algos cipher_id_from_name(const char * const restrict n) __attribute__((pure, nonnull(1)));

/*!
 * \brief         Get hash ID, given its name
 * \param[in]  n  Hash name
 * \return        The ID used by libgcrypt
 *
 * Get the ID used internally by libgcrypt for the given hash name.
 */
extern enum gcry_md_algos hash_id_from_name(const char * const restrict n) __attribute__((pure, nonnull(1)));

/*!
 * \brief         Get cipher mode ID, given its name
 * \param[in]  n  Mode name
 * \return        The ID used by libgcrypt
 *
 * Get the ID used internally by libgcrypt for the given mode name.
 */
extern enum gcry_cipher_modes mode_id_from_name(const char * const restrict n) __attribute__((pure, nonnull(1)));

/*!
 * \brief         Get MAC ID, given its name
 * \param[in]  n  MAC name
 * \return        The ID used by libgcrypt
 *
 * Get the ID used internally by libgcrypt for the given MAC.
 */
extern enum gcry_mac_algos mac_id_from_name(const char * const restrict n) __attribute__((pure, nonnull(1)));

/*!
 * \brief         Get cipher name, given its ID
 * \param[in]  c  The libgcrypt cipher enum
 * \return        A string representation of the cipher
 *
 * "Correct" the name that libgcrypt gives us; this ensures
 * compatibility with the Android version.
 */
extern const char *cipher_name_from_id(enum gcry_cipher_algos c) __attribute__((pure));

/*!
 * \brief         Get hash name, given its ID
 * \param[in]  h  The libgcrypt hash enum
 * \return        A string representation of the hash
 *
 * "Correct" the name that libgcrypt gives us; this ensures
 * compatibility with the Android version.
 */
extern const char *hash_name_from_id(enum gcry_md_algos h) __attribute__((pure));

/*!
 * \brief         Get cipher mode name, given its ID
 * \param[in]  m  The libgcrypt mode enum
 * \return        A string representation of the mode
 *
 * As this isn’t provided by libgcrypt, in the same way that it is for
 * algorithms, and the fact we’re artificially limiting the choices,
 * here’s a function to get the name from the enum.
 */
extern const char *mode_name_from_id(enum gcry_cipher_modes m) __attribute__((pure));

/*!
 * \brief         Get MAC algorithm name, given its ID
 * \param[in]  m  The libgcrypt MAC enum
 * \return        A string representation of the MAC
 *
 * "Correct" the name that libgcrypt gives us; this ensures
 * compatibility with the Android version.
 */
extern const char *mac_name_from_id(enum gcry_mac_algos m) __attribute__((pure));

/*!
 * \brief         Get cipher mode name, given its ID
 * \param[in]  m  The libgcrypt mode enum
 * \return        A string representation of the mode
 *
 * As this isn’t provided by libgcrypt, in the same way that it is for
 * algorithms, and the fact we’re artificially limiting the choices,
 * here’s a function to get the name from the enum.
 */
extern const char *mode_name_from_id(enum gcry_cipher_modes m) __attribute__((pure));

/*!
 * \brief         Verify whether a give cipher and mode are suitable
 * \param[in]  c  The cipher to check
 * \param[in]  m  The cipher mode
 * \return        True if the mode is suitable for the cipher, false
 *                otherwise
 *
 * Check if the given cipher mode is usable for the given cipher. For
 * instance stream ciphers require a stream capable mode.
 */
extern bool mode_valid_for_cipher(enum gcry_cipher_algos c, enum gcry_cipher_modes m);

#endif /* _COMMON_CRYPT_H_ */
