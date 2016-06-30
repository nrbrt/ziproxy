/* auth.h
 * HTTP proxy authentication routines.
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

#ifndef SRC_AUTH_H
#define SRC_AUTH_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "strtables.h"

/* authentication modes */
#define AUTH_NONE 0
#define AUTH_PASSWD_FILE 1
#define AUTH_SASL 2
/* min and maximum integer values.
   update this when new AuthModes
   are created */
#define AUTH_MIN 0
#define AUTH_MAX 2

void auth_set_mode (int auth_mode);
int auth_passwdfile_init (const char *passwd_file);
int auth_basic_check (const char *userpass);
char *auth_get_username (const char *userpass);

#ifdef SASL
int auth_sasl_init (const char *sasl_conf_path);
int auth_sasl_end (void);
#endif

#endif
