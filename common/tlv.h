/*
 * Common TLV code
 * Copyright (c) 2011, albinoloverats ~ Software Development
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
     * \date    2011
     * \brief   Common code for manipulating TLV structures
     *
     * Common code for building and parsing TLV structures and data
     * buffers
     */

    #include <inttypes.h>
    #include "common.h"
    #include "list.h"

    /*
     * TODO add Doxygen comments
     */

    typedef struct tlv_t
    {
        uint8_t tag;
        uint16_t length;
        uint8_t *value;
    } __attribute__((packed))
    tlv_t;

    extern tlv_t tlv_combine(uint8_t t, uint16_t l, void *v) __attribute__((nonnull(3)));

    extern uint64_t tlv_build(uint8_t **b, list_t *l) __attribute__((nonnull(2)));

#endif /* _COMMON_TLV_H_ */
