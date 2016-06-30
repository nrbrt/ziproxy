/* txtfiletools.h
 * Misc tools when dealing with text files.
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

//To stop multiple inclusions.
#ifndef SRC_TXTFILETOOLS_H
#define SRC_TXTFILETOOLS_H

extern int fix_linebreaks_qp (const unsigned char *src, int srclen, unsigned char *dst);
extern char *load_textfile_to_memory (const char *given_filename);
extern int get_line_len_char (const char *src, const char which_char);
extern int get_line_len (const char *src);
extern void remove_junk_data (const char *src, char *dst);
extern int file_exists (const char *given_filename);
extern int dump_string_to_textfile (const char *given_filename, const char *given_string);

#endif

