/* strtables.h
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

//To stop multiple inclusions.
#ifndef SRC_STRTABLES_H
#define SRC_STRTABLES_H

// private
/* for strings without meta-characters '*' */
typedef struct {
	int total_entries;
	int *hashes_tab;	// contains the pseudo-hashes. when a expression contain a '*', this data is meaningless
	char **exp_tab;	// array of pointers to expressions
	char *exp_data;	// the expressions themselves
	int exp_data_len; // in chars
} t_st_strtable_nometa;

// private
/* for strings with meta-characters '*' */
typedef struct {
	int total_entries;
	char **exp_tab;	// array of pointers to expressions
	char *exp_data;	// the expressions themselves
	int exp_data_len; // in chars
	int largest_len;  // the total of chars of the largest entry
} t_st_strtable_meta;

// private
typedef struct {
	t_st_strtable_nometa nometa;
	t_st_strtable_meta meta;
} t_st_strtable;


extern t_st_strtable *st_create (void);
extern void st_destroy (t_st_strtable *st_strtable);
extern void st_insert (t_st_strtable *st_strtable, const char *strexp);
extern void st_insert_nometa (t_st_strtable *st_strtable, const char *strexp);
extern int st_check_if_matches_nometa (const t_st_strtable *st_strtable, const char *strdata);
extern int st_check_if_matches_meta (const t_st_strtable *st_strtable, const char *strdata);
extern int st_check_if_matches (const t_st_strtable *st_strtable, const char *strdata);

// for testing purposes only
/*
extern void st_dump_strings (t_st_strtable *st_strtable);
*/

#endif

