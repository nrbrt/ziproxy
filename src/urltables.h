/* urltables.h
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

//To stop multiple inclusions.
#ifndef SRC_URLTABLES_H
#define SRC_URLTABLES_H

#include "strtables.h"

// public
typedef struct {
	t_st_strtable *host;    // private
	t_st_strtable *url;	// private
} t_ut_urltable;

extern t_ut_urltable *ut_create (void);
extern void ut_destroy (t_ut_urltable *ut_urltable);
extern void ut_insert (t_ut_urltable *ut_urltable, const char *host, const char *path);
extern int ut_check_if_matches (const t_ut_urltable *ut_urltable, const char *host, const char *path);
extern t_ut_urltable *ut_create_populate_from_file (const char *filename);

#endif

