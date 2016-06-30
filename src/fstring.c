/* fstring.c
 * FASTER STRING ROUTINES REPLACING STRING.H ONES (may be an incomplete implementation)
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2007-2014 Daniel Mealha Cabrita
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


#include "fstring.h"

#ifdef USE_CUSTOM_STRING_ROUTINES

/* returns ==0 equal, !=0 different */
int custom_strncasecmp (const unsigned char *src1, const unsigned char *src2, int len)
{
	int	retcode = 0;
	
	while (len--) {
		if (lowercase_table [*(src1++)] != lowercase_table [*(src2++)]) {
			retcode = 1;
			len = 0;
		}
	}
	return (retcode);
}

/* returns ==0 equal, !=0 different */
int custom_strncmp (const unsigned char *src1, const unsigned char *src2, int len)
{
	int     retcode = 0;

	while (len--) {
		if (*(src1++) != *(src2++)) {
			retcode = 1;
			len = 0;
		}
	}
	return (retcode);
}

int custom_strlen (const unsigned char *src)
{
	int	srclen = 0;

	while (*(src++) != '\0')
		srclen++;

	return (srclen);
}

#endif

