/* qparser.h
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

#ifndef QPARSER_H
#define QPARSER_H

#define t_qp_bool int
#define t_qp_flags int
#define t_qp_initflags int

/* applied to t_qp_bool */
#define QP_TRUE 1
#define QP_FALSE 0

/* applied to t_qp_flags */
#define QP_FLAG_NONE 0
#define QP_FLAG_REQUIRED 1

/* applied to t_qp_init_flags */
#define QP_INITFLAG_NONE 0
#define QP_INITFLAG_IGNORECASE 1

/* private */
/* harcoded limit to parser error log 
 * must be 2 or higher */
#define QC_ERRORLOG_MAX_ERRORS 10

#define t_qp_error enum enum_qp_error
#define t_qp_parm_status enum enum_qp_parm_status

/* private as is, use it as 't_qp_error' instead */
enum enum_qp_error {QP_ERROR_NONE,
		QP_ERROR_SYNTAX,
		QP_ERROR_MISSING_CONFKEY,
		QP_ERROR_INVALID_PARM,
		QP_ERROR_DUPLICATED,
		QP_ERROR_MORE_ERRORS,
		QP_ERROR_OBSOLETE,
		QP_ERROR_NOT_SUPPORTED
};

/* private as is, use it as 't_qp_parm_status'
   private: QP_PARM_STATUS_VALID
   public: the rest */
enum enum_qp_parm_status {QP_PARM_STATUS_VALID, QP_PARM_STATUS_OBSOLETE, QP_PARM_STATUS_NOT_SUPPORTED};

/* private */
typedef struct {
	const char *conf_key;
	t_qp_error error_code;
} t_qp_errorlog;

typedef struct {
	const char *filename;	/* private */
	char *filedata;		/* private */
	int errorlog_total;	/* private */
	t_qp_errorlog errorlog [QC_ERRORLOG_MAX_ERRORS];	/* private */
	int total_lines;		/* private */
	t_qp_bool *requested_conf;	/* private */
} t_qp_configfile;

t_qp_configfile *qp_init (const char *conf_filename, t_qp_initflags qp_initflags);
void qp_end (t_qp_configfile *conf_handler);
int qp_get_error (t_qp_configfile *conf_handler, int error_num, const char **conf_key, t_qp_error *error_code);
void qp_getconf_float (t_qp_configfile *conf_handler, const char *conf_key, float *dst, const t_qp_flags qp_flags);
void qp_getconf_int (t_qp_configfile *conf_handler, const char *conf_key, int *dst, const t_qp_flags qp_flags);
void qp_getconf_str (t_qp_configfile *conf_handler, const char *conf_key, char **dst, const t_qp_flags qp_flags);
void qp_getconf_bool (t_qp_configfile *conf_handler, const char *conf_key, t_qp_bool *dst, const t_qp_flags qp_flags);
void qp_getconf_array_bool (t_qp_configfile *conf_handler, const char *conf_key, int position, t_qp_bool *dst, const t_qp_flags qp_flags);
void qp_getconf_array_str (t_qp_configfile *conf_handler, const char *conf_key, int position, char **dst, const t_qp_flags qp_flags);
void qp_getconf_array_int (t_qp_configfile *conf_handler, const char *conf_key, int position, int *dst, const t_qp_flags qp_flags);
void qp_getconf_array_float (t_qp_configfile *conf_handler, const char *conf_key, int position, float *dst, const t_qp_flags qp_flags);
int qp_get_array_size (t_qp_configfile *conf_handler, const char *conf_key);
void qp_set_parameter_status (t_qp_configfile *conf_handler, t_qp_parm_status parm_status, const char *conf_key);
int qp_check_parameter_existence (t_qp_configfile *conf_handler, const char *conf_key);
void qp_fail_unrecognized_conf (t_qp_configfile *conf_handler);

#endif

