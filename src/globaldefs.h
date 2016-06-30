/* globaldefs.h
 * Global definitions, types, macros etc.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2009-2014 Daniel Mealha Cabrita
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * ---------------------------------------------------------------------
 */

// To stop multiple inclusions.
#ifndef SRC_GLOBALDEFS_H
#define SRC_GLOBALDEFS_H

#include <stdlib.h>

typedef int SOCKET;
typedef unsigned int UINT;

/* defines ZP_DATASIZE_TYPE (must be at least 64bit) */
#if SIZEOF_INT >= 8
/* int is at least 64bit */
#define ZP_DATASIZE_TYPE int
#define ZP_DATASIZE_STR "d"
#define ZP_DATASIZE_MSTR ""
#define ZP_CONVERT_STR_TO_DATASIZE(a) atoi(a)
#elif SIZEOF_LONG_INT >= 8
/* long int is at least 64bit */
#define ZP_DATASIZE_TYPE long int
#define ZP_DATASIZE_STR "ld"
#define ZP_DATASIZE_MSTR "l"
#define ZP_CONVERT_STR_TO_DATASIZE(a) atol(a)
#else
/* last resort, assumes long long int is at least 64bit */
#define ZP_DATASIZE_TYPE long long int
#define ZP_DATASIZE_STR "lld"
#define ZP_DATASIZE_MSTR "ll"
#define ZP_CONVERT_STR_TO_DATASIZE(a) atoll(a)
#endif

/* defines ZP_TIMEVAL_TYPE (must be at least 64bit) */
#if SIZEOF_INT >= 8
/* int is at least 64bit */
#define ZP_TIMEVAL_TYPE int
#define ZP_TIMEVAL_STR "d"
#define ZP_TIMEVAL_MSTR ""
#elif SIZEOF_LONG_INT >= 8
/* long int is at least 64bit */
#define ZP_TIMEVAL_TYPE long int
#define ZP_TIMEVAL_STR "ld"
#define ZP_TIMEVAL_MSTR "l"
#else
/* last resort, assumes long long int is at least 64bit */
#define ZP_TIMEVAL_TYPE long long int
#define ZP_TIMEVAL_STR "lld"
#define ZP_TIMEVAL_MSTR "ll"
#endif

/* defines ZP_FLAGS (must be at least 32bit) */
#if SIZEOF_INT >= 4
/* int is at least 32bit */
#define ZP_FLAGS int
#elif SIZEOF_LONG_INT >= 4
/* long int is at least 32bit */
#define ZP_FLAGS long int
#else
/* last resort, assumes long long int is at least 32bit */
#define ZP_FLAGS long long int
#endif

/* uses GCC-specific optimizations, if supported. */
#if defined(__GNUC__) && (__GNUC__ >= 2) && defined(__OPTIMIZE__)
/* compiler supports that, good */
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
/* compiler does not support that, make those innocuous */
#define LIKELY(x) x
#define UNLIKELY(x) x
#endif

#endif //SRC_GLOBALDEFS_H

