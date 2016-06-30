/* txtfiletools.c
 * Misc tools when dealing with text files.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* converts CRLF and CR to that standard and closes dst with '\0' 
 * it also replaces '\0' in the middle of the text with spaces
 * return: size of resulting text (may be <= srclen because of CR suppression, for example) */
/* *dst may be the same pointer as *src, but the buffer must be at least srclen+1 */
int fix_linebreaks_qp (const unsigned char *src, int srclen, unsigned char *dst)
{
	const unsigned char	*rpos = src;
	unsigned char		*wpos = dst;
	unsigned char		prevchar = '\0';
	unsigned char		curchar;
	int	prevchar_was_cr_or_lf = 0;
	int	curchar_is_cr_or_lf;
	int	count = srclen;
	int	dstlen = 0;

	while (count--) {
		curchar = *(rpos++);
		curchar_is_cr_or_lf = 0;
		if ((curchar == '\n') || (curchar == '\r')) {
			curchar_is_cr_or_lf = 1;
			if ((!prevchar_was_cr_or_lf) || ((prevchar_was_cr_or_lf) && (prevchar == curchar))) {
				*(wpos++) = '\n';
				dstlen++;
			}
		} else if (curchar != '\0') {
			*(wpos++) = curchar;
			dstlen++;
		} else {
			*(wpos++) = ' ';
			dstlen++;
		}
		prevchar_was_cr_or_lf = curchar_is_cr_or_lf;
		prevchar = curchar;
	}
	*wpos = '\0';

	return (dstlen);
}

/* returns null if unsuccessful
 * pointed buffer must be freed after usage
 * returns file size into *filesize, if filesize != NULL
 * the returned data consists of the file data itself, with an
 * appended '\0' to it (thus, it's a valid C string) */
/* FIXME: this routine does not expect IO errors */
char *load_textfile_to_memory (const char *given_filename)
{
	int my_filesize;
	char *my_buff;
	FILE *my_file;

	if ((my_file = fopen (given_filename, "rb")) != NULL) {
		/* determine file size */
		fseek (my_file, 0, SEEK_END);
		my_filesize = ftell (my_file);
		fseek (my_file, 0, SEEK_SET);

		if ((my_buff = malloc (my_filesize + 1)) != NULL) {
			fread (my_buff, my_filesize, 1, my_file);
			fclose (my_file);
			*(my_buff + my_filesize) = '\0';
			return (my_buff);
		} else {
			fclose (my_file);
		}
	}

	return(NULL);
}

/* equivalent to strlen, but instead of stopping at '\0', stops at <which_char> (or '\0') */
/* returns line len (including the trailing <which_char>) */
int get_line_len_char (const char *src, const char which_char)
{
	int linelen = 0;

	while ((*src != '\0') && (*src != which_char)) {
		src++;
		linelen++;
	}
	if (*src == which_char)
		linelen++;

	return (linelen);
}



/* equivalent to strlen, but instead of stopping at '\0', stops at '\n' (or '\0') */
/* returns line len (including the trailing '\n') */
/* TODO: reimplement reusing get_line_len_char() */
int get_line_len (const char *src)
{
	int linelen = 0;

	while ((*src != '\0') && (*src != '\n')) {
		src++;
		linelen++;
	}
	if (*src == '\n')
		linelen++;

	return (linelen);
}

/* remove comments and empty lines, leave only useful config data */
/* 'src' may be the same as 'dst' */
void remove_junk_data (const char *src, char *dst)
{
	int linelen, counter;
	int emptyline;
	const char *readpos, *readpos2;
	char *writepos, *writepos2;

	/* remove comments */
	readpos = src;
	writepos = dst;
	while ((linelen = get_line_len (readpos)) != 0) {
		counter = linelen;
		while (counter) {
			if (*readpos == '#') {
				/* ignore the rest of the line */
				if (*(readpos + counter - 1) == '\n')
					*(writepos++) = '\n';
				readpos += counter;
				counter = 0;
			} else {
				*(writepos++) = *(readpos++);
				counter--;
			}
		}
	}
	*writepos = '\0';
	
	/* remove empty lines (just spaces/tabs/etc or simply empty) */
	readpos = dst;
	writepos = dst;
	while ((linelen = get_line_len (readpos)) != 0) {
		counter = linelen;
		emptyline = 1;
		readpos2 = readpos;
		writepos2 = writepos;
		while (counter--) {
			if ((*readpos2 > ' ') && (*readpos2 != '\n'))
				emptyline = 0;
			readpos2++;
		}

		if (emptyline == 0) {
			counter = linelen;
			while (counter--)
				*(writepos++) = *(readpos++);
		} else {
			readpos += linelen;
		}
	}
	*writepos = '\0';

	/* remove unneccessary spaces */
	readpos = dst;
	writepos = dst;
	while ((linelen = get_line_len (readpos)) != 0) {
		counter = linelen;
		while (counter) {
			if ((*readpos <= ' ') && (*readpos != '\n')) {
				readpos++;
			} else if (*readpos == '\\') {
				*(writepos++) = *(readpos++);
				if (counter > 1) {
					*(writepos++) = *(readpos++);
					counter--;
				}
			} else {
				*(writepos++) = *(readpos++);
			}
			counter--;
		}
	}
	*writepos = '\0';
}


/* check whether file exists
   returns: !=0 exists, ==0 does not */
int file_exists (const char *given_filename)
{
	return (! access (given_filename, F_OK));
}

/* dumps a '\0'-ended string to a file (overwrite previous file contents)
   returns: ==0 OK, !=0 error */
int dump_string_to_textfile (const char *given_filename, const char *given_string)
{
	FILE *my_file;

	if (my_file = fopen (given_filename, "w")) {
		fputs (given_string, my_file);
		fclose (my_file);
		return (0);
	}
	return (1);
}

