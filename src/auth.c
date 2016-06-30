/* auth.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include "txtfiletools.h"
#include "strtables.h"
#include "auth.h"
#include "log.h"

static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

static int local_AuthMode;
static t_st_strtable *passwd_table;
static const char *local_AuthPasswdFile;
#ifdef SASL
static const char *local_AuthSASLConfPath;
#endif

t_st_strtable *auth_create_populate_from_file (const char *filename);

// Decode login:pass pair for HTTP authorization
char *base64_decode (const char *input) {
    unsigned char in[4], out[3], v;
    int r, i, len, str_len;
    char *outstr;
    int outpos=0;

    outstr=(char *)malloc(512);
    str_len = strlen(input);

    r = 0;
    while (input[r] && outpos < 508 ) {
        for (len = 0, i=0; i<4 && input[r]; i++) {
            v = 0;
            while(input[r] && v == 0 ) {
                v = (unsigned char) input[r++];
                v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
                if(v)
		  v = (unsigned char) ((v == '$') ? 0 : v - 61);
            }
            if (input[r] || ( (str_len % 4) == 0 && input[str_len-1] != '=' ) ) {
	      len++;
	      if (v)
		in[i] = (unsigned char) (v - 1);
	    } else {
	      in[i] = 0;
	    }
        }

        if(len) {
	  out[0] = (unsigned char) (in[0] << 2 | in[1] >> 4);
	  out[1] = (unsigned char) (in[1] << 4 | in[2] >> 2);
	  out[2] = (unsigned char) (((in[2] << 6) & 0xc0) | in[3]);
	  for (i=0; i<len-1; i++)
	    outstr[outpos++] = out[i];

        }
    }
    outstr[outpos]=0;
    return(outstr);
}

/* load auth file into memory */
/* the returned structure must be free'ed manually later */
t_st_strtable *auth_create_populate_from_file (const char *filename)
{
	char *userpasstxt;
	t_st_strtable *userpass;
	char *userpass_pos, *lone_userpass;
	int linelen, useful_linelen;

	if ((userpasstxt = load_textfile_to_memory (filename)) == NULL)
		return (NULL);
	
	if ((userpass = st_create ()) == NULL) {
		free (userpasstxt);
		return (NULL);
	}

	/* preprocess text in order to be more palatable */
	fix_linebreaks_qp (userpasstxt, strlen (userpasstxt), userpasstxt);
	remove_junk_data (userpasstxt, userpasstxt);

	/* this routine assumes there are no empty lines (just LF) */
	userpass_pos = userpasstxt;
	while ((linelen = get_line_len (userpass_pos)) != 0) {
		if (*(userpass_pos + (linelen - 1)) == '\n')
			useful_linelen = linelen - 1;
		else
			useful_linelen = linelen;

		lone_userpass = calloc (useful_linelen + 1, sizeof (char));
		strncpy (lone_userpass, userpass_pos, useful_linelen);
		*(lone_userpass + useful_linelen) = '\0';

		/* nometa prevents a '*' being incorrectly treated as a meta-character */
		st_insert_nometa (userpass, lone_userpass);

		free (lone_userpass);
		userpass_pos += linelen;
	}

	free (userpasstxt);
	return (userpass);
}

/* init passwd file authentication system
   returns: ==0 success, !=0 error */
/* this function has no 'deinit' counterpart currently,
   it relies on OS cleanup when the process is finished */
int auth_passwdfile_init (const char *passwd_file)
{
	local_AuthPasswdFile = passwd_file;
	if ((passwd_table = auth_create_populate_from_file (passwd_file)) == NULL)
		return 1;
	return 0;
}

#ifdef SASL

/* init SASL authentication system
   returns: ==0 success, !=0 error */
/* if sasl_conf_path==NULL, don't change SASL configuration path */
int auth_sasl_init (const char *sasl_conf_path)
{
	local_AuthSASLConfPath = sasl_conf_path;

	if (sasl_conf_path != NULL) {
		if (sasl_set_path (SASL_PATH_TYPE_CONFIG, local_AuthSASLConfPath) != SASL_OK)
			return 1;
	}
	if (sasl_server_init (NULL, "ziproxy") != SASL_OK)
		return 1;

	return 0;
}

/* counterpart of auth_sasl_init */
int auth_sasl_end (void)
{
	sasl_done ();
}

/* check authentication through SASL
   returns: !=0 ok, ==0 invalid auth */
int auth_sasl_check (const char *user, const char *pass)
{
	sasl_conn_t *conn;
	int sn_result;
	int cp_result;

	sn_result = sasl_server_new ("ziproxy", NULL, "", NULL, NULL, NULL, 0, &conn);
	if (sn_result != SASL_OK) {
		error_log_printf (LOGMT_ERROR, LOGSS_UNSPECIFIED,
			"Failed to create SASL server: %d\n", sn_result);
		return 0; /* Treat as failed auth */
	}

	cp_result = sasl_checkpass (conn, user, strlen (user), pass, strlen (pass));
	if (cp_result != SASL_OK) {
		debug_log_printf ("Failed SASL authentication for %s: %d\n", user, cp_result);
		return 0;
	}

	return 1; /* success */
}

#endif

/* FIXME: SECURITY ISSUE
   add a provision to filter userpass to restrict to certain valid characters.
   this unfiltered processing is vulnerable to SQL injection. */
// TODO: optimize this -- store userpass in base64 encoding and compare base64-to-base64 instead
/* check BASIC HTTP authentication */
/* userpass is formated as follow: user:password (base64 encoded) */
/* returns: !=0 u/p is valid, ==0 invalid u/p */
int auth_basic_check (const char *userpass)
{
	char *decoded_userpass;
	int retcode = 0;
	char *user;
	char *pass;
	char *between;

	decoded_userpass = base64_decode (userpass);

	switch (local_AuthMode) {
	case AUTH_PASSWD_FILE:
		retcode = st_check_if_matches_nometa (passwd_table, decoded_userpass);
		break;
#ifdef SASL
	case AUTH_SASL:
		/* split userpass into user and pass */
		between = strchr (decoded_userpass, ':');
		if (between == NULL)
			return 0;
		*between = '\0';
		user = decoded_userpass;
		pass = between + 1;

		retcode = auth_sasl_check (user, pass);
		break;
#endif
	}

	free (decoded_userpass);
	return retcode;
}

/* defines the authentication mode to be used.
   this function does NOT initialize the proper authentication
   engine, that must be done separately. */
void auth_set_mode (int auth_mode)
{
	local_AuthMode = auth_mode;
}

/* get the username string from basic authentication */
/*  userpass is formated as follow: user:password (base64 encoded) */
/* the returned string is malloc'ed (may be changed) and must be free'ed later */
char *auth_get_username (const char *userpass)
{
	char *decoded_userpass;
	char *username_end;

	if ((decoded_userpass = base64_decode (userpass)) != NULL)
		if ((username_end = strchr (decoded_userpass, ':')) != NULL)
			*username_end = '\0';
	return (decoded_userpass);
}
