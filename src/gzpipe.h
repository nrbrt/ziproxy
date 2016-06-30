/* gzpipe.h
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


//To stop multiple inclusions.
#ifndef SRC_ZPIPE_H
#define SRC_ZPIPE_H

/* Use faster (thread-unsafe) variants of getc() and putc(), if possible. */
#ifdef getc_unlockedXXXX
#define fastgetc(x) getc_unlocked(x)
#else
#define fastgetc(x) getc(x)
#endif

#ifdef putc_unlocked
#define fastputc(x,y) putc_unlocked(x,y)
#else
#define fastputc(x,y) putc(x,y)
#endif

#include "globaldefs.h"

int gzip_stream_stream (FILE *source, FILE *dest, int level, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen, int de_chunk);
int gunzip_stream_stream (FILE *source, FILE *dest, ZP_DATASIZE_TYPE *inlen, ZP_DATASIZE_TYPE *outlen, int de_chunk, int max_ratio, ZP_DATASIZE_TYPE min_eval);
int gzip_memory_stream (const char *source, FILE *dest, int level, ZP_DATASIZE_TYPE inlen, ZP_DATASIZE_TYPE *outlen);

#endif //SRC_ZPIPE_H

