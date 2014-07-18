/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Bruce.Li <jiangwlee@163.com>, 2014
 */


#ifndef __SHM_COMMON_H_
#define __SHM_COMMON_H_

#include <sys/types.h>
#include <iostream>

#include "shm_stl_config.h"

__SHM_STL_BEGIN

static inline bool
is_power_of_2(u_int32_t num) {
    return (num & (num - 1)) == 0;
}

/* convert to a number to power of 2 which is greater than it
 * For example, convert 3 to power of 2, the result is 4
 */
static inline u_int32_t
convert_to_power_of_2(u_int32_t num) {
    u_int32_t bits = 0;
    while (num) {
        ++bits;
        num >>= 1;
    }

    // bits should not exceed 31
    if (bits >= sizeof(num) * 8)
        bits = sizeof(num) * 8 - 1;

#ifdef DEBUG
    std::cout << "bits is " << bits << std::endl;
    std::cout << "sizeof(num) * 8 = " << sizeof(num) * 8 << std::endl;
#endif
    return (bits == 0) ? 0 : 1 << bits;
}

/* Does integer division with rounding-up of result. */
inline u_int32_t
div_roundup(u_int32_t numerator, u_int32_t denominator)
{
    return (numerator + denominator - 1) / denominator;
}

/* Increases a size (if needed) to a multiple of alignment. */
inline u_int32_t
align_size(u_int32_t val, u_int32_t alignment)
{
    return alignment * div_roundup(val, alignment);
}

__SHM_STL_END

#endif
