/* image.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <gif_lib.h>

#include <jpeglib.h>
//remove some leaks from jpeglib headers:
#undef METHODDEF
#undef EXTERN
#undef LOCAL
#undef GLOBAL

#include <png.h>
#include <zlib.h>

#ifdef JP2K
#include <jasper/jasper.h>
#include "jp2tools.h"
#endif

#define SRC_IMAGE_C
#include "image.h"
#include "cfgfile.h"
#include "log.h"
#include "cvtables.h"
#include "globaldefs.h"

/* 1GB roof for max_raw_size */
#define MAX_RAW_SIZE_ROOF 0x3fffffffLL

// minimal image file size to _bother_ trying to recompress
#define MIN_INSIZE_GIF 100
#define MIN_INSIZE_PNG 100
#define MIN_INSIZE_JPEG 600
#define MIN_INSIZE_JP2K 800
#define MIN_INSIZE_TO_JPEG 600
#define MIN_INSIZE_TO_JP2K 800

//Forwards. There are more utility functions, but they're used only once.
static raw_bitmap *new_raw_bitmap();

static int png2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size);
static int gif2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size);
static int jpg2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size);

static int bitmap2jpg(raw_bitmap  * bmp, int quality, char ** outb, int * outl);

static int bitmap2png(raw_bitmap  * bmp, char ** outb, int * outl);

static int rgb2gray(raw_bitmap * bmp);
static int depalettize(raw_bitmap * bmp);

static int optimize_alpha_channel (raw_bitmap *bmp);
static int optimize_palette (raw_bitmap *bmp);
static int remove_alpha_channel (raw_bitmap *bmp);

#ifdef JP2K
int calculate_jp2_rawsize (raw_bitmap *bmp, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int*bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA, int discard_alpha);
float estimate_jp2rate_from_quality (raw_bitmap *bmp, int quality, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int *bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA);
static int bitmap2jp2 (raw_bitmap *bmp, float rate, char **outb, int *outl, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int *bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA);
static int jp22bitmap (char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size);
#endif

t_content_type detect_type (char *line, int len)
{
	t_content_type image = OTHER_CONTENT;

	if (len < 4) return OTHER_CONTENT;
    // Check for image signatures
	if ((line[0] == -1) && (line[1] == -40) &&
		(line[2] == -1) && (line[3] == -32))
		image = IMG_JPEG;
	else if ((line[0] == 'G') && (line[1] == 'I') &&
		     (line[2] == 'F') && (line[3] == '8'))
		image = IMG_GIF;
	else if ((line[0] == -119) && (line[1] == 'P') &&
		     (line[2] == 'N') && (line[3] == 'G'))
		image = IMG_PNG;
#ifdef JP2K
	// TODO: improve JP2K detection
	else if ((line[4] == 'j') && (line[5] == 'P'))
		image = IMG_JP2K;
#endif
	return image;
}

/* Recompress pictures.
 * inbuf/insize: original compressed image (jpeg/png/etc)
 * outb/outl: new recompressed image (to jpeg/png etc).
 *            a buffer will be malloc()'ed automatically in this case.
 *
 * Returns: IMG_RET_OK, recompression successful. Or error code.
 *
 * Note: this routine was rewritten (the old one is used in versions <= 3.0.x)
 *       and does not have the exact same behavior as the previous one.
 */
/* FIXME: This routine does not support data > 2GB (limited to int) */
/* FIXME: inbuf AND outb cannot be safely free()'d afterwards!
          when compression fails inbuf==outb, so trying to free() both will fail. */
int compress_image (http_headers *serv_hdr, http_headers *client_hdr, char *inbuf, ZP_DATASIZE_TYPE insize, char **outb, ZP_DATASIZE_TYPE *outl){	
	int st = IMG_RET_ERR_OTHER;
	int pngstatus = IMG_RET_ERR_OTHER;
	int jp2status = IMG_RET_ERR_OTHER;
	int jpegstatus = IMG_RET_ERR_OTHER;
	t_content_type outtype = OTHER_CONTENT;
	int jpeg_q;
	raw_bitmap *bmp;
	long long int max_raw_size;
	t_content_type detected_ct;
	t_content_type source_type;
	t_content_type target_lossy;
	t_content_type target_lossless = IMG_PNG; /* only PNG is available */
	int try_lossless = 1, try_lossy = 1;
	int has_transparency = 0;
	char *buf_lossy = NULL, *buf_lossless = NULL;
	int buf_lossy_len, buf_lossless_len;
	int max_outlen;
	int lossy_status, lossless_status;
	const int *j2bitlenYA, *j2bitlenRGBA, *j2bitlenYUVA, *j2csamplingYA, *j2csamplingRGBA, *j2csamplingYUVA;
	int source_is_lossless = 0;	/* !=0 if source is gif or png */

	// "rate" below: JP2 rate, the native compression setting of JP2
	// ziproxy tries to emulate JPEG's quality setting to JP2, and this
	// var represents the 'real thing' which is hidden from the user.
	float rate = -1.0;
	int jp2_q;


	detected_ct = detect_type (inbuf, insize);
	if (detected_ct != OTHER_CONTENT)
		serv_hdr->type = detected_ct;

	source_type = serv_hdr->type;

	max_raw_size = insize * MaxUncompressedImageRatio;

	/* max_raw_size is undefined, set it to the internal roof then */
	if (max_raw_size == 0) {
		debug_log_printf ("WARNING: MaxUncompressedImageRatio set to 0 (no " \
			"ratio limit). Using internal absolute limit of %lld bytes.\n", \
			MAX_RAW_SIZE_ROOF);
		max_raw_size = MAX_RAW_SIZE_ROOF;
	}

	/* max_raw_size beyond roof, set it to internal roof instead */
	if (max_raw_size > MAX_RAW_SIZE_ROOF) {
		debug_log_printf ("WARNING: Max image size (%lld) bigger than internal " \
			"roof (%lld). Using internal roof instead.\n", max_raw_size, \
			MAX_RAW_SIZE_ROOF);
		max_raw_size = MAX_RAW_SIZE_ROOF;
	}

	debug_log_puts ("Starting image decompression...");

	switch (source_type) {
		case IMG_PNG:
			if (insize >= MIN_INSIZE_PNG)
				st = png2bitmap (inbuf, insize, &bmp, max_raw_size);
			else
				st = IMG_RET_TOO_SMALL;
			source_is_lossless = 1;
			break;
		case IMG_GIF:
			if (insize >= MIN_INSIZE_GIF)
				st = gif2bitmap (inbuf, insize, &bmp, max_raw_size);
			else
				st = IMG_RET_TOO_SMALL;
			source_is_lossless = 1;
			break;
		case IMG_JPEG:
			if (insize >= MIN_INSIZE_JPEG)
				st = jpg2bitmap (inbuf, insize, &bmp, max_raw_size);
			else
				st = IMG_RET_TOO_SMALL;
			break;
#ifdef JP2K
		case IMG_JP2K:
			if ((insize >= MIN_INSIZE_JP2K) || (ForceOutputNoJP2))
				st = jp22bitmap (inbuf, insize, &bmp, max_raw_size);
			else
				st = IMG_RET_TOO_SMALL;
			break;
#endif
		default:
			debug_log_puts ("ERROR: Unrecognized image format!\n");
			st = IMG_RET_ERR_OTHER;
			break;
	}

	// error, forward unchanged
	if (st != IMG_RET_OK) {
		debug_log_puts ("Error while decompressing image.");
		*outb = inbuf;
		*outl = insize;
		return st;
	}
	if (bmp->o_color_type == OCT_PALETTE) {
		debug_log_printf ("Image parms (palette) -- w: %d, h: %d, " \
			"palette with %d colors, pal_bpp: %d.\n", \
			bmp->width, bmp->height, bmp->pal_entries, bmp->pal_bpp);
	} else {
		debug_log_printf ("Image parms (non-palette) -- w: %d, h: %d, bpp: %d\n", \
			bmp->width, bmp->height, bmp->bpp);
	}

	optimize_palette (bmp);
	optimize_alpha_channel (bmp);

	/*
	 * STRATEGY DECISIONS
	 */

	debug_log_puts ("Deciding image compression strategy...");

	/* does it have transparency? */
	if (bmp->raster != NULL) {
		/* palette image */
		if ((bmp->pal_bpp == 2) || (bmp->pal_bpp == 4))
			has_transparency = 1;
	} else {
		/* non-palette image */
		if ((bmp->bpp == 2) || (bmp->bpp == 4))
			has_transparency = 1;
	}

	/* which lossy format to use? */
#ifdef JP2K
	if (ProcessToJP2 && (! ForceOutputNoJP2) && \
		((! JP2OutRequiresExpCap) || (JP2OutRequiresExpCap && client_hdr->client_explicity_accepts_jp2))) {
		target_lossy = IMG_JP2K;
	} else {
		target_lossy = IMG_JPEG;
	}
#else
	target_lossy = IMG_JPEG;
#endif

	/* is lossy suitable for this picture? */
#ifdef JP2K
	if (target_lossy == IMG_JP2K) {
		jp2_q = getJP2ImageQuality (bmp->width, bmp->height);
		if ((jp2_q == 0) || (insize < MIN_INSIZE_TO_JP2K))
			try_lossy = 0;
	}
#endif
	if (target_lossy == IMG_JPEG) {
		jpeg_q = getImageQuality (bmp->width, bmp->height);
		if ((jpeg_q == 0) || (insize < MIN_INSIZE_TO_JPEG)) {
			try_lossy = 0;
		} else if (has_transparency) {
			if (AllowLookCh)
				remove_alpha_channel (bmp);
			else
				try_lossy = 0;
		}
	}

	/* compressed data may not be bigger than max_outlen */
	max_outlen = insize - 1;

#ifdef JP2K
	/* should we convert from jp2k even if the final size is bigger? */
	if ((source_type == IMG_JP2K) && (ForceOutputNoJP2)) {
		// up to 100%+500bytes of uncompressed bitmap, otherwise it's an abnomaly
		max_outlen = (bmp->width * bmp->height * bmp->bpp) + 500;
	}
#endif

	/* let's try saving some CPU load:
	   is it worth trying lossless compression? */
	if ((try_lossless != 0) && (try_lossy != 0) && (source_is_lossless == 0)) {
		try_lossless = 0;
	}

	/* no viable target? return */
	if ((try_lossy == 0) && (try_lossless == 0)) {
		debug_log_puts ("No viable image target (lossy or lossless).");
		return IMG_RET_NO_AVAIL_TARGET;
	}

	/*
	 * END OF STRATEGY DECISIONS
	 */

	debug_log_puts ("Strategy defined. Continuing...");

	if (try_lossy) {
		buf_lossy_len = max_outlen;

		/* for lossy, full RGB image is required */
		if (bmp->bitmap == NULL)
			depalettize (bmp);
	}
	if (try_lossless) {
		/* bitmap2png requires a preallocated buffer */
		buf_lossless = (char *) malloc (sizeof (char) * max_outlen);
		buf_lossless_len = max_outlen;
	}

	if (ConvertToGrayscale && (bmp->bitmap != NULL)) {
		debug_log_puts ("Converting image to grayscale...");
		rgb2gray (bmp);
	}

#ifdef JP2K
	if ((try_lossy) && (target_lossy == IMG_JP2K)) {
		debug_log_puts ("Attempting JP2K compression...");

		// get the components' bit depth specifically for this image (based on image dimensions)
		j2bitlenYA = getJP2KBitLenYA (bmp->width, bmp->height);
		j2bitlenRGBA = getJP2KBitLenRGBA (bmp->width, bmp->height);
		j2bitlenYUVA = getJP2KBitLenYUVA (bmp->width, bmp->height);

		// get the components' sampling (scaling) parameters specifically for this image (based on image dimensions)
		j2csamplingYA = getJP2KCSamplingYA (bmp->width, bmp->height);
		j2csamplingRGBA = getJP2KCSamplingRGBA (bmp->width, bmp->height);
		j2csamplingYUVA = getJP2KCSamplingYUVA (bmp->width, bmp->height);

		rate = estimate_jp2rate_from_quality (bmp, jp2_q, \
			JP2Colorspace, j2bitlenYA, j2bitlenRGBA, j2bitlenYUVA, \
			j2csamplingYA, j2csamplingRGBA, j2csamplingYUVA);

		if (rate * (float) calculate_jp2_rawsize (bmp, JP2Colorspace, \
			j2bitlenYA, j2bitlenRGBA, j2bitlenYUVA, j2csamplingYA, \
			j2csamplingRGBA, j2csamplingYUVA, 0) <= (float) max_outlen) {

			jp2status = bitmap2jp2 (bmp, rate, &buf_lossy, &buf_lossy_len, \
				JP2Colorspace, j2bitlenYA, j2bitlenRGBA, \
				j2bitlenYUVA, j2csamplingYA, j2csamplingRGBA, \
				j2csamplingYUVA);
		} else {
			jp2status = IMG_RET_TOO_BIG;
		}
	} else {
		jp2status = IMG_RET_ERR_OTHER;
	}
#endif

	if ((try_lossy) && (target_lossy == IMG_JPEG)) {
		debug_log_puts ("Attempting JPEG compression...");
		jpegstatus = bitmap2jpg (bmp, jpeg_q, &buf_lossy, &buf_lossy_len);
	}

	/* try_lossless implies PNG */
	if (try_lossless != 0) {
		debug_log_puts ("Attempting PNG compression...");
		pngstatus = bitmap2png (bmp, &buf_lossless, &buf_lossless_len);
	}

	debug_log_printf ("Compression return codes -- JP2K:%d JPEG:%d PNG:%d\n", jp2status, jpegstatus, pngstatus);

	lossless_status = pngstatus;
	if (target_lossy == IMG_JPEG)
		lossy_status = jpegstatus;
	else
		lossy_status = jp2status;

	/* decide which compressed version to use, or none */
	if ((lossless_status == IMG_RET_OK) && (lossy_status == IMG_RET_OK)) {
		/* TODO: add some fuzzy logic here
		  (smallest size is not always the best choice) */
		if (buf_lossy_len < buf_lossless_len) {
			outtype = target_lossy;
		} else {
			outtype = target_lossless;
		}
	} else if (lossless_status == IMG_RET_OK) {
		outtype = target_lossless;
	} else if (lossy_status == IMG_RET_OK) {
		outtype = target_lossy;
	} else {
		outtype = OTHER_CONTENT;
	}

	/* select buffer and discard the other one (or both) */
	if (outtype == target_lossy) {
		*outb = buf_lossy;
		*outl = buf_lossy_len;
		if (buf_lossless != NULL)
			free (buf_lossless);
	} else if (outtype == target_lossless) {
		*outb = buf_lossless;
		*outl = buf_lossless_len;
		if (buf_lossy != NULL)
			free (buf_lossy);
	} else {
		*outb = inbuf;
		*outl = insize;
		if (buf_lossy != NULL)
			free (buf_lossy);
		if (buf_lossless != NULL)
			free (buf_lossless);
	}

	if (serv_hdr->where_content_type > 0){ 
		if(outtype != OTHER_CONTENT)
			switch(outtype){
				case IMG_JP2K:
					serv_hdr->hdr[serv_hdr->where_content_type] =
						"Content-Type: image/jp2";
					break;
				case IMG_JPEG:		
					serv_hdr->hdr[serv_hdr->where_content_type] = 
						"Content-Type: image/jpeg";
					break;
				case IMG_PNG:
					serv_hdr->hdr[serv_hdr->where_content_type] = 
						"Content-Type: image/png";
					break;
			}
	}

	return IMG_RET_OK;
}

typedef struct {
		char* buf;
		int size;
		union {
			int pos;
			char jpeg_unexpected_end;
		}x;
}IODesc;


raw_bitmap *new_raw_bitmap(){
	raw_bitmap * bmp = (raw_bitmap*)malloc(sizeof(raw_bitmap));

	bmp->bitmap = bmp->bitmap_yuv = bmp->raster = bmp->palette = NULL;
	bmp->height = bmp->width = bmp->bpp = bmp->pal_bpp = -1;
	bmp->pal_entries = -1;
	bmp->o_depth_R = bmp->o_depth_G = bmp->o_depth_B = -1;
	bmp->o_depth_Y = bmp->o_depth_U = bmp->o_depth_V = -1;
	bmp->o_depth_A = -1;
	bmp->o_color_type = OCT_UNDEFINED;
	bmp->opt_pal_transp = -1;
	return bmp;
}


static void mem_to_png(png_structp png_ptr,
        png_bytep data, png_size_t length)
{
		IODesc *desc=(IODesc*)png_get_io_ptr(png_ptr);
		
		if(desc->x.pos + length >= desc->size) png_error(png_ptr, "Reading past input buffer\n");
		
	memcpy(data, desc->buf + desc->x.pos,length);
	desc->x.pos += length;
}

static void png_warn_still(png_structp png_ptr, png_const_charp msg){}

static void png_err_still(png_structp png_ptr, png_const_charp msg)
{
	longjmp(png_jmpbuf(png_ptr), IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_DECOMP);
}

int png2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size)
{
  	png_structp png_ptr;
   	png_infop info_ptr, end_info = NULL;
   	int color_type, bit_depth;
	png_bytepp row_pointers;
	png_bytep onerow;
	int i;
	IODesc desc;
	raw_bitmap *bmp;
	long long int raw_size;
	png_uint_32 width_png_uint_32, height_png_uint_32;
	unsigned char *dest_bitmap;	/* points to bmp->bitmap (RGB) or bmp->raster (palette) */
	int dest_bitmap_bpp = 0;

   /* Compare the first 8 bytes of the signature.
      Return with error if they don't match. */

	if ((insize < 8) || png_sig_cmp (inbuf, (png_size_t) 0, 8))
		return IMG_RET_ERR_BAD_DATA_FORMAT + IMG_RET_FLG_WHILE_DECOMP;

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		return IMG_RET_ERR_OUT_OF_MEM + IMG_RET_FLG_WHILE_DECOMP;
	}
	
	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		return IMG_RET_ERR_OUT_OF_MEM + IMG_RET_FLG_WHILE_DECOMP;
	}
	if ((i = setjmp (png_jmpbuf(png_ptr)))) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return i;
	}

	png_set_error_fn (png_ptr, NULL, png_err_still, png_warn_still);

	desc.buf = inbuf;
	desc.size = insize;
	desc.x.pos = 0;

	bmp = new_raw_bitmap();
	*out = bmp;

	png_set_read_fn (png_ptr, (void *) &desc, mem_to_png);
	
	png_read_info (png_ptr,info_ptr);
	png_get_IHDR (png_ptr, info_ptr, &width_png_uint_32, &height_png_uint_32,
		&bit_depth, &color_type, NULL, NULL, NULL);
	if ((width_png_uint_32 > 0x7fffffff) || (height_png_uint_32 > 0x7fffffff))
		return (IMG_RET_TOO_EXPANSIVE); /* too huge, unworkable */
	bmp->width = (int) width_png_uint_32;
	bmp->height = (int) height_png_uint_32;

	/* limit each image component to 8 bits */
	if (bit_depth == 16) {
		png_set_strip_16 (png_ptr);
		bit_depth = 8;
	}

	switch (color_type) {
		case PNG_COLOR_TYPE_PALETTE:
			dest_bitmap_bpp = 1;	/* (palette color) */
			bmp->o_color_type = OCT_PALETTE;
			break;
		case PNG_COLOR_TYPE_GRAY:
			dest_bitmap_bpp = 1;	/* Y */
			bmp->o_color_type = OCT_GRAY;
			bmp->o_depth_Y = bit_depth;
			bmp->bpp = dest_bitmap_bpp;
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			dest_bitmap_bpp = 2;	/* YA */
			bmp->o_color_type = OCT_GRAY;
			bmp->o_depth_Y = bmp->o_depth_A = bit_depth;
			bmp->bpp = dest_bitmap_bpp;
			break;
		case PNG_COLOR_TYPE_RGB:
			dest_bitmap_bpp = 3;	/* RGB */
			bmp->o_color_type = OCT_RGB;
			bmp->o_depth_R = bmp->o_depth_G = bmp->o_depth_B = 8;
			bmp->bpp = dest_bitmap_bpp;
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			dest_bitmap_bpp = 4;	/* RGBA */
			bmp->o_color_type = OCT_RGB;
			bmp->o_depth_R = bmp->o_depth_G = bmp->o_depth_B = bmp->o_depth_A = 8;
			bmp->bpp = dest_bitmap_bpp;
			break;
		default:
			return IMG_RET_ERR_NOT_IMPL_YET;
			break;
	}

	/* MaxUncompressedImageRatio checking */
	/* this raw_size represents the real raw size of the bitmap,
	   which may (and probably will) be smaller than the
	   bitmap allocated in memory */
	raw_size = ((long long int) bmp->width * (long long int) bmp->height * (long long int) dest_bitmap_bpp);
	if ((max_raw_size > 0) && (raw_size > max_raw_size))
		return IMG_RET_TOO_EXPANSIVE;

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		/* define palette/alpha data */
		{
			png_bytep trans;
			int num_trans;
			png_color_16p trans_values;
			png_colorp palette_png;
			int num_palette;

			/* get alpha, if present */
			if (png_get_tRNS (png_ptr, info_ptr, &trans, &num_trans, &trans_values) == 0)
				num_trans = 0;
			if (num_trans > 0)
				bmp->pal_bpp = 4;	/* RGBA palette */
			else
				bmp->pal_bpp = 3;	/* RGB palette */

			/* create raw bmp palette structure and initialize
				(0xFF for default opaque alpha) */
			bmp->palette = malloc (sizeof (unsigned char) * 1024);
			for (i = 0; i < 1024; i++)
				bmp->palette [i] = (unsigned char) 0xff;

			/* get RGB palette data */
			png_get_PLTE (png_ptr, info_ptr, &palette_png, &num_palette);
			bmp->pal_entries = num_palette;

			/* populate definitive palette structure */
			for (i = 0; i < num_palette; i++) {
				bmp->palette [i *  bmp->pal_bpp] = palette_png [i].red;
				bmp->palette [i *  bmp->pal_bpp + 1] = palette_png [i].green;
				bmp->palette [i *  bmp->pal_bpp + 2] = palette_png [i].blue;
			}

			/* add alpha data, if any */
			if (bmp->pal_bpp == 4) {
				for (i = 0; i < 256; i++) {
					bmp->palette [i * 4 + 3] = (unsigned char) 0xff;
				}
				for (i = 0; i < num_trans; i++) {
					bmp->palette [i * 4 + 3] = trans [i];
				}
			}

		}

		if (bit_depth < 8)
			png_set_packing (png_ptr); /* force 1 byte per pixel */

		bmp->raster = (unsigned char *) malloc ((bmp->width) * (bmp->height));
		dest_bitmap = bmp->raster;

	} else {
		/* Y, YA, RGB, RGBA */

		if ((color_type == PNG_COLOR_TYPE_GRAY) || (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)) {
			if (bit_depth < 8)
				png_set_expand_gray_1_2_4_to_8 (png_ptr); /* force 1 byte per pixel */
		}

		bmp->bitmap = (unsigned char *) malloc ((bmp->width) * (bmp->height) * (bmp->bpp));
		dest_bitmap = bmp->bitmap;
	}

	/* decompress bitmap to raw buffer */
	row_pointers = (png_bytepp) malloc (bmp->height * sizeof (png_bytep));
	onerow = (png_bytep) dest_bitmap;
	for (i=0; i < bmp->height; i++) {
		row_pointers [i] = onerow;
		onerow += (bmp->width) * (dest_bitmap_bpp);
	}
	png_read_image (png_ptr, row_pointers);
	free (row_pointers);

	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

	return IMG_RET_OK;
}

static void png_to_mem(png_structp png_ptr,
        png_bytep data, png_size_t length){
		IODesc *desc=(IODesc*)png_get_io_ptr(png_ptr);
		if(length + desc->x.pos > desc->size)
			png_error(png_ptr, "Writing past output buffer\n");
		
	memcpy(desc->buf + desc->x.pos, data ,length);
	desc->x.pos += length;
}

static void png_flush_mem(png_structp png_ptr){};

static int bitmap2png(raw_bitmap  * bmp, char ** outb, int * outl){
	int i, ctype, bits = 8;
	IODesc desc;
	png_bytepp row_pointers;
	png_bytep onerow;
	png_color_16 transcol;
	char *tr = NULL;
	png_infop info_ptr;

	png_structp png_ptr = png_create_write_struct
	(PNG_LIBPNG_VER_STRING, NULL,
	NULL, NULL);

	if (png_ptr == NULL)
	return (IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_COMPRESS);

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr,
		 (png_infopp)NULL);
		return (IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_COMPRESS);
	}
	
	if ((i = setjmp(png_jmpbuf(png_ptr)))) {
		png_destroy_write_struct(&png_ptr,
		 &info_ptr);
		return (i);
	}

	png_set_error_fn(png_ptr,NULL, 
			png_err_still, png_warn_still);	

	if(bmp->pal_entries > 0){
		png_color *palette_png;

		/* export palette */
		
	 	palette_png = (png_colorp) png_malloc (png_ptr, bmp->pal_entries * sizeof (png_color));

		/* alpha for exportation */
		if ((bmp->pal_bpp == 2) || (bmp->pal_bpp == 4)) {
			tr = (char*) malloc (bmp->pal_entries);
		}

		palette_png = malloc (bmp->pal_entries * sizeof (png_color));
		for (i = 0; i < bmp->pal_entries; i++) {
			if ((bmp->pal_bpp == 2) || (bmp->pal_bpp == 4)) {
				/*
				if (bmp->palette [(i + 1) * bmp->pal_bpp - 1] == 0) {
					transcol.index = i;
					tr [i] = 0;
				} else {
					tr [i] = 255;
				}
				*/
				tr [i] = bmp->palette [(i + 1) * bmp->pal_bpp - 1];
			}
				
			if (bmp->pal_bpp < 3) {
				palette_png [i].red =
					palette_png [i].green =
					palette_png [i].blue =
					bmp->palette [i * bmp->pal_bpp];
			} else {
				palette_png [i].red = bmp->palette [i * bmp->pal_bpp];
				palette_png [i].green = bmp->palette [i * bmp->pal_bpp + 1];
				palette_png [i].blue = bmp->palette [i * bmp->pal_bpp + 2];
			}
		}

		ctype = PNG_COLOR_TYPE_PALETTE;
		png_set_PLTE (png_ptr, info_ptr, palette_png, bmp->pal_entries);

		if ((bmp->pal_bpp == 2) || (bmp->pal_bpp == 4)) {
			/* store optimized transparency, if available */
			if (bmp->opt_pal_transp > 0) {
				png_set_tRNS (png_ptr, info_ptr, tr, bmp->opt_pal_transp, NULL);
			} else {
				png_set_tRNS (png_ptr, info_ptr, tr, bmp->pal_entries, NULL);
			}
			//png_set_bKGD(png_ptr, info_ptr, &transcol);	
		}

		if (bmp->pal_entries <= 2) bits = 1;
		else if (bmp->pal_entries <= 4) bits = 2;
		else if (bmp->pal_entries <= 16) bits = 4;
		
		onerow = (png_bytep) bmp->raster;
	}else{
		switch (bmp->bpp){
			case 1: ctype = PNG_COLOR_TYPE_GRAY;
				break;
			case 2: ctype = PNG_COLOR_TYPE_GRAY_ALPHA;
				break;
			case 3: ctype = PNG_COLOR_TYPE_RGB;
				break;
			case 4: ctype = PNG_COLOR_TYPE_RGB_ALPHA;
				break;
		}
/*		color_bpps.red = 8;
		color_bpps.green = 8;
		color_bpps.blue = 8;
		color_bpps.gray = 8;
		color_bpps.alpha = 8;*/

		png_set_filter (png_ptr, 0, PNG_FILTER_SUB);

//		png_set_sBIT(png_ptr, info_ptr, &color_bpps);
		onerow = (png_bytep) bmp->bitmap;
	}

	png_set_IHDR(png_ptr, info_ptr, bmp->width, bmp->height, bits,
			ctype,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
	
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	
	row_pointers = (png_bytepp)malloc(bmp->height * sizeof(png_bytep));
	
	for (i=0; i < bmp->height; i++) {
		row_pointers[i]=onerow;
		onerow += (bmp->width) * 
			(ctype == PNG_COLOR_TYPE_PALETTE ? 1 : bmp->bpp);
	}
	
	png_set_rows(png_ptr, info_ptr, row_pointers);
	
	desc.buf = *outb;
	desc.size = *outl;
	desc.x.pos = 0;
	png_set_write_fn (png_ptr, (void*) &desc, png_to_mem, png_flush_mem);

	png_write_png (png_ptr, info_ptr,PNG_TRANSFORM_PACKING, NULL);

	*outl = desc.x.pos;

	return IMG_RET_OK;
}

static int gif_mem_input(GifFileType *GifFile, GifByteType* data, int bytes)
{
	IODesc *desc = (IODesc*) GifFile->UserData;
	
	int copied;
	
	if(bytes + desc->x.pos >= desc->size) copied = desc->size - desc->x.pos;
	else copied=bytes;
	
	
	memcpy(data,desc->buf + desc->x.pos,copied);

	desc->x.pos+=copied;

	return copied;
}

/**
 * Get transparency color from graphic extension block
 *
 * Return: transparency color or -1
 */
static int getTransparentColor(GifFileType * file)
{
  int i;
  ExtensionBlock * ext = file->SavedImages[0].ExtensionBlocks;
 
  for (i=0; i < file->SavedImages[0].ExtensionBlockCount; i++, ext++) {
  
    if (ext->Function == GRAPHICS_EXT_FUNC_CODE) {
      if (ext->Bytes[0] & 1)/* there is a transparent color */
        return (unsigned char) ext->Bytes[3];/* here it is */
    }
  }

  return -1;
}


int gif2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size)
{
	GifFileType *GifFile;
	SavedImage* LastImg = NULL;
	GifImageDesc *LastImgDesc = NULL;
	ColorMapObject *Colors;
	GifColorType *Pixel;
	IODesc desc;
	int i, j, k, c, offset, transColor;
	unsigned char* BufferP, *Raster;
	const int    InterlacedOffset[] = { 0, 4, 2, 1 }; /* The way Interlaced image should. */
	const int    InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
	raw_bitmap *bmp;
	unsigned char *MyColorMap = NULL;
	int MyColorCount = -1;
	long long int raw_size;

	desc.buf=inbuf;
	desc.size=insize;
	desc.x.pos=0;

	if ((GifFile = DGifOpen((void*)&desc, &gif_mem_input)) == NULL) 
    		return( IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_DECOMP);//more possible reasons

	bmp = new_raw_bitmap();
	*out = bmp;
	bmp->width=GifFile->SWidth;
	bmp->height=GifFile->SHeight;

	/* MaxUncompressedImageRatio checking */
	/* this is an estimation of the bitmap size used in memory,
	   it assumes 8 bits per pixel.
	   there's no transparency information at this point. */
	raw_size = (long long int) bmp->width * (long long int) bmp->height;
	if ((max_raw_size > 0) && (raw_size > max_raw_size))
		return (IMG_RET_TOO_EXPANSIVE);

	if(DGifSlurp(GifFile)!=GIF_OK)
	{
			debug_log_puts ("ERROR: GIF decoding failed.");
			return(IMG_RET_ERR_BAD_DATA_FORMAT + IMG_RET_FLG_WHILE_DECOMP);
	}

	transColor = getTransparentColor(GifFile);


	//We're interested in last frame. Searching for last complete redraw
	//of image.
	i = GifFile->ImageCount - 1;
	if(i > 0 && !AllowLookCh) return IMG_RET_ERR_POSSIBLE_LOOK_CHANGE + IMG_RET_FLG_WHILE_DECOMP;
	for(; i>=0; i--)
		if(GifFile->SavedImages[i].ImageDesc.Left == 0 &&
			GifFile->SavedImages[i].ImageDesc.Top == 0 &&
			GifFile->SavedImages[i].ImageDesc.Width == bmp->width &&
			GifFile->SavedImages[i].ImageDesc.Height == bmp->height
			)
				LastImg = GifFile->SavedImages + i;

	if (LastImg != NULL) {
		
		Raster = (unsigned char*)malloc(bmp->width*bmp->height);
		memcpy(Raster, LastImg->RasterBits, bmp->width*bmp->height);
		
		LastImgDesc = &LastImg->ImageDesc;
	
		Colors = (LastImgDesc->ColorMap
			? LastImgDesc->ColorMap
			: GifFile->SColorMap);

	}else{

		 if(!AllowLookCh) return IMG_RET_ERR_POSSIBLE_LOOK_CHANGE + IMG_RET_FLG_WHILE_DECOMP;
		//no complete image found - Make overlay of all images
		Raster = (unsigned char*)malloc(bmp->width*bmp->height);
		
		for(i=0;i<bmp->height;i++)
			for(j=0;j<bmp->width;j++)
				Raster[i*bmp->width + j] = GifFile->SBackGroundColor;
		
		for(i=0;i<bmp->height;i++)
			for(j=0;j<bmp->width;j++)
				for(k = 0;k < GifFile->ImageCount; k++){
					int left = GifFile->SavedImages[k].ImageDesc.Left;
					int right = left + GifFile->SavedImages[k].ImageDesc.Width;
					int top = GifFile->SavedImages[i].ImageDesc.Top;
					int bottom = top + GifFile->SavedImages[i].ImageDesc.Height == bmp->height;

					if(i >= top && i <= bottom && j >= left && j <= right){
						Raster[(i - top)*bmp->width + (j-left)] =
							GifFile->SavedImages[k].RasterBits[(i - top)*(left-right) + (j-left)];
						break;
					}
					
				}
		Colors = GifFile->SColorMap;
	}
	
	//Often there are unused colors in the palette.
	MyColorMap = (unsigned char*)malloc(Colors->ColorCount);
	for(i=0; i< Colors->ColorCount;i++)
		MyColorMap[i] = 0;
	
	MyColorCount = 0;
	for(i=0; i< bmp->width*bmp->height;i++){
		if(!MyColorMap[Raster[i]]){
			MyColorCount++; 
			//We're numbering the colors here from 1 upwards. But in 
			//the final raster they'd be from 0.
			MyColorMap[Raster[i]] = MyColorCount;
			if((255 == MyColorCount) || (MyColorCount == Colors->ColorCount)){
				free(MyColorMap);//we're using full palette
				MyColorMap = NULL;
				MyColorCount = Colors->ColorCount;
				break;
			}
		}
	}
	
	bmp->bpp=1;
	for(i = 0; i < Colors->ColorCount; i++)
	{
		if((i != transColor) && (!MyColorMap || MyColorMap[i]) &&
			(( Colors->Colors[i].Red !=  Colors->Colors[i].Green ) ||
			( Colors->Colors[i].Green != Colors->Colors[i].Blue)))
		{
			bmp->bpp=3;
			break;
		}
	}
	if (MyColorMap && !MyColorMap[transColor])
			transColor = -1;

	if (transColor >= 0 ){
		bmp->bpp++; //add alpha channel
	}
	
	assert(bmp->bpp > 0 && bmp->bpp <= 4);
	
	bmp->pal_entries = MyColorCount;
	bmp->palette = (unsigned char*) malloc (sizeof (unsigned char) * 1024);
	
	BufferP = bmp->palette;
	for(i = 0; i < Colors->ColorCount; i++){
		if(MyColorMap) {
			if(!MyColorMap[i]) continue;
			BufferP = bmp->palette + bmp->bpp*(MyColorMap[i] - 1);
		}
		Pixel = Colors->Colors + i;
		for(k=0;k<bmp->bpp;k++)
			switch(k){
				case 0: *BufferP++ = Pixel->Red;
					break;
				case 1: if( bmp->bpp > 2)
						*BufferP++ = Pixel->Green;
					else 
						*BufferP++ = 
						( i == transColor ?
						0 : 255);

					break;
				case 2: *BufferP++ = Pixel->Blue;
					break;
				case 3: *BufferP++ = 
					( i == transColor ?
					0 : 255);
					break;
			}

	}
	
	
	if(GifFile->Image.Interlace){
		bmp->raster=(unsigned char*)malloc(bmp->width * bmp->height);
		BufferP=bmp->raster;
		offset = -1;
			
	    for (c = 0; c < 4; c++)
		for (i = InterlacedOffset[c]; i < bmp->height;
					 i += InterlacedJumps[c]){
			BufferP = bmp->raster + i*bmp->width;
			for(j=0; j < bmp->width; j++){
				offset++;
				if(MyColorMap)
					*BufferP++ = MyColorMap[Raster[offset]] - 1;
				else
					*BufferP++ = Raster[offset];
			}
		}
	    free(Raster);

	}else{
		if(MyColorMap) for(i =0; i< bmp->width*bmp->height; i++){
			Raster[i]=MyColorMap[Raster[i]] - 1;
		}
		
		bmp->raster = Raster;
	}
	bmp->pal_bpp = bmp->bpp;
	
	DGifCloseFile(GifFile);
	return IMG_RET_OK;
}

IODesc * get_src (IODesc* ptr)
{
	static IODesc *myptr;

	if (ptr != NULL) myptr = ptr;
	return myptr;
}

static void jpeg_src_noop(j_decompress_ptr dinfo){}

static void jpeg_src_init(j_decompress_ptr dinfo)
{
	IODesc * desc = get_src(NULL);

	dinfo-> src ->next_input_byte = desc->buf;
	dinfo-> src -> bytes_in_buffer = desc->size;
}

static boolean jpeg_src_fill_input(j_decompress_ptr dinfo)
{
	return FALSE;
}

static void jpeg_src_skip_input (j_decompress_ptr dinfo, long num_bytes)
{
	IODesc * desc = get_src(NULL);

	if ((char*)(dinfo->src->next_input_byte + num_bytes) > desc->buf + desc->size || 
		(char*)dinfo->src->next_input_byte == &desc->x.jpeg_unexpected_end )
		{ //silly behavior - recommended by jpeglib docs
			dinfo->src->next_input_byte = &desc->x.jpeg_unexpected_end;
			dinfo->src->bytes_in_buffer = 1; 
		}
		else
		{
			dinfo->src->next_input_byte += num_bytes;
			dinfo->src->bytes_in_buffer -= num_bytes;
		}
}

jmp_buf errjmp;

static void jpeg_error_exit (j_common_ptr cinfo)
{
	longjmp(errjmp,1);
}

/* does nothing, but defined otherwise libjpeg will keep
   pestering us dumping messages to stderr */
static void jpeg_emit_message (j_common_ptr cinfo, int msg_level)
{
}

int jpg2bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size)
{
		
        struct jpeg_decompress_struct dinfo;
	char *BufferP;
        struct jpeg_error_mgr jerr;
	struct jpeg_source_mgr jsrc;
	IODesc desc;
	int imgsize,row_stride;
	raw_bitmap *bmp;
	long long int raw_size;

	desc.buf = inbuf;
	desc.size = insize;
	desc.x.jpeg_unexpected_end = JPEG_EOI;

	get_src(&desc);

	dinfo.err = jpeg_std_error(&jerr);
	dinfo.err->error_exit = jpeg_error_exit;
	dinfo.err->emit_message = jpeg_emit_message;
	
        jpeg_create_decompress(&dinfo);

	jsrc.init_source = jpeg_src_init;
	jsrc.term_source = jpeg_src_noop; 
	jsrc.fill_input_buffer = jpeg_src_fill_input;
	jsrc.skip_input_data = jpeg_src_skip_input;
	dinfo.src = &jsrc;
	
	if (setjmp(errjmp))
			return IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_DECOMP;

	bmp = new_raw_bitmap();
	*out = bmp;
	jpeg_read_header(&dinfo, TRUE);
	jpeg_start_decompress(&dinfo);
	
	imgsize=dinfo.output_width*dinfo.output_height*dinfo.output_components;
	row_stride=dinfo.output_width*dinfo.output_components;
	
	bmp->width=dinfo.output_width;
	bmp->height=dinfo.output_height;
	bmp->bpp=dinfo.output_components;

	/* MaxUncompressedImageRatio checking */
	/* this is not the real raw_size (libjpeg may downscale certain
	   components internally) but, instead, the bitmap size to be
	   allocated in memory */
	raw_size = (long long int) bmp->width * (long long int) bmp->height * (long long int) bmp->bpp;
	if ((max_raw_size > 0) && (raw_size > max_raw_size))
		return (IMG_RET_TOO_EXPANSIVE);

	bmp->bitmap = (unsigned char*)malloc (imgsize);
	BufferP=bmp->bitmap;
	while (dinfo.output_scanline < dinfo.output_height)
	{
			if (jpeg_read_scanlines(&dinfo,(JSAMPARRAY)(&BufferP),1) == 0)
				break;
			BufferP+=row_stride;
	}
	
	jpeg_finish_decompress(&dinfo);
	jpeg_destroy_decompress(&dinfo);
	return IMG_RET_OK;
}

static boolean jpeg_dest_empty_output_buffer()
{
		return FALSE; //output file larger than original..
}

// needed to satisfy libjpeg (see compress_to_jpg() and bitmap2jpg())
static void jpeg_dest_init_void (j_compress_ptr cinfo)
{
}

// needed to satisfy libjpeg (see compress_to_jpg() and bitmap2jpg())
static void jpeg_dest_term_void (j_compress_ptr cinfo)
{
}

int bitmap2jpg(raw_bitmap  * bmp, int quality, char ** outb, int * outl)
{
	struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
	struct jpeg_destination_mgr memdest;
	int row_stride = bmp->width*bmp->bpp;
	int i,j;
	unsigned char *BufferP, *noA;
	int outb_size = *outl;

 	cinfo.err = jpeg_std_error(&jerr);
	cinfo.err->error_exit = jpeg_error_exit;
	cinfo.err->emit_message = jpeg_emit_message;

	if (bmp->bitmap == NULL)
		return IMG_RET_SOFTWARE_BUG;

	//translate i -> error code ?
	if((i = setjmp(errjmp)))
		return IMG_RET_ERR_UNKNOWN + IMG_RET_FLG_WHILE_COMPRESS;

	jpeg_create_compress (&cinfo);
	
	memdest.init_destination = jpeg_dest_init_void;
	memdest.empty_output_buffer = jpeg_dest_empty_output_buffer;
	memdest.term_destination = jpeg_dest_term_void;

	cinfo.dest = &memdest;
	memdest.next_output_byte = *outb = malloc (sizeof (char *) * outb_size);
	memdest.free_in_buffer = outb_size;

	cinfo.image_width = bmp->width; 	/* image width and height, in pixels */
	cinfo.image_height = bmp->height;

	if(bmp->bpp <= 2){
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	}else{
		cinfo.input_components = 3;	
		cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
	}
		
	jpeg_set_defaults(&cinfo);

	if(quality > 100){
		quality -=128;
		cinfo.smoothing_factor = 10;
	}

	cinfo.dct_method = JDCT_FLOAT;	
	cinfo.optimize_coding = TRUE;
	jpeg_set_quality(&cinfo, quality, FALSE );

	jpeg_start_compress(&cinfo, TRUE);
	BufferP = bmp->bitmap;

	if(bmp->bpp != cinfo.input_components)
		noA = (unsigned char *)malloc(bmp->width * cinfo.input_components);

	for(i=0; (i == cinfo.next_scanline) && (cinfo.next_scanline < cinfo.image_height); i++) {
		//remove alpha channel if any
		if(bmp->bpp != cinfo.input_components){
			unsigned char * p = noA;
			for(j=0;j < bmp->width * bmp->bpp;j++)
				if(bmp->bpp - 1 != (j % bmp->bpp)){
					*p = BufferP[j];
					p++;
				}
		}else noA = BufferP;
		
		jpeg_write_scanlines (&cinfo,(JSAMPARRAY)(&noA),1);
		BufferP += row_stride;
	}

	if (i < cinfo.image_height) {
		jpeg_abort_compress(&cinfo);
		*outl = -1;
		free (*outb);
		return IMG_RET_ERR_OUT_OF_MEM + IMG_RET_FLG_WHILE_COMPRESS;
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	*outl = outb_size - cinfo.dest->free_in_buffer;
	return IMG_RET_OK;
}

//what about gamma?
int rgb2gray(raw_bitmap * bmp) {
	int i, Y;
	int size = bmp->height * bmp->width;

	if (bmp->bitmap == NULL)
		return IMG_RET_FLG_WHILE_TRANSFORM + IMG_RET_ERR_BAD_DATA_FORMAT;	

	if (bmp->bpp < 3)
		return IMG_RET_OK; /* already grayscale */

	bmp->pal_bpp = bmp->bpp;

	if (bmp->bpp == 3) {
		/* RGB */
		for (i = 0; i < size; i++) {
			//approx. formula from libpng
			Y = (6969 * bmp->bitmap [3 * i] + 
				23434 * bmp->bitmap [3 * i + 1] + 
				2365 * bmp->bitmap [3 * i + 2]) / 32768;
			if (Y > 255)
				Y = 255;

			bmp->bitmap [i] = Y;
		}
	} else {
		/* RGBA */
		for (i = 0; i < size; i++) {
			//approx. formula from libpng
			Y = (6969 * bmp->bitmap [4 * i] + 
				23434 * bmp->bitmap [4 * i + 1] + 
				2365 * bmp->bitmap [4 * i + 2]) / 32768;
			if (Y > 255)
				Y = 255;

			bmp->bitmap [i * 2] = Y;
			bmp->bitmap [i * 2 + 1] = bmp->bitmap [4 * i + 3];
		}
	}

	bmp->bpp -= 2;
	return IMG_RET_OK;
}

static int depalettize (raw_bitmap * bmp)
{
	int i, j;
	int size = bmp->height * bmp->width;
	unsigned char *Buffer, *BufferP;
	
	if ((bmp->pal_entries <= 0) || (bmp->raster == NULL))
		return IMG_RET_FLG_WHILE_TRANSFORM + IMG_RET_ERR_BAD_DATA_FORMAT;

	Buffer = BufferP = (unsigned char*) malloc (size * bmp->pal_bpp);
	
	for (i = 0; i < size; i++)
		for(j = 0; j < bmp->pal_bpp; j++)
			*BufferP++ = bmp->palette [bmp->raster [i] * bmp->pal_bpp + j];

	bmp->bpp = bmp->pal_bpp;
	bmp->bitmap = Buffer;
	
	return IMG_RET_OK;	
}

#ifdef JP2K

/*
 * R = Y + (1.4075 * (V - 128));
 * G = Y - (0.3455 * (U - 128) - (0.7169 * (V - 128));
 * B = Y + (1.7790 * (U - 128);
 *
 * Y = 0.299*R + 0.587*G + 0.114*B
 * U = -0.169*R - 0.331*G + 0.500*B + 128.0
 * V = 0.500*R - 0.419*G - 0.081*B + 128.0
 */                          

/* convert image from RGB/RGBA to YUV/YUVA */
// TODO: optimize this
int rgb2yuv (raw_bitmap * bmp) {
	float srcR, srcG, srcB;
	float dstY, dstU, dstV;
	int imgsize;
	int step = bmp->bpp;
	int rpos;
	unsigned char *read_base, *write_base;
	int i;

	imgsize = bmp->width * bmp->height * bmp->bpp;
	
	/* allocate YUV bitmap buffer if needed */
	if (bmp->bitmap_yuv == NULL)
		bmp->bitmap_yuv = (unsigned char *) malloc (imgsize);

	read_base = bmp->bitmap;
	write_base = bmp->bitmap_yuv;

	for (rpos = 0; rpos < imgsize; rpos += step) {
		srcR = *(read_base + rpos);
		srcG = *(read_base + rpos + 1);
		srcB = *(read_base + rpos + 2);
		
		dstY = (0.299 * srcR) + (0.587 * srcG) + (0.114 * srcB);
		dstU = (-0.169 * srcR) - (0.331 * srcG) + (0.500 * srcB) + 128;
		dstV = (0.500 * srcR) - (0.419 * srcG) - (0.081 * srcB) + 128;

		if (dstU > 255)
			dstU = 255;
		if (dstV > 255)
			dstV = 255;
		if (dstU < 0)
			dstU = 0;
		if (dstV < 0)
			dstV = 0;
		
		*(write_base + rpos) = dstY;
		*(write_base + rpos + 1) = dstU;
		*(write_base + rpos + 2) = dstV;
	}

	/* migrate alpha channel, if exists */
	if (bmp->bpp == 4) {
		for (i = 3; i < imgsize; i += step) {
			bmp->bitmap_yuv [i] = bmp->bitmap [i];
		}
	}

	return IMG_RET_OK;
}

/* convert image from YUV/YUVA to RGB/RGBA */
// TODO: optimize this
int yuv2rgb (raw_bitmap * bmp) {
	float srcY, srcU, srcV;
	float dstR, dstG, dstB;
	int imgsize;
	int step = bmp->bpp;
	int rpos;
	unsigned char *read_base, *write_base;
	int i;

	imgsize = bmp->width * bmp->height * bmp->bpp;
	
	/* allocate RGB bitmap buffer if needed */
	if (bmp->bitmap_yuv == NULL)
		bmp->bitmap = (unsigned char *) malloc (imgsize);
	
	read_base = bmp->bitmap_yuv;
	write_base = bmp->bitmap;

	for (rpos = 0; rpos < imgsize; rpos += step) {

		srcY = *(read_base + rpos);
		srcU = *(read_base + rpos + 1);
		srcV = *(read_base + rpos + 2);
		
		dstR = srcY + (1.4075 * (srcV - 128));
		dstG = srcY - (0.3455 * (srcU - 128)) - (0.7169 * (srcV - 128));
		dstB = srcY + (1.7790 * (srcU - 128));

		if (dstR > 255)
			dstR = 255;
		if (dstG > 255)
			dstG = 255;
		if (dstB > 255)
			dstB = 255;
		if (dstR < 0)
			dstR = 0;
		if (dstG < 0)
			dstG = 0;
		if (dstB < 0)
			dstB = 0;

		*(write_base + rpos) = dstR;
		*(write_base + rpos + 1) = dstG;
		*(write_base + rpos + 2) = dstB;
	}

	/* migrate alpha channel, if exists */
	if (bmp->bpp == 4) {
		for (i = 3; i < imgsize; i += step) {
			bmp->bitmap [i] = bmp->bitmap_yuv [i];
		}
	}

	return IMG_RET_OK;
}

/* compress a bitmap to jpeg.
 * we CANNOT use bitmap2jpg() because that function is promiscually making reference to global vars and who knows what else.
 * jpegout returns a pointer to a buffer containing a pointer to jpeg data (should be free'ed manually later)
 * needs: maxoutl = max size the compressed data may be
 * provides: *outl = size of the compressed data
 * returns: 0 if ok, !=0 error */
int compress_to_jpg (raw_bitmap *bmp, int quality, int maxoutl, int *outl, unsigned char **jpegout)
{
	unsigned char *outb;	// compressed data will be stored here
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	unsigned char *BufferP, *noA;
	int i, j;
	struct jpeg_destination_mgr dest;
	int outbufsize;
	int row_stride;		    // physical row width in image buffer

	if (bmp->bitmap == NULL)
		depalettize (bmp);

	cinfo.err = jpeg_std_error(&jerr);
	//cinfo.err->error_exit = jpeg_error_exit;
	cinfo.err->emit_message = jpeg_emit_message;

	jpeg_create_compress(&cinfo);

	// jpeg does not support alpha, so it doesn't count in this case (if present)
	if(bmp->bpp <= 2){
		cinfo.input_components = 1;	// Y only
		cinfo.in_color_space = JCS_GRAYSCALE;
	}else{
		cinfo.input_components = 3;	// R, G, B
		cinfo.in_color_space = JCS_RGB;
	}

	// we want output to memory, set things accordingly
	outbufsize = maxoutl;	// max compressed size and buffer size are limited by maxoutl
	outb = malloc (outbufsize);
	cinfo.dest = &dest;
	dest.next_output_byte = outb;
	dest.free_in_buffer = outbufsize;
	//
	dest.init_destination = jpeg_dest_init_void;
	dest.empty_output_buffer = jpeg_dest_empty_output_buffer;
	dest.term_destination = jpeg_dest_term_void;

	// dest.data_precision = XX; // hmm.. this may be interesting later
	cinfo.image_width = bmp->width;
	cinfo.image_height = bmp->height;

	// this command is picky on its position, related to other references
	// of cinfo (some must come before, other ones after this)
	jpeg_set_defaults (&cinfo);

	cinfo.dct_method = JDCT_FLOAT;
	cinfo.optimize_coding = TRUE;
	BufferP = bmp->bitmap;
	row_stride = cinfo.image_width * cinfo.input_components;
	jpeg_set_quality (&cinfo, quality, FALSE);
	jpeg_start_compress (&cinfo, TRUE);

	if (bmp->bpp != cinfo.input_components)
		noA = (unsigned char *) malloc (bmp->width * cinfo.input_components);

	for (i=0; (i == cinfo.next_scanline) && (cinfo.next_scanline < cinfo.image_height); i++) {
		//remove alpha channel if any
		if(bmp->bpp != cinfo.input_components){
			unsigned char * p = noA;
			for (j=0;j < bmp->width * bmp->bpp;j++)
				if(bmp->bpp - 1 != (j % bmp->bpp)){
					*p = BufferP [j];
					p++;
				}
		} else noA = BufferP;
		jpeg_write_scanlines (&cinfo, (JSAMPARRAY) (&noA), 1);
		BufferP += row_stride;
	}

	if (bmp->bpp != cinfo.input_components)
		free (noA);
	
	if (i < cinfo.image_height) {
		jpeg_abort_compress (&cinfo);
		*outl = -1;
		free (outb);
		return (IMG_RET_ERR_OUT_OF_MEM + IMG_RET_FLG_WHILE_COMPRESS);
	}

	jpeg_finish_compress(&cinfo);
	
	*outl = outbufsize - cinfo.dest->free_in_buffer;
	*jpegout = outb;
	
	jpeg_destroy_compress(&cinfo);

	return (IMG_RET_OK);
}

/* this calculates the _real_ jp2 rawsize (differently from the
   routine embbeded in estimate_jp2rate_from_quality()) */
int calculate_jp2_rawsize (raw_bitmap *bmp, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int*bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA, int discard_alpha)
{
	int rawsize = 0;
	int bpp;	// bpp minus alpha channel
	const int *cbitlen;	// 0:first component, 1:second component.. and so on
	const int *ucsampling; // 0:xpos0, 1:ypos0, 2:xstep0, 3:ystep0... xpos1, ypos1.. and so on
	const int *this_ucsampling;
	int cmptno;
	int cwidth, cheight;

	bpp = bmp->bpp;
	if (discard_alpha) {
		if (bmp->bpp == 4)
			bpp = 3;
		else if (bmp->bpp == 2)
			bpp = 1;
	}

	/* see which colorspace we'll really use and set things accordingly */
	if ((target_clrspc == CENC_YUV) && (bmp->bpp >= 3)) {
		cbitlen = bitlenYUVA;
		ucsampling = csamplingYUVA;
	} else {
		/* the other option is RGBA/YA (RGBA when color, YA when B&W) */
		if (bmp->bpp >= 3) {
			cbitlen = bitlenRGBA;
			ucsampling = csamplingRGBA;
		} else {
			cbitlen = bitlenYA;
			ucsampling = csamplingYA;
		}
	}

	/* calculate rawsize of the picture, taking into account
	 * bit length and (under)sampling of each component while compressing to jp2 */
	for (cmptno = 0; cmptno < bpp; cmptno++) {
		this_ucsampling = ucsampling + (4 * cmptno);
		cwidth = return_downscaled_array_len (bmp->width, this_ucsampling [0], this_ucsampling [2]);	// xpos, xtep
		cheight = return_downscaled_array_len (bmp->height, this_ucsampling [1], this_ucsampling [3]);	// xpos, xtep
		rawsize += (cwidth * cheight * cbitlen [cmptno]) / 8;
	}

	/* preventive measure (no problems so far, but let's be safe):
	   avoid downwards rounding of rawsize in cases
	   one or more of the components is less than 8 bits */
	if ((cwidth > 0) && (cheight > 0))
		rawsize += bmp->bpp;

	return (rawsize);
}

/* returns the necessary jp2-rate to have results
 * roughly similar to jpeg-type quality.
 * requires pre-malloc'ed outbuf array sized: (bmp->width * bmp->height * bmp->bpp)
 * limitations: the alpha channel is not taken into account */
/* returns < 0 if error */
float estimate_jp2rate_from_quality (raw_bitmap *bmp, int quality, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int*bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA)
{
	float rawsize;
	float rate;
	int outl;
	int maxoutl;
	unsigned char *jpegout;

	/* Calculate rawsize of the picture, taking into account
	 * bit length and (under)sampling of each component while compressing to jp2.
	 * Note: we cannot take alpha into account since jpeg does not support that.
	 * that means innacurate compression rate when dealing with pictures containing such component. */
	rawsize = calculate_jp2_rawsize (bmp, target_clrspc, bitlenYA, bitlenRGBA, bitlenYUVA, csamplingYA, csamplingRGBA, csamplingYUVA, 1);

	maxoutl = rawsize + 1000;	// avoids a too-small buffer if the raw picture is minuscle (libjpeg recommends ~600 bytes for headers)
	if (compress_to_jpg (bmp, quality, maxoutl, &outl, &jpegout) != IMG_RET_OK) {
		debug_log_puts ("ERROR: Unable to compress to jpeg (estimate_jp2rate_from_quality).");
		return (-1);
	}
	free (jpegout);	// the data itself is not needed in this routine, we just want to know its size
		
	rate = (outl + 200) / rawsize;	// estimate the JP2k rate

	return (rate);
}

static int bitmap2jp2 (raw_bitmap *bmp, float rate, char **outb, int *outl, const t_color_space target_clrspc, const int *bitlenYA, const int *bitlenRGBA, const int *bitlenYUVA, const int *csamplingYA, const int *csamplingRGBA, const int *csamplingYUVA)
{
	jas_image_t *jimg;
	jas_matrix_t *jmatrix [4];
	jas_image_cmptparm_t cmptparms [4];
	jas_image_cmptparm_t *cmptparm;
	int pos0 = -1, pos1 = -1, pos2 = -1, pos3 = -1;     // holds jp2-component number for each -- FIXME: this is pointless, we may use constants in this routine instead.
	// uint_fast16_t cmptno; // TODO: should i use this instead? check that later
	int x, y, cmptno;
	jas_stream_t *js;
	char mode[30], *cmode;
	int outfmt;
	unsigned char *pixel_base;
	const int *cbitlen;		// points to either bitlenYA/bitlenRGBA/bitlenYUVA
	int cshiftR [4];		// right shift for each component
	unsigned char * ubitmap;	// used bitmap while compressing, points to either bmp->bitmap or bmp->bitmap_yuv
	int csampling_effective [16];	/* usually a mirror of either bitlenYA/bitlenRGBA/bitlenYUVA, but it will change to proper values
					   if original parameters are invalid _or_
					   invalid for this specific image (image to small for the requested downsampling, for example) */
	int cwidth [4];			// component width, (may differ if each component have different sampling from each other)
	int cheight [4];		// analogous to cwidth
	const int *ucsampling;		// points to either bitlenYA/bitlenRGBA/bitlenYUVA

	if(jas_init()) return 5;

	if (bmp->bitmap == NULL)
		return IMG_RET_SOFTWARE_BUG;

	/* see which colorspace we'll really use and set things accordingly */
	if ((target_clrspc == CENC_YUV) && (bmp->bpp >= 3)) {
		/* requested YUVA and it's a color bitmap */
		rgb2yuv (bmp);	// the default buffer is RGB(A), convert to YUV(A) -- this will allocate bmp->bitmap_yuv automatically, if needed
		ubitmap = bmp->bitmap_yuv;
		cbitlen = bitlenYUVA;
		ucsampling = csamplingYUVA;
	} else {
		/* the other option is RGBA/YA (RGBA when color, YA when B&W) */
		ubitmap = bmp->bitmap;
		if (bmp->bpp >= 3) {
			cbitlen = bitlenRGBA;
			ucsampling = csamplingRGBA;
		} else {
			cbitlen = bitlenYA;
			ucsampling = csamplingYA;
		}
	}

	// change the sampling rate for each component (scaling each component to a new (lower) resolution, in a way)
	jp2_downsample_image_components (ubitmap, bmp->width, bmp->height, bmp->bpp, ucsampling, csampling_effective, cwidth, cheight);

	// initialize components
	for (cmptno = 0, cmptparm = cmptparms; cmptno < bmp->bpp; ++cmptno, ++cmptparm) {
		cmptparm->tlx = csampling_effective [(cmptno * 4) + 0];	// 0 = max quality
		cmptparm->tly = csampling_effective [(cmptno * 4) + 1]; // 0 = max quality
		cmptparm->hstep = csampling_effective [(cmptno * 4) + 2]; // 1 = max quality
		cmptparm->vstep = csampling_effective [(cmptno * 4) + 3]; // 1 = max quality
		cmptparm->width = cwidth [cmptno];
		cmptparm->height = cheight [cmptno];
		cmptparm->prec = cbitlen [cmptno];
		cmptparm->sgnd = false;	// we only generate unsigned components

		cshiftR [cmptno] = 8 - cbitlen [cmptno]; // calculate the resampling shift for this component
	}

	if ((jimg = jas_image_create (bmp->bpp, cmptparms, JAS_CLRSPC_UNKNOWN)) == NULL)
		return IMG_RET_ERR_UNKNOWN; // unable to create image

	// initialize image parameters
	// FIXME: this does not work, libjasper always 'guess' the width/height based on the components' dimensions
	//        unfortunately certain combinations of components' dimensions make libjasper output a slightly smaller picture
	/*
	jimg->tlx_ = 0;
	jimg->brx_ = bmp->width;
	jimg->tly_ = 0;
	jimg->bry_ = bmp->height;
	*/
	
	// more components initialization
	switch (bmp->bpp) {
	case 1:
	case 2:
		jas_image_setclrspc (jimg, JAS_CLRSPC_SGRAY);
		jas_image_setcmpttype (jimg, 0, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_GRAY_Y));
		pos0 = 0;

		if (bmp->bpp == 2) {
			jas_image_setcmpttype (jimg, 1, JAS_IMAGE_CT_OPACITY);
			pos1 = 1;
		}
		break;
	case 3:
	case 4:
		if (target_clrspc == CENC_YUV) {
			/* YUV */
			jas_image_setclrspc (jimg, JAS_CLRSPC_SYCBCR);
			jas_image_setcmpttype (jimg, 0, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_Y));
			pos0 = 0;
			jas_image_setcmpttype (jimg, 1, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_CB));
			pos1 = 1;
			jas_image_setcmpttype (jimg, 2, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_CR));
			pos2 = 2;
		} else {
			/* RGB */
			jas_image_setclrspc (jimg, JAS_CLRSPC_SRGB);
			jas_image_setcmpttype (jimg, 0, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R));
			pos0 = 0;
			jas_image_setcmpttype (jimg, 1, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G));
			pos1 = 1;
			jas_image_setcmpttype (jimg, 2, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B));
			pos2 = 2;
		}
		
		if (bmp->bpp == 4) {
			jas_image_setcmpttype (jimg, 3, JAS_IMAGE_CT_OPACITY);
			pos3 = 3;
		}
		break;
	}

	// allocate intermediate line buffers
	// note: although we do not need parallel buffers for each component (see routines below), and theoretically
	//       we could use only one sized as the widest component,
	//       libjasper does not accept a line buffer with bigger width than the component's width
	//       (what will happen if we use component with different sampling steps, thus different width).
	for (cmptno = 0; cmptno < bmp->bpp; cmptno++) {
		if ((jmatrix [cmptno] = jas_matrix_create (1, cwidth [cmptno])) == NULL)
			return IMG_RET_ERR_UNKNOWN; // memory allocation error
	}

	// transfer image to jimg, through intermediate buffer
	for (cmptno = 0; cmptno < bmp->bpp; cmptno++) {
		pixel_base = ubitmap + cmptno;
		
		for (y = 0; y < cheight [cmptno]; y++) {
			// populate intermediate line buffer
			for (x = 0; x < cwidth [cmptno]; x++) {
				jas_matrix_setv (jmatrix [cmptno], x, *pixel_base >> cshiftR [cmptno]);
				pixel_base += bmp->bpp;
			}
		
			// transfer line to jimg
			if (jas_image_writecmpt (jimg, cmptno, 0, y, cwidth [cmptno], 1, jmatrix [cmptno]))
				return IMG_RET_ERR_UNKNOWN; // unable to transfer line to jimg
		}
	}

	js = jas_stream_memopen (0, 0);
	if(js == NULL) return 0;
	
	if(rate > 0.9999 ){
		cmode= "int"; 
		rate = 1.0;
	}
	else cmode = "real";

	snprintf(mode, sizeof(mode), "mode=%s rate=%f", cmode, rate);
	outfmt = jas_image_strtofmt ("jp2");

	// encode intermediate buffer into jp2 data
	if (jas_image_encode (jimg, js, outfmt, mode)) {
		// unable to encode to jp2
		return 0; // is that an error?
	}

	jas_stream_flush(js);
	//May not be future compatible!
	jas_stream_memobj_t * jmem = (jas_stream_memobj_t *)js->obj_;
	*outl = jmem->len_;
	*outb = jmem->buf_;
	jmem->myalloc_ = 0; //don't delete buf_ on closing
	jas_stream_close(js);//close

	// TODO: archaic code already commented. check later if there's something useable from this, otherwise delete it.
	/*	
	outlen = jas_stream_tell(js);
	outbuf = malloc(outlen);
	jas_stream_rewind(js);
	jas_stream_read(js, outbuf, outlen);
	jas_stream_close(js);*/
	
	return IMG_RET_OK;
}

/* this routine decodes a JP2K file within what is supported by the bitmap2jp2() function */
int jp22bitmap(char *inbuf, int insize, raw_bitmap **out, long long int max_raw_size)
{
	jas_stream_t *js;
	jas_image_t *jimg;
	jas_matrix_t *jmatrix [4];
	int components, colorspace;
	int pos0 = -1, pos1 = -1, pos2 = -1, pos3 = -1;	// posX where 'X' is the position in the internal buffer, holds the jp2-component number equivalent - its meanings depends on the color model (Y, RGBA, YUV etc)
	int imgsize;
	int x, y, i, cmptno;
	raw_bitmap *bmp;
	unsigned char *pixel_base;
	int cbitlen [4];		// (R, G, B, [alpha]) || (Y, U, V [alpha]) || (Y, [alpha])
	int cwidth [4];
	int cheight [4];
	int csampling [16];		// max_components * 4
	unsigned char * ubitmap;	// used bitmap while decompressing, points to either bmp->bitmap or bmp->bitmap_yuv
	long long int raw_size;

	if(jas_init()) return 5;

	js = jas_stream_memopen (inbuf, insize);
	if ((jimg = jas_image_decode (js, -1, 0)) == NULL) {
		debug_log_puts ("ERROR: JP2K - unable to decode data.");
		return IMG_RET_ERR_UNKNOWN;
	}

	bmp = new_raw_bitmap ();
	*out = bmp;

	/* get image information */
	components = jas_image_numcmpts (jimg);
	bmp->width = jas_image_width (jimg);
	bmp->height = jas_image_height (jimg);
	colorspace = jas_image_clrspc (jimg);

	// TODO: component signedness is not supported (assumes: unsigned)

	i = 0;
	while (i < components) {
		int comptype = jas_image_cmpttype (jimg, i);

		/* the following looks kludgy because
		 * libjasper defines some things in a peculiar way, like
		 * both JAS_CLRSPC_CHANIND_RGB_R and JAS_CLRSPC_CHANIND_GRAY_Y as 0 */ 

		switch (colorspace) {
		case JAS_CLRSPC_SGRAY:
			switch (comptype) {
			case JAS_CLRSPC_CHANIND_GRAY_Y:
				pos0 = i;
				break;
			/* bug workaround: libjasper renumerates JAS_IMAGE_CT_OPACITY to 1 */
			case 1:
			case JAS_IMAGE_CT_OPACITY:
				pos1 = i;
				break;
			default:
				debug_log_printf ("ERROR: JP2K - component type not supported: %x\n", comptype);
				jas_stream_close (js);
				return IMG_RET_ERR_NOT_IMPL_YET;
				break;
			}
			break;
		case JAS_CLRSPC_SYCBCR:
			switch (comptype) {
			case JAS_CLRSPC_CHANIND_YCBCR_Y:
				pos0 = i;
				break;
			case JAS_CLRSPC_CHANIND_YCBCR_CB:
				pos1 = i;
				break;
			case JAS_CLRSPC_CHANIND_YCBCR_CR:
				pos2 = i;
				break;
			/* bug workaround: libjasper renumerates JAS_IMAGE_CT_OPACITY to 3 */
			case 3:
			case JAS_IMAGE_CT_OPACITY:
				pos3 = i;
				break;
			default:
				debug_log_printf ("ERROR: JP2K - component type not supported: %x\n", comptype);
				jas_stream_close (js);
				return IMG_RET_ERR_NOT_IMPL_YET;
				break;
			}
			break;
		case JAS_CLRSPC_SRGB:
			switch (comptype) {
			case JAS_CLRSPC_CHANIND_RGB_R:
				pos0 = i;
				break;
			case JAS_CLRSPC_CHANIND_RGB_G:
				pos1 = i;
				break;
			case JAS_CLRSPC_CHANIND_RGB_B:
				pos2 = i;
				break;
			/* bug workaround: libjasper renumerates JAS_IMAGE_CT_OPACITY to 3 */
			case 3:
			case JAS_IMAGE_CT_OPACITY:
				pos3 = i;
				break;
			default:
				debug_log_printf ("ERROR: JP2K - component type not supported: %x\n", comptype);
				jas_stream_close (js);
				return IMG_RET_ERR_NOT_IMPL_YET;
				break;
			}
			break;
		}

		cbitlen [i] = jas_image_cmptprec (jimg, i);
		cwidth [i] = jas_image_cmptwidth (jimg, i);
		cheight [i] = jas_image_cmptheight (jimg, i);
		csampling [(i * 4) + 0] = jas_image_cmpttlx(jimg, i);
		csampling [(i * 4) + 1] = jas_image_cmpttly(jimg, i);
		csampling [(i * 4) + 2] = jas_image_cmpthstep(jimg, i);
		csampling [(i * 4) + 3] = jas_image_cmptvstep(jimg, i);
			
		i++;
	}
	
	// see what kind of bitmap we're dealing with
	switch (colorspace) {
	case JAS_CLRSPC_SGRAY:
		// checks alpha channel presence
		if (pos1 >= 0)
			bmp->bpp = 2;
		else
			bmp->bpp = 1;
		break;
	case JAS_CLRSPC_SYCBCR:
	case JAS_CLRSPC_SRGB:
		// checks alpha channel presence
		if (pos3 >= 0)
			bmp->bpp = 4;
		else
			bmp->bpp = 3;
		break;
	default:
		debug_log_printf ("ERROR: JP2K - colorspace not supported: %x\n", colorspace);
		jas_stream_close (js);
		return IMG_RET_ERR_NOT_IMPL_YET;
		break;
	}
	// TODO: check if there are no components missing from what is expected from the colorspace

	/* MaxUncompressedImageRatio checking */
	/* this is not the real raw_size
	   (it does not take into consideration bit length per component etc)
	   but, instead, the bitmap size to be allocated in memory */
	raw_size = (long long int) bmp->width * (long long int) bmp->height * (long long int) bmp->bpp;
	if ((max_raw_size > 0) && (raw_size > max_raw_size))
		return (IMG_RET_TOO_EXPANSIVE);

	// calculate bitmap size and allocate its buffer
	imgsize = bmp->width * bmp->height * bmp->bpp;
	bmp->bitmap = (unsigned char *) malloc (imgsize);
	// allocate YUV buffer too, if image comes in that format
	if (colorspace == JAS_CLRSPC_SYCBCR) {
		bmp->bitmap_yuv = (unsigned char *) malloc (imgsize);
		ubitmap = bmp->bitmap_yuv;
	} else {
		ubitmap = bmp->bitmap;
	}
	
	// allocate intermediate line buffers
	// note: although we do not need parallel buffers for each component (see routines below), and theoretically
	//       we could use only one sized as the widest component,
	//       libjasper does not accept a line buffer with bigger width than the component's width
	//       (what will happen if we use component with different sampling steps, thus different width).
	for (cmptno = 0; cmptno < bmp->bpp; cmptno++) {
		if ((jmatrix [cmptno] = jas_matrix_create (1, cwidth [cmptno])) == NULL)
			return IMG_RET_ERR_UNKNOWN; // memory allocation error
	}

	// transfer image to jimg, through intermediate buffer
	for (cmptno = 0; cmptno < bmp->bpp; cmptno++) {
		pixel_base = ubitmap + cmptno;
		
		for (y = 0; y < cheight [cmptno]; y++) {
			// transfer decompressed image line, to line buffer
			jas_image_readcmpt (jimg, cmptno, 0, y, cwidth [cmptno], 1, jmatrix [cmptno]);

			// transfer data from line buffer to final bitmap
			for (x = 0; x < cwidth [cmptno]; x++) {
				*pixel_base = UPSAMPLE_TO_U8BIT(jas_matrix_getv (jmatrix [cmptno], x), cbitlen [cmptno]);
				pixel_base += bmp->bpp;
			}
		}
	}

	// are any of the image components scaled-down? if so, rescale them to the intended display size.
	jp2_upsample_image_components (ubitmap, bmp->width, bmp->height, bmp->bpp, csampling, cwidth, cheight);
	
	/* if YUV, convert back to RGB */
	if (colorspace == JAS_CLRSPC_SYCBCR)
		yuv2rgb (bmp);

	jas_stream_close (js);
	return IMG_RET_OK;
}

#endif

/* alpha channel optimization (= removal under certain circumstances)
   applies to non-palette images only (RGBA, YUVA, YA) */
static int optimize_alpha_channel (raw_bitmap *bmp)
{
	unsigned char *which_bitmap;
	unsigned char *read_bitmap;
	unsigned char *write_bitmap;
	int i;
	int do_remove = 1;
	unsigned long long int totpix = bmp->width * bmp->height;
	unsigned long long int alpha_avg = 0;
	int also_rgb = 1; /* true == 2 ! */

	if (! ((bmp->bitmap != NULL) || (bmp->bitmap_yuv != NULL)))
		return 0;
	if (! ((bmp->bpp == 2) || (bmp->bpp == 4)))
		return 0;

	debug_log_puts ("Image has alpha channel. Analysing...");

	if (bmp->bitmap_yuv != NULL) {
		which_bitmap = bmp->bitmap_yuv;

		/* assumes bitmap and bitmap_yuv are in sync
		   and that's how it should be */
		if (bmp->bitmap != NULL)
			also_rgb++;
	} else {
		which_bitmap = bmp->bitmap;
	}

	read_bitmap = which_bitmap;
	read_bitmap += (bmp->bpp - 1);

	/* determine average alpha channel value */
	for (i = 0; i < totpix; i++) {
		alpha_avg += *read_bitmap;
		read_bitmap += bmp->bpp;	
	}
	alpha_avg = (alpha_avg * 1000000) / (totpix * 255);
	debug_log_printf ("Image average alpha channel opacity: %d/1000000\n", alpha_avg);

	/* report and decide what to do */
	if (alpha_avg == 1000000) {
		debug_log_puts ("Image has unnecessary alpha channel. Image is completely opaque.");
	} else if (alpha_avg >= AlphaRemovalMinAvgOpacity) {
		debug_log_puts ("Image is opaque enough to have its alpha channel removed.");
	} else {
		debug_log_puts ("Keeping image alpha channel.");
		do_remove = 0;
	}

	/* remove alpha channel */
	if (do_remove) {
		debug_log_puts ("Removing image alpha channel...");
		read_bitmap = write_bitmap = which_bitmap;

		while (also_rgb--) {
			if (bmp->bpp == 4) {
				for (i = 0; i < totpix; i++) {
					*(write_bitmap++) = *(read_bitmap++);
					*(write_bitmap++) = *(read_bitmap++);
					*(write_bitmap++) = *(read_bitmap++);
					read_bitmap++;
				}
			} else {
				/* bmp->bpp==2 then */
				for (i = 0; i < totpix; i++) {
					*(write_bitmap++) = *(read_bitmap++);
					read_bitmap++;
				}
			}

			read_bitmap = write_bitmap = bmp->bitmap;
		}

		bmp->bpp -= 1;
	}

	return 0;
}

/* optimize palette of raster image
   if provided image has no palette (RGB etc), does nothing and returns.
   returns: ==0 ok, !=0 error */
static int optimize_palette (raw_bitmap *bmp)
{
	int color_entry [256];
	int color_remap [256]; /* data = old color number */
	int color_remap2 [256]; /* data = new color number */
	unsigned char new_palette [1024];
	int rawsize = bmp->width * bmp->height;
	int i, j;
	int cur_id, cur_id2, used_colors = 0;
	int useful_alpha = 0;
	int total_alpha = 0;

	/* is that really a image with palette? */
	if ((bmp->pal_entries <= 0) || (bmp->raster == NULL))
		return 1;

	/* remove unnecessary palette entries */

	/* determine used_colors */
	for (i = 0; i < 256; i++)
		color_entry [i] = 0;
	for (i = 0; i < rawsize; i++)
		color_entry [bmp->raster [i]] = 1;
	for (i = 0; i < 256; i++)
		used_colors += color_entry [i];

	if (used_colors < bmp->pal_entries) {
		debug_log_printf ("Image has unnecessary palette entries. Allocated: %d  Used: %d\n", bmp->pal_entries, used_colors);

		/* generate remap tables */
		cur_id = 0;
		for (i = 0; i < 256; i++) {
			if (color_entry [i] != 0) {
				color_remap [cur_id] = i;
				color_remap2 [i] = cur_id;
				cur_id++;
			}
		}

		/* regenerate palette */
		for (i = 0; i < used_colors; i++) {
			for (j = 0; j < bmp->pal_bpp; j++) {
				new_palette [i * bmp->pal_bpp + j] = bmp->palette [color_remap [i] * bmp->pal_bpp + j];
			}
		}
		memcpy (bmp->palette, new_palette, sizeof (unsigned char) * bmp->pal_bpp * used_colors);

		/* regenerate bitmap */
		for (i = 0; i < rawsize; i++) {
			bmp->raster [i] = color_remap2 [bmp->raster [i]];
		}

		bmp->pal_entries = used_colors;
	}

	/* detect unecessary alpha in palette and remove it */

	if (bmp->pal_bpp == 4) {
		for (i = 0; i < bmp->pal_entries; i++) {
			if (bmp->palette [i * 4 + 3] != (unsigned char) 0xff)
				useful_alpha = 1;
		}

		if (useful_alpha == 0) {
			debug_log_puts ("Image has unnecessary alpha in palette entries.");
			for (i = 0; i < bmp->pal_entries; i++) {
				bmp->palette [i * 3] = bmp->palette [i * 4];
				bmp->palette [i * 3 + 1] = bmp->palette [i * 4 + 1];
				bmp->palette [i * 3 + 2] = bmp->palette [i * 4 + 2];
			}
			bmp->pal_bpp = 3;
		}
	}

	/* optimize colors with alpha entries (put those first), if necessary */

	if (bmp->pal_bpp == 4) {
		for (i = 0; i < bmp->pal_entries; i++) {
			if (bmp->palette [i * 4 + 3] != 0xff)
				total_alpha++;
		}

		if (total_alpha < bmp->pal_entries) {
			j = 0;
			for (i = total_alpha; i < bmp->pal_entries; i++) {
				if (bmp->palette [i * 4 + 3] != 0xff)
					j++;
			}

			if (j > 0) {
				debug_log_puts ("Image has unoptimized transparency.");

				/* reorder palette */

				/* generate remap tables */
				cur_id = 0; /* with alpha */
				cur_id2 = total_alpha; /* without alpha */
				for (i = 0; i < bmp->pal_entries; i++) {
					if (bmp->palette [i * 4 + 3] != 0xff) {
						color_remap [cur_id] = i;
						color_remap2 [i] = cur_id;
						cur_id++;
					} else {
						color_remap [cur_id2] = i;
						color_remap2 [i] = cur_id2;
						cur_id2++;
					}
				}

				/* regenerate palette */
				for (i = 0; i < used_colors; i++) {
					for (j = 0; j < bmp->pal_bpp; j++) {
						new_palette [i * bmp->pal_bpp + j] = bmp->palette [color_remap [i] * bmp->pal_bpp + j];
					}
				}
				memcpy (bmp->palette, new_palette, sizeof (unsigned char) * bmp->pal_bpp * used_colors);

				/* regenerate bitmap */
				for (i = 0; i < rawsize; i++)
					bmp->raster [i] = color_remap2 [bmp->raster [i]];
			}

			/* at this point, transparency is optimized regardless */
			bmp->opt_pal_transp = total_alpha;
		}
	}

	return 0;
}

/* remove alpha channel from RGBA bitmaps,
   does not apply to palettized images */
/* returns: ==0 alpha detected and removed, !=0 failure (nothing changed) */
static int remove_alpha_channel (raw_bitmap *bmp)
{
	int rawsize;
	unsigned char *which_bitmap;
	unsigned char *read_bitmap;
	unsigned char *write_bitmap;
	int i;

	which_bitmap = bmp->bitmap;
	if (which_bitmap == NULL)
		return 1;
	read_bitmap = write_bitmap = which_bitmap;

	rawsize = bmp->width * bmp->height;

	if (bmp->bpp == 4) {
		for (i = 0; i < rawsize; i++) {
			*(write_bitmap++) = *(read_bitmap++);
			*(write_bitmap++) = *(read_bitmap++);
			*(write_bitmap++) = *(read_bitmap++);
			read_bitmap++;
		}
		bmp->bpp = 3;
	} else if (bmp->bpp == 2) {
		for (i = 0; i < rawsize; i++) {
			*(write_bitmap++) = *(read_bitmap++);
			read_bitmap++;
		}
		bmp->bpp = 1;
	} else {
		return 1;
	}

	return 0;
}

