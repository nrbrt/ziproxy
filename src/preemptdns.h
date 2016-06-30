/* preemptdns.h
 * Preemptive name resolution feature.
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


//#include <stdio.h>
//#include <time.h>
//#include <sys/time.h>

//If this is included in preemptdns.c, make declarations local.
#ifdef SRC_PREEMPTDNS_C
#define EXTERN
#else
#define EXTERN extern
#endif

//To stop multiple inclusions.
#ifndef SRC_PREEMPTDNS_H
#define SRC_PREEMPTDNS_H
#endif

EXTERN void preempt_dns_from_html (char* inbuf, int inlen);
