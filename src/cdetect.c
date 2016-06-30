/* cdetect.c
 * content detection routines
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


#include <stdio.h>
#include <string.h>
#include "fstring.h"
#include "cdetect.h"

/*
 * checks wheter a tag is the same as the compared one
 * src points to the character just after the '<' (opening html tag)
 * returns: =0 equal, != not equal
 */
int compare_tag_cd (const unsigned char *src, const unsigned char *tag, int taglen)
{
	const unsigned char	*rpos = src;

	if (*rpos == '\0')
		return (1);
	
	if (WHICH_STRNCASECMP (rpos, tag, taglen) == 0) {
		switch (*(rpos + taglen)) {
		case ' ':
		case '>':
		case '/':
		case '\n':
			return (0);
		default:
			return (1);
		}		
	}
	return (1);
}

/* 
 * returns src length until breakpoint or '\0', (including breakpoint itself, if exists)
 * returns the size of it, or 0 if *src='\0'
 */
int return_chars_until_chr_cd (const unsigned char *src, const unsigned char breakpoint)
{
	const unsigned char	*rpos = src;
	int	charslen = 0;

	while (*rpos != '\0') {
		charslen++;
		if (*(rpos++) == breakpoint)
			return (charslen);
	}

	return (charslen);
}

/* conservative attempt to determine whether the text is HTML data */
int is_that_text_html (const char *content)
{
	const char *rpos = content;
	char xchar;
	int ends_now;

	while (*rpos != '\0') {
		switch (*rpos) {
		case '<':
			rpos++;
			if (compare_tag_cd (rpos, "HTML", 4) == 0)
				return (1);
			rpos += return_chars_until_chr_cd (rpos, '>');
			break;
		case '\'':
		case '"':
		case '(':
		case '[':
		case '{':
			xchar = *rpos;
			rpos++;
			rpos += return_chars_until_chr_cd (rpos, xchar);
			break;
		case '/':
			if (*(rpos + 1) == '/') {
				/* slash-slash quote, skip to CR or LF */
				while ((*rpos != '\0') && (*rpos != '\n') && (*rpos != '\r'))
					rpos++;
			} else if (*(rpos + 1) == '*') {
				/* slash-asterisk quote, skip to asterisk-slash */
				ends_now = 0;
				while ((*rpos != '\0') && (ends_now == 0)) {
					if ((*rpos == '*') && (*(rpos + 1) == '/')) {
						ends_now = 1;
						rpos++;
					}
					rpos++;
				}
			} else {
				rpos++;
			}
			break;
		case '\\':
			if (*(rpos + 1) != '\0')
				rpos += 2;
			else
				rpos++;
			break;
		default:
			rpos++;
			break;
		}
	}

	return (0);
}

enum t_cd_content_type detect_content_type (const char *content)
{
	if (is_that_text_html (content) != 0)
		return (CD_TEXT_HTML);
	return (CD_UNKNOWN);
}

