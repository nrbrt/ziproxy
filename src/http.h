/* http.h
 * HTTP communication functions.
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

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "globaldefs.h"

//If this is included in http.c, make declarations local.
#ifdef SRC_HTTP_C
#define EXTERN
#else
#define EXTERN extern
#endif 

//To stop multiple inclusions.
#ifndef SRC_HTTP_H
#define SRC_HTTP_H

#define MAX_HEADERS 200

enum enum_content_type {OTHER_CONTENT, IMG_PNG, IMG_GIF, IMG_JPEG, IMG_JP2K, TEXT_HTML, TEXT_CSS, APPLICATION_JAVASCRIPT};
#define t_content_type enum enum_content_type

typedef struct {
	char * hdr[MAX_HEADERS];
	int lines;

	/* those "where_*" vars store the position in the array of headers */
	int where_content_type, where_content_length, where_chunked;
	int where_content_encoding, where_etag;
	
	// If headers contain request from client, status is always -1.
	// It may be 200, 404 etc
	int status;

	char *method, *url, *proto, *host, *path, *content_encoding, *content_type, *user_agent, *x_ziproxy_flags;
	int flags, content_encoding_flags;
	t_content_type type;
	ZP_DATASIZE_TYPE content_length;
	int has_content_range;
	int client_explicity_accepts_jp2;
	unsigned short int port;

	int chunklen;	
} http_headers;

#define H_WILLGZIP (1<<1)	// whether the (real, user's) client supports Gzip
#define H_USE_SSL (1<<3)
#define H_KEEPALIVE (1<<4)
#define H_SIMPLE_RESPONSE (1<<5)
#define H_TRANSP_PROXY_REQUEST (1<<6)

#define DO_NOTHING 0
#define DO_COMPRESS (1<<10)
#define DO_PRE_DECOMPRESS (1<<12)
#define DO_OPTIMIZE_HTML (1<<13)
#define DO_OPTIMIZE_CSS (1<<14)
#define DO_OPTIMIZE_JS (1<<15)
#define DO_PREEMPT_DNS (1<<16)
#define DO_RECOMPRESS_PICTURE (1<<17)

// Includes all the flags commanding some sort of modification to the body
#define META_CONTENT_MODIFICATION (DO_COMPRESS | DO_PRE_DECOMPRESS | DO_OPTIMIZE_HTML | DO_OPTIMIZE_CSS | DO_OPTIMIZE_JS | DO_RECOMPRESS_PICTURE)

// Includes all the flags commanding some operation requiring reading the body
// Currently: (META_ALL_CONTENT_MODIFICATION | DO_PREEMPT_DNS)
#define META_CONTENT_MUSTREAD (DO_COMPRESS | DO_PRE_DECOMPRESS | DO_OPTIMIZE_HTML | DO_OPTIMIZE_CSS | DO_OPTIMIZE_JS | DO_PREEMPT_DNS | DO_RECOMPRESS_PICTURE)

#define PROP_ENCODED_NONE 0
#define PROP_ENCODED_GZIP (1<<0)
#define PROP_ENCODED_DEFLATE (1<<1)
#define PROP_ENCODED_COMPRESS (1<<2)
#define PROP_ENCODED_UNKNOWN (1<<10)

EXTERN int is_sending_data;

EXTERN http_headers * parse_initial_request(void);
EXTERN void proxy_http (http_headers *client_hdr, FILE* sockrfp, FILE* sockwfp);
EXTERN void blind_tunnel (http_headers *hdr, FILE* sockrfp, FILE* sockwfp);

EXTERN void send_error( int status, char* title, char* extra_header, char* text );
EXTERN void send_headers( int status, char* title, char* extra_header, char* mime_type, int length, time_t mod );

EXTERN http_headers *new_headers(void);
EXTERN char *find_header(const char* key, const http_headers *hdr);
EXTERN int find_header_nr(const char* key, const http_headers *hdr);
EXTERN int add_header(http_headers *hdr, const char *newhdr);
EXTERN int remove_header(http_headers *hdr, int n);
EXTERN void remove_header_str(http_headers *hdr, const char* key);
EXTERN void replace_header_str(http_headers *hdr, const char* key, const char *newhdr);
EXTERN http_headers * get_response_headers(FILE *sockrfp);
EXTERN void send_headers_to(FILE * sockfp, http_headers *hdr);
EXTERN int return_content_encoding(http_headers *shdr);
EXTERN void decide_what_to_do(http_headers *chdr, http_headers *shdr);

EXTERN void fix_request_url (http_headers *hdr);
EXTERN void get_client_headers(http_headers * hdr);

#define SERVER_NAME "ziproxy"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_LINELEN 16384

#endif //SRC_HTTP_H

#undef EXTERN

