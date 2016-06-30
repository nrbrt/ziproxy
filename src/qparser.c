/* qparser.c
 * Quick'n'Dirty config-file parser.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "txtfiletools.h"

#include <string.h>
#include "qparser.h"

/* applied to t_qp_bool */
#define QP_INVALID -1

#define t_qp_datatype enum enum_qp_datatype

enum enum_qp_datatype {QP_DATATYPE_STRING,
		QP_DATATYPE_INT_FLOAT,
		QP_DATATYPE_BOOL,
		QP_DATATYPE_ARRAY,
		QP_DATATYPE_INVALID,
		QP_DATATYPE_ANY,
		QP_DATATYPE_EMPTY
};

static t_qp_initflags qp_globalflags;

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n)
{
	char *newstr = NULL;

	if ((newstr = calloc (n + 1, sizeof (char))) != NULL) {
		strncpy (newstr, s, n);
		*(newstr + n) = '\0';
	}

	return (newstr);
}
#endif

t_qp_datatype return_datatype (const char *src);
char *remove_quotes (char *src, t_qp_datatype datatype);
void log_error (t_qp_configfile *conf_handler, const char *conf_key, t_qp_error error_code);
char *array_get_element (char *src, int position);

int strcmp_x (const char *s1, const char *s2)
{
	if (qp_globalflags & QP_INITFLAG_IGNORECASE)
		return (strcasecmp (s1, s2));
	return (strcmp (s1, s2));
}

int strncmp_x (const char *s1, const char *s2, size_t n)
{
	if (qp_globalflags & QP_INITFLAG_IGNORECASE)
		return (strncasecmp (s1, s2, n));
	return (strncmp (s1, s2, n));
}

/* remove comments and empty lines, leave only useful config data */
/* 'src' may be the same as 'dst' */
void remove_useless_data (const char *src, char *dst)
{
	int linelen, counter;
	int emptyline;
	int inside_grouping;
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
			} else if (*readpos == '"') {
				*(writepos++) = *(readpos++);
				counter--;

				/* this may seem like a possible memory invasion case,
				 * but the text as whole has a trailing '\0' still.
				 * this is just an ugly shortchut */
				while (counter && (*readpos != '"')) {
					*(writepos++) = *(readpos++);
					counter--;
				}
				if (counter && (*readpos == '"')) {
					*(writepos++) = *(readpos++);
					counter--;
				}
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
			} else if (*readpos == '"') {
				*(writepos++) = *(readpos++);
				counter--;

				/* this may seem like a possible memory invasion case,
				 * but the text as whole has a trailing '\0' still.
				 * this is just an ugly shortchut */
				while (counter && (*readpos != '"')) {
					*(writepos++) = *(readpos++);
					counter--;
				}
				if (counter && (*readpos == '"')) {
					*(writepos++) = *(readpos++);
					counter--;
				}
				counter++;
			} else {
				*(writepos++) = *(readpos++);
			}
			counter--;
		}
	}
	*writepos = '\0';

	/* join lines from the same entry into just one
	 * typically lines in the {} context */
	readpos = dst;
	writepos = dst;
	counter = strlen (readpos);
	inside_grouping = 0;
	while (counter) {
		switch (*readpos) {
		case '"':
			*(writepos++) = *(readpos++);
			counter--;

			/* this may seem like a possible memory invasion case,
			 * but the text as whole has a trailing '\0' still.
			 * this is just an ugly shortchut */
			while (counter && (*readpos != '"')) {
				*(writepos++) = *(readpos++);
				counter--;
			}
			if (counter && (*readpos == '"')) {
				*(writepos++) = *(readpos++);
				counter--;
			}
			break;
		case '\\':
			*(writepos++) = *(readpos++);
			counter--;
			if (counter) {
				*(writepos++) = *(readpos++);
				counter--;
			}
			break;
		case '{':
			inside_grouping = 1;
			*(writepos++) = *(readpos++);
			counter--;
			break;
		case '}':
			inside_grouping = 0;
			*(writepos++) = *(readpos++);
			counter--;
			break;
		case '\n':
			if (inside_grouping == 0)
				*(writepos++) = *(readpos++);
			else
				readpos++;
			counter--;
			break;
		default:
			*(writepos++) = *(readpos++);
			counter--;
		}
	}
	*writepos = '\0';

}

/*
 * check whether the key matches or not
 * returns: 0:does not  1:matches -1:syntax error
 */
int check_conf_key (const char *src, int src_len, const char *conf_key)
{
	int loop = src_len;
	int equal_pos = -1;
	const char *readpos;

	/* where is the (first) '=' ? */
	readpos = src;
	while (loop) {
		if (*(readpos++) == '=') {
			equal_pos = src_len - loop;
			loop = 0;
		} else {
			loop--;
		}
	}
	if (equal_pos <= 0) {
		return (-1);
	}

	if (strlen (conf_key) == equal_pos) {
		if (! strncmp_x (src, conf_key, equal_pos))
			return (1);
	}

	return (0);
}

/*
 * get entry from config file as string
 * (if it's supposed to be int, should be converted later from string)
 * returns: string with value, or NULL if not found
 * NOTE: the returned pointer, if != NULL, should be freed manually
 */
char *get_single_val (t_qp_configfile *conf_handler, const char *conf_key, t_qp_parm_status parm_status)
{
	char *readpos;
	int linelen;
	int conf_key_len;
	char *new_str;
	char *ret_data = NULL;
	int data_len;
	int line_number = 0;
	
	conf_key_len = strlen (conf_key);
	
	readpos = conf_handler->filedata;
	while ((linelen = get_line_len (readpos)) != 0) {
		if (check_conf_key (readpos, linelen, conf_key) == 1) {
			if (parm_status == QP_PARM_STATUS_OBSOLETE)
				log_error (conf_handler, conf_key, QP_ERROR_OBSOLETE);
			if (parm_status == QP_PARM_STATUS_NOT_SUPPORTED)
				log_error (conf_handler, conf_key, QP_ERROR_NOT_SUPPORTED);

			if (ret_data != NULL)
				log_error (conf_handler, conf_key, QP_ERROR_DUPLICATED);
			
			conf_handler->requested_conf [line_number] = QP_TRUE;

			data_len = linelen - (conf_key_len + 1);
			new_str = strndup (readpos + conf_key_len + 1, data_len);

			/* remove trailing \n if present */
			if (*(new_str + (data_len - 1)) == '\n')
				*(new_str + (data_len - 1)) = '\0';

			ret_data = new_str;
		} else {
			/* TODO: syntax error in this line */
		}

		readpos += linelen;
		line_number++;
	}
	return (ret_data);
}

/* returns: !=0: sane, ==0: syntax error */
int check_datatype_sanity (t_qp_configfile *conf_handler, const char *conf_key, char *src, t_qp_datatype datatype)
{
	char *ret_data;

	ret_data = strdup (src);

	if (return_datatype (ret_data) == datatype) {
		if ((datatype == QP_DATATYPE_STRING) || (datatype == QP_DATATYPE_ARRAY)) {
			if (remove_quotes (ret_data, datatype) == NULL) {
				/* syntax error related to " or {} */
				log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
				free (ret_data);
				return (0);
			}
		}

		free (ret_data);
		return (1);
	} else {
		/* syntax error (invalid datatype) */
		log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		free (ret_data);
		return (0);
	}

	free (ret_data);
	return (1);
}

/*
 *  same as get_single_val(), but:
 * - also verifies whether the readen value is coherent with datatype
 * - add error messages to log when appropriate
 * - remove quotes " from string, removes {} from arrays
 */
char *get_single_val_typed_adjusted (t_qp_configfile *conf_handler, const char *conf_key, t_qp_datatype datatype, const t_qp_flags qp_flags)
{
	char *ret_data;
	
	if ((ret_data = get_single_val (conf_handler, conf_key, QP_PARM_STATUS_VALID)) != NULL) {
		if (check_datatype_sanity (conf_handler, conf_key, ret_data, datatype)) {
			if ((datatype == QP_DATATYPE_STRING) || (datatype == QP_DATATYPE_ARRAY))
				remove_quotes (ret_data, datatype);
			return (ret_data);
		}
	} else {
		/* not found */
		if (qp_flags & QP_FLAG_REQUIRED)
			log_error (conf_handler, conf_key, QP_ERROR_MISSING_CONFKEY);
	}

	return (NULL);
}

/*
 *  similar to get_single_val_typed_adjusted() but used for array data instead
 * - also verifies whether the readen value is coherent with datatype
 * - add error messages to log when appropriate
 * - remove quotes " from string
 * - won't report error if data addressed by position does not exist
 *   (since it may be used to determine the array length)
 */
char *get_array_val_typed_adjusted (t_qp_configfile *conf_handler, const char *conf_key, int position, t_qp_datatype datatype, const t_qp_flags qp_flags)
{
	char *ret_data;
	char *element_data;

	if ((ret_data = get_single_val_typed_adjusted (conf_handler, conf_key, QP_DATATYPE_ARRAY, qp_flags)) != NULL) {
		element_data = array_get_element (ret_data, position);
		free (ret_data);

		if (element_data == NULL)
			return (NULL);

		if (datatype == QP_DATATYPE_ANY)
			return (element_data);

		if (check_datatype_sanity (conf_handler, conf_key, element_data, datatype)) {
			if (datatype == QP_DATATYPE_STRING)
				remove_quotes (element_data, datatype);
			return (element_data);
		}
	} else {
		/* not found */
		if (qp_flags & QP_FLAG_REQUIRED)
			log_error (conf_handler, conf_key, QP_ERROR_MISSING_CONFKEY);
	}

	return (NULL);
}

/*
 * src contains boolean value, this routine interprets its value.
 * returns: QP_TRUE, QP_FALSE or QP_INVALID (syntax error)
 */
t_qp_bool collect_bool_from_str (const char *src)
{
	int counter;
	const char tab_true [][5] = {"TRUE", "T", "Y", "YES", ""};
	const char tab_false [][6] = {"FALSE", "F", "N", "NO", ""};

	/* check if true */
	counter = 0;
	while (tab_true [counter][0] != '\0') {
		if (! strcmp_x (src, tab_true [counter]))
			return (QP_TRUE);
		counter++;
	}

	/* check if false */
	counter = 0;
	while (tab_false [counter][0] != '\0') {
		if (! strcmp_x (src, tab_false [counter]))
			return (QP_FALSE);
		counter++;
	}

	/* neither, error */
	return (QP_INVALID);
}

/* return what kind of data is the one pointed by 'src' */
t_qp_datatype return_datatype (const char *src)
{
	switch (*src) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '.':
	case '-':
		return (QP_DATATYPE_INT_FLOAT);
	case '{':
		return (QP_DATATYPE_ARRAY);
	case '\0':
		return (QP_DATATYPE_EMPTY);
	case '"':
		return (QP_DATATYPE_STRING);
	default:
		if (collect_bool_from_str (src) != QP_INVALID)
			return (QP_DATATYPE_BOOL);
		return (QP_DATATYPE_INVALID);
	}
}

/*
 * remove surrounding [ " ] or [ { and } ] from string depending on its type
 * if everything ok, returns 'src'
 * if syntax error (braces not closed, for example) returns NULL
 * applies only to QP_DATATYPE_ARRAY and QP_DATATYPE_STRING
 */
char *remove_quotes (char *src, t_qp_datatype datatype)
{
	int srclen;

	srclen = strlen (src);
	if (srclen < 2)
		return (NULL);

	/* check if there's syntax error */
	switch (datatype) {
	case QP_DATATYPE_ARRAY:
		if (*src == '{') {
			if (*(src + (srclen - 1)) != '}')
				return (NULL);
		}
		break;
	case QP_DATATYPE_STRING:
		if (*src == '"') {
			if (*(src + (srclen - 1)) != '"') {
				return (NULL);
			}
		}
		break;
	default:
		return (NULL);
	}

	memmove (src, src + 1, (srclen - 2) * sizeof (char));
	*(src + (srclen - 2)) = '\0';
	return (src);
}

void log_error (t_qp_configfile *conf_handler, const char *conf_key, t_qp_error error_code)
{
	int counter = 0;

	/* check whether the error is not logged already */
	while (counter < conf_handler->errorlog_total) {
		if (! strcmp_x (conf_handler->errorlog [counter].conf_key, conf_key)) {
			if (conf_handler->errorlog [counter].error_code == error_code) {
				/* already logged */
				return;
			}
		}
		counter++;
	}

	/* return if cannot add more error entries */
	if (conf_handler->errorlog_total == QC_ERRORLOG_MAX_ERRORS)
		return;

	/* the last error is reserved to report 'more errors, but not displayed' (error table overflow) */
	if (conf_handler->errorlog_total == (QC_ERRORLOG_MAX_ERRORS - 1)) {
		conf_handler->errorlog [conf_handler->errorlog_total].conf_key = strdup ("(...)");
		conf_handler->errorlog [conf_handler->errorlog_total].error_code = QP_ERROR_MORE_ERRORS;
	} else {
		conf_handler->errorlog [conf_handler->errorlog_total].conf_key = strdup (conf_key);
		conf_handler->errorlog [conf_handler->errorlog_total].error_code = error_code;
	}
	
	(conf_handler->errorlog_total)++;
}

/* get str len of a element from within {} array
 * delimiter is ',' (if outside quotes) or '\0' (regardless)
 * returns: element size, _not_ including the following ',' if present */
int array_get_element_size (const char *src)
{
	const char *readpos = src;
	int element_size = 0;
	int inside_quote = 0;
	
	while (*readpos != '\0') {
		switch (*readpos) {
		case '"':
			inside_quote ^= 1;
			readpos++;
			element_size++;
			break;
		case '\\':
			if (*readpos != '\0') {
				readpos += 2;
				element_size += 2;;
			} else {
				readpos++;
				element_size++;
			}
			break;
		case ',':
			if (inside_quote) {
				readpos++;
				element_size++;
			} else {
				return (element_size);
			}
			break;
		default:
			element_size++;
			readpos++;
		}
	}

	return (element_size);
}

/* get the a element from the array.
 * src must be in the following format:
 * element1,element2,element3
 * returns: pointer to a string containing that element (that string must be freeded manually)
 * 	or NULL, if element not found */
char *array_get_element (char *src, int position)
{
	int element_size;

	element_size = array_get_element_size (src);
	while (position) {
		src += element_size;
		if (*src == '\0')
			return (NULL);
		src++;
		element_size = array_get_element_size (src);

		position--;
	}

	return ((char *) strndup (src, element_size));
}

/* 
 * API (public) functions start here
 */

t_qp_configfile *qp_init (const char *conf_filename, t_qp_initflags qp_initflags)
{
	t_qp_configfile *conf_handler;
	int conf_len;
	char *readpos;
	int counter;

	conf_handler = malloc (sizeof (t_qp_configfile));
	if (conf_handler == NULL) {
		/* unable to allocate mem */
		return (NULL);
	}

	conf_handler->filename = conf_filename;

	conf_handler->filedata = load_textfile_to_memory (conf_handler->filename);
	if (conf_handler->filedata == NULL) {
		/* unable to load file into memory */
		free (conf_handler);
		return (NULL);
	}
	
	conf_len = strlen (conf_handler->filedata);
	fix_linebreaks_qp (conf_handler->filedata, conf_len, conf_handler->filedata);

	remove_useless_data (conf_handler->filedata, conf_handler->filedata);

	conf_handler->errorlog_total = 0;
	qp_globalflags = qp_initflags;

	/* create and initialize array for requested_conf */
	conf_len = 0;
	readpos = conf_handler->filedata;
	conf_handler->total_lines = 0;
	while (*readpos != '\0') {
		if (*readpos == '\n')
			conf_handler->total_lines++;
		readpos++;
		conf_len++;
	}
	if ((*(conf_handler->filedata + (conf_len - 1)) != '\n') && (*(conf_handler->filedata + (conf_len - 1)) != '\0'))
		conf_handler->total_lines++;
	if ((conf_handler->requested_conf = calloc (conf_handler->total_lines, sizeof (t_qp_bool))) == NULL) {
		free (conf_handler);
		return (NULL);
	}
	counter = 0;
	while (counter < conf_handler->total_lines) {
		conf_handler->requested_conf [counter] = QP_FALSE;
		counter++;
	}

	return (conf_handler);
}

void qp_end (t_qp_configfile *conf_handler)
{
	/* TODO: free strdup()s from error log */

	free (conf_handler->requested_conf);
	free (conf_handler);
}

/* used to see errors while parsing for later processing (warning the user, etc)
 * error_num starts from 0
 * returns !=0 if there's a error at error_num, ==0 if there's none */
int qp_get_error (t_qp_configfile *conf_handler, int error_num, const char **conf_key, t_qp_error *error_code)
{
	if (conf_handler->errorlog_total < (error_num + 1))
		return (0);

	*conf_key = conf_handler->errorlog [error_num].conf_key;
	*error_code = conf_handler->errorlog [error_num].error_code;
	return (1);
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_float (t_qp_configfile *conf_handler, const char *conf_key, float *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	float pre_dst;

	if ((ret_data = get_single_val_typed_adjusted (conf_handler, conf_key, QP_DATATYPE_INT_FLOAT, qp_flags)) != NULL) {
		if (! sscanf (ret_data, "%f", &pre_dst)) {
			/* syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_int (t_qp_configfile *conf_handler, const char *conf_key, int *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	int pre_dst;

	if ((ret_data = get_single_val_typed_adjusted (conf_handler, conf_key, QP_DATATYPE_INT_FLOAT, qp_flags)) != NULL) {
		if (! sscanf (ret_data, "%d", &pre_dst)) {
			/* syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable
 * neither allocating memory for the result string */
void qp_getconf_str (t_qp_configfile *conf_handler, const char *conf_key, char **dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	
	if ((ret_data = get_single_val_typed_adjusted (conf_handler, conf_key, QP_DATATYPE_STRING, qp_flags)) != NULL) {
		if (dst == NULL) {
			free (ret_data);
		} else {
			*dst = ret_data;
			/* TODO: add ret_data to free() pool */
		}
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_bool (t_qp_configfile *conf_handler, const char *conf_key, t_qp_bool *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	t_qp_bool pre_dst;

	if ((ret_data = get_single_val_typed_adjusted (conf_handler, conf_key, QP_DATATYPE_BOOL, qp_flags)) != NULL) {
		switch (collect_bool_from_str (ret_data)) {
		case QP_TRUE:
			pre_dst = QP_TRUE;
			break;
		case QP_FALSE:
			pre_dst = QP_FALSE;
			break;
		default:
			/* invalid boolean, syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_array_bool (t_qp_configfile *conf_handler, const char *conf_key, int position, t_qp_bool *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	t_qp_bool pre_dst;

	if ((ret_data = get_array_val_typed_adjusted (conf_handler, conf_key, position, QP_DATATYPE_BOOL, qp_flags)) != NULL) {
		switch (collect_bool_from_str (ret_data)) {
		case QP_TRUE:
			pre_dst = QP_TRUE;
			break;
		case QP_FALSE:
			pre_dst = QP_FALSE;
			break;
		default:
			/* invalid boolean, syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable
 * neither allocating memory for the result string */
void qp_getconf_array_str (t_qp_configfile *conf_handler, const char *conf_key, int position, char **dst, const t_qp_flags qp_flags)
{
	char *ret_data;

	if ((ret_data = get_array_val_typed_adjusted (conf_handler, conf_key, position, QP_DATATYPE_STRING, qp_flags)) != NULL) {
		if (dst == NULL) {
			free (ret_data);
		} else {
			*dst = ret_data;
			// TODO: add element_data to free() pool */
		}
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_array_int (t_qp_configfile *conf_handler, const char *conf_key, int position, int *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	int pre_dst;

	if ((ret_data = get_array_val_typed_adjusted (conf_handler, conf_key, position, QP_DATATYPE_INT_FLOAT, qp_flags)) != NULL) {
		if (! sscanf (ret_data, "%d", &pre_dst)) {
			/* syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

/* accepts dst == NULL, when wanting to check the configuration option presence but not updating any variable */
void qp_getconf_array_float (t_qp_configfile *conf_handler, const char *conf_key, int position, float *dst, const t_qp_flags qp_flags)
{
	char *ret_data;
	float pre_dst;

	if ((ret_data = get_array_val_typed_adjusted (conf_handler, conf_key, position, QP_DATATYPE_INT_FLOAT, qp_flags)) != NULL) {
		if (! sscanf (ret_data, "%f", &pre_dst)) {
			/* syntax error */
			log_error (conf_handler, conf_key, QP_ERROR_SYNTAX);
		}
		if (dst != NULL)
			*dst = pre_dst;
		free (ret_data);
	}

	return;
}

int qp_get_array_size (t_qp_configfile *conf_handler, const char *conf_key)
{
	char *ret_data;
	int array_size = 0;

	while ((ret_data = get_array_val_typed_adjusted (conf_handler, conf_key, array_size, QP_DATATYPE_ANY, QP_FLAG_NONE)) != NULL) {
		free (ret_data);
		array_size++;
	}

	return (array_size);
}

/* used to warn uses that a parameter has special status
   DO NOT use for parameters already declared with qp_getconf_*,
   since those are already considered QP_PARM_STATUS_VALID */
void qp_set_parameter_status (t_qp_configfile *conf_handler, t_qp_parm_status parm_status, const char *conf_key)
{
	get_single_val (conf_handler, conf_key, parm_status);
}

/* returns !=0 is parameter exists in config file */
int qp_check_parameter_existence (t_qp_configfile *conf_handler, const char *conf_key)
{
	if (get_single_val (conf_handler, conf_key, QP_PARM_STATUS_VALID) != NULL)
		return (1);
	return (0);
}

/* add to the errorlist pool options which were not requested (using qp_getconf_* funtions)
 * up to the point this function is called
 * use this if you do _not_ want the parser to simply ignore unrecognized options */
void qp_fail_unrecognized_conf (t_qp_configfile *conf_handler)
{
	int counter = 0;
	int linelen;
	const char *readpos = conf_handler->filedata;
	char *new_str;

	if (conf_handler->total_lines == 0)
		return;
	
	while (counter < conf_handler->total_lines) {
		linelen = get_line_len (readpos);
		if (conf_handler->requested_conf [counter] == QP_FALSE) {

			new_str = (char *) strndup (readpos, linelen);
			if (*(new_str + (linelen - 1)) == '\n')
				*(new_str + (linelen - 1)) = '\0';

			log_error (conf_handler, new_str, QP_ERROR_INVALID_PARM);
		}
		counter++;
		readpos += linelen;
	}
}


/* ends here */
