/* gzpipe.c
 * zlib pipe-pipe routines
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

/* those routines were based on zpipe.c: */
/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>

#include "gzpipe.h"
#include "cfgfile.h"
#include "log.h"
#include "tosmarking.h"
#include "globaldefs.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define BUFSIZE 16384

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int gzip_stream_stream (FILE *source, FILE *dest, int level, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen, int de_chunk)
{
	int ret, flush;
	unsigned have;
	z_stream strm;
	unsigned char in [BUFSIZE];
	unsigned char out [BUFSIZE];
	unsigned char gzip_header[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
	unsigned char gzip_footer[8];
	uLong crc;
	unsigned int last_write_bytes; // 'last_read_bytes' is 'strm.avail_in', so no need for a new variable
	int pending_chunk_len = 0;
	int to_read_len = BUFSIZE;
	int first_chunk = 1;

	*inlen = 0;
	*outlen = 0;
	
	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	// ret = deflateInit(&strm, level);
	ret = deflateInit2 (&strm, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK)
		return (ret);

	/* new block started, send gzip header */
	*outlen += fwrite (&gzip_header, 1, 10, dest); // gzip header
	crc = crc32 (0L, Z_NULL, 0);

	/* compress until end of file */
	do {
		if (de_chunk) {
			if (pending_chunk_len == 0) {
				// discards chunk end CRLF
				if (first_chunk == 0) {
					fgetc (source);
					fgetc (source);
				} else {
					first_chunk = 0;
				}
				
				fscanf (source, "%x", &pending_chunk_len);
				if (pending_chunk_len != 0) {
					char prevchar = '\0';
					char curchar = '\0';

					// Eat any chunk-extension(RFC2616) up to CRLF.
					while (! ((prevchar == '\r') && (curchar == '\n'))) {
						prevchar = curchar;
						curchar = fgetc (source);
					}
				} else {
					// the rest of source will be discarded, nothing to do here
				}
				feof (source);
			}

			if (pending_chunk_len > BUFSIZE)
				to_read_len = BUFSIZE;
			else
				to_read_len = pending_chunk_len;
			pending_chunk_len -= to_read_len;
		}
			
		strm.avail_in = fread (in, 1, to_read_len, source);
		*inlen += strm.avail_in;
		crc = crc32(crc, in, strm.avail_in);

		/* update access log stats */
		access_log_def_inlen(*inlen);

		if (ferror(source)) {
			(void)deflateEnd(&strm);
			debug_log_puts ("stream gzip: IO error (source). Aborting.");
			return (Z_ERRNO);
		}
		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		/* run deflate() on input until output buffer not full, finish
		   compression if all of source has been read in */
		do {
			strm.avail_out = BUFSIZE;
			strm.next_out = out;
			ret = deflate(&strm, flush);    /* no bad return value */
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			have = BUFSIZE - strm.avail_out;
			tosmarking_add_check_bytecount (have);	/* update TOS if necessary */
			if ((last_write_bytes = fwrite(out, 1, have, dest)) != have || ferror(dest)) {
				(void)deflateEnd(&strm);
				*outlen += last_write_bytes;
				debug_log_puts ("stream gzip: IO error (dest). Aborting.");
				return (Z_ERRNO);
			}
			*outlen += last_write_bytes;

			/* update access log stats */
			access_log_def_outlen(*outlen);
			
		} while (strm.avail_out == 0);
		assert(strm.avail_in == 0);     /* all input will be used */

		// If we are sending a big file down a slow line, we
		// need to reset the alarm once a while.
		if (ConnTimeout)
			alarm(ConnTimeout);
	
		/* block end, send gzip footer */
		if (flush == Z_FINISH) {
			gzip_footer[0] = crc & 0xff;
			gzip_footer[1] = (crc >> 8) & 0xff;
			gzip_footer[2] = (crc >> 16) & 0xff;
			gzip_footer[3] = (crc >> 24) & 0xff;
			gzip_footer[4] = *inlen & 0xff;
			gzip_footer[5] = (*inlen >> 8) & 0xff;
			gzip_footer[6] = (*inlen >> 16) & 0xff;
			gzip_footer[7] = (*inlen >> 24) & 0xff;
			*outlen += fwrite(&gzip_footer, 1, 8, dest); // gzip footer
		}
	
		/* done when last data in file processed */
	} while (flush != Z_FINISH);
	assert(ret == Z_STREAM_END);        /* stream will be complete */

	/* clean up and return */
	(void)deflateEnd(&strm);
	return (Z_OK);
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int gunzip_stream_stream (FILE *source, FILE *dest, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen, int de_chunk, int max_ratio, ZP_DATASIZE_TYPE min_eval)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in [BUFSIZE];
	unsigned char out [BUFSIZE];
	unsigned int last_write_bytes; // 'last_read_bytes' is 'strm.avail_in', so no need for a new variable
	int pending_chunk_len = 0;
	int to_read_len = BUFSIZE;
	int first_chunk = 1;

	*inlen = 0;
	*outlen = 0;
    
	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	//ret = inflateInit(&strm);
	ret = inflateInit2 (&strm, 16 + MAX_WBITS);
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	do {
		if (de_chunk) {
			if (pending_chunk_len == 0) {
				// discards chunk end CRLF
				if (first_chunk == 0) {
					fgetc (source);
					fgetc (source);
				} else {
					first_chunk = 0;
				}
				
				fscanf (source, "%x", &pending_chunk_len);
				if (pending_chunk_len != 0) {
					char prevchar = '\0';
					char curchar = '\0';

					// Eat any chunk-extension(RFC2616) up to CRLF.
					while (! ((prevchar == '\r') && (curchar == '\n'))) {
						prevchar = curchar;
						curchar = fgetc (source);
					}
				} else {
					// the rest of source will be discarded, nothing to do here
				}
				feof (source);
			}

			if (pending_chunk_len > BUFSIZE)
				to_read_len = BUFSIZE;
			else
				to_read_len = pending_chunk_len;
			pending_chunk_len -= to_read_len;
		}

		strm.avail_in = fread(in, 1, to_read_len, source);
		*inlen += strm.avail_in;

		/* update access log stats */
		access_log_def_inlen(*inlen);

		if (ferror(source)) {
			(void)inflateEnd(&strm);
			debug_log_puts ("stream gunzip: IO error (source). Aborting.");
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = BUFSIZE;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = BUFSIZE - strm.avail_out;
			tosmarking_add_check_bytecount (have);	/* update TOS if necessary */
			if ((last_write_bytes = fwrite(out, 1, have, dest)) != have || ferror(dest)) {
				*outlen += last_write_bytes;
				(void)inflateEnd(&strm);
				debug_log_puts ("stream gunzip: IO error (dest). Aborting.");
				return Z_ERRNO;
			}
			*outlen += last_write_bytes;

			/* update access log stats */
			access_log_def_outlen(*outlen);

			/* evaluate whether decompression rate is exceeded */
			if ((max_ratio != 0) && (*outlen >= min_eval)) {
				if (((*inlen * max_ratio) / 100) < *outlen) {
					/* ratio is exceeded, abort decompression and streaming */
					access_log_set_flags (LOG_AC_FLAG_LLCOMP_TOO_EXPANSIVE);
					debug_log_puts ("stream gunzip: Decompression ratio exceeded. Aborting.");
					return Z_ERRNO;
				}
			}
			
		} while (strm.avail_out == 0);

		// If we are sending a big file down a slow line, we
		// need to reset the alarm once a while.
		if (ConnTimeout)
			alarm(ConnTimeout);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int gzip_memory_stream (const char *source, FILE *dest, int level, ZP_DATASIZE_TYPE inlen, ZP_DATASIZE_TYPE *outlen)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char out [BUFSIZE];
	unsigned char gzip_header[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
	unsigned char gzip_footer[8];
	uLong crc;
	unsigned int last_write_bytes; // 'last_read_bytes' is 'strm.avail_in', so no need for a new variable
	
	*outlen = 0;
	
	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	// ret = deflateInit(&strm, level);
	ret = deflateInit2 (&strm, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
	if (ret != Z_OK)
		return (ret);

	/* new block started, send gzip header */
	*outlen += fwrite (&gzip_header, 1, 10, dest); // gzip header
	crc = crc32 (0L, Z_NULL, 0);

	strm.avail_in = inlen;
	strm.next_in = source;
	crc = crc32(crc, source, inlen);
		
	/* run deflate() on input until output buffer not full, finish
	   compression if all of source has been read in */
	do {
		strm.avail_out = BUFSIZE;
		strm.next_out = out;
		ret = deflate(&strm, Z_FINISH);    /* no bad return value */
		assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		have = BUFSIZE - strm.avail_out;
		tosmarking_add_check_bytecount (have);	/* update TOS if necessary */
		if ((last_write_bytes = fwrite(out, 1, have, dest)) != have || ferror(dest)) {
			(void)deflateEnd(&strm);
			*outlen += last_write_bytes;
			return (Z_ERRNO);
		}
		*outlen += last_write_bytes;

		/* update access log stats */
		access_log_def_outlen(*outlen);

		// If we are sending a big file down a slow line, we
		// need to reset the alarm once a while.
		if (ConnTimeout)
			alarm(ConnTimeout);
		
	} while (strm.avail_out == 0);
	assert(strm.avail_in == 0);     /* all input will be used */

	/* done when last data in file processed */
	assert(ret == Z_STREAM_END);        /* stream will be complete */

	/* block end, send gzip footer */
	gzip_footer[0] = crc & 0xff;
	gzip_footer[1] = (crc >> 8) & 0xff;
	gzip_footer[2] = (crc >> 16) & 0xff;
	gzip_footer[3] = (crc >> 24) & 0xff;
	gzip_footer[4] = inlen & 0xff;
	gzip_footer[5] = (inlen >> 8) & 0xff;
	gzip_footer[6] = (inlen >> 16) & 0xff;
	gzip_footer[7] = (inlen >> 24) & 0xff;
	*outlen += fwrite(&gzip_footer, 1, 8, dest); // gzip footer

	/* clean up and return */
	(void)deflateEnd(&strm);
	return (Z_OK);
}


