/* cttables.c
 * Routines for Content-Type lookup from tables
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
#include "cttables.h"
#include "strtables.h"
#include "misc.h"

/* NOTE WHEN READING THE CODE:
	anything looking like:
	contenttype -> full content-type string (aaaa/bbbb)
	cttype -> only the main type of the content-type (aaaa)
	subtype -> only the subtype of the content-type (bbbb) */

/* creates the table
	if subtype_add_x_prefix==1, then for each content-type
	added with ct_insert(), adds an additional content-type
	with "x-" prefixed to the subtype.
	ex.: aaaa/bbbb -> adds: aaaa/bbbb AND aaaa/x-bbbb
	EXCEPT when:
		the subtype already has a "x-" prefix,
		or the subtype is empty */
t_ct_cttable *ct_create (int subtype_add_x_prefix)
{
	t_ct_cttable *ct_cttable;

	if ((ct_cttable = calloc (1, sizeof (t_ct_cttable))) != NULL) {
		if ((ct_cttable->cttype = st_create ()) != NULL) {
			if ((ct_cttable->ctcontenttype = st_create ()) != NULL) {
				ct_cttable->subtype_add_x_prefix = subtype_add_x_prefix;
				return (ct_cttable);
			}
			free (ct_cttable->cttype);
		}
		free (ct_cttable);
	}

	return (NULL);
}

void ct_destroy (t_ct_cttable *ct_cttable)
{
	st_destroy (ct_cttable->ctcontenttype);
	st_destroy (ct_cttable->cttype);
	free (ct_cttable);
}

/* returns a string which is '<host>/<path>'
 * this string must be free'ed manually later */
char *ct_aux_rebuild_contenttype (const char *in_cttype, const char *in_ctsubtype)
{
	int buflen;
	char *newstr;
	int in_ctsubtype_len;

	in_ctsubtype_len = strlen (in_ctsubtype);

	buflen = strlen (in_cttype) + in_ctsubtype_len + 2; /* "/\0" */
	if ((newstr = calloc (buflen, sizeof (char))) == NULL)
		return (NULL);

	if (in_ctsubtype_len != 0)
	        sprintf (newstr, "%s/%s", in_cttype, in_ctsubtype);
	else
		sprintf (newstr, "%s", in_cttype);

        return (newstr);
}

/* returns a pointer to a string containing the
   'type' part of the content type,
	or an empty string (if content-type is empty),
	or NULL (malloc error).
   this pointer must be free()'ed manually later */
char *ct_extract_cttype (const char *ct_contenttype)
{
	char *local_cttype;
	char *cttype_slash;	/* points to a position in local_cttype */

	if ((local_cttype = strdup (ct_contenttype)) == NULL)
		return (NULL);

	if ((cttype_slash = strchr (local_cttype, '/')) != NULL)
		*cttype_slash = '\0';

	return (local_cttype);
}

/* returns a pointer to a string containing the
   'subtype' part of the content type,
	or an empty string (if content-type is empty),
	or NULL (malloc error).
   this pointer must be free()'ed manually later */
char *ct_extract_ctsubtype (const char *ct_contenttype)
{
	char *local_subtype;
	char *subtype_slash;	/* points to a position in ct_contenttype */

	if ((subtype_slash = strchr (ct_contenttype, '/')) == NULL) {
		if ((local_subtype = (char *) calloc (1, sizeof (char))) == NULL)
			return (NULL);
		*local_subtype = '\0';
	} else {
		if ((local_subtype = (char *) calloc ((strlen (subtype_slash + 1) + 1), sizeof (char))) == NULL)
			return (NULL);
		strcpy (local_subtype, subtype_slash + 1);
	}

	return (local_subtype);
}

void ct_insert (t_ct_cttable *ct_cttable, const char *ctcontenttype_in)
{
	char *ctcontenttype;	/* to-lower-case'ed content-type */
	char *local_cttype;
	char *local_subtype;
	char *local_contenttype;
	char *local_subtype_x;
	char *local_contenttype_x;
	char *temp_str;

	/* content-type is case-insensitive, we'll work with lowercase only */
	ctcontenttype = strdup (ctcontenttype_in);
	misc_convert_str_tolower (ctcontenttype, ctcontenttype);
	
	local_cttype = ct_extract_cttype (ctcontenttype);
	local_subtype = ct_extract_ctsubtype (ctcontenttype);
	local_contenttype = ct_aux_rebuild_contenttype (local_cttype, local_subtype);

	st_insert (ct_cttable->cttype, local_cttype);
	st_insert (ct_cttable->ctcontenttype, local_contenttype);

	/* if content-type is "aaaa / *", adds "aaaa" too */
	if (! strcmp ("*", local_subtype)) {
		st_insert (ct_cttable->cttype, local_cttype);
		st_insert (ct_cttable->ctcontenttype, local_cttype);
	}

	/* if content-type is "aaaa", adds "aaaa / *" too */
	if (*local_subtype == '\0') {
		if ((temp_str = calloc (strlen (local_contenttype) + 3, sizeof (char))) != NULL) {
			sprintf (temp_str, "%s/*", local_cttype);
			st_insert (ct_cttable->cttype, local_cttype);
			st_insert (ct_cttable->ctcontenttype, temp_str);
			free (temp_str);
		}
	}

	/* adds "x-" preffix to subtype if requested (during ct_create()) AND there is a subtype specified */
	if ((ct_cttable->subtype_add_x_prefix) && (*local_subtype != '\0')) {
		if (strncmp ("x-", local_subtype, 2) && (strlen (local_subtype) > 0)) {
			if ((local_subtype_x = calloc (strlen (local_subtype) + 3, sizeof (char))) != NULL) {
				sprintf (local_subtype_x, "x-%s", local_subtype);
				local_contenttype_x = ct_aux_rebuild_contenttype (local_cttype, local_subtype_x);

				st_insert (ct_cttable->cttype, local_cttype);
				st_insert (ct_cttable->ctcontenttype, local_contenttype_x);

				free (local_contenttype_x);
				free (local_subtype_x);
			}
		}
	}

	free (local_cttype);
	free (local_subtype);
	free (local_contenttype);
	free (ctcontenttype);
}

/* returns !=0 if present, ==0 if not */
/* if ctcontenttype==NULL, this routine will consider
	that as an empty string instead "" */
int ct_check_if_matches (const t_ct_cttable *ct_cttable, const char *ctcontenttype_in)
{
	char *ctcontenttype;	/* to-lower-case'ed content-type */
	char *local_cttype;
	int retcode = 0;

	if (ctcontenttype_in == NULL) {
		ctcontenttype = strdup ("");
	} else {
		/* content-type is case-insensitive, we'll work with lowercase only */
		ctcontenttype = strdup (ctcontenttype_in);
		misc_convert_str_tolower (ctcontenttype, ctcontenttype);
	}

	local_cttype = ct_extract_cttype (ctcontenttype);

	if (st_check_if_matches (ct_cttable->cttype, local_cttype)) {
		if (st_check_if_matches (ct_cttable->ctcontenttype, ctcontenttype)) {
			retcode = 1;
		}
	}
	free (local_cttype);
	free (ctcontenttype);
	return (retcode);
}

