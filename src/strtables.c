/* strtables.c
 * Auxiliary routines for list generation and list lookup.
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

int string_fingerprint (const char *given_string)
{
	int     fingerprint = 0;
	int     this_char;

	this_char = (int) *given_string;
	while (this_char != 0) {
		fingerprint += this_char;
		fingerprint ^= (this_char) << 15;
		fingerprint ^= fingerprint << 7;
		fingerprint ^= fingerprint >> 24;

		this_char = (int) *given_string;
		given_string++;
	}

	return (fingerprint);
}

t_st_strtable *st_create (void)
{
	t_st_strtable *st_strtable;

	if ((st_strtable = malloc (sizeof (t_st_strtable))) == NULL)
		return (NULL);

	st_strtable->nometa.total_entries = 0;
	st_strtable->nometa.hashes_tab = NULL;
	st_strtable->nometa.exp_tab = NULL;
	st_strtable->nometa.exp_data = NULL;
	st_strtable->nometa.exp_data_len = 0;

	st_strtable->meta.total_entries = 0;
	st_strtable->meta.exp_tab = NULL;
	st_strtable->meta.exp_data = NULL;
	st_strtable->meta.exp_data_len = 0;
	st_strtable->meta.largest_len = 0;

	return (st_strtable);
}

void st_destroy (t_st_strtable *st_strtable)
{
	if (st_strtable->nometa.total_entries != 0) {
		free (st_strtable->nometa.hashes_tab);
		free (st_strtable->nometa.exp_tab);
		free (st_strtable->nometa.exp_data);
	}
	
	if (st_strtable->meta.total_entries != 0) {
		free (st_strtable->meta.exp_tab);
		free (st_strtable->meta.exp_data);
	}

	free (st_strtable);
}

void st_insert_base (t_st_strtable *st_strtable, const char *strexp, int force_nometa)
{
	int has_meta;	/* !=0 if meta */
	int strexp_len;
	int prev_exp_data_len;
	int i, pos;
	
	int *total_entries;
	int **hashes_tab;
	char ***exp_tab;
	char **exp_data;
	int *exp_data_len;
	int *largest_len;

	if (force_nometa == 0) {
		if (strchr (strexp, '*') == NULL)
			has_meta = 0;
		else
			has_meta = 1;
	} else {
		has_meta = 0;
	}
	strexp_len = strlen (strexp);
	
	if (has_meta == 0) {
		/* nometa */
		total_entries = &(st_strtable->nometa.total_entries);
		hashes_tab = &(st_strtable->nometa.hashes_tab);
		exp_tab = &(st_strtable->nometa.exp_tab);
		exp_data = &(st_strtable->nometa.exp_data);
		exp_data_len = &(st_strtable->nometa.exp_data_len);
	} else {
		/* meta */
		total_entries = &(st_strtable->meta.total_entries);
		exp_tab = &(st_strtable->meta.exp_tab);
		exp_data = &(st_strtable->meta.exp_data);
		exp_data_len = &(st_strtable->meta.exp_data_len);
		largest_len = &(st_strtable->meta.largest_len);
	}
	
	prev_exp_data_len = *exp_data_len;
	*total_entries += 1;
	*exp_data_len += strexp_len + 1;
	*exp_tab = realloc (*exp_tab, sizeof (char *) * *total_entries);
	*exp_data = realloc (*exp_data, sizeof (char) * *exp_data_len);
	if (has_meta == 0)
		*hashes_tab = realloc (*hashes_tab, sizeof (int *) * *total_entries);
	
	/* generate hash */
	if (has_meta == 0)
		(*hashes_tab)[*total_entries - 1] = string_fingerprint (strexp);

	/* update largest_len */
	if (has_meta != 0) {
		if (strexp_len > *largest_len)
			*largest_len = strexp_len;
	}
	
	strcpy (*exp_data + prev_exp_data_len, strexp);

	/* realloc'ed buffer, regenerate exp_tab */
	pos = 0;
	for (i = 0; i < *total_entries; i++) {
		(*exp_tab)[i] = *exp_data + pos;
		pos += strlen (*exp_data + pos) + 1;
	}
}

/* insert strexp into the table */
void st_insert (t_st_strtable *st_strtable, const char *strexp)
{
	st_insert_base (st_strtable, strexp, 0);
}

/* insert strexp into the table, except that '*' are not considered meta-characters
 * for this reason, only exact matches may happen */
void st_insert_nometa (t_st_strtable *st_strtable, const char *strexp)
{
	st_insert_base (st_strtable, strexp, 1);
}

// for testing purposes only
/*
void st_dump_strings (t_st_strtable *st_strtable)
{
	int i;

	printf ("\ntotal nometa: %d\n", st_strtable->nometa.total_entries);
	for (i = 0; i < st_strtable->nometa.total_entries; i++) {
		printf ("[%s] [%x]\n", st_strtable->nometa.exp_tab[i], st_strtable->nometa.hashes_tab[i]);
	}

	printf ("\ntotal meta: %d\n", st_strtable->meta.total_entries);
	for (i = 0; i < st_strtable->meta.total_entries; i++) {
		printf ("[%s]\n", st_strtable->meta.exp_tab[i]);
	}
}
*/

/* returns != 0 if present in nometa array, == 0 if absent */
int st_check_if_matches_nometa (const t_st_strtable *st_strtable, const char *strdata)
{
	int strdata_hash;
	int pos;
	int total_entries;

	strdata_hash = string_fingerprint (strdata);

	total_entries = st_strtable->nometa.total_entries;
	pos = 0;
	while (pos < total_entries) {
		if (st_strtable->nometa.hashes_tab[pos] == strdata_hash) {
			if (strcmp (strdata, st_strtable->nometa.exp_tab[pos]) == 0)
				return (1);
		}
		pos++;
	}

	return (0);
}

/* returns != 0 if present in meta array, == 0 if absent */
/* '*' followed just after another '*' is not tolerated */
/* TODO: optimize this, add faster code when meta string either starts or ends with a '*' */
int st_check_if_matches_meta (const t_st_strtable *st_strtable, const char *strdata)
{
	int pos;
	int total_entries;
	const char *query_str;
	char *meta_str;
	char tmp [st_strtable->meta.largest_len + 3]; // max len + trailing '\0' + 2 stopping '\0'
	int meta_str_len;
	int i;
	int both_match;
	int first_asterisk;	// !=0 if first char in meta_str is a '*'

	total_entries = st_strtable->meta.total_entries;
	pos = 0;
	while (pos < total_entries) {
		both_match = 1;
		
		strcpy (tmp, st_strtable->meta.exp_tab[pos]);
		meta_str_len = strlen (tmp);

		/* ends with a triple '\0'
		   we will detect only double '\0', this is a kludge to avoid problems with meta_str starting with '*'
		   in such case the triple '\0' may turn into a double '\0' */
		tmp [meta_str_len + 1] = '\0';
		tmp [meta_str_len + 2] = '\0';
		
		/* segmentate meta string in particles, separated by '\0' */
		meta_str = tmp;
		for (i = 0; i < meta_str_len; i++, meta_str++) {
			if (*meta_str == '*')
				*meta_str = '\0';
			else if (*meta_str == '\0')
				break;
		}

		 if (*(st_strtable->meta.exp_tab[pos]) == '*')
			 first_asterisk = 1;
		 else
			 first_asterisk = 0;
		
		/* compares each particle if matches sequentially */
		meta_str = tmp;
		if (first_asterisk != 0) // if first char == '*' we skip one char to avoid double '\0' and ending prematurely
			meta_str++;
		query_str = strdata;
		while (*meta_str != '\0') {
			query_str = strstr (query_str, meta_str);
			if (query_str == NULL) {
				both_match = 0; // does not match
				break;
			}

			/* jump to next particle */
			while (*meta_str != '\0') {
				meta_str++;
				query_str++;
			}
			meta_str++;
		}

		if (both_match == 0) {
			// does not match
			pos++;
			continue;
		}
		
		/* passed for now, need more tests */

		/* if last char has no '*' */
		if ((*query_str != '\0') && (meta_str_len > 0)) {
			if (*(st_strtable->meta.exp_tab[pos] + meta_str_len - 1) != '*') {
				// does not match
				pos++;
				continue;
			}
		}

		/* if first char has no '*' */
		meta_str = tmp;
		if (first_asterisk == 0) {
			if (strncmp (meta_str, strdata, strlen (meta_str) != 0)) {
				// does not match
				pos++;
				continue;
			}
		}

		// if it reached this point, the strings match
		return (1);
	}

	return (0);
}

/* returns != 0 if strdata matches any entry in the table,
 * otherwise returns == 0 */
int st_check_if_matches (const t_st_strtable *st_strtable, const char *strdata)
{
	if (st_check_if_matches_nometa (st_strtable, strdata) != 0)
		return (1);
	
	return(st_check_if_matches_meta (st_strtable, strdata));
}


