/*
 * Common code for dealing with pairs of values.
 * Copyright © 2022-2022, albinoloverats ~ Software Development
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

#ifndef _COMMON_PAIR_H_
#define _COMMON_PAIR_H_

/*!
 * \file    pair.h
 * \author  albinoloverats ~ Software Development
 * \date    2022-2022
 * \brief   Common pair-o-values code shared between projects
 *
 * Common map implementation.
 */

#include <inttypes.h>
#include <stdbool.h>

typedef struct
{
	bool b1;
	bool b2;
}
pair_boolean_t;

typedef struct
{
	int64_t i1;
	int64_t i2;
}
pair_integer_t;

typedef struct
{
	_Float128 d1;
	_Float128 d2;
}
pair_decimal_t;

typedef struct
{
	char *s1;
	char *s2;
}
pair_string_t;

typedef struct
{
	void *p1;
	void *p2;
}
pair_object_t;

typedef union
{
	pair_boolean_t boolean;
	pair_integer_t integer;
	pair_decimal_t decimal;
	pair_string_t  string;
	pair_object_t  object;
}
pair_u;

#endif /* _COMMON_PAIR_H_ */
