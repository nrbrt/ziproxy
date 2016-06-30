/* http.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define SRC_HTTP_C

#include "http.h"
#include "image.h"
#include "cfgfile.h"
#include "htmlopt.h"
#include "log.h"
#include "text.h"
#include "preemptdns.h"
#include "cdetect.h"
#include "urltables.h"
#include "embbin.h"
#include "auth.h"
#include "misc.h"
#include "tosmarking.h"
#include "globaldefs.h"
#include "session.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LOCAL_HOSTNAME_LEN 256
#define HEADER_REPLACEMENT_ENTRY_LEN 512

int is_sending_data = 0;

static char line[MAX_LINELEN];

//Local forwards.

static int check_trim( char* line ); 
ZP_DATASIZE_TYPE forward_content (http_headers *hdr, FILE *from, FILE *to);
ZP_DATASIZE_TYPE read_content (http_headers *hdr, FILE *from, FILE *to, char ** inbuf, ZP_DATASIZE_TYPE *inlen);
static void clean_hdr(char* ln);
void replace_data_and_send (http_headers *serv_hdr);

// close( sockfd );
void proxy_http (http_headers *client_hdr, FILE* sockrfp, FILE* sockwfp)
{
	http_headers *serv_hdr;
	long i;
	char *tempp;
	char *inbuf, *outbuf;
	ZP_DATASIZE_TYPE inlen, outlen;
	int status = 0;
	ZP_DATASIZE_TYPE original_size;
	char new_user_agent [HEADER_REPLACEMENT_ENTRY_LEN];
	ZP_DATASIZE_TYPE streamed_len;	// used when load into memory failed and data was streamed

	is_sending_data = 0;

	if (URLNoProcessing != NULL) {
		if (ut_check_if_matches (urltable_noprocessing, client_hdr->host, client_hdr->path)) {
			/* we won't touch this data, just tunnel it */
			add_header (client_hdr, "Connection: close");
			debug_log_puts ("Headers sent to server:");
			send_headers_to (sockwfp, client_hdr);
			debug_log_puts ("Sending untouched HTTP data...");
			blind_tunnel (client_hdr, sockrfp, sockwfp);

			access_log_set_flags (LOG_AC_FLAG_URL_NOTPROC);
			access_log_dump_entry ();
			return;
		}
	}

	// set TOS accordingly if URL matches
	tosmarking_check_url (client_hdr->host, client_hdr->path);

	// blocks forbidden URLs
	if (URLDeny != NULL) {
		if (ut_check_if_matches (urltable_deny, client_hdr->host, client_hdr->path)) {
			debug_log_puts ("Locally forbidden URL (HTTP 403).");
			send_error (403, "Forbidden", NULL, "Forbidden URL.");
		}
	}

#ifdef JP2K
	if (AnnounceJP2Capability)
		replace_header_str (client_hdr, "X-Ziproxy-Flags", "X-Ziproxy-Flags: jp2");
#endif
	if (OverrideAcceptEncoding)
		replace_header_str(client_hdr, "Accept-Encoding", "Accept-Encoding: gzip");

	if (RedefineUserAgent != NULL) {
		snprintf (new_user_agent, HEADER_REPLACEMENT_ENTRY_LEN, "User-Agent: %s\n", RedefineUserAgent);
		replace_header_str(client_hdr, "User-Agent", new_user_agent);
	}
	
	add_header(client_hdr, "Connection: close");

	// Send request
	debug_log_puts ("Headers sent to server:");
	
	send_headers_to(sockwfp, client_hdr);

	// If there's content, forward that too
	if(client_hdr->content_length > 0)
		forward_content (client_hdr, sess_rclient, sockwfp);

	debug_log_difftime ("Connecting, forwarding headers");

	// Get the response
	debug_log_puts ("In Headers:");
	serv_hdr = get_response_headers(sockrfp);

	// set TOS accordingly if Content-Type matches
	tosmarking_check_content_type (serv_hdr->content_type);

	decide_what_to_do(client_hdr, serv_hdr);

	// replace data entirely if URL is listed in the table
	if (URLReplaceData != NULL) {
		if (ut_check_if_matches (urltable_replacedata, client_hdr->host, client_hdr->path)) {
			replace_data_and_send (serv_hdr);
			return;
		}
	}

	// replace data entirely if URL is listed in the table _and_ content-type matches
	if ((URLReplaceDataCT != NULL) && (serv_hdr->where_content_type > 0)) {
		if (ut_check_if_matches (urltable_replacedatact, client_hdr->host, client_hdr->path)) {
			if (ct_check_if_matches (urltable_replacedatactlist, serv_hdr->content_type) != 0) {
				replace_data_and_send (serv_hdr);
				return;
			}
		}
	}

	//log so far
	debug_log_printf ("Image = %d, Chunked = %d\n",
		serv_hdr->type, (serv_hdr->where_chunked > 0));

	debug_log_printf ("WillGZip = %d, Compress = %d, DoPreDecompress = %d\n",
		(client_hdr->flags & H_WILLGZIP) != 0, 
		(serv_hdr->flags & DO_COMPRESS) != 0,
		(serv_hdr->flags & DO_PRE_DECOMPRESS) != 0);

	//if no data requested only forward header and exit
	if (strcasecmp(client_hdr->method, "HEAD") == 0) {
       		debug_log_puts ("Forwarding header only.");
		add_header(serv_hdr, "Connection: close");
		add_header(serv_hdr, "Proxy-Connection: close");

		send_headers_to (sess_wclient, serv_hdr);
		fflush (sess_wclient);

		access_log_dump_entry ();
		return;
	}

	// if HTTP/0.9 simple response is received, just forward that and exit
	if (serv_hdr->flags & H_SIMPLE_RESPONSE) {
		debug_log_puts ("Forwarding HTTP/0.9 Simple Response.");
		fputs (serv_hdr->hdr[0], sess_wclient); // first few bytes of a simple response
		outlen = forward_content (serv_hdr, sockrfp, sess_wclient);

		access_log_def_inlen(outlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}

	// data is encoded (gzip/other) and cannot be decoded (either because is unsupported or user requested no decoding)
	// just stream that, unmodified
	if  ( ((serv_hdr->content_encoding_flags & ~PROP_ENCODED_GZIP) != PROP_ENCODED_NONE) ||
		( (serv_hdr->content_encoding_flags == PROP_ENCODED_GZIP) && (! (serv_hdr->flags & DO_PRE_DECOMPRESS)) ) ) {

		debug_log_puts ("Data is encoded and cannot be decoded");	
		is_sending_data = 1;
		debug_log_puts ("Forwarding header and streaming data.");
		add_header (serv_hdr, "Connection: close");
		add_header (serv_hdr, "Proxy-Connection: close");
		send_headers_to (sess_wclient, serv_hdr);
		outlen = forward_content (serv_hdr, sockrfp, sess_wclient);

		access_log_def_inlen(outlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}

	// stream-to-stream compression, if we're not requesting pre-decompression AND (either one of the following):
	// - gzip is the only optimization requested
	// - file too big, but gzipping requested - we can do gzip, so stream it
	// 	(no other optimization/processing will be applied)
	// - streaming file (can't know its size unless we download it) and requests gzipping
	// 	(no other optimization/processing will be applied)
	if ( (! (serv_hdr->flags & DO_PRE_DECOMPRESS)) && \
			( \
			  ( (serv_hdr->flags & DO_COMPRESS) && (MaxSize && (serv_hdr->content_length > MaxSize)) ) \
			  || ((serv_hdr->flags & META_CONTENT_MUSTREAD) == DO_COMPRESS) \
			) \
		) {
		int ret;

		ret = do_compress_stream_stream (serv_hdr, sockrfp, sess_wclient, &inlen, &outlen);
		if (ret != 0) {
			// TODO: add flags of 'error' to access log in this case
			debug_log_printf ("Error while gzip-streaming: %d\n", ret);
		}

		access_log_def_inlen(inlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}

	// stream-to-stream decompression, if client does not support gzip AND (either one of the following):
	// - gunzip is the only operation requested
	// - file too big, but gunzipping requested and NO gzipping afterwards
	// 	we can do gunzip, so stream it (no other optimization/processing will be applied)
	// - streaming file (can't know its size unless we download it) and requests gunzipping and NO gzipping
	// 	(no other optimization/processing will be applied)
	if (((serv_hdr->flags & DO_PRE_DECOMPRESS) && (! (serv_hdr->flags & DO_COMPRESS))) && \
			( \
			  ( (serv_hdr->flags & DO_PRE_DECOMPRESS) && (MaxSize && (serv_hdr->content_length > MaxSize)) ) \
			  || ((serv_hdr->flags & META_CONTENT_MUSTREAD) == DO_PRE_DECOMPRESS) \
			) \
		) {
		int ret;
		
		ret = do_decompress_stream_stream (serv_hdr, sockrfp, sess_wclient, &inlen, &outlen, MaxUncompressedGzipRatio, MinUncompressedGzipStreamEval);
		if (ret != 0) {
			// TODO: add flags of 'error' to access log in this case
			debug_log_printf ("Error while gunzip-streaming: %d\n", ret);
		}
	
		access_log_def_inlen(inlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}

	// if either:
	// - the server advertises the data as > MaxSize,
	// - there's no process to be done to the data.
	// - there's nothing except DECOMPRESS->COMPRESS (gzip, again) -- semi-useless (we could gain a few bytes, but adds latency)
	// don't even try to load into memory, stream that directly and reduce latency.
	if ( \
			(MaxSize && (serv_hdr->content_length > MaxSize)) \
			|| ((serv_hdr->flags & META_CONTENT_MUSTREAD) == DO_NOTHING) \
			|| ( ! ((serv_hdr->flags & META_CONTENT_MUSTREAD) & ~(DO_COMPRESS | DO_PRE_DECOMPRESS)) ) \
			) {
		is_sending_data = 1;
		if ((serv_hdr->flags & META_CONTENT_MUSTREAD) == DO_NOTHING)
			debug_log_puts ("Nothing to do - streaming original data");
		else
			debug_log_puts ("MaxSize reached - streaming original data");
		add_header(serv_hdr, "Connection: close");
		add_header(serv_hdr, "Proxy-Connection: close");

		send_headers_to (sess_wclient, serv_hdr);
		outlen = forward_content(serv_hdr, sockrfp, sess_wclient);

		access_log_def_inlen(outlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}
	
	debug_log_puts ("Trying to load the whole data into memory");
	// this will read both streaming data and data with specified content-length
	if ((streamed_len = read_content(serv_hdr,sockrfp, sess_wclient, &inbuf, &inlen)) != 0) {
		debug_log_puts ("Data is of streaming type and doesn't fit MaxSize - streamed original data");

		/* flag this as 'W' since we had to fall back to streaming instead */
		access_log_set_flags (LOG_AC_FLAG_TOOBIG_NOMEM);
		access_log_def_inlen(streamed_len);
		access_log_def_outlen(streamed_len);
		access_log_dump_entry ();
		return;
	}
	debug_log_puts ("Ok, whole data loaded into memory");
	original_size = inlen;

	/* IF IT REACHES THIS POINT
	   it means that the data is wholly loaded into memory and it will be processed */

	// user requested data, but there's none.
	// only forward header and exit
	if (inlen == 0) {
		debug_log_puts ("Forwarding header only.");
		add_header(serv_hdr, "Connection: close");
		add_header(serv_hdr, "Proxy-Connection: close");

		send_headers_to (sess_wclient, serv_hdr);
		fflush (sess_wclient);

		access_log_dump_entry ();
		return;
	}
	
	//serv_hdr->chunklen isn't used anywhere else
	/* TODO: i'm not sure what this block of code does (legacy code).
	         verify this later */
	if(serv_hdr->chunklen > 0){
		if('1' == client_hdr->proto[7]){
			is_sending_data = 1;
			debug_log_puts ("MaxSize reached - streaming original chunked data");
			add_header(serv_hdr, "Transfer-Encoding: chunked");
			add_header(serv_hdr, "Connection: close");
			add_header(serv_hdr, "Proxy-Connection: close");
			send_headers_to (sess_wclient, serv_hdr);
			if(inlen > 0){
				printf("%"ZP_DATASIZE_MSTR"X\r\n",inlen);//TODO verify format
				tosmarking_add_check_bytecount (inlen);	/* check if TOS needs to be changed */
				fwrite (inbuf, 1, inlen, sess_wclient);
				fputs ("\r\n", sess_wclient);
			}
			printf("%X\r\n",serv_hdr->chunklen);
			outlen = forward_content (serv_hdr, sockrfp, sess_wclient);
		}else{
			//It is not worth to code proper unchunking
			//for this exceptional situation.
			send_error(500,"Internal error",NULL,
					"Too big file. Try using HTTP/1.1 client.");
		}

		access_log_def_inlen(outlen);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		return;
	}

	if (inlen != serv_hdr->content_length) debug_log_printf ("In Content-Length: %"ZP_DATASIZE_STR"\n", inlen);

	/* unpacks data gzipped by remote server, in order to process it */
	if (serv_hdr->flags & DO_PRE_DECOMPRESS) {
		char **inbuf_addr;
		int new_inlen;

		debug_log_puts ("Decompressing Gzip data...");
		inbuf_addr = &inbuf;
		new_inlen = replace_gzipped_with_gunzipped(inbuf_addr, inlen, MaxUncompressedGzipRatio);
		if (new_inlen >= 0) {
			inlen = new_inlen;
			inbuf = *inbuf_addr;

			/* no longer gzipped, modify headers accordingly */
			serv_hdr->content_encoding_flags = PROP_ENCODED_NONE;
			serv_hdr->content_encoding = NULL;
			serv_hdr->where_content_encoding = -1;
			remove_header_str(serv_hdr, "Content-Encoding");

			/* data size is changed, modify headers accordingly */
			serv_hdr->content_length = inlen;
			serv_hdr->where_content_length = -1;
			remove_header_str(serv_hdr, "Content-Length");

			debug_log_printf ("Gzip body decompressed for further processing. Decompressed size: %"ZP_DATASIZE_STR"\n", inlen);
		} else {
			switch (new_inlen * -1) {
			case 100:
				send_error( 500, "Internal Error", NULL, "Uncompressed gzipped data exceedes safety threshold." );
				break;
			case 120:
				debug_log_puts ("Broken Gzip data. Forwarding unmodified data.");
				/* will not attempt to compress it again */
				/* since the data is a blackbox, neither we can apply Preemptive DNS */
				serv_hdr->flags &= ~META_CONTENT_MUSTREAD;
				break;
			}
		}
	} else if (serv_hdr->content_encoding_flags != PROP_ENCODED_NONE) {
		/* either:
		 * - data is gzipped but is not supposed to be uncompressed (!DO_PRE_DECOMPRESS)
		 * - data is encoded in an unknown way
		 * we cannot modify it, neither compress it */
		/* since the data is a blackbox, neither we can apply Preemptive DNS */
		serv_hdr->flags &= ~META_CONTENT_MUSTREAD;
		debug_log_puts ("Data is gzipped but is not supposed to be uncompressed OR\n	Data is encoded in an unknown way.");
	}

	//in case something fails later and forgets to do this:
	outbuf = inbuf;
	outlen = inlen;

	// (start) only if data is not encoded
	// data may (still) be encoded in case gzip decompressing failed
	if ((serv_hdr->content_encoding_flags == PROP_ENCODED_NONE) && (inlen > 0)) {
	
	/* text/html optimizer */
	/* FIXME: inbuf must be at least (inlen + 1) chars big in order to hold added '\0' from htmlopt */
	if (serv_hdr->flags & DO_OPTIMIZE_HTML) {
		HOPT_FLAGS hopt_flags = HOPT_NONE;
		if (ProcessHTML_CSS)
			hopt_flags |= HOPT_CSS;
		if (ProcessHTML_JS)
			hopt_flags |= HOPT_JAVASCRIPT;
		if (ProcessHTML_tags)
			hopt_flags |= HOPT_HTMLTAGS;
		if (ProcessHTML_text)
			hopt_flags |= HOPT_HTMLTEXT;
		if (ProcessHTML_PRE)
			hopt_flags |= HOPT_PRE;
		if (ProcessHTML_TEXTAREA)
			hopt_flags |= HOPT_TEXTAREA;
		if (ProcessHTML_NoComments)
			hopt_flags |= HOPT_NOCOMMENTS;

		/* we may find files claiming to be "text/html" while in fact they're not,
		 * (typically CSS or JS)
		 * we cannot optimize those as HTML otherwise we'll get garbage */
		switch (detect_content_type (inbuf)) {
		case CD_TEXT_HTML:
			debug_log_puts ("HTMLopt -> HTML");
			inlen = hopt_pack_html (inbuf, inlen, inbuf, hopt_flags);
			outlen = inlen;
			break;
		default:
			debug_log_puts ("HTMLopt WARNING: Data claimed to be HTML, but it's not.");
			break;
		}
	}

	/* text/css optimizer */
	/* FIXME: inbuf must be at least (inlen + 1) chars big in order to hold added '\0' from htmlopt */
	if (serv_hdr->flags & DO_OPTIMIZE_CSS) {
		debug_log_puts ("HTMLopt -> CSS");
		inlen = hopt_pack_css (inbuf, inlen, inbuf);
		outlen = inlen;
	}

	/* application/[x-]javascript optimizer */
	/* FIXME: inbuf must be at least (inlen + 1) chars big in order to hold added '\0' from htmlopt */
	if (serv_hdr->flags & DO_OPTIMIZE_JS) {
		debug_log_puts ("HTMLopt -> JS");
		inlen = hopt_pack_javascript (inbuf, inlen, inbuf);
		outlen = inlen;
	}
	
	/* preemptive name resolution */
	if (serv_hdr->flags & DO_PREEMPT_DNS)
		preempt_dns_from_html (inbuf, inlen);

	if (serv_hdr->flags & DO_RECOMPRESS_PICTURE) {
		status = compress_image(serv_hdr, client_hdr, inbuf, inlen, &outbuf, &outlen);
		if ((status & IMG_UNIQUE_RET_MASK) == IMG_RET_TOO_EXPANSIVE) {
			debug_log_puts ("WARNING: Image too expansive. Not recompressed.");
			access_log_set_flags (LOG_AC_FLAG_IMG_TOO_EXPANSIVE);
		}
		debug_log_difftime ("Image modification/compression");
		debug_log_printf ("  and returned %d.\n", status);
	}

 	if(serv_hdr->flags & DO_COMPRESS){
		do_compress_memory_stream (serv_hdr, inbuf, sess_wclient, inlen, &outlen);

		access_log_def_inlen(original_size);
		access_log_def_outlen(outlen);
		access_log_dump_entry ();
		exit (0);
	}
	
	} /* (end) only if data is not encoded */

	is_sending_data = 1;
	
	debug_log_puts ("Forwarding header and modified content.");
	debug_log_puts ("Out Headers:");

	snprintf (line, sizeof(line), "Content-Length: %"ZP_DATASIZE_STR, outlen);
	if (serv_hdr->where_content_length > 0) {
		serv_hdr->hdr[serv_hdr->where_content_length] = strdup (line);
	} else {
		add_header (serv_hdr, line);
	}
	add_header (serv_hdr, "Connection: close");
	add_header (serv_hdr, "Proxy-Connection: close");

	send_headers_to (sess_wclient, serv_hdr);

	//forward content
	tempp = outbuf;
	tosmarking_add_check_bytecount (outlen); /* update TOS if necessary */
	if (inlen != 0){
		int outcount = outlen;
		
		do{
			i = fwrite(tempp, 1, outcount, sess_wclient);
			outcount -= i;
			tempp += i;
		}while((i > 0) && outcount);
		
		fflush (sess_wclient);

		if(outcount == 0) {
			fsync(1);
			debug_log_difftime ("Forwarding");
		}
		else debug_log_printf ("Error - Last %d bytes of content could not be written\n",
			outcount);
	}


	access_log_def_inlen(original_size);
	access_log_def_outlen(outlen);
	access_log_dump_entry ();
}

/* replace content with empty one and send to the user */
void replace_data_and_send (http_headers *serv_hdr)
{
	char content_len_str [200];

	/* change headers according to the new data */
	sprintf (content_len_str, "Content-Length: %d", embbin_empty_image_size);
	replace_header_str (serv_hdr, "Content-Length", content_len_str);
	replace_header_str (serv_hdr, "Content-Type", "Content-type: image/gif");
	remove_header_str (serv_hdr, "Content-Encoding");
	remove_header_str (serv_hdr, "Content-Range");
	remove_header_str (serv_hdr, "Transfer-Encoding");
	remove_header_str (serv_hdr, "Connection");

	/* we will read this data only to satisfy the remote server */
	add_header (serv_hdr, "Connection: close");
	debug_log_puts ("Headers sent to client:");
	send_headers_to (sess_wclient, serv_hdr);

	/* send the new data to the client */
	debug_log_puts ("Replaced data. Sent to client.");
	fwrite (embbin_empty_image, embbin_empty_image_size, 1, sess_wclient);

	access_log_def_outlen(embbin_empty_image_size);
	// FIXME: accesslog_data->inlen ---- calculate this
	// TODO: read data provided from server
	if (serv_hdr->content_length != -1) {
		/* we have Content-Length, no need to download
		 * the data from the server (soon we'll just drop the connection) */
		access_log_def_inlen(serv_hdr->content_length);
	} else {
		/* no Content-Length provided */
		/* FIXME: download the whole stuff and check its size.
		 * Currently it just repeats the embbin_empty_image_size, in order
		 * to avoid (major) disruption when generating statistics
		 * from the access_log file.
		 * We cannot simply download the raw data since it may be chunked
		 * and it would reflect the wrong size (due to chunking overhead). */
		access_log_def_inlen(embbin_empty_image_size);
	}

	access_log_set_flags (LOG_AC_FLAG_REPLACED_DATA);
	access_log_dump_entry ();
	return;
}

/* just transfers data in both directions with no regard of what is that */
/* hdr = client http headers */
void blind_tunnel (http_headers *hdr, FILE* sockrfp, FILE* sockwfp)
{
	int client_read_fd, server_read_fd, client_write_fd, server_write_fd;
	struct timeval *timeout = NULL;
	fd_set fdset;
	int maxp1, r;
	char buf [10000];
	ZP_DATASIZE_TYPE total_transferred = 0;
    
	/* Now forward (SSL packets) || (other data) in both directions until done. */
	client_read_fd = fileno (sess_rclient);
	server_read_fd = fileno (sockrfp);
	client_write_fd = fileno (sess_wclient);
	server_write_fd = fileno (sockwfp);
    
	if (ConnTimeout)
		timeout = malloc (sizeof (struct timeval));
    
	if ( client_read_fd >= server_read_fd )
		maxp1 = client_read_fd + 1;
	else
		maxp1 = server_read_fd + 1;
	(void) alarm (0);
	for (;;) {
		if (ConnTimeout) {
			timeout->tv_sec = ConnTimeout;
			timeout->tv_usec = 0;
		}
		FD_ZERO (&fdset);
		FD_SET (client_read_fd, &fdset);
		FD_SET (server_read_fd, &fdset);

		r = select (maxp1, &fdset, NULL, NULL, timeout);
		if (r == 0) {
			// send_error( 408, "Request Timeout", NULL, "Request timed out." );
			return;
		} else if (FD_ISSET (client_read_fd, &fdset)) {
			/* data from client to server */
			r = read (client_read_fd, buf, sizeof (buf));
			if (r <= 0)
				break;
			r = write (server_write_fd, buf, r);
			if (r <= 0)
				break;
		} else if (FD_ISSET (server_read_fd, &fdset)) {
			/* data from server to client */
			r = read (server_read_fd, buf, sizeof (buf));
			tosmarking_add_check_bytecount (r);
			if (r <= 0)
			break;
			r = write (client_write_fd, buf, r);
			total_transferred += r;
			if (r <= 0)
				break;
		}
		
		// update access log stats
		access_log_def_inlen(total_transferred);
		access_log_def_outlen(total_transferred);
	}
}

void send_error( int status, char* title, char* extra_header, char* text )
    {
	    if(is_sending_data){
	    //if already sending data, sending error headers is pointless
		    status = -status;
	    }else{
		FILE *txtfile;
		char *txtfilename;
		char *txtfilebuf;
		int txtfilesize;
		int custom_error_sent = 0;
		char local_hostname [LOCAL_HOSTNAME_LEN];
		time_t local_time;
		struct tm local_time_tm;
		char local_time_str [32];
		
		switch (status){
		case 400:
			txtfilename = CustomError400;
			break;
		case 403:
			txtfilename = CustomError403;
			break;
		case 404:
			txtfilename = CustomError404;
			break;
		case 407:
			txtfilename = CustomError407;
			break;
		case 408:
			txtfilename = CustomError408;
			break;
		case 409:
			txtfilename = CustomError409;
			break;
		case 500:
			txtfilename = CustomError500;
			break;
		case 503:
			txtfilename = CustomError503;
			break;
		default:
			txtfilename = NULL;
		}

		/* displays custom error page, if specified and available */
		if (txtfilename != NULL){
			if ((txtfile = fopen (txtfilename, "r")) != NULL){
				fseek (txtfile, 0, SEEK_END);
				txtfilesize = ftell (txtfile);
				fseek (txtfile, 0, SEEK_SET);			
				if ((txtfilebuf = malloc (txtfilesize + 1)) != NULL){
					fread (txtfilebuf, 1, txtfilesize, txtfile);
					*(txtfilebuf+txtfilesize) = '\0';				
					send_headers( status, title, extra_header, "text/html", -1, -1 );
					printf ("%s", txtfilebuf); /* we don't want problems with '%' characters */
					fflush (sess_wclient);
					custom_error_sent = 1;					
					free (txtfilebuf);
				}
				fclose (txtfile);
			}
		}
		
		if (custom_error_sent == 0){
			local_hostname [0] = '\0';
			gethostname (local_hostname, LOCAL_HOSTNAME_LEN - 1);
			time (&local_time);
			localtime_r (&local_time, &local_time_tm);
			asctime_r (&local_time_tm, local_time_str);

			send_headers( status, title, extra_header, "text/html", -1, -1 );
			fprintf (sess_wclient, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n<BODY BGCOLOR=\"#cc9999\"><H4>%d %s</H4>\n", status, title, status, title);
			fprintf (sess_wclient, "%s\n", text);
			fprintf (sess_wclient, "<HR>\n<ADDRESS>Generated %s by %s (ziproxy/" VERSION ")</ADDRESS>\n</BODY></HTML>\n", local_time_str, local_hostname);
			fflush (sess_wclient);
		}
	    }

    //log
    debug_log_printf ("ERROR - %d %s ( %s )\n", status, title, text);
    exit( 1 );
    }


void send_headers( int status, char* title, char* extra_header, char* mime_type, int length, time_t mod )
{
	time_t now;
	char timebuf[100];

	fprintf (sess_wclient, "%s %d %s\r\n", PROTOCOL, status, title);
	fprintf (sess_wclient, "Server: %s\r\n", SERVER_NAME);
	now = time (NULL);
	strftime (timebuf, sizeof (timebuf), RFC1123FMT, gmtime (&now));
	fprintf (sess_wclient, "Date: %s\r\n", timebuf);
	if (extra_header != NULL)
		fprintf (sess_wclient, "%s\r\n", extra_header);
	if (mime_type != NULL)
		fprintf (sess_wclient, "Content-Type: %s\r\n", mime_type);
	if (length >= 0)
		fprintf (sess_wclient, "Content-Length: %d\r\n", length);
	if (mod != (time_t) -1)
	{
		strftime (timebuf, sizeof (timebuf), RFC1123FMT, gmtime (&mod));
		fprintf (sess_wclient, "Last-Modified: %s\r\n", timebuf);
	}
	fprintf (sess_wclient, "Connection: close\r\nProxy-Connection: close\r\n\r\n");
}


http_headers *new_headers(void){
	http_headers *h = malloc(sizeof(http_headers));

	h->lines = h->flags = 0;
	h->has_content_range = 0;
	h->content_encoding_flags = PROP_ENCODED_NONE;
	h->client_explicity_accepts_jp2 = 0;

	h->where_content_type = h->where_content_length = 
		h->where_chunked = h->where_content_encoding = 
		h->where_etag = h->status = 
		h->chunklen = -1;

	h->content_length = -1;

	h->port = -1;

	h->user_agent = h->content_encoding = h->method = h->url = h->path = h->host = h->proto = NULL;
	
	return h;
}

char *find_header(const char* key, const http_headers *hdr){
	int i;
	for(i=0; i< hdr->lines; i++)
		if(!strncasecmp(hdr->hdr[i], key, strlen(key)))
			return hdr->hdr[i] + strlen(key);
	return NULL;
}

int find_header_nr(const char* key, const http_headers *hdr){
	int i;
	for(i=0; i< hdr->lines; i++)
		if(!strncasecmp(hdr->hdr[i], key, strlen(key)))
			return i;
	return -1;
}

/*Returns index in array of headers*/
int add_header(http_headers *hdr, const char *newhdr){
	if(hdr->lines + 1 <MAX_HEADERS) {
		hdr->hdr[hdr->lines] = strdup(newhdr);
		hdr->lines ++;
	}
	else {
		char *keyword;
		
		if(hdr->status == -1)
			keyword = "Bad request";
		else 
			keyword = "Bad response";
		
		send_error( 400, keyword , NULL, "Too many headers.");
	}
	return hdr->lines - 1;
}


int remove_header(http_headers *hdr, int n){
	int i;
	if(n >= hdr->lines || n < 0) return -1;

	for(i=n; i<hdr->lines - 1;i++){
		hdr->hdr[i] = hdr->hdr[i+1];
	}
	hdr->hdr[i] = NULL;
	hdr->lines--;
	return 0;
}

/* will remove all matching headers */
void remove_header_str(http_headers *hdr, const char* key){
	int header_pos;

	header_pos = find_header_nr (key, hdr);
	while (header_pos >= 0) {
		remove_header (hdr, header_pos);
		header_pos = find_header_nr (key, hdr);
	}
}

/* replace all matching headers with the provided one
 * (only one copy, no matter how many times those headers are present) */
void replace_header_str(http_headers *hdr, const char* key, const char *newhdr){
	remove_header_str(hdr, key);
	add_header(hdr, newhdr);
}

http_headers * parse_initial_request(void){
	size_t linelen;
	int was_auth=0;

	http_headers *hdr = new_headers();
	/* Read the first line of the request. */
	if (fgets (line, sizeof(line), sess_rclient) ==  NULL)
	{
		send_error( 400, "Bad Request", NULL, 
					"No request found or request too long." );
	}
	linelen = check_trim(line);
	if (linelen == 0)
	{
                send_error( 400, "Bad Request", NULL,
					"No request found or request too long." );
	}
	add_header(hdr, line);

	hdr->method=(char*)malloc(6*linelen);
	hdr->url = hdr->method + linelen;
	hdr->proto = hdr->url + linelen;
	hdr->host = hdr->proto + linelen;
	hdr->path = hdr->host + linelen;
	hdr->content_encoding = hdr->path + linelen;

	/* Parse it. */
	if ( sscanf( line, "%[^ ] %[^ ] %[^ ]", hdr->method, hdr->url, hdr->proto ) != 3 )
		send_error( 400, "Bad Request", NULL, "Can't parse request." );

	if (NULL == hdr->url)
		send_error( 400, "Bad Request", NULL, "Null URL." );

	if ( strncasecmp( hdr->url, "http://", 7 ) == 0 )
	{

		strncpy( hdr->url, "http", 4 );	/* make sure it's lower case */
		if ( sscanf( hdr->url, "http://%[^:/]:%hu%s", hdr->host, &hdr->port, hdr->path ) == 3 );			
		else if ( sscanf( hdr->url, "http://%[^/]%s", hdr->host, hdr->path ) == 2 )
		{
		   	hdr->port = 80;
		}
		else if ( sscanf( hdr->url, "http://%[^:/]:%hu", hdr->host, &hdr->port ) == 2 )
		{
			hdr->path=hdr->url;
		}
		else if ( sscanf( hdr->url, "http://%[^/]", hdr->host ) == 1 )
		{
		hdr->port = 80;
			hdr->path=hdr->url;
		}
		else
			send_error( 400, "Bad Request", NULL, "Can't parse URL." );

		// we must fix that at a later stage, since NextProxy expects
		// something like 'http://xxxxxx' instead of '/xxxx'.
		if (NextProxy != NULL)	hdr->path = hdr->url;

		//log URL
		debug_log_printf ("URL - %s\n", hdr->url);
	}
	else if ( strcmp( hdr->method, "CONNECT" ) == 0 )
	{
		if ( sscanf( hdr->url, "%[^:]:%hu", hdr->host, &hdr->port ) == 2 ) ;
		else if ( sscanf( hdr->url, "%s", hdr->host ) == 1 )//equivalent to strcpy until first whitespace?
		    hdr->port = 443;
		else
		    send_error( 400, "Bad Request", NULL, "Can't parse URL." );

		if (AuthMode != AUTH_NONE) {
			// Look for auth header
			while (fgets (line, sizeof (line), sess_rclient) != NULL)
			{
				if ((strcmp (line, "\n") == 0) || (strcmp (line, "\r\n") == 0))
					break;

				if ((strncasecmp (line, "Proxy-Authorization: Basic ", 27) == 0) && (strlen (line) > 30)) {
					char *username;

					// check if user/password is valid
					was_auth = auth_basic_check (line + 27);

					// get username (from auth basic composite string) for access log
					if ((username = auth_get_username (line + 27)) != NULL) {
						access_log_define_username (username);
						free (username);
					}
				}
			}

			if(!was_auth) {
				debug_log_puts ("Requesting HTTP auth from client for CONNECT method");
				send_error( 407, "Proxy Authentication Required", "Proxy-Authenticate: Basic realm=\"internet\"", "You have to be registered before using this proxy.");
			}

		}

		hdr->flags |= H_USE_SSL;
		debug_log_printf ("URL - %s\n", hdr->url);
	}
	else if ((*(hdr->url) == '/') && TransparentProxy)
	{
		hdr->flags |= H_TRANSP_PROXY_REQUEST;

		// transparent proxy is 80 (default HTTP),
		// it may change at a later stage if Host: xxxx defines a port (we need further reading the headers)
		hdr->port = 80;
		strcpy (hdr->path, hdr->url);
		*(hdr->host) = '\0';	// hdr->host is left undefined for now, we need Host: data at a later stage
		/* hdr->url will need to be fixed at a later stage (at this point it only has the file path) */

		if (NextProxy != NULL)	hdr->path = hdr->url;

		//log URL
		// FIXME: this will not log the URL, but only the path instead (no hostname)
		debug_log_printf ("URL - %s\n", hdr->url);
	}
	else
		send_error( 400, "Bad Request", NULL, "Unknown URL type." );
		
	return hdr;
}

//If it returns line length > 0, line was correctly ended with newline - use to 
//check output
//from fgets(). 
static int check_trim( char* line ) {
    int k,l;

    k = l = strlen( line );
	
    while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r'))
		line[--l] = '\0';

	return (k != l)? l : 0;
}


void get_client_headers(http_headers * hdr){
	int was_via=0;
	int linelen;
	int was_auth=0;

	debug_log_puts ("Headers from client:");
	
	
	while (fgets(line, sizeof(line), sess_rclient) != NULL)
	{
		if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
			break;
		
		if((linelen=check_trim(line))==0)
			send_error( 400, "Bad request", NULL, "Line too long.");

//		if (strncasecmp(line,"User-Agent:", 11) == 0)
		debug_log_puts_hdr (line);
		clean_hdr(line);
		if (strncasecmp(line,"Connection:",11) == 0)
				continue;

		if (strncasecmp(line,"Proxy-Connection:",17) == 0)
				continue;

		// does special processing only if proxy authentication is required
		if (AuthMode != AUTH_NONE) {
			if (strncasecmp(line,"Proxy-Authorization:",20) == 0 && strlen(line) > 30) {
				if ((strncasecmp (line, "Proxy-Authorization: Basic ", 27) == 0) && (strlen (line) > 30)) {
					char *username;

					// check if user/password is valid
					was_auth = auth_basic_check (line + 27);

					// get username (from auth basic composite string) for access log
					if ((username = auth_get_username (line + 27)) != NULL) {
						access_log_define_username (username);
						free (username);
					}

					if (was_auth != 0)
						continue;
				}
			}
		}

		if (strncasecmp (line, "X-Ziproxy-Flags:", 16) == 0) {
			char *provided_ziproxy_flags;
			
			provided_ziproxy_flags = line + 16;
			if (*provided_ziproxy_flags == ' ')
				provided_ziproxy_flags++;
				
			hdr->x_ziproxy_flags = strdup (provided_ziproxy_flags);

			if (strstr (hdr->x_ziproxy_flags, "jp2") != NULL)
				hdr->client_explicity_accepts_jp2 = 1;
			else
				hdr->client_explicity_accepts_jp2 = 0;

			continue;	// there's no point in forwarding this header
		}
		
		if (strncasecmp(line,"Keep-Alive:",11) == 0)
				continue;
	
		/* REMOVEME: we do accept ranges now
		 * compression/optimization is limited in such cases though. */
		//hmmm, this should not happen. But we should forward this unchanged
		//instead!
		//if (strncasecmp(line, "Range:", 6) == 0) continue; 

		// define host data if empty
		if (strncasecmp(line,"Host:", 5) == 0) {
			if (*(hdr->host) == '\0') {
				char *provided_host_str;
				char *colonpos;
				
				provided_host_str = line + 5;
				if (*provided_host_str == ' ')
					provided_host_str++;
				
				hdr->host = strdup (provided_host_str);

				// if host contains a defined port, processess that too
				colonpos = strchr (hdr->host, (int) ':');
				if (colonpos != NULL) {
					*colonpos = '\0';
					sscanf (colonpos + 1, "%hu", &hdr->port);
				}
			}
		}

		//get User-Agent
		if (strncasecmp(line, "User-Agent:", 11 ) == 0)
			hdr->user_agent = strdup (line + 11);

		//get content length
		if (strncasecmp(line, "Content-Length:", 15 ) == 0)
			hdr->content_length = ZP_CONVERT_STR_TO_DATASIZE(&(line[15]));

		//can accept gzip?
		else if (strncasecmp(line, "Accept-Encoding:", 16) == 0)
		{
			if (strstr (line + 16, "gzip") != NULL)
				if (DoGzip)
					hdr->flags |= H_WILLGZIP;
			add_header(hdr, line);
			continue;
		}

		if(strncasecmp(line,"Via:",4) == 0){
			if (ServHost != NULL) {
				if((strstr(line,ServHost) != NULL) && (strstr(line, SERVER_NAME) != NULL)) 
						/*prevent infinite recursion*/
					send_error( 503, "Service Unavailable", NULL,
							"Connection refused (based on Via header)." );
				
				if(linelen + strlen(ServHost) + 
					sizeof(SERVER_NAME) + 15 >= MAX_LINELEN) continue;
				
				snprintf(line + linelen , sizeof(line) - linelen, 
						", 1.1 %s (%s)",ServHost ,SERVER_NAME);
				was_via = 1;
				
			}else continue; //forget Via header
		}
		add_header(hdr, line);
		
	}

	if ((ServHost != NULL) && !was_via)
	{
		snprintf(line,sizeof(line), "Via: 1.1 %s (%s)",ServHost , SERVER_NAME);
		add_header(hdr, line);
	}

	if ((AuthMode != AUTH_NONE) && (! was_auth)) {
		debug_log_puts ("Requesting HTTP auth from client");
		send_error( 407, "Proxy Authentication Required", "Proxy-Authenticate: Basic realm=\"internet\"", "You have to be registered before using this proxy.");
	}
}//get_client_headers

/*Does not release the headers ! Rather use special function?*/
void send_headers_to(FILE * sockfp, http_headers *hdr){
	int i, len;
	
	if (0 == hdr->lines) return; //useful for simple response
	
	if (hdr->status == -1){//treat first line specially
		i = 1;
		len = snprintf(line,sizeof(line),"%s %s %s\r\n", hdr->method, hdr->path, hdr->proto);
		fputs(line, sockfp);
		
		line[len-2] = '\0';
		debug_log_puts_hdr (line);
	}else i = 0;
		
	// Forward the remainder of the request from the client
	for(;i < hdr->lines && i<MAX_HEADERS;i++)
	{
		fputs(hdr->hdr[i],sockfp);
		debug_log_puts_hdr (hdr->hdr[i]);
		fputs("\r\n",sockfp);
	}
	fputs("\r\n",sockfp);
	fflush(sockfp);
}

// TODO: we should standardize such defines in only one, global, one.
// This may not be > 2GB
#define STRM_BUFSIZE 16384

// returns forwarded content size
ZP_DATASIZE_TYPE forward_content (http_headers *hdr, FILE *from, FILE *to)
{
	unsigned char stream [STRM_BUFSIZE];
	int inlen = 0;
	int outlen = 0;
	int alarmcount = 0;
	ZP_DATASIZE_TYPE remainlen = hdr->content_length; // if == -1 then content-length is not provided by client: so we won't rely on size checking.
	int requestlen = STRM_BUFSIZE;

	while ((remainlen != 0) && (feof (from) == 0) && (ferror (from) == 0) && (ferror (to) == 0) && (feof (to) == 0)) {
		if (remainlen >= 0) {
			if (remainlen < STRM_BUFSIZE)
				requestlen = remainlen;
			remainlen -= requestlen;
		}
			
		inlen = fread (stream, 1, requestlen, from);
		tosmarking_add_check_bytecount (inlen);
		access_log_add_inlen(inlen);
		alarmcount += inlen;

		// If we are sending a big file down a slow line, we
		// need to reset the alarm once a while.
		if ((ConnTimeout) && (alarmcount > 10000)) {
			alarmcount = 0;
			alarm (ConnTimeout);
		}
		
		if (inlen > 0) {
			outlen = fwrite (stream, 1, inlen, to);
			access_log_add_outlen(outlen);
		}

		// If we are sending a big file down a slow line, we
		// need to reset the alarm once a while.
		if ((ConnTimeout) && (alarmcount > 10000)) {
			alarmcount = 0;
			alarm (ConnTimeout);
		}
	}

	fflush (to);
	
	return (access_log_ret_outlen());
}

#define BUF_ALLOCATION_GRANULARITY 65536

// get data and store for compression
/* FIXME: This routine does not support data > 2GB (limited to int) */
ZP_DATASIZE_TYPE read_content (http_headers *hdr, FILE *from, FILE *to, char ** inbuf, ZP_DATASIZE_TYPE *inlen)
{
	char * buf;
	int buf_alloc;
	int buf_used = 0;
	int pending_chunk_len = 0;
	int block_read;
	int to_read_len;
	int first_chunk = 1;
	int de_chunk = 0;
	int stream_instead = 0;	// != 0 if streaming data exceeded MaxSize
	int streamed_len = 0;
	
	/* see if we can allocate a buffer the exact size we need */
        if (hdr->content_length == -1)
		buf_alloc = BUF_ALLOCATION_GRANULARITY;
	else
		buf_alloc = hdr->content_length + 1;	/* add 1 character for safety */
	if ((buf = malloc (buf_alloc)) == NULL) {
		debug_log_puts ("ERROR: Unable to allocate memory.");
		exit;
	}

        // unchunk if needed
	if (hdr->where_chunked > 0) {
		remove_header (hdr, hdr->where_chunked);
		hdr->where_chunked = -1;
		de_chunk = 1;
		debug_log_puts ("Chunked data. De-chunking while loading into memory.");
	}		
	
	while ((feof (from) == 0) && (ferror (from) == 0) && (feof (to) == 0) && (ferror (to) == 0)) {
		to_read_len = (buf_alloc - buf_used);
		
		if (de_chunk) {
			if (pending_chunk_len == 0) {
				// discards chunk end CRLF
				if (first_chunk == 0) {
					fgetc (from);
					fgetc (from);
				} else {
					first_chunk = 0;
				}
				
				fscanf (from, "%x", &pending_chunk_len);
				if (pending_chunk_len != 0) {
					char prevchar = '\0';
					char curchar = '\0';

					// Eat any chunk-extension(RFC2616) up to CRLF.
					while (! ((prevchar == '\r') && (curchar == '\n'))) {
						prevchar = curchar;
						curchar = fgetc (from);
					}
				} else {
					// the rest of source will be discarded, nothing to do here
				}
				feof (from);
			}

			if (pending_chunk_len < (buf_alloc - buf_used))
				to_read_len = pending_chunk_len;
			pending_chunk_len -= to_read_len;
		}

		block_read = fread (buf + buf_used, 1, to_read_len, from);
		buf_used += block_read;

		/* we are streaming instead of trying to load into memory */
		if (stream_instead != 0) {
			// If we are sending a big file down a slow line, we
			// need to reset the alarm once a while.
			if (ConnTimeout)
				alarm(ConnTimeout);

			tosmarking_add_check_bytecount (buf_used); /* update TOS, if necessary */
			fwrite (buf, 1, buf_used, to);
			streamed_len += buf_used;

			access_log_def_inlen(streamed_len);
			access_log_def_outlen(streamed_len);
		
			buf_used = 0; // avoids indefinite growth of buffer size (see below)
		}

		if (buf_alloc == buf_used) {
			if ((buf = realloc (buf, buf_alloc + BUF_ALLOCATION_GRANULARITY)) == NULL) {
				debug_log_puts ("ERROR: Unable to allocate memory.");
				exit;
			}
			buf_alloc += BUF_ALLOCATION_GRANULARITY;
		}

		if ((MaxSize > 0) && (buf_used > MaxSize) && (stream_instead == 0)) {
			/* we've just detected that the streaming data exceeded MaxSize, plan B now */
			stream_instead = 1;

			/* dump headers.. */
			add_header (hdr, "Connection: close");
			add_header (hdr, "Proxy-Connection: close");
			send_headers_to (to, hdr);

			/* ensure that buffer will have at least STRM_BUFSIZE bytes, since
			 * we want to avoid extra processing using a small buffer */
			if (buf_alloc < STRM_BUFSIZE) {
				if ((buf = realloc (buf, STRM_BUFSIZE)) == NULL) {
					debug_log_puts ("ERROR: Unable to allocate memory.");
					exit;
				}
				buf_alloc = STRM_BUFSIZE;
			}
			
			// perhaps we've already spent too much time downloading that file,
			// let's reset the timeout alarm
			if (ConnTimeout)
				alarm(ConnTimeout);

			fwrite (buf, 1, buf_used, to);
			streamed_len += buf_used;
			buf_used = 0;
		}
	}

	/* we already streamed the data, let's close the shop */
	if (stream_instead != 0) {
		fflush (to);
		return (streamed_len);
	}
	
	/* avoids possible bugs in htmlopt, make sure the data has a trailing '\0' */
	if (buf_alloc == buf_used) {
		if ((buf = realloc (buf, buf_alloc + 1)) == NULL) {
			debug_log_puts ("ERROR: Unable to allocate memory.");
			exit;
		}
		buf_alloc += 1;
	}
	*(buf + buf_used) = '\0'; // doesn't count as part of the file
	
        if (hdr->content_length == -1)
		*inlen = buf_used;
	else
		*inlen = hdr->content_length;
	
	*inbuf = buf;
	*inlen = buf_used;

	return (0);
}

http_headers * get_response_headers(FILE *sockrfp){
	http_headers *hdr = new_headers();
	int n, linelen, *savepos;
	char *tempp;

	// Process the first few characters to see if it's an HTTP/1.0
	// simple response i.e. a response without any header
	// lines.

	// FIXME: this variable should NOT be global
	line [0] = '\0';

	// read the first 4 characters
	fgets(line, 5, sockrfp);
	n = strlen(line);
	if (0 == n){
		send_error(500, "Server error", NULL, "Empty response from server");
	}

	// If no HTTP nor ICE (icecast) response, assumes HTTP/0.9 simple response.
	if (n < 4 || (strncasecmp(line, "HTTP", 4) && strncasecmp(line, "ICY", 3))) {
		hdr->flags |= H_SIMPLE_RESPONSE; // It's a simple response.
		// Check for image signatures
		hdr->type = detect_type(line, 4);
		hdr->lines = 0;
		//cache the line here
		hdr->hdr[0] = strdup(line);

		debug_log_puts ("Received HTTP/0.9 Simple Response.");
		// We're passing along all other types of data.
		// TODO: Handle other compressable types.
		return hdr;
	}

	// read the rest of the first line
	fgets(line+n, sizeof(line), sockrfp);

	//get header
	do{
		if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
			break;

		if ((linelen = check_trim(line)) == 0)
				send_error(500,"Internal Error",NULL,
				"Too long line in response from server");
		
		debug_log_puts_hdr (line);
		clean_hdr(line);
		savepos = NULL;

		//get content length
		if (strncasecmp(line, "Content-Length:", 15) == 0)
		{
			hdr->content_length = ZP_CONVERT_STR_TO_DATASIZE(&(line[15]));
			savepos = &hdr->where_content_length;
		}
		else if (strncasecmp(line, "Content-Encoding:", 17) == 0) {
			// note: the string itself (hdr->content_encoding) will be stored later
			savepos = &hdr->where_content_encoding;
		}

		else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0)
		{//check for chunking
			tempp = line + 19;
			if (strncasecmp(tempp, "chunked", 7) == 0)
				savepos = &hdr->where_chunked;
		}

		else if (strncasecmp(line, "Content-Type:", 13) == 0)
			savepos = &hdr->where_content_type;
	
		/* REMOVEME: we do accept ranges now
		 * compression/optimization is limited in such cases though. */
		//else if (strncasecmp(line, "Accept-Ranges:", 14) == 0)
		//		strcpy(&line[14], " none");
		else if (strncasecmp(line, "Content-range:", 14) == 0)
			hdr->has_content_range = 1;
				
		else if (strncasecmp(line, "ETag:", 5) == 0)
			savepos = &hdr->where_etag;

		else if ((strncasecmp(line, "HTTP/1.1", 8) == 0) ||
			(strncasecmp(line, "HTTP/1.0", 8) == 0) ||
			(strncasecmp(line, "ICY", 3) == 0))
			hdr->status = atoi(&line[8]);
		else {
			tempp = NULL;

			if(!strncasecmp(line, "Connection:", 11))
					tempp = line + 11;
			else if(!strncasecmp(line, "Proxy-Connection:", 17))
					tempp = line + 17;
			else if(!strncasecmp(line,"Keep-Alive:",11))
					tempp = line;
			
			if((tempp != NULL) && (strstr(tempp, "Keep-Alive") != NULL))
				hdr->flags |= H_KEEPALIVE;
		}

		//store header entry, except certain ones
		if(strncasecmp(line, "Connection:", 11) && 
		 strncasecmp(line, "Proxy-Connection:", 17) &&
		 strncasecmp(line,"Keep-Alive:",11)){
			n = add_header(hdr,line);
			if (savepos != NULL) *savepos = n;
			if (0 == n) hdr->proto = hdr->hdr[0]; 
		}

	} while (fgets(line, sizeof(line), sockrfp) != 0);

	//store pending-to-be-stored string data
	//(pointers-only, uses data stored in header structure,
	//thus it can only be stored at this stage)
	if (hdr->where_content_encoding > 0) {
		hdr->content_encoding = find_header ("Content-Encoding:", hdr);
		hdr->content_encoding_flags = return_content_encoding (hdr);
	}

	if (hdr->where_content_type > 0) {
		hdr->content_type = strdup (find_header ("Content-Type:", hdr));
		/* ignore anything past the ';' (charset=xxxx etc) */
		if ((tempp = strchr (hdr->content_type, ';')) != NULL)
			*tempp = '\0';
		misc_cleanup_string (hdr->content_type, hdr->content_type);
	}

	return hdr;
}

/* returns the properties in Content-Encoding field
 * appliable only to server response */
/* FIXME: currently it only reports: gzip, deflate, compress
 * (does not report other encodings, which may render garbled data if such are used)
 * TODO: if other (unknown) encodings are present, it should return PROP_ENCODED_UNKNOWN */
int return_content_encoding(http_headers *shdr){
	int content_encoding = PROP_ENCODED_NONE;
	
	if (shdr->where_content_encoding > 0) {
		if (strstr (shdr->content_encoding, "gzip") != NULL)
			content_encoding |= PROP_ENCODED_GZIP;
		if (strstr (shdr->content_encoding, "deflate") != NULL)
			content_encoding |= PROP_ENCODED_DEFLATE;
		if (strstr (shdr->content_encoding, "compress") != NULL)
			content_encoding |= PROP_ENCODED_COMPRESS;

		/* kludgy workaround for buggy sites which send character set information
		   in the Content-Encoding field (which violates RFC 2616) */
		if (strstr (shdr->content_encoding, "ISO-8859-1") != NULL)
			content_encoding |= PROP_ENCODED_UNKNOWN;
	}

	return (content_encoding);
}

/*
 * chdr - headers of client request
 * shdr - headers of server response
 */
void decide_what_to_do(http_headers *chdr, http_headers *shdr){
	char *tempp;

	shdr->type = OTHER_CONTENT;
	shdr->flags &= ~DO_COMPRESS;
	shdr->flags &= ~DO_PRE_DECOMPRESS;
	
	if(-1 == shdr->where_content_type) return; 

	/* is this something we can losslessly compress? */
	if (ct_check_if_matches (lossless_compress_ct, shdr->content_type) != 0) {
		if (DoGzip)
			shdr->flags |= DO_COMPRESS;
	}

	tempp = shdr->hdr[shdr->where_content_type] + 14;
	if (!strncasecmp(tempp,"text/html", 9)) {
		shdr->type = TEXT_HTML;
	} else if (!strncasecmp(tempp,"text/css", 8)) {
		shdr->type = TEXT_CSS;
	} else if (!strncasecmp(tempp, "image/jpeg", 10) ||
		!strncasecmp(tempp, "image/pjpeg", 11) ||
		!strncasecmp(tempp, "image/jpg", 9) ||
		!strncasecmp(tempp, "image/pjpg", 10) 
		) {
		shdr->type = IMG_JPEG;
		if (ProcessJPG)
			shdr->flags |= DO_RECOMPRESS_PICTURE;
#ifdef JP2K
	} else if (!strncasecmp(tempp, "image/jp2", 9)) {
		shdr->type = IMG_JP2K;
		if (ProcessJP2)
			shdr->flags |= DO_RECOMPRESS_PICTURE;
#endif
	} else if (!strncasecmp(tempp, "image/gif", 9)) {
		shdr->type = IMG_GIF;
		if (ProcessGIF)
			shdr->flags |= DO_RECOMPRESS_PICTURE;
	} else if (!strncasecmp(tempp, "image/png", 9)) {
		shdr->type = IMG_PNG;
		if (ProcessPNG)
			shdr->flags |= DO_RECOMPRESS_PICTURE;
	} else if ((!strncasecmp(tempp,"application/x-javascript", 24)) || (!strncasecmp(tempp,"application/javascript", 22)) || (!strncasecmp(tempp,"text/javascript", 15))) {
		shdr->type = APPLICATION_JAVASCRIPT;
	}

	if ((ProcessHTML) && (shdr->type == TEXT_HTML))
		shdr->flags |= DO_OPTIMIZE_HTML;

	if ((ProcessCSS) && (shdr->type == TEXT_CSS))
		shdr->flags |= DO_OPTIMIZE_CSS;       

	if ((ProcessJS) && (shdr->type == APPLICATION_JAVASCRIPT))
		shdr->flags |= DO_OPTIMIZE_JS;

	if ((PreemptNameRes) && (shdr->type == TEXT_HTML))
			shdr->flags |= DO_PREEMPT_DNS;

	/* is the incoming data gzipped (and _only_ gzipped) ?
	 * if so, should we decompress that before further processing? */
	if (shdr->content_encoding_flags == PROP_ENCODED_GZIP) {
		if (DecompressIncomingGzipData)
			shdr->flags |= DO_PRE_DECOMPRESS;
	}
	
	/* 
	 * From this point, manage flags only to clear DO_* bits
	 */

	/* don't gzip if browser doesn't accept gzip */
	if (! (chdr->flags & H_WILLGZIP))
		shdr->flags &= ~DO_COMPRESS;

	/* Send partial-data requests, if there are no potential problems with data consistency.
	 * If possible to honour the request, treat the data as a black-box, since it's partial data. */
	/* FIXME: if reprocessing JP2K is implemented in the future, it _must_ have an entry here aswell */
	if (shdr->has_content_range) {
		if ((shdr->flags & META_CONTENT_MODIFICATION) != DO_NOTHING) {
			send_error (409, "Conflict", NULL, "Client has requested partial content for a dynamically-optimized Content-Type.");
		} else {
			debug_log_puts ("Content-Range provided (partial data). Disabling PreemptDNS.");
			shdr->flags &= ~DO_PREEMPT_DNS;
		}
	}
	
	/* Workaround for MSIE's pseudo-feature "Show friendly HTTP error messages."
	 * Don't optimize the body of this in any way, since it could go down below 256 or 512 bytes
	 * and be replaced with a local error message instead.
	 * In certain cases the body has crucial data, like a HTML redirection or so, and
	 * that would be broken if a "friendly error" replaces it. */
	if ((WA_MSIE_FriendlyErrMsgs) && (shdr->status >= 400) && (shdr->status < 600) && (chdr->user_agent != NULL)) {
		if (strstr (chdr->user_agent, "; MSIE ") != NULL) {
			debug_log_puts ("BUG WORKAROUND APPLIED: MSIE friendly error messages");
			shdr->flags &= ~META_CONTENT_MODIFICATION;
		}
	}
	
}

//Remove extra whitespace that may prevent correct parsing.
void clean_hdr(char* ln){
	int spaces = 0;
	char * colon, *i;
	
	colon = strchr(ln,':');
	if (colon == NULL) return;
	for(i = colon + 1; *i != '\0'; i++){
		if(' ' == *i )
			spaces++;
		else break;//*i is now first char after space(s)
	}
	if (spaces > 1) memmove(colon+2,i,strlen(ln)-(colon-ln) - 2);
}

/*
 * transform the request location from HTTP-host (path) to proxy format (full URL), for internal Ziproxy's usage
 * it's used when processing requests as transparent proxy
 * -- requires get_response_headers() being called previously
 */
void fix_request_url (http_headers *hdr) {
	char *new_url;
	
	if (*(hdr->host) == '\0')
		send_error (400, "Bad Request", NULL, "Malformed request or non-HTTP/1.1 compliant.");

	new_url = malloc (strlen (hdr->url) + strlen (hdr->host) + 7 + 1);
	sprintf (new_url, "http://%s%s", hdr->host, hdr->url);
	hdr->url = new_url;

	if (NextProxy != NULL) hdr->path = hdr->url;
}

