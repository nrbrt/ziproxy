/* cttables.h
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

//To stop multiple inclusions.
#ifndef SRC_CTTABLES_H
#define SRC_CTTABLES_H

#include "strtables.h"

// public
typedef struct {
	t_st_strtable *cttype;    	// private
	t_st_strtable *ctcontenttype;	// private
	int subtype_add_x_prefix;	// private
} t_ct_cttable;

extern t_ct_cttable *ct_create (int subtype_add_x_prefix);
extern void ct_destroy (t_ct_cttable *ct_cttable);
extern void ct_insert (t_ct_cttable *ct_cttable, const char *ctcontenttype);
extern int ct_check_if_matches (const t_ct_cttable *ct_cttable, const char *ctcontenttype);

#endif

