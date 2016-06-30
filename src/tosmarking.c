/* tosmarking.c
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "globaldefs.h"

#include "urltables.h"
#include "cttables.h"
#include "log.h"

/* private, local. those are not the same as the vars with the same name */
int tosmarking_enabled;
SOCKET sock_child_out;
int TOSFlagsDefault;
int TOSFlagsDiff;
const t_ut_urltable *tos_markasdiff_url;
const t_ct_cttable *tos_maskasdiff_ct;
ZP_DATASIZE_TYPE TOSMarkAsDiffSizeBT;

int current_tos;
ZP_DATASIZE_TYPE tos_bytecount;	/* counter used by TOSMarkAsDiffSizeBT */

void tosmarking_change_tos (int new_tos);


void tosmarking_change_tos (int new_tos)
{
	setsockopt (sock_child_out, IPPROTO_IP, IP_TOS, (const void *) &new_tos, sizeof (new_tos));
}

/* this is the first function to be invoked before calling any TOS-related other */
/* returns: 0 - nothing done, != 0 changed TOS (because default TOS is specified) */
int tosmarking_init (const int in_tosmarking_enabled, const SOCKET in_sock_child_out, const int in_TOSFlagsDefault, const int in_TOSFlagsDiff, const t_ut_urltable *in_tos_markasdiff_url, const t_ct_cttable *in_tos_maskasdiff_ct, const int in_TOSMarkAsDiffSizeBT)
{
	tos_bytecount = 0;
	current_tos = -1; /* OS default, unknown */

	tosmarking_enabled = in_tosmarking_enabled;

	if (tosmarking_enabled) {
		sock_child_out = in_sock_child_out;
		TOSFlagsDefault = in_TOSFlagsDefault;
		TOSFlagsDiff = in_TOSFlagsDiff;

		tos_markasdiff_url = in_tos_markasdiff_url;
		tos_maskasdiff_ct = in_tos_maskasdiff_ct;
		TOSMarkAsDiffSizeBT = in_TOSMarkAsDiffSizeBT;

		if (TOSFlagsDefault >= 0) {
			tosmarking_change_tos (TOSFlagsDefault);
			current_tos = TOSFlagsDefault;
			return (1);
		}
	}
	return (0);
}

/* sets byte counter to 0 */
void tosmarking_reset_bytecount ()
{
	tos_bytecount = 0;
}

/* check if matches and marks accordingly */
/* to be invoked each time bytes are transferred to client when total file fize is unknown,
   or called just once when total file size is already known. */
/* returns: 0 - nothing done, != 0 changed TOS */
int tosmarking_add_check_bytecount (const ZP_DATASIZE_TYPE in_bytes)
{
	tos_bytecount += in_bytes;

	if ((tosmarking_enabled) && \
	   (current_tos != TOSFlagsDiff) && \
	   (TOSMarkAsDiffSizeBT >= 0) && \
	   (tos_bytecount > TOSMarkAsDiffSizeBT)) {
		tosmarking_change_tos (TOSFlagsDiff);
		current_tos = TOSFlagsDiff;
		debug_log_printf ("TOS: Bytecount reached at %"ZP_DATASIZE_STR" bytes. Changing TOS to 0x%x.\n", tos_bytecount, TOSFlagsDiff);
		access_log_set_flags (LOG_AC_FLAG_TOS_CHANGED);
		return (1);
	}
	return (0);
}

/* check if matches and marks accordingly */
/* to be called just once */
/* returns: 0 - nothing done, != 0 changed TOS */
int tosmarking_check_content_type (const char *in_content_type)
{
	if ((tosmarking_enabled) && \
	   (TOSFlagsDiff >= 0) && \
	   (current_tos != TOSFlagsDiff) && \
	   (tos_maskasdiff_ct != NULL)) {
		if (ct_check_if_matches (tos_maskasdiff_ct, in_content_type)) {
			tosmarking_change_tos (TOSFlagsDiff);
			current_tos = TOSFlagsDiff;
			debug_log_printf ("TOS: Content-Type matches. Changing TOS to 0x%x.\n", TOSFlagsDiff);
			access_log_set_flags (LOG_AC_FLAG_TOS_CHANGED);
			return (1);
		}
	}
	return (0);
}

/* check if matches and marks accordingly */
/* to be called just once */
/* returns: 0 - nothing done, != 0 changed TOS */
int tosmarking_check_url (const char *in_host, const char *in_path)
{
	if ((tosmarking_enabled) && \
	   (TOSFlagsDiff >= 0) && \
	   (current_tos != TOSFlagsDiff) && \
	   (tos_markasdiff_url != NULL)) {
		if (ut_check_if_matches (tos_markasdiff_url, in_host, in_path)) {
			tosmarking_change_tos (TOSFlagsDiff);
			current_tos = TOSFlagsDiff;
			debug_log_printf ("TOS: URL matches. Changing TOS to 0x%x.\n", TOSFlagsDiff);
			access_log_set_flags (LOG_AC_FLAG_TOS_CHANGED);
			return (1);
		}
	}
	return (0);
}

