/* log.h
 * Convenient logging functions.
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
 *
 * This code also contains portions under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c) Juraj Variny, 2004
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY JURAJ VARINY
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ---------------------------------------------------------------------
 */

//To stop multiple inclusions.
#ifndef SRC_LOG_H
#define SRC_LOG_H

#include "globaldefs.h"
#include <stdarg.h>

#define t_log_msg_type enum enum_log_msg_type
#define t_log_subsystem enum enum_log_subsystem

enum enum_log_msg_type {LOGMT_ERROR, LOGMT_FATALERROR, LOGMT_WARN, LOGMT_INFO};
enum enum_log_subsystem {LOGSS_CONFIG, LOGSS_PARAMETER, LOGSS_DAEMON, LOGSS_UNSPECIFIED};

extern ZP_DATASIZE_TYPE accesslog_inlen;	/* PRIVATE */
extern ZP_DATASIZE_TYPE accesslog_outlen;	/* PRIVATE */

/* accesslog-related */
#define LOG_AC_FLAG_NONE			0
#define LOG_AC_SOFTWARE_BUG			1 << 9 /* something very wrong happened */
#define LOG_AC_FLAG_CONV_PROXY			1 << 10 /* P - conventional proxy */
#define LOG_AC_FLAG_TRANSP_PROXY		1 << 11 /* T - transparent proxy */
#define LOG_AC_FLAG_BROKEN_PIPE			1 << 12 /* B - broken pipe during connection */
#define LOG_AC_FLAG_IMG_TOO_EXPANSIVE		1 << 13 /* K - image too expansive */
#define LOG_AC_FLAG_LLCOMP_TOO_EXPANSIVE	1 << 14 /* G - lossless data too expansive (gzip etc) */
#define LOG_AC_FLAG_TOS_CHANGED			1 << 15	/* Q - ToS was changed */
#define LOG_AC_FLAG_CONN_METHOD			1 << 16 /* S - HTTP CONNECT method (typical HTTPS) */
#define LOG_AC_FLAG_XFER_TIMEOUT		1 << 17 /* Z - transfer timeout */
#define LOG_AC_FLAG_URL_NOTPROC			1 << 18 /* N - URL not processed - see URLNoProcessing */
#define LOG_AC_FLAG_TOOBIG_NOMEM		1 << 19 /* W - (see description in documentation) */
#define LOG_AC_FLAG_REPLACED_DATA		1 << 20 /* R - data was replaced */
#define LOG_AC_FLAG_SIGSEGV			1 << 21 /* 1 - SIGSEGV received */
#define LOG_AC_FLAG_SIGFPE			1 << 22 /* 2 - SIGFPE received */
#define LOG_AC_FLAG_SIGILL			1 << 23 /* 3 - SIGILL received */
#define LOG_AC_FLAG_SIGBUS			1 << 24 /* 4 - SIGBUS received */
#define LOG_AC_FLAG_SIGSYS			1 << 25 /* 5 - SIGSYS received */
#define LOG_AC_FLAG_SIGTERM			1 << 26 /* X - SIGTERM received */

extern int debug_log_init (const char *debuglog_filename);
extern int debug_log_printf (char *fmt, ...);
extern void debug_log_puts (char *str);
extern void debug_log_puts_hdr (char *str);
extern void debug_log_reset_difftime (void);
extern void debug_log_difftime (char *activity);
extern void debug_log_set_pid_current (void);

extern int access_log_init (const char *accesslog_filename);
extern void access_log_reset (void);
extern void access_log_define_client_adrr (const char *client_addr);
extern void access_log_define_username (const char *username);
extern void access_log_define_method (const char *method);
extern void access_log_define_url (const char *url);
extern void access_log_set_flags (ZP_FLAGS given_flags);
extern void access_log_unset_flags (ZP_FLAGS given_flags);
extern ZP_FLAGS access_log_get_flags (void);
#define access_log_def_inlen(g_len) (accesslog_inlen = g_len)
#define access_log_add_inlen(g_len) (accesslog_inlen += g_len)
#define access_log_ret_inlen(empty) accesslog_outlen
#define access_log_def_outlen(g_len) (accesslog_outlen = g_len)
#define access_log_add_outlen(g_len) (accesslog_outlen += g_len)
#define access_log_ret_outlen(empty) accesslog_outlen
extern void access_log_dump_entry (void);

extern int error_log_init (const char *errfilename);
extern void error_log_pre_init (void);
extern void error_log_no_stderr (void);
extern int error_log_vprintf (t_log_msg_type log_msg_type, t_log_subsystem log_subsystem, const char *fmt, va_list ap);
extern int error_log_printf (t_log_msg_type log_msg_type, t_log_subsystem log_subsystem, const char *fmt, ...);
extern void error_log_puts (t_log_msg_type log_msg_type, t_log_subsystem log_subsystem, const char *str);

#endif //SRC_LOG_H

