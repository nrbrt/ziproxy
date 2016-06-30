/* simplelist.c
 * High level routines for dealing with text files (read-only) with an item per line.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "txtfiletools.h"
#include "strtables.h"
#include "simplelist.h"

/* loads text file into memory and return struct to be used for queries
   returns NULL if unable to load file or create structure */
t_st_strtable *slist_create (const char* given_filename)
{
	char *filedata;
	int filedata_len;
	t_st_strtable *slist_table;
	int linelen;
	char *curpos;

	if ((filedata = load_textfile_to_memory (given_filename)) != NULL) {
		filedata_len = strlen (filedata);
		fix_linebreaks_qp (filedata, filedata_len, filedata);
		remove_junk_data (filedata, filedata);
		if ((slist_table = st_create ()) != NULL) {
			curpos = filedata;
			while ((linelen = get_line_len (curpos))) {
				if (*(curpos + linelen - 1) == '\n')
					*(curpos + linelen - 1) = '\0';
				if (strchr (curpos, '*') == NULL)
					st_insert_nometa (slist_table, curpos);
				else
					st_insert (slist_table, curpos);
				curpos += linelen;
			}
			/* finished, return */
			free (filedata);
			return (slist_table);
		}
		free (filedata);
	}
	return (NULL);
}

void slist_destroy (t_st_strtable *slist_table)
{
	st_destroy (slist_table);
}

/* if string is present in the list, returns !=0
   otherwise returns 0.
   this function makes pattern-mathing (based on '*') */
int slist_check_if_matches (t_st_strtable *slist_table, const char *strdata)
{
	return (st_check_if_matches (slist_table, strdata));
}

