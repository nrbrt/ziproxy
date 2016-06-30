/* image.h
 * Image reading/writing wrappers.
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

#include "http.h"

//If this is included in image.c, make declarations local.
#ifdef SRC_IMAGE_C
#define EXTERN
#else
#define EXTERN extern
#endif 

//To stop multiple inclusions.
#ifndef SRC_IMAGE_H
#define SRC_IMAGE_H

#include "globaldefs.h"

enum enum_color_space {CENC_RGB, CENC_YUV}; // only applies to color images, irrelevant to mono (LUMA) images
#define t_color_space enum enum_color_space
enum enum_original_ct {OCT_UNDEFINED, OCT_PALETTE, OCT_GRAY, OCT_RGB, OCT_YUV}; // original color type (does not consider alpha)
#define t_original_ct enum enum_original_ct

#define I_PALETTE (1<<1)
#define I_TRANSPARENT (1<<2)

//In case flags contains this, look for error code?.
#define I_ERROR (1<<15)

#define IMG_UNIQUE_RET_MASK 0x0000ffff

#define IMG_RET_OK 0
#define IMG_RET_ERR_BAD_DATA_FORMAT 1
#define IMG_RET_ERR_OUT_OF_MEM 2
#define IMG_RET_ERR_UNKNOWN 3
#define IMG_RET_ERR_NOT_IMPL_YET 4
#define IMG_RET_ERR_POSSIBLE_LOOK_CHANGE 5
#define IMG_RET_TOO_SMALL 32
#define IMG_RET_ERR_OTHER 33
#define IMG_RET_TOO_EXPANSIVE 34
#define IMG_RET_NO_AVAIL_TARGET 35
#define IMG_RET_TOO_BIG 36
#define IMG_RET_SOFTWARE_BUG 37
#define IMG_RET_FLG_WHILE_DECOMP (1<<16)
#define IMG_RET_FLG_WHILE_TRANSFORM (1<<17)
#define IMG_RET_FLG_WHILE_COMPRESS (1<<18)

/* Components of raster/palette entries are known from bpp:
 * 1 -> Grayscale (Y)
 * 2 -> Y + Alpha (255 = opaque 0 = transparent)
 * 3 -> RGB (may contain YUV version in alternate buffer)
 * 4 -> RGB (may contain YUV version in alternate buffer) + Alpha
 */

typedef struct {
	unsigned char * raster;	//For paletted images
	unsigned char * palette;
	unsigned char * bitmap;	//For grayscale/truecolor RGB images (always present and the default bitmap)
	unsigned char * bitmap_yuv; //For truecolor YUV images (allocated on demand)
	int pal_entries;
	int width, height;
	int pal_bpp, bpp;
	int o_depth_R, o_depth_G, o_depth_B;	/* original bit depth of each component before recompression */
	int o_depth_Y, o_depth_U, o_depth_V;	/* same as above */
	int o_depth_A;				/* same as above */
	t_original_ct o_color_type;		/* original color type before recompression */
	int opt_pal_transp;			/* total of pixels with alpha data, when palette is optimized */
} raw_bitmap;

//EXTERN raw_image *new_raw_image(int height, int width, int bpp, int flags);
EXTERN t_content_type detect_type(char *line, int len);

EXTERN int compress_image (http_headers *serv_hdr, http_headers *client_hdr, char *inbuf, ZP_DATASIZE_TYPE insize, char **outb, ZP_DATASIZE_TYPE *outl);	

#endif //SRC_IMAGE_H

#undef EXTERN
