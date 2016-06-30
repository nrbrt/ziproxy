/* urltables.c
 * Routines for URL lookup from tables (for use with ACLs etc).
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
#include <stdlib.h>
#include <stdio.h>
#include "txtfiletools.h"
#include "urltables.h"
#include "strtables.h"

t_ut_urltable *ut_create (void)          
{
	t_ut_urltable *ut_urltable;

	if ((ut_urltable = malloc (sizeof (t_ut_urltable))) != NULL) {
		if ((ut_urltable->host = st_create ()) != NULL) {
			if ((ut_urltable->url = st_create ()) != NULL)
				return (ut_urltable);
			free (ut_urltable->host);
		}
		free (ut_urltable);
	}

	return (NULL);
}

void ut_destroy (t_ut_urltable *ut_urltable)
{
	st_destroy (ut_urltable->url);
	st_destroy (ut_urltable->host);
	free (ut_urltable);
}

/* returns a string which is '<host>/<path>'
 * this string must be free'ed manually later */
char *ut_aux_build_url (const char *host, const char *path)
{
	int buflen;
	char *newstr;

	if (*path == '/')
		path++;
	
	buflen = strlen (host) + strlen (path) + 2; /* "/\0" */
	if ((newstr = calloc (buflen, sizeof (char))) == NULL)
		return (NULL);

	sprintf (newstr, "%s/%s", host, path);
	return (newstr);
}

void ut_insert (t_ut_urltable *ut_urltable, const char *host, const char *path)
{
	char *url;
	
	url = ut_aux_build_url (host, path);

	st_insert (ut_urltable->host, host);
	st_insert (ut_urltable->url, url);

	free (url);
}

/* returns !=0 if present, ==0 if not */
int ut_check_if_matches (const t_ut_urltable *ut_urltable, const char *host, const char *path)
{
	const char *url;

	url = ut_aux_build_url (host, path);

	if (st_check_if_matches (ut_urltable->host, host)) {
		if (st_check_if_matches (ut_urltable->url, url)) {
			return (1);
		}
	}
	return (0);
}

/* ======================================================================= */

/* same as ut_create() but also populates the tables with the URLs in the specified file */
t_ut_urltable *ut_create_populate_from_file (const char *filename)
{
	char *urltxt;
	char *url_pos, *lone_url;
	t_ut_urltable *ut_urltable;
	int linelen, useful_linelen;
	char *host, *path;
	int hostlen;

	if ((urltxt = load_textfile_to_memory (filename)) == NULL)
		return (NULL);
	
	if ((ut_urltable = ut_create ()) == NULL) {
		free (urltxt);
		return (NULL);
	}

	/* preprocess text in order to be more palatable */
	fix_linebreaks_qp (urltxt, strlen (urltxt), urltxt);
	remove_junk_data (urltxt, urltxt);

	/* this routine assumes there are no empty lines (just LF) */
	url_pos = urltxt;
	while ((linelen = get_line_len (url_pos)) != 0) {
		if (*(url_pos + (linelen - 1)) == '\n')
			useful_linelen = linelen - 1;
		else
			useful_linelen = linelen;

		lone_url = calloc (useful_linelen + 1, sizeof (char));
		strncpy (lone_url, url_pos, useful_linelen);
		*(lone_url + useful_linelen) = '\0';

		/* inserts only http URLs */
		if (strncmp (lone_url, "http://", 7) == 0) {
			host = lone_url + 7;
			hostlen = get_line_len_char (host, '/');
			path = host + hostlen;

			/* adjust hostname string */
			strncpy (lone_url, host, hostlen);
			host = lone_url;
			if (*(host + (hostlen - 1)) == '/')
				*(host + (hostlen - 1)) = '\0';
			else
				*(host + hostlen) = '\0';
		
			/* empty path means root path "/" */
			if (*path != '\0')
				ut_insert (ut_urltable, host, path);
			else
				ut_insert (ut_urltable, host, "/");
		}

		free (lone_url);
		url_pos += linelen;
	}

	free (urltxt);
	return (ut_urltable);
}


