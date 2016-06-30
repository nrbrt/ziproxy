/* htmlopt.h
 * HTML/JS/CSS optimization routines
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2007-2014 Daniel Mealha Cabrita
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

#ifndef HTMLOPT_H


#define HOPT_FLAGS      int

/* acceptable flags for HOPT_FLAGS */
#define HOPT_NONE       0
#define HOPT_HTMLTEXT   1 << 0
#define HOPT_HTMLTAGS   1 << 1
#define HOPT_JAVASCRIPT 1 << 2
#define HOPT_CSS        1 << 3
#define HOPT_NOCOMMENTS 1 << 4
#define HOPT_PRE        1 << 5
#define HOPT_TEXTAREA	1 << 6
#define HOPT_ALL        0xffff

int hopt_pack_css (const unsigned char *src, int srclen, unsigned char *dst);
int hopt_pack_javascript (const unsigned char *src, int srclen, unsigned char *dst);
int hopt_pack_html (const unsigned char *src, int srclen, unsigned char *dst, HOPT_FLAGS flags);


#define HTMLOPT_H
#endif

