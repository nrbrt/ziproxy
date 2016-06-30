/* htmlopt.c
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

#include <stdio.h>
#include <string.h>
#include "htmlopt.h"
#include "fstring.h"

enum chunk_type {CT_HTML_TAG_OTHER,
		CT_HTML_TEXT,
		CT_HTML_PRE_TEXT,
		CT_HTML_TEXTAREA,
	       	CT_JAVASCRIPT,
		CT_COMMENT,
	       	CT_STYLE,
	       	CT_COMMENT_EXTENSION,
	       	CT_EXCLAM_TYPES,
		CT_CDATA};

/* chunk flags */
// #define CF_LAST_IN_LINE	1 << 0 /* UNUSED */

typedef struct {
	enum chunk_type	c_type;
} chunk_info;


/* ### DEBUG ROUTINES */

#ifdef DEBUG

void debug_dump_chunk_data (const char *chunkdata, int chunksize, const char *label)
{
	fprintf (stderr, "\n\n*** CHUNK - %s (len: %d):\n", label, chunksize);
	fwrite (chunkdata, chunksize, 1, stderr);
}

#endif

/* END OF ### DEBUG ROUTINES */

/* similar to strcpy(), except is allows overlapping strings */
/* NOTE: this is not truly overlapping, only if src >= dst */
void strcpy_overlapping (const unsigned char *src, char *dst)
{
	while (*src != '\0')
		*(dst++) = *(src++);
	*dst = '\0';
}

/* similar to strcpy(), except is allows overlapping strings */
/* NOTE: this is not truly overlapping, only if src >= dst */
void strncpy_overlapping (const unsigned char *src, char *dst, int srclen)
{
	while (srclen--)
		*(dst++) = *(src++);
}


/* ### STYLE OPTIMIZATION ROUTINES */

/* returns:
 *   0 = non_concatenable (a-z, A-Z, 0-9, '_', '.', '#', '%')
 *  !0 = ok,concatenable (other) */
int style_concatenable (const unsigned char *given_char)
{
	int	concatenable = 0;
	
	switch (*given_char) {
	case ':':
	case ';':
	case ',':
//	case '.':
//	case '#':
	case '{':
	case '}':
		concatenable = 1;
	}

	return (concatenable);
}

/* src: source style text
 * dst: destination of compressed text
 * srclen: size of src text
 * returns: chars outputted into dst */
int compress_style_chunk (const unsigned char *src, int srclen, unsigned char *dst)
{
	const unsigned char	*rpos = src;
	unsigned char		*wpos = dst;
	int		count;
	int		dstlen = 0;
	unsigned char	previous_char = '\0';
	unsigned char	previous_char2 = '\0'; /* 1 char before previous_char */
	unsigned char	quote_char;
	int		break_loop;

	count = srclen;
	while (count) {
		if (*rpos <= ' ') { /* space */
			if (previous_char != ' ') {
				*(wpos++) = ' ';
				dstlen++;
			}
			rpos++;
			count--;
		} else if (*rpos == '/') { /* perhaps "[double slash]" or "[slash]*" ? */
			if (count >= 2) {
				if (*(rpos + 1) == '*') {
					/* skip comment */
					rpos += 2;
					count -= 2;

					while ((count >= 2) && ((*rpos != '*') || (*(rpos + 1) != '/'))) {
						rpos++;
						count--;
					}

					/* skip remaining trailing *[slash], if existant */
					if (count) {
						rpos++;
						count--;
					}
					if (count) {
						rpos++;
						count--;
					}			
				} else { /* "/" followed by something else, write that */
					*(wpos++) = *(rpos++);
					dstlen++;
					count--;
				}
			} else { /* "/" followed by something else, write that */
				*(wpos++) = *(rpos++);
				dstlen++;
				count--;
			}		
		} else if ((*rpos == '"') || (*rpos == '\'')) { /* quoted text, skip escaped quotes */
			if (previous_char == ' ') {	/* no need for a space preceding quoted text, remove that */
				wpos--;
				dstlen--;
			}
			quote_char = *(rpos++);
			*(wpos++) = quote_char;
			dstlen++;
			count--;
			break_loop = 0;
			while ((count) && (! break_loop)) {
				if (*rpos == '\\') {	/* escape char preceded by '\' (avoids closing quote when "\"" appears) */
					*(wpos++) = *(rpos++);
					if (count > 1) {
						*(wpos++) = *(rpos++);
						dstlen++;
						count--;
					}
				} else if (*rpos == quote_char) {
					*(wpos++) = *(rpos++);
					break_loop = 1;
				} else {
					*(wpos++) = *(rpos++);
				}
				dstlen++;
				count--;
			}
		} else { /* other char (a-z 0-9 etc) */
			if (previous_char == ' ') {
				if (style_concatenable (&previous_char2) || style_concatenable (rpos)) {
					previous_char = previous_char2;
					previous_char2 = '\0';
					wpos--;
					dstlen--;
				}
			}
			*(wpos++) = *(rpos++);
			dstlen++;
			count--;
		}	

		if (dstlen)
			previous_char = *(wpos - 1);
		else
			previous_char = '\0';
		if (dstlen >= 2)
			previous_char2 = *(wpos - 2);
		else
			previous_char2 = '\0';
	}

	return (dstlen);
}

/* END OF ### STYLE OPTIMIZATION ROUTINES */


/* ### JAVASCRIPT OPTIMIZATION ROUTINES */

/* locate substr occurence in str which is a // (javascript) comment
 * will search until '\n' or end of str (srclen), whichever reaches first
 * srclen is the length of str string, substrlen (...) of substr string
 * returns !=0 found at least once, ==0 not found */
int is_str_present_in_ss_comment (const unsigned char *src, int srclen, const unsigned char *substr, int substrlen)
{
	srclen = srclen - substrlen + 1;
	while (srclen--) {
		if (*src == '\n')
			return (0);
		if (! WHICH_STRNCMP (src++, substr, substrlen))
			return (1);
	}
	return (0);
}

/* returns:
 *   0 = non_concatenable (a-z, A-Z, 0-9, '_', '.', '#', '%')
 *  !0 = ok,concatenable (other) */
int javascript_concatenable (const unsigned char *given_char)
{
	int	concatenable = 0;
	
	switch (*given_char) {
	case '=':
	case '+':
	case '-':
	case '*':
	case '/':
	case '(':
	case ')':
	case '>':
	case '<':
	case ';':
	case ':':
	case ',':
	case '[':
	case ']':
	case '{':
	case '}':
	case '"':
		concatenable = 1;
	}

	return (concatenable);
}

/* compress javascript code (may contain "<!--" and "-->" tags) */
/* returns the size of data dumped into dst */
int compress_javascript_chunk (const unsigned char *src, int srclen, unsigned char *dst)
{
	const unsigned char	*rpos = src;
	unsigned char		*wpos = dst;
	int		count;
	int		dstlen = 0;
	unsigned char	previous_char = '\0';
	unsigned char	previous_char2 = '\0'; /* 1 char before previous_char */
	unsigned char	close_quote_char;
	int		break_loop;

	count = srclen;
	while (count) {
		if (*rpos == '\n') {
		       	/* we cannot convert all '\n' to spaces unless we fully parse the "if", contexts etc
			 * whoever designed javascript made it to allow commands not finishing with ';' to be legal,
			 * when those are finished with '\n' */
			
			if (! ((*(rpos + 1) == '-') && (count >= 4))) {
				if (previous_char == ' ') {	/* no use for space followed by '\n', remove that */
					wpos--;
					dstlen--;
					previous_char = previous_char2;
					previous_char2 = '\0';
				}
				if ((previous_char != '\0') && (previous_char != '\n') && (previous_char != ';') && (previous_char != ':') && (previous_char != '{') && (previous_char != '}')) {
					*(wpos++) = '\n';
					dstlen++;
				}
				rpos++;
				count--;
			} else {
				if (! WHICH_STRNCMP (rpos, "\n-->", 4)) {
					/* we have a "\n-->" we must write exactly that */
					strncpy_overlapping (rpos, wpos, 4);
					rpos += 4;
					wpos += 4;
					count -= 4;
					dstlen += 4;					
				} else {
					/* false alarm, process as a normal '\n' */
					if (previous_char == ' ') {	/* no use for space followed by '\n', remove that */
						wpos--;
						dstlen--;
						previous_char = previous_char2;
						previous_char2 = '\0';
					}
					if ((previous_char != '\0') && (previous_char != '\n') && (previous_char != ';') && (previous_char != ':') && (previous_char != '{') && (previous_char != '}')) {
						*(wpos++) = '\n';
						dstlen++;
					}
					rpos++;
					count--;				
				}
			}
		} else if ((*rpos == '{') || (*rpos == '}')) {
			if ((previous_char == ' ') || (previous_char == '\n')) {
				wpos--;
				dstlen--;
			}
			dstlen++;
			*(wpos++) = *(rpos++);
			count--;
		} else if (*rpos <= ' ') { /* space */
			if ((previous_char != ' ') && (previous_char != '\n')) {
				*(wpos++) = ' ';
				dstlen++;
			}
			rpos++;
			count--;
		} else if (*rpos == '\\') { /* non-quoted escaped chars */
			*(wpos++) = *(rpos++);
			count--;
			dstlen++;
			if (count) {
				/* FIXME: ***********************************
				 * escaped spaces cannot be processed as normal spaces
				 * replace that with an unused char then, at the end of all, turn those chars back to spaces
				 * WHILE skipping quotes 
				 * *************************************************** */
				if (*rpos == ' ')
					/* DIRTY HACK: we're replacing with \t in order to avoid it being suppressed by the optimizer
					 * it will convert a char into another, but at least the javascript will not break while parsing
					 * (in worst case JS code may break if it expects a space) */
					*(wpos++) = '\t';
				else
					*(wpos++) = *(rpos++);
				count--;
				dstlen++;
			}
		} else if (*rpos == '/') { /* perhaps "[double slash]" or "[slash]*" ? */
			if (count >= 2) {
				if (*(rpos + 1) == '(') {
					/* escape sequences of /(xxx) */
					/* FIXME: should i escape backslashes inside /(...) ?
					 * this routine does NOT do that */
					*(wpos++) = *(rpos++);
					*(wpos++) = *(rpos++);
					count -= 2;
					dstlen += 2;
					
					while ((count) && (*rpos != ')')) {
						*(wpos++) = *(rpos++);
						count--;
						dstlen++;
					}

					*(wpos++) = *(rpos++); /* remaining ')' */
					count--;
					dstlen++;
				} else if (*(rpos + 1) == '*') {
					/* skip comment */
					rpos += 2;
					count -= 2;

					while ((count >= 2) && ((*rpos != '*') || (*(rpos + 1) != '/'))) {
						rpos++;
						count--;
					}

					/* skip remaining trailing *[slash], if existant */
					if (count) {
						rpos++;
						count--;
					}
					if (count) {
						rpos++;
						count--;
					}
				} else if (*(rpos + 1) == '/'){
					int done_proc_comment = 0;

					if (previous_char == ' ') {	/* no use for space followed by comment, remove that */
						wpos--;
						dstlen--;
						previous_char = previous_char2;
						previous_char2 = '\0';
					}
					
					if (count >= 11) {
						if (is_str_present_in_ss_comment (rpos, count, "<![CDATA[", 9)) {
							/* generate a comment containing only the //<![CDATA[ */
							strncpy_overlapping ("//<![CDATA[\n", wpos, 12);
							wpos += 12;
							dstlen += 12;
							done_proc_comment = 1;
						}
					}
					if ((done_proc_comment == 0) && (count >= 5)) {
						if (is_str_present_in_ss_comment (rpos, count, "-->", 3)) {
							/* generate a comment containing only the HTML closing-comment tag */
							strncpy_overlapping ("//-->", wpos, 5);
							wpos += 5;
							dstlen += 5;
						} else if (is_str_present_in_ss_comment (rpos, count, "]]>", 3)) {
							/* generate a comment containing only the //]]> */
							strncpy_overlapping ("//]]>\n", wpos, 6);
							wpos += 6;
							dstlen += 6;
						}
					}
					/* discard the rest of the comment */
					while (count && (*rpos != '\n')) {
						rpos++;
						count--;
					}						
				} else { /* "/" followed by something else, write that */
					*(wpos++) = *(rpos++);
					dstlen++;
					count--;
				}
			} else { /* "/" followed by something else, write that */
				*(wpos++) = *(rpos++);
				dstlen++;
				count--;
			}
		} else if ((*rpos == '"') || (*rpos == '\'') || (*rpos == '[')) { /* quoted text, leave content unmodified */
			if (previous_char == ' ') {	/* no need for a space preceding quoted text, remove that */
				wpos--;
				dstlen--;
			}

			switch (*rpos) {
				case '"':
				case '\'':
					close_quote_char = *rpos;
					break;
				case '[':
					close_quote_char = ']';
					break;
			}
			*(wpos++) = *(rpos++);
			dstlen++;
			count--;
			break_loop = 0;
			while ((count) && (! break_loop)) {
				if (*rpos == '\\') {	/* escape char preceded by '\' (avoids closing quote when "\"" appears) */
					*(wpos++) = *(rpos++);
					dstlen++;
					count--;
					if (count) {
						*(wpos++) = *(rpos++);
						dstlen++;
						count--;
					}
				} else if (*rpos == close_quote_char) {
					*(wpos++) = *(rpos++);
					dstlen++;
					count--;
					break_loop = 1;
				} else {
					*(wpos++) = *(rpos++);
					dstlen++;
					count--;
				}
			}
		} else { /* other char (a-z 0-9 etc) */
			if (previous_char == ' ') {
				if (javascript_concatenable (&previous_char2) || javascript_concatenable (rpos)) {
					previous_char = previous_char2;
					previous_char2 = '\0';
					wpos--;
					dstlen--;
				}
			}
			*(wpos++) = *(rpos++);
			dstlen++;
			count--;
		}	

		if (dstlen)
			previous_char = *(wpos - 1);
		else
			previous_char = '\0';
		if (dstlen >= 2)
			previous_char2 = *(wpos - 2);
		else
			previous_char2 = '\0';
	}

	return (dstlen);
}

/* END OF ### JAVASCRIPT OPTIMIZATION ROUTINES */


/* ### HTML OPTIMIZATION ROUTINES */

/* optimize a html tag (removes unneeded spaces/LFs etc) */
/* returns	the size of data dumped */
int compress_html_tag (const unsigned char *src, int srclen, unsigned char *dst)
{
	const unsigned char	*rpos = src;
	unsigned char		*wpos = dst;
	int	count, count2;
	int	dstlen = 0;
	int	previous_space;
	char	quote_char;
	int	quote_len;	/* quote length, including the quotes chars */
	int	can_remove_quotes;
	char	previous_char = '\0';	/* previous non-quoted char */
	char	current_char;

	/* removes unneeded spaces/tabs/LFs,
	 * removes unneeded quotes,
	 * convert tags to lowercase (better gzip compression, if applied to html)
	 * eliminates self-contained tag denominator " />" */

	count = srclen;
	previous_space = 0;
	while (count--) {
		current_char = *rpos;
		if ((*rpos == '"') || (*rpos == '\'')) { /* quoted data, let's skip it */
			quote_char = *rpos;
			can_remove_quotes = 1;
			previous_space = 0; 
			quote_len = 1;
	
			*(wpos++) = *(rpos++);
			dstlen++;
			if (count) {
				count2 = count;
				while (count2--) {
					quote_len++;
					*(wpos++) = *rpos;
					dstlen++;
					count--;
					if (*rpos == quote_char){
						count2 = 0;
					} else if ((*rpos == '"') || (*rpos == '\'')) {
						if (quote_len == 2) {
							/* if the first char inside the quotes is a quote_char itself,
							 * (obviously, different from the quote char used in this case)
							 * the quotes cannot be removed */
							can_remove_quotes = 0;
						}
					} else if ((*rpos <= ' ') || (*rpos == '>') || \
						       	(*rpos >= 0x80) || \
							(*rpos == ',') || \
							(*rpos == '=') || \
							(*rpos == ':') || \
							(*rpos == ';') || \
							(*rpos == '.') || \
							(*rpos == '?') || \
							(*rpos == '&') || \
							(*rpos == '$') || \
							(*rpos == '>') || \
							(*rpos == '*')) {
						can_remove_quotes = 0;
					} else if (*rpos == '>') {	/* abnormal quote termination */
						count2 = 0;
						count = 0;
					}
					rpos++;
				}

				/* FIXME: some quoted data may break (not verified yet) if unquoted and having "funny" characters */
				/* FIXME: (not sure) if the quote-supressed quoted data ends with '/' and is followed by '>' could
				 * 		it be interpreted as '/>'? I guess not as the browser parser would probably look for spaces/LF/etc or '>'
				 * 		but may be convenient to leave this (theoretical) possibility mentioned here */
				if ((can_remove_quotes) && (quote_len > 2)) {
					/* eliminates quote chars if text has no spaces within,
					 * except if the quoted data is empty (lenght = 0 chars)
					 * (in this case, keeps the quote) */					
					strncpy_overlapping (wpos - (quote_len - 1), wpos - quote_len, quote_len - 2);
					wpos -= 2;
					dstlen -= 2;
					if (*(rpos - 1) == '>') {	/* abnormal quote termination */
						*(wpos++) = '>';
						dstlen++;			
					} else if ((*rpos > ' ') && (*rpos != '>')){
						/* if there's some junk just after the quotes (now supressed),
						 * adds a space to avoid the quoted data to be corrupted
						 * solution for ex.: "<blabla qweqwe='dddd'/>"  */
						*(wpos++) = ' ';
						dstlen++;
					}
				}
			} else {
				dstlen++;
			}	
		} else {
			/* spaces of any kind */
			if (*rpos <= ' ') {
				if (! previous_space) {
					/* no spaces after '=' */
					if (previous_char != '=') {
						*(wpos++) = ' ';
						dstlen++;
					}
					previous_space = 1;
				}

			} else {

				/* anything else that is not considered spaces */
				
				if (*rpos == '=') {
					/* no spaces before '=' */
					if (previous_space) {
						wpos--;
						dstlen--;
					}
				} else if (*rpos == '>') {
					/* no spaces before '>' */
					if (previous_space) {
						wpos--;
						dstlen--;
					}
				}

				/* no to-lowecase conversion */
				// *(wpos++) = current_char;
				/* *** OR *** */
				/* convert TAG string to lowercase */
				*(wpos++) = lowercase_table [current_char];

				dstlen++;
				previous_space = 0;
			}

			rpos++;

			/* if data to follow is attribute ('=' before that),
			 * skip spaces before the attribute and skip the attribute itself it not between quotes */
			if (current_char == '=') {
				/* skip spaces */
				count2 = 0;
				while ((*(rpos + count2) <= ' ') && (*(rpos + count2) != '\0') && count) {
					count2++;
					count--;
				}
				rpos += count2;
				
				/* process unquoted attribute */
				count2 = 0;
				if ((*(rpos + count2) != '"') && (*(rpos + count2) != '\'')) {
					while ((*(rpos + count2) > ' ') && (*(rpos + count2) != '>') && count) {
						current_char = *(rpos++);
						*(wpos++) = current_char;
						count2++;
						count--;
						dstlen++;
					}

				}
			}
		}
		previous_char = current_char;
	}

	/* converts ending " />" into ">" (eliminates self-contained tag denominator)
	 * This is a pedantic part of HTML syntax, which is irrelevant to browsers anyway.
	 * Since most sites simply do not follow this syntax,
	 * chances are that we're not risking lack of browser support by doing this. */
	if (dstlen >= 5) {
		wpos = dst + (dstlen - 1);	/* last char */
		if ((*(wpos - 1) == '/') && (*(wpos - 2) == ' ')) {
			*(wpos - 2) = '>';
			dstlen -= 2;
		}
	}
	
	return (dstlen);
}

/* optimize html text (removes unneeded spaces/LFs etc) */
/* if allow_empty_html_text != 0, will not nullify (strlen==0) a text which has only spaces/tabs/LFs/etc */
/* returns	dst: the size of data dumped, 0 if string has only spaces/LF/tabs/etc */
int compress_html_text_chunk (const unsigned char *src, int srclen, unsigned char *dst, int allow_empty_html_text)
{
	const unsigned char	*rpos = src;
	unsigned char		*wpos = dst;
	int		count;
	int		dstlen = 0;
	int		previous_space;

	/* convert multiple spaces/tabs/LFs to just one space */
	previous_space = 0;
	count = srclen;
	while (count--) {
		if (*rpos <= ' ') {
			if (! previous_space) {
				*(wpos++) = ' ';
				dstlen++;
				previous_space = 1;
			}
		} else {
			previous_space = 0;
			*(wpos++) = *rpos;
			dstlen++;
		}
		rpos++;
	}

	/* if string has only spaces/tabs/LFs, turn it into an empty string (only if !allow_empty_html_text) */
	if ((dstlen == 1) && (*dst == ' ') && (! allow_empty_html_text))
		dstlen = 0;
	
	return (dstlen);
}

/* END OF ### HTML OPTIMIZATION ROUTINES */


/* ### BASE HTML PARSER ### */


/*
 * checks wheter a tag is the same as the compared one
 * src points to the character just after the '<' (opening html tag)
 * returns: =0 equal, != not equal
 */
int compare_tag (const unsigned char *src, const unsigned char *tag, int taglen)
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
int return_chars_until_chr (const unsigned char *src, const unsigned char breakpoint)
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

/* src: points to first char in javascript itself (just after "<SCRIPT ...>")
 * src must end with a '\0'
 * returns: size until closing tag "</script>" (not counting the tag itself) or EOF
 * 
 * due to the massive amount of broken pages (because webbrowsers tolerate such stuff)
 * we cannot just skip this data merely seeing this data as HTML (like skipping data between <!-- -->),
 * we have to parse it as javascript-HTML mixed data */
int return_javascript_body_len (const unsigned char *src)
{
	const unsigned char	*rpos = src;
	int	srclen;
	int	junklen = 0;
	int	count;
	unsigned char	close_quote_char;
	int 	break_loop;

	srclen = WHICH_STRLEN (src);
	count = srclen;

	while (count) {
		switch (*rpos) {
		case '<':
			if (*(rpos + 1) == '/') {
				if (compare_tag (rpos + 2, "SCRIPT", 6) == 0) {
					// "</SCRIPT"
					return (junklen);
				}
			} else if (count >= 9) {
				// "<![CDATA["
				if (! WHICH_STRNCMP ("<![CDATA[", rpos, 9)) {
					junklen += 9;
					count -= 9;
					rpos += 9;

					while (count) {
						if (*rpos == ']') {
							if (! WHICH_STRNCMP ("]]>", rpos, 3)) {
								// "]]>" (closes CDATA)
								junklen += 3;
								count -= 3;
								rpos += 3;
								break;
							}
						}
						junklen++;
						count--;
						rpos++;
					}
					break;
				}
			}
			junklen++;
			count--;
			rpos++;

			break;

		case '"':
		case '\'':
		case '[':
			switch (*rpos) {
			case '"':
			case '\'':
				close_quote_char = *rpos;
				break;
			case '[':
				close_quote_char = ']';
				break;
			}

			break_loop = 0;

			/* skips first quoted_char */
			junklen++;
			count--;
			rpos++;
			
			while ((count) && (! break_loop)) {
				if (*rpos == '\\') {
					/* skip escaped chars (meant to avoid \" and \') */
					junklen++;
					count--;
					rpos++;
				} else if (*rpos == close_quote_char) {
					break_loop = 1;
				}

				if (count) {
					junklen++;
					count--;
					rpos++;
				}
			} 
			break;
			
		case '\\':
			if (count > 1) {
				junklen += 2;
				rpos += 2;
				count -= 2;
			} else {
				junklen++;
				rpos++;
				count--;
			}
			break;
			
		case '/':
			if (*(rpos + 1) == '(') {
				/* /(x) escaped char 'x'
				 * FIXME (fixme?) not sure how this escape mode works, seems to work fine though */
				break_loop = 0;

				junklen += 2;
				count -= 2;
				rpos += 2;
				while ((count) && (! break_loop)) {
					if (*rpos == ')')
						break_loop = 1;
					junklen++;
					count--;
					rpos++;
				}
			} else {
				junklen++;
				count--;
				rpos++;
			}
			break;
		default:
			junklen++;
			count--;
			rpos++;
		}
	}

	/* if it reached here, it means EOF before finding the closing tag */
	return (junklen);
}



/* finds size of individual elements of composite chunk (opening tag, content and closing tag)
 * composite chunks are the ones like: javascript, style etc
 * closing_tag must start with '/' and without braces ('<', '>')
 * assumes that:- opening and closing tag exists
 *		- opening tag starts at first char (*src[0] == '<')
 * returns: == 0: OK, != 0: broken composite chunk
 * also returns data into: opening_tag_size, content_size, closing_tag_size
 */
int break_composite_chunk (const unsigned char *src, int srclen, const unsigned char *closing_tag, int closing_tag_len, int *opening_tag_size, int *content_size, int *closing_tag_size)
{
	const unsigned char	*rpos = src;
	int	content_at, closing_tag_at;
	int	count = srclen;

	if (! WHICH_STRNCASECMP (closing_tag, "/SCRIPT", 7)) {
      		/* javascript needs a smarter size detection because its anomalous body text */
		*opening_tag_size = return_chars_until_chr (rpos, '>');
		rpos += *opening_tag_size;
		*content_size = return_javascript_body_len (rpos);		
		rpos += *content_size;
		*closing_tag_size = return_chars_until_chr (rpos, '>');

		return (0);
	}

	/* finds content position */
	content_at = 0;
	while ((count) && (*(src + content_at) != '>')) {
		count--;
		content_at++;
	}
	if (*(src + content_at) == '>')
		content_at++;
	*opening_tag_size = content_at;
	
	/* finds closing tag position */
	closing_tag_at = content_at;
	while (count) {
		if (*(src + closing_tag_at) == '<') {
			if (! compare_tag (src + closing_tag_at + 1, closing_tag, closing_tag_len)) {
				count = 0;
			} else {
				count--;
				closing_tag_at++;
			}
		} else {
			count--;
			closing_tag_at++;
		}
	}
	*content_size = closing_tag_at - content_at;
	*closing_tag_size = srclen - closing_tag_at;

	/* checks sanity of returned sizes */
	if (*closing_tag_size <= 0)
		return (1);
	if ((*opening_tag_size + *content_size + *closing_tag_size) != srclen)
		return (1);

	/* checks whether closing tag is sane */
	if (*(src + (srclen - 1)) != '>')
		return (1);
	
	return (0);
}

/*
 * used to define the size of chunks with non-HTML data inside (Javascript, <PRE> texts..)
 * closing_tag must be something like "SCRIPT", "PRE" (no '>', '<' nor '/' included) etc, non-case sensitive
 * returns the size of this chunk, including the closing_tag
 */
/* src points to '<' */
int return_junky_chunk_len (const unsigned char *src, const unsigned char *closing_tag, int closing_tag_len)
{
	const unsigned char	*rpos = src;
	int	junklen = 0;
	int	otagsize;
	int	bodysize;
	int	ctagsize;
	
	if (! WHICH_STRNCASECMP (closing_tag, "SCRIPT", 6)) {
		/* javascript needs a smarter size detection because it anomalous body text */
		otagsize = return_chars_until_chr (rpos, '>');
		rpos += otagsize;
		bodysize = return_javascript_body_len (rpos);
		rpos += bodysize;
		ctagsize = return_chars_until_chr (rpos, '>');
		junklen += (otagsize + bodysize + ctagsize);
	} else {	
		while (*rpos != '\0') {
			if (*rpos == '<') {
				if (*(rpos + 1) == '/') {
					if (compare_tag (rpos + 2, closing_tag, closing_tag_len) == 0) {
						return (junklen + 2 + return_chars_until_chr (rpos + 2, '>'));
					}
				}
			}
			junklen++;
			rpos++;
		}
	}
	return (junklen);
}

/* similar to return_junky_chunk_len(), except it deals with chunks between "<!--" and "-->" */
/* src points to '<', _assumes_ first characters as "<!--" */
int return_comment_chunk_len (const unsigned char *src)
{
	const unsigned char	*rpos = src + 4;
	int	charslen = 4;

	while (*rpos != '\0') {
		if (*rpos == '-') {
			if (*(rpos + 1) == '-') {
				if (*(rpos + 2) == '>')
					return (charslen + 3);
			}
		}
		charslen++;
		rpos++;
	}

	return (charslen);
}

/* similar to return_junky_chunk_len(), except it deals with chunks between "<![CDATA[" and "]]>" */
/* src points to '<', _assumes_ first characters as "<![CDATA[" */
int return_cdata_len (const unsigned char *src)
{
	const unsigned char	*rpos = src + 9;
	int	charslen = 9;

	while (*rpos != '\0') {
		if (*rpos == ']') {
			if (*(rpos + 1) == ']') {
				if (*(rpos + 2) == '>')
					return (charslen + 3);
			}
		}
		charslen++;
		rpos++;
	}

	return (charslen);
}

/* returns: !=0 ok and size_of_chunk(chars), ==0 EOF or error */
int get_chunk_info (const unsigned char *src, chunk_info *info)
{
	const unsigned char *secd = src + 1;
	int	c_size;

	/* detect chunk type */

	if (*src == '<') {
		/* anything, except CT_HTML_TEXT */
		if (*secd == '\0')
			return (0);
		
		if ((*secd == 'P') || (*secd == 'p')) {
			if (compare_tag (secd, "PRE", 3) == 0) {
				info->c_type = CT_HTML_PRE_TEXT;
				c_size = return_junky_chunk_len (src, "PRE", 3);
			} else {
				info->c_type = CT_HTML_TAG_OTHER;
				c_size = return_chars_until_chr (src, '>');
			}
		} else if ((*secd == 'T') || (*secd == 't')) {
			if (compare_tag (secd, "TEXTAREA", 8) == 0) {
				info->c_type = CT_HTML_TEXTAREA;
				c_size = return_junky_chunk_len (src, "TEXTAREA", 8);
			} else {
				info->c_type = CT_HTML_TAG_OTHER;
				c_size = return_chars_until_chr (src, '>');
			}	
		} else if (*secd == '!'){
			if (WHICH_STRNCMP (secd, "!--[", 4) == 0) {
				info->c_type = CT_COMMENT_EXTENSION;
				c_size = return_comment_chunk_len (src);
			} else if (WHICH_STRNCMP (secd, "!--", 3) == 0) {
				info->c_type = CT_COMMENT;
				c_size = return_comment_chunk_len (src);
			} else if (compare_tag (secd, "![CDATA[", 8) == 0) {
				info->c_type = CT_CDATA;
				c_size = return_cdata_len (src);
			} else {
				info->c_type = CT_EXCLAM_TYPES;
				c_size = return_chars_until_chr (src, '>');
			}
		} else if ((*secd == 'S') || (*secd == 's')){
			if (compare_tag (secd, "STYLE", 5) == 0) {
				info->c_type = CT_STYLE;
				c_size = return_junky_chunk_len (src, "STYLE", 5);
			/* FIXME: this is technically wrong
			 * <SCRIPT...> may contain a language other than javascript.
			 * in practice, all such containers hold javascript code
			 * we'll worry about that later */
			} else if (compare_tag (secd, "SCRIPT", 6) == 0) {
				info->c_type = CT_JAVASCRIPT;
				c_size = return_junky_chunk_len (src, "SCRIPT", 6);
			} else {
				info->c_type = CT_HTML_TAG_OTHER;
				c_size = return_chars_until_chr (src, '>');
			}
		} else {
			/* other HTML tag */
			info->c_type = CT_HTML_TAG_OTHER;
			c_size = return_chars_until_chr (src, '>');
		}
	} else if (*src == '\0') {
		return (0);
	} else	{
		/* CT_HTML_TEXT only */
		info->c_type = CT_HTML_TEXT;
		c_size = return_chars_until_chr (src, '<');
		if (*(src + (c_size - 1)) == '<')
			c_size--;
	}

	return (c_size);
}

/* END OF ### BASE HTML PARSER ### */

/* converts CRLF and CR to that standard and closes dst with '\0' 
 * it also replaces '\0' in the middle of the text with spaces
 * return: size of resulting text (may be <= srclen because of CR suppression, for example) */
/* *dst may be the same pointer as *src, but the buffer must be at least srclen+1 */
int fix_linebreaks (const unsigned char *src, int srclen, unsigned char *dst)
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

int hopt_pack_css (const unsigned char *src, int srclen, unsigned char *dst)
{
	return (compress_style_chunk (src, srclen, dst));
}

int hopt_pack_javascript (const unsigned char *src, int srclen, unsigned char *dst)
{
	return (compress_javascript_chunk (src, srclen, dst));
}

/* optimize chunks which are composed of opening_tag + content + closing_tag (javascript, style, pre text etc) */
int pack_composite_chunk (const unsigned char *src, int srclen, unsigned char *dst, HOPT_FLAGS which_content, HOPT_FLAGS pack_htmltags)
{
	int	(*comp_call) (const unsigned char *, int, unsigned char *) = NULL;
	const unsigned char	*closing_tag = NULL;
	int			closing_tag_len;
	int	dstlen;
	int	composite_situation;	/* composite chunk (such as javascript, style etc) sanity */
	int	rsize1, rsize2, rsize3;	/* size of opening_tag, content and closing_tag within composite chunks */
	int	wsize1, wsize2, wsize3; /* same as rsize*, except it's the size after compression */

	switch (which_content) {
	case HOPT_JAVASCRIPT:
		comp_call = compress_javascript_chunk;
		closing_tag = "/SCRIPT";
		closing_tag_len = 7;
		break;
	case HOPT_CSS:
		comp_call = compress_style_chunk;
		closing_tag = "/STYLE";
		closing_tag_len = 6;
		break;
	case CT_HTML_PRE_TEXT:
		/* comp_call does not exist for PRE text */
		closing_tag = "/PRE";
		closing_tag_len = 4;
		break;
	case CT_HTML_TEXTAREA:
		/* comp_call does not exist for TEXTAREA */
		closing_tag = "/TEXTAREA";
		closing_tag_len = 9;
		break;
	}

	if (closing_tag != NULL)
		composite_situation = break_composite_chunk (src, srclen, closing_tag, closing_tag_len, &rsize1, &rsize2, &rsize3);
	else
		composite_situation = 1;

	if (composite_situation == 0) {
		/* opening tag "<blabla ..." */
		if (pack_htmltags) {
			wsize1 = compress_html_tag (src, rsize1, dst);
		} else {
			strncpy_overlapping (src, dst, rsize1);
			wsize1 = rsize1;
		}

		/* the content itself (javascript, CSS, etc.. including "<!--" and "-->" if existant) */
		if (comp_call != NULL) {
			wsize2 = (*comp_call) (src + rsize1, rsize2, dst + wsize1);
		} else {
			/* just dump the unmodified data */
			strncpy_overlapping (src + rsize1, dst + wsize1, rsize2);
			wsize2 = rsize2;
		}
	
		/* closing tag "</blabla>" */
		if (pack_htmltags) {
			wsize3 = compress_html_tag (src + rsize1 + rsize2, rsize3, dst + wsize1 + wsize2);
		} else {
			strncpy_overlapping (src + rsize1 + rsize2, dst + wsize1 + wsize2, rsize3);
			wsize3 = rsize3;
		}

		dstlen = wsize1 + wsize2 + wsize3;
	} else {
		/* composite chunk is broken somehow, let's just dump it unmodified */
		strncpy_overlapping (src, dst, srclen);
		dstlen = srclen;
	}		

	return (dstlen);	
}
	
/* src = html text to be compressed (no need to be suffixed by '\0')
 * dst = destination buffer, may be the same as src. size must be at least src_size + 1. will be suffixed by '\0'.
 * srclen = html text size (not counting trailing '\0' if existant)
 * returns: size of optimized html text
 */
int hopt_pack_html (const unsigned char *src, int srclen, unsigned char *dst, HOPT_FLAGS flags)
{
	const unsigned char	*rpos;
	unsigned char		*wpos;
	chunk_info	info;
	int		rc_size, wc_size;
	int		w_total_size = 0;
	int		allow_empty_html_text = 0; /* does not allow more than 1 empty text-chunk after another */

	srclen = fix_linebreaks (src, srclen, dst);

	/* since src is a constant array of chars, and we need to pre-modify the data in order to work with that,
	 * we'll use dst buffer as src and dst simultaneously (the routines tolerate this), thus avoiding the need
	 * to malloc a third buffer */

	*(dst + srclen) = '\0'; /* trailing zero needed, the optimization routines may rely on this if the JS/HTML meets EOF prematurely */
	
	src = dst; /* 'linebreak_fixed' text is now in dst, we work there from now on */
	rpos = src;
	wpos = dst;
	
	while ((rc_size = get_chunk_info (rpos, &info))) {
		switch (info.c_type) {
		case CT_COMMENT:
			/* ommits comments, unless it's a "<!-- -->" which, interestingly,
			 * was only found in the micros_ft website and, if removed, breaks
			 * the formatting badly */
			if (flags & HOPT_NOCOMMENTS) {
				wc_size = 0;
				if (rc_size == 8) {
					if (! WHICH_STRNCMP ("<!-- -->", rpos, 8)) {
						strncpy_overlapping (rpos, wpos, rc_size);
						wc_size = 8;
					}
				}
			} else {
				strncpy_overlapping (rpos, wpos, rc_size);
				wc_size = rc_size;
			}
			break;
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "COMMENT");
#endif
		case CT_HTML_TEXT:
			if (flags & HOPT_HTMLTEXT) {
				wc_size = compress_html_text_chunk (rpos, rc_size, wpos, allow_empty_html_text);
			} else {
				strncpy_overlapping (rpos, wpos, rc_size);
				wc_size = rc_size;
			}

			if (wc_size > 0)
				allow_empty_html_text = 1;
			else
				allow_empty_html_text = 0;
			break;
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "HTML_TEXT");
#endif
		case CT_HTML_TAG_OTHER:
			if (flags & HOPT_HTMLTAGS) {
				wc_size = compress_html_tag (rpos, rc_size, wpos);
			} else {
				strncpy_overlapping (rpos, wpos, rc_size);
				wc_size = rc_size;
			}
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "HTML_TAG_OTHER");
#endif
			break;
		case CT_JAVASCRIPT:
			wc_size = pack_composite_chunk (rpos, rc_size, wpos, flags & HOPT_JAVASCRIPT, flags & HOPT_HTMLTAGS);
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "JAVASCRIPT");
#endif
			break;
		case CT_STYLE:
			wc_size = pack_composite_chunk (rpos, rc_size, wpos, flags & HOPT_CSS, flags & HOPT_HTMLTAGS);
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "STYLE");
#endif
			break;
		case CT_HTML_PRE_TEXT:
			wc_size = pack_composite_chunk (rpos, rc_size, wpos, flags & HOPT_PRE, flags & HOPT_HTMLTAGS);
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "PRE_TEXT");
#endif	
			break;
		case CT_HTML_TEXTAREA:
			wc_size = pack_composite_chunk (rpos, rc_size, wpos, flags & HOPT_TEXTAREA, flags & HOPT_HTMLTAGS);
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "TEXTAREA");
#endif
			break;
		case CT_COMMENT_EXTENSION:
		case CT_EXCLAM_TYPES:
			/* TODO: these tags may be optimizable (a guess) by the HTML-tag routines, since i'm unsure right now let's not modify */
			/* note: "<!DOCTYPE" is probably useless in practice (at least when WWW browsers are concerned)
			 * and perhaps it can be simply supressed */

			/* dumps the data, unmodified */
			strncpy_overlapping (rpos, wpos, rc_size);
			wc_size = rc_size;
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "COMMENT/EXCLAM");
#endif
			break;
		case CT_CDATA:
			/* dumps the data, unmodified */
			strncpy_overlapping (rpos, wpos, rc_size);
			wc_size = rc_size;
#ifdef DEBUG
			debug_dump_chunk_data (wpos, wc_size, "CDATA");
#endif
			break;
		}

		rpos += rc_size;
		wpos += wc_size;
		w_total_size += wc_size;
	}
	
	/* finished, close the text as it should be */
	*wpos = '\0';
	
	return (w_total_size);
}


/* coming next: EOF */


