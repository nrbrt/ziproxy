/* tosmarking.h
 * Dynamic TOS management.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2009-2014 Daniel Mealha Cabrita
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

// To stop multiple inclusions.
#ifndef SRC_TOSMARKING_H
#define SRC_TOSMARKING_H

#include "globaldefs.h"

#include "urltables.h"
#include "cttables.h"

extern int tosmarking_init (const int in_tosmarking_enabled, const SOCKET in_sock_child_out, const int in_TOSFlagsDefault, const int in_TOSFlagsDiff, const t_ut_urltable *in_tos_markasdiff_url, const t_ct_cttable *in_tos_maskasdiff_ct, const int in_TOSMarkAsDiffSizeBT);

extern void tosmarking_reset_bytecount ();
extern int tosmarking_add_check_bytecount (const ZP_DATASIZE_TYPE in_bytes);
extern int tosmarking_check_content_type (const char *in_content_type);
extern int tosmarking_check_url (const char *in_host, const char *in_path);

#endif //SRC_TOSMARKING_H

