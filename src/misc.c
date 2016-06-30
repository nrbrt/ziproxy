/* misc.c
 * Miscellaneous routines.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2005-2014 Daniel Mealha Cabrita
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

#include <string.h>

/* Remove heading and trailing spaces/control_chars
   Used when collecting a parameter, to avoid things like
   mismatched strings when they should match instead.
   in_str: string to be cleaned
   out_str: cleaned string (malloc at least the same as in_str)
   in_str may be the same as out_str */
void misc_cleanup_string (const char *in_str, char *out_str)
{
	char *out_pos;
	const char *in_pos;
	int out_len;

	/* skip the heading junk */
	in_pos = in_str;
	while ((*in_pos <= ' ') && (*in_pos != '\0'))
		in_pos++;

	/* copy the rest (no strcpy because they may overlap) */
	out_pos = out_str;
	while (*in_pos != '\0')
		*(out_pos++) = *(in_pos++);
	*out_pos = '\0';

	/* remove the trailing junk */
	out_len = strlen (out_str);
	out_pos = out_str + out_len - 1;
	while ((out_len-- > 0) && (*out_pos <= ' '))
		*(out_pos--) = '\0';
}

/* convert in_str uppercase characters and dump the resulting string
   to out_str.
   in_str may be the same as out_str */
void misc_convert_str_tolower (const char *in_str, char *out_str)
{
	const char *in_pos = in_str;
	char *out_pos = out_str;
	char in_char;

	in_char = *in_pos;
	while (in_char != '\0') {
		*(out_pos++) = in_char + (((in_char >= 'A') && (in_char <= 'Z')) ? 32 : 0);
		in_char = *(++in_pos);
	}
	*out_pos = '\0';
}

