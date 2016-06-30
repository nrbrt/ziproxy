/* text.c
 * HTML modification, text compression fuctions.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> //for off_t
#include <zlib.h>
#include <assert.h>

#include "http.h"
#include "cfgfile.h"
#include "image.h"
#include "log.h"
#include "gzpipe.h"

#define CHUNKSIZE 4050
#define GUNZIP_BUFF 16384

ZP_DATASIZE_TYPE gunzip (char* inbuf, ZP_DATASIZE_TYPE inlen, char** outbuf, ZP_DATASIZE_TYPE *outlen, int max_growth);

enum {ONormal,OChunked, OStream, OGzipStream};

//TODO correct return value, print status into logs
/* zlib compress streaming from 'from' to 'to' */
/* returns: --> result of gzip_stream_stream() */
/* inlen and outlen will be written with the uncompressed and compressed sizes respectively */
int do_compress_stream_stream (http_headers *hdr, FILE *from, FILE *to, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen){
	int status;
	int de_chunk = 0;

	/* if http body is chunked, de-chunk it while compressing */
	if (hdr->where_chunked > 0) {
		remove_header(hdr, hdr->where_chunked);
		de_chunk = 1;
	}
	
	/* previous content-length is invalid, discard it */
	hdr->where_content_length = -1;
	remove_header_str(hdr, "Content-Length");

	add_header(hdr, "Content-Encoding: gzip");
	add_header(hdr, "Connection: close");
	add_header(hdr, "Proxy-Connection: close");

	debug_log_puts ("Gzip stream-to-stream. Out Headers:");
	send_headers_to(to, hdr);
	fflush(to);
	
	status = gzip_stream_stream(from, to, Z_BEST_COMPRESSION, inlen, outlen, de_chunk);
	fflush(to);

	debug_log_difftime ("Compression+streaming");

	return (status);
}

//TODO correct return value, print status into logs
/* similar to do_compress_stream_stream() but decompress instead */
int do_decompress_stream_stream (http_headers *hdr, FILE *from, FILE *to, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen, int max_ratio, ZP_DATASIZE_TYPE min_eval){
	int status;
	int de_chunk = 0;

	/* if http body is chunked, de-chunk it while decompressing */
	if (hdr->where_chunked > 0) {
		remove_header(hdr, hdr->where_chunked);
		de_chunk = 1;
	}
	
	/* no longer gzipped, modify headers accordingly */
	hdr->content_encoding_flags = PROP_ENCODED_NONE;
	hdr->content_encoding = NULL;
	hdr->where_content_encoding = -1;
	remove_header_str(hdr, "Content-Encoding");

	/* previous content-length is invalid, discard it */
	hdr->where_content_length = -1;
	remove_header_str(hdr, "Content-Length");

	add_header(hdr, "Connection: close");
	add_header(hdr, "Proxy-Connection: close");
	
	debug_log_puts ("Gunzip stream-to-stream. Out Headers:");
	send_headers_to(to, hdr);
	fflush(to);
	
	status = gunzip_stream_stream(from, to, inlen, outlen, de_chunk, max_ratio, min_eval);
	fflush(to);

	debug_log_difftime ("Decompression+streaming");

	return (status);
}

int do_compress_memory_stream (http_headers *hdr, const char *from, FILE *to, const ZP_DATASIZE_TYPE inlen, ZP_DATASIZE_TYPE *outlen){
	int status;
	int de_chunk = 0;

	/* if http body is chunked, de-chunk it while compressing */
	if (hdr->where_chunked > 0) {
		remove_header(hdr, hdr->where_chunked);
		de_chunk = 1;
	}
	
	add_header(hdr, "Content-Encoding: gzip");
	add_header(hdr, "Connection: close");
	add_header(hdr, "Proxy-Connection: close");
	
	debug_log_puts ("Gzip memory-to-stream. Out Headers:");
	remove_header(hdr, hdr->where_content_length);
        hdr->where_content_length=-1;
	send_headers_to(to, hdr);
	fflush(to);
	
	status = gzip_memory_stream(from, to, Z_BEST_COMPRESSION, inlen, outlen);
	fflush(to);

	debug_log_difftime ("Compression+streaming");

	return (status);
}



/* FIXME; kludgy unpacker, rewrite that in a more elegant way */
/* *outbuf must be NULL or an allocated memory address
 * max_growth (in %) is the maximum allowable uncompressed size relative
 * 	to inlen, if exceeded the compressor will stop and no data will be written in outbuf
 * 	if max_growth==0 then there will be no limit (other than memory) */
/* FIXME: This routine does not support data > 2GB (limited to int) */
ZP_DATASIZE_TYPE gunzip (char* inbuf, ZP_DATASIZE_TYPE inlen, char** outbuf, ZP_DATASIZE_TYPE *outlen, int max_growth)
{
		int filedes, filedes_unpack;
		FILE *file_pack, *file_unpack;
		char filenam[] = "/tmp/ziproxy_XXXXXX";
		char filenam_unpack[] = "/tmp/ziproxy_XXXXXX";
		gzFile gzfile = Z_NULL;
		char buff_unpack[GUNZIP_BUFF];
		int len_unpack_block;
		ZP_DATASIZE_TYPE max_outlen;

		max_outlen = (inlen * max_growth) / 100;
		*outlen = 0;
		
		if((filedes = mkstemp(filenam)) < 0) return 10;
		unlink(filenam);
		if((filedes_unpack = mkstemp(filenam_unpack)) < 0) return 10;
		unlink(filenam_unpack);

		if ((file_pack = fdopen(filedes, "w+")) == NULL) return 20;
		if ((file_unpack = fdopen(filedes_unpack, "w+")) == NULL) return 20;
		
		/* dump packed data into the file */
		fwrite(inbuf, inlen, 1, file_pack);
		fseek(file_pack, 0, SEEK_SET);

		/* proceed with unpacking data into another file */
		if((gzfile = gzdopen(dup(filedes), "rb")) == Z_NULL) return 20;
		while ((len_unpack_block = gzread(gzfile, buff_unpack, GUNZIP_BUFF)) > 0) {
			*outlen += len_unpack_block;
			if ((max_growth != 0) && (*outlen > max_outlen))
				return 100;
			fwrite(buff_unpack, len_unpack_block, 1, file_unpack);
		}

		/* a smaller decompressed file is not necessarily result of broken gzip data */
		//if (*outlen < inlen)
		//	return 120;

		gzclose(gzfile);
		fclose(file_pack);

		/* load unpacked data to the memory */
		if ((*outbuf=realloc(*outbuf, *outlen)) != NULL) {
			fseek(file_unpack, 0, SEEK_SET);
			fread(*outbuf, *outlen, 1, file_unpack);
		}
		fclose(file_unpack);
		
		return 0;
}

/* unpacks gzipped data in inoutbuf, reallocs inoutbuf and dumps
 * unpacked data into the same inoutbuf
 * returns: new size of data,
 * 	of negative value if error, value varies according to the error
 * 	(in this case, inoutbuff is unchanged)
 * max_growth works in a similar way as gunzip()		 */
/* FIXME: This routine does not support data > 2GB (limited to int) */
ZP_DATASIZE_TYPE replace_gzipped_with_gunzipped (char **inoutbuf, ZP_DATASIZE_TYPE inlen, int max_growth)
{
	ZP_DATASIZE_TYPE outlen;
	ZP_DATASIZE_TYPE retcode;
	char *temp_inoutbuf = *inoutbuf;

	retcode = gunzip(*inoutbuf, inlen, &temp_inoutbuf, &outlen, max_growth);
	if (retcode == 0) {
		*inoutbuf = temp_inoutbuf;
		return (outlen);
	} else {
		return (retcode * -1);
	}
}

