/* jp2tools.c
 * Auxiliary routines for jp2 processing.
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

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "jp2tools.h"
#include "cfgfile.h"

#ifndef M_PIl
#define M_PIl 3.1415926535897932384626433832795029L
#endif

#ifndef RESAMPLER_LANCZOS_RADIUS
#define RESAMPLER_LANCZOS_RADIUS 3
#endif

/* return the number of downscaled elements in a unidimensional array,
 * width = upscaled size
 * given the expected upscaled size and the downlscaled parameters (step and position) */
/* returns <= 0 if invalid array */
int return_downscaled_array_len (const int width, const int xpos, const int xstep)
{
	int useful_width = width - xpos;
	int divided;

	if ((xstep == 0) || (xpos >= width))
		return (0);	// invalid array

	if (xstep == 1)
		return (useful_width);

	if ((useful_width > xstep) && (xstep > 1)) {
		divided = useful_width / xstep;
		if ((divided * xstep) < useful_width)
			return (divided + 1);
		else
			return (divided);
	}
	
	if (useful_width <= xstep)
		return (1);
}

/* compute the neighboor-pixel percentage-contribution-to-pixel table */
void build_downsample_pixelprop_table (int *x_pixel_coord, float *x_pixel_percent, int xstep)
{
	int x_pixel_len = (xstep * 2) - 1;
	float mulstep = 1.0 / xstep;
	int x_middle = xstep - 1;
	float curr_mul;
	int i;
	float sum_pre_percents = 0;

	// central element
	curr_mul = 1.0;
	x_pixel_percent [x_middle] = curr_mul;
	sum_pre_percents += curr_mul;
	x_pixel_coord [x_middle] = 0;
	
	// other elements
	for (i = 1; i < xstep; i++) {
		curr_mul = 1.0 - (mulstep * i);
		x_pixel_percent [x_middle + i] = curr_mul;
		x_pixel_percent [x_middle - i] = curr_mul;
		sum_pre_percents += 2 * curr_mul;
		x_pixel_coord [x_middle + i] = i;
		x_pixel_coord [x_middle - i] = -i;
		
	}
	
	// normalize x_pixel_percent (the sum of all elements must be 1)
	for (i = 0; i < x_pixel_len; i++) {
		x_pixel_percent [i] = x_pixel_percent [i] / sum_pre_percents;
		x_pixel_percent [i] = x_pixel_percent [i] * 0.999999999;     // avoid the need (later) to check whether result > 255 toround to 255
	}
}

/* this works for both x/y calculations, the naming as 'width' etc is just a convention */
void downsample_component_line (unsigned char *x_in_buff, unsigned char *x_out_buff, const int width, const int xpos, const int xstep, const int x_pixel_len, const int *x_pixel_coord, const float *x_pixel_percent)
{
	int i, apos, k;
	float tpix;
	
	/* downsample line */
	apos = 0;
	for (i = xpos; i < width; i += xstep) {
		tpix = 0;
		for (k = 0; k < x_pixel_len; k++)
			tpix += x_in_buff [i + x_pixel_coord [k]] * x_pixel_percent [k];

		// fix rounding error
		// not needed anymore, we avoid rounding errors multiplying the multipliers by 0.99999 (see build_downsample_pixelprop_table())
		//if (tpix > 255)
		//	tpix = 255;

		x_out_buff [apos++] = (unsigned char) tpix;
	}
}

/* downsamples an array of one component (being R, G, B, Y or anything else) */
/* accepts: xstep/ystep >= 1, xpos/ypos >= 0 */
/* carray is both the input and output buffer. */
/* this option will not check whether xpos/ypos are within width/height constraints,
 * those parameters should be already valid before calling this. */
void downsample_component (unsigned char *carray, int width, int height, const int xpos, const int ypos, const int xstep, const int ystep, int *out_width, int *out_height)
{
	int x_pixel_len = (xstep * 2) - 1;	/* x_pixel_coord/x_pixel_percent array size */
	int x_pixel_coord [x_pixel_len];	/* relative position (used by x_pixel_percent array) */
	float x_pixel_percent [x_pixel_len];	/* multiplier (1 to 0) (in practice 0,5 to 0) */
	unsigned char x_out_buff [width];
	unsigned char x_in_buff [width + (xstep - 1) * 2];

	int y_pixel_len = (ystep * 2) - 1;	/* y_pixel_coord/y_pixel_percent array size */
	int y_pixel_coord [y_pixel_len];	/* relative position (used by y_pixel_percent array) */
	float y_pixel_percent [y_pixel_len];	/* multiplier (1 to 0) (in practice 0,5 to 0) */
	unsigned char y_out_buff [height];
	unsigned char y_in_buff [height + (ystep - 1) * 2];

	int linecount;
	unsigned char *datastart;
	int i;

	*out_width = width;	// set this in case width is not modified
	*out_height = height;	// set this in case height is not modified

	/* example of hardcoded table xstep=ystep=2 */
	/*
	x_pixel_coord [0] = -1;
	x_pixel_coord [1] = 0;
	x_pixel_coord [2] = 1;
	x_pixel_percent [0] = 0.25;
	x_pixel_percent [1] = 0.5;
	x_pixel_percent [2] = 0.25;

	y_pixel_coord [0] = -1;
	y_pixel_coord [1] = 0;
	y_pixel_coord [2] = 1;
	y_pixel_percent [0] = 0.25;
	y_pixel_percent [1] = 0.5;
	y_pixel_percent [2] = 0.25;
	*/

	build_downsample_pixelprop_table (x_pixel_coord, x_pixel_percent, xstep);
	build_downsample_pixelprop_table (y_pixel_coord, y_pixel_percent, ystep);

	/* vertical downsampling */
	*out_height = return_downscaled_array_len (height, ypos, ystep);
	if ((ystep != 1) || (ypos != 0)) {
		for (linecount = 0; linecount < width; linecount++) {
			datastart = carray + linecount;

			// vertical line downsampling
			memset (y_in_buff, *datastart, ystep - 1);					// fill left border
			memset ((y_in_buff + ystep - 1 + height), *(datastart + ((height - 1) * width)), ystep - 1); // fill right border

			// to resample buffer
			for (i = 0; i < height; i++)
				*(y_in_buff + (ystep - 1) + i) = *(datastart + (width * i));

			downsample_component_line (y_in_buff + (ystep - 1), y_out_buff, height, ypos, ystep, y_pixel_len, y_pixel_coord, y_pixel_percent);
			
			// from resample buffer
			for (i = 0; i < *out_height; i++)
				*(datastart + (width * i)) = *(y_out_buff + i);
		}
	}

	/* horizontal downsampling */
	*out_width = return_downscaled_array_len (width, xpos, xstep);
	if ((xstep != 1) || (xpos != 0)) {
		for (linecount = 0; linecount < *out_height; linecount++) {
			datastart = carray + (linecount * width);

			// horizontal line downsampling
			memset (x_in_buff, *datastart, xstep - 1);					// fill left border
			memset ((x_in_buff + xstep - 1 + width), *(datastart + (width - 1)), xstep - 1);	// fill right border
			
			memcpy (x_in_buff + (xstep - 1), datastart, width);	// to resample buffer
			downsample_component_line (x_in_buff + (xstep - 1), x_out_buff, width, xpos, xstep, x_pixel_len, x_pixel_coord, x_pixel_percent);
			memcpy (datastart, x_out_buff, *out_width);	// from resample buffer
		}
	}
}

/* compute the neighboor-pixel percentage-contribution-to-pixel table */
void build_upsample_pixelprop_table (float *x_pixel_percent1, float *x_pixel_percent2, int xstep)
{
	int i;
	float sum_both;
	float mulstep = 1.0 / xstep;

	for (i = 0; i < xstep; i++) {
		x_pixel_percent1 [i] = 1 - (mulstep * i);
		x_pixel_percent2 [i] = mulstep * i;
	}

	// normalize x_pixel_percent1[n] to x_pixel_percent2[n] for each value of 'n' (the sum of both must be 1)
	for (i = 0; i < xstep; i++) {
		sum_both = x_pixel_percent1 [i] + x_pixel_percent2 [i];
		x_pixel_percent1 [i] = x_pixel_percent1 [i] / sum_both;
		x_pixel_percent2 [i] = x_pixel_percent2 [i] / sum_both;

		// avoid the need (later) to check whether result > 255 to round to 255
		x_pixel_percent1 [i] = x_pixel_percent1 [i] * 0.999999999;
		x_pixel_percent2 [i] = x_pixel_percent2 [i] * 0.999999999;
	}
}

/* return the resulted width */
/* this works for both x/y calculations, the naming as 'width' etc is just a convention */
void upsample_component_line_linear (unsigned char *x_in_buff, unsigned char *x_out_buff, const int width, const int xpos, const int xstep, const int compressed_width, const float *x_pixel_percent1, const float *x_pixel_percent2)
{
	int i, k;
	int base_write_pos;
	float tpix;
	
	// upsample line
	if (xpos > 0)
		memset (x_out_buff, *x_in_buff, xpos);
	// i = compressed step
	for (i = 0; i < compressed_width; i++) {
		// k = decompressed step
		base_write_pos = (i * xstep) + xpos;

		for (k = 0; k < xstep; k++) {
			tpix = (x_in_buff [i] * x_pixel_percent1 [k]) + (x_in_buff [i + 1] * x_pixel_percent2 [k]);

			// fix rounding error
			// not needed anymore, we avoid rounding errors multiplying the multipliers by 0.99999 (see build_upsample_pixelprop_table ())
			//if (tpix > 255)
			//	tpix = 255;

			x_out_buff [base_write_pos + k] = (unsigned char) tpix;
		}
	}
}

/* upsamples an array of one component (being R, G, B, Y or anything else) */
/* accepts: xstep/ystep >= 2, xpos/ypos >= 0 */
/* width/height - the final dimensions once upsampled, it also specifies the carray dimensions
 * xpos/ypos xstep/ytem - used to determine the original data */
/* carray is both the input and output buffer */
void upsample_component_linear (unsigned char *carray, int width, int height, const int xpos, const int ypos, const int xstep, const int ystep)
{
	float x_pixel_percent1 [xstep];	/* multiplier (1 to 0) (in practice 0,5 to 0) */
	float x_pixel_percent2 [xstep];
	unsigned char x_out_buff [width + xpos + xstep];
	unsigned char x_in_buff [width + 1];	// worst case (xstep = 1)

	unsigned char y_out_buff [height + ypos + ystep];
	unsigned char y_in_buff [height + 1];	// worst case (ystep = 1)
	float y_pixel_percent1 [ystep];	/* multiplier (1 to 0) (in practice 0,5 to 0) */
	float y_pixel_percent2 [ystep];

	int compressed_width;
	int compressed_height;
	
	int linecount;
	unsigned char *datastart;
	int i;

	compressed_width = return_downscaled_array_len (width, xpos, xstep);
	compressed_height = return_downscaled_array_len (height, ypos, ystep);

	/* example of hardcoded table xstep=ystep=2 */
	/*
	x_pixel_percent1 [0] = 1.0; // current (from current) == current
	x_pixel_percent1 [1] = 0.5; // middle (from current)
	x_pixel_percent2 [0] = 0.0; // current (from next)
	x_pixel_percent2 [1] = 0.5; // middle (from next)
	y_pixel_percent1 [0] = 1.0;
	y_pixel_percent1 [1] = 0.5;
	y_pixel_percent2 [0] = 0.0;
	y_pixel_percent2 [1] = 0.5;
	*/

	build_upsample_pixelprop_table (x_pixel_percent1, x_pixel_percent2, xstep);
	build_upsample_pixelprop_table (y_pixel_percent1, y_pixel_percent2, ystep);

	/* horizontal upsampling */
	if ((xstep != 1) || (xpos != 0)) {
		for (linecount = 0; linecount < compressed_height; linecount++) {
			datastart = carray + (linecount * width);

			// to resample buffer
			memcpy (x_in_buff, datastart, compressed_width);

			// add extra last byte (required by upsample_component_line_linear())
			x_in_buff [compressed_width] = x_in_buff [compressed_width - 1];
		
			upsample_component_line_linear (x_in_buff, x_out_buff, width, xpos, xstep, compressed_width, x_pixel_percent1, x_pixel_percent2);

			memcpy (datastart, x_out_buff, width);	// from resample buffer
		}
	}

	/* vertical upsampling */
	if ((ystep != 1) || (ypos != 0)) {
		for (linecount = 0; linecount < width; linecount++) {
			datastart = carray + linecount;

			// to resample buffer
			for (i = 0; i < compressed_height; i++)
				*(y_in_buff + i) = *(datastart + (width * i));
		
			// add extra last byte (required by upsample_component_line_linear()) - why?
			y_in_buff [compressed_height] = y_in_buff [compressed_height - 1];

			upsample_component_line_linear (y_in_buff, y_out_buff, height, ypos, ystep, compressed_height, y_pixel_percent1, y_pixel_percent2);
		
			// from resample buffer
			for (i = 0; i < height; i++)
				*(datastart + (width * i)) = *(y_out_buff + i);
		}
	}
}




/* returns newly-allocated array with only the selected component (must be free'ed later, manually) */
/* ncomp: which component (0 to ...)
 * totcomp: total components per pixel (1 to ...) - ex.: RGBA=4
 * csize: total of pixels to collect
 * tsize: target size, how many bytes to allocate to the new buffer
 *        (one may want more than csize if upsampling the components)
 *        must be >= csize _or_ 0 (then tsize = csize)
 * darray: target array (only the desired component)
 * sarray: source array (all the components together) */
void collect_component (unsigned char *darray, const unsigned char *sarray, int ncomp, int totcomp, int csize)
{
	int i;

	// TODO: optimize this
	for (i = 0; i < csize; i++)
		*(darray + i) = *(sarray + (i * totcomp) + ncomp);
}

/* insert component array into image array (replacing the previous component data) */
/* sarray - 1 component only (source)
 * darray - 2, 3 4.. components (target array)
 * ncomp - component in darray to overwrite with sarray data */
void rewrite_component (unsigned char *darray, const unsigned char *sarray, int ncomp, int totcomp, int csize)
{
	int i;

	// TODO: optimize this
	for (i = 0; i < csize; i++)
		*(darray + (i * totcomp) + ncomp) = *(sarray + i);
}

/* when we downscale a component, it maintains is 'displaybility' under the physical array x/y dimensions
 * (it 'looks' smaller and anchored to the top-left position)
 * this routine reorganizes that data to a linear way */
/* xbuf is the physical width of the image array
 * xcomp/ycomp are the dimensions of the (smaller, downscaled) data within the array */
void pack_component_array (unsigned char *carray, int xbuf, int xcomp, int ycomp)
{
	int i;

	// TODO: optimize this
	// the first line is always in the right place, don't worry
	for (i = 1; i < ycomp; i++)
		memmove (carray + (xcomp * i), carray + (xbuf * i), xcomp);
}

/* analogous to pack_component_array() but in a reverse way */
/* xbuf is the physical width of the image array
 * xcomp/ycomp are the dimensions of the (smaller, downscaled) data within the array */
void unpack_component_array (unsigned char *carray, int xbuf, int xcomp, int ycomp)
{
	int i;

	// TODO: optimize this
	// the first line is always in the right place, don't worry
	for (i = ycomp - 1; i > 0; i--)
		memmove (carray + (xbuf * i), carray + (xcomp * i), xcomp);
}

/* downsample each component from a bitmap and write data back to bitmap
 * bitmap parameters: width, height, bpp
 * requested components' resampling parameters: csampling [components * 4]
 * writes to: csampling_effective [components * 4], cwidth [components], cheight [components] */
void jp2_downsample_image_components (unsigned char *bitmap, const int width, const int height, const int bpp, const int *csampling, int *csampling_effective, int *cwidth, int *cheight)
{
	unsigned char *carray;
	int compstep;
	int out_width, out_height;
	int *csampling_base;
	int i;

	carray = malloc (width * height);
	for (compstep = 0; compstep < bpp; compstep++) {
		// transfer parameters to csampling_effective and sanitize them
		for (i = 0; i < 4; i++)
			csampling_effective [(compstep * 4) + i] = csampling [(compstep * 4) + i];
		csampling_base = csampling_effective + (compstep * 4); // component's: [0]-xpos [1]-ypos [2]-xstep [3]-ystep
		// xpos
		if ((csampling_base [0] >= width) || (csampling_base [0] < 0))
			csampling_base [0] = 0;
		// ypos
		if ((csampling_base [1] >= height) || (csampling_base [1] < 0))
			csampling_base [1] = 0;
		// xstep
		if (csampling_base [2] < 1)
			csampling_base [2] = 1;
		// ystep
		if (csampling_base [3] < 1)
			csampling_base [3] = 1;
	
		collect_component (carray, bitmap, compstep, bpp, width * height);
		downsample_component (carray, width, height, csampling_base [0], csampling_base [1], csampling_base [2], csampling_base [3], &out_width, &out_height);
		pack_component_array (carray, width, out_width, out_height);
		rewrite_component (bitmap, carray, compstep, bpp, out_width * out_height);

		cwidth [compstep] = out_width;
		cheight [compstep] = out_height;
	}

	free (carray);
}

/* upsample each component from a bitmap and write data back to bitmap
 * bitmap parameters: width, height, bpp
 * requested components' resampling parameters: csampling [components * 4], cwidth [components], cheight [components] */
void jp2_upsample_image_components (unsigned char *bitmap, const int width, const int height, const int bpp, const int *csampling, const int *cwidth, const int *cheight)

{
	unsigned char *carray;
	int compstep;
	const int *csampling_base;

	carray = malloc (width * height);
	for (compstep = 0; compstep < bpp; compstep++) {
		csampling_base = csampling + (compstep * 4); // component's: [0]-xpos [1]-ypos [2]-xstep [3]-ystep
	
		collect_component (carray, bitmap, compstep, bpp, cwidth [compstep] * cheight [compstep]);
		unpack_component_array (carray, width, cwidth [compstep], cheight [compstep]);

		switch (JP2Upsampler) {
			case UPS_LANCZOS:
				upsample_component_lanczos (carray, width, height, csampling_base [0], csampling_base [1], csampling_base [2], csampling_base [3]);
				break;
			case UPS_LINEAR:
			default:
				upsample_component_linear (carray, width, height, csampling_base [0], csampling_base [1], csampling_base [2], csampling_base [3]);
				break;
		}

		rewrite_component (bitmap, carray, compstep, bpp, width * height);
	}

	free (carray);
}

/*
 * Lanczos resampling routines
 */

/* return the resulted width */
/* this works for both x/y calculations, the naming as 'width' etc is just a convention */
void upsample_component_line_lanczos (const float *x_in_buff, float *x_out_buff, float *weight_x_out_buff, const float *x_contrib, const int x_contrib_size, const int width, const int xpos, const int xstep, const int compressed_width)
{
	int i;
	int rpos;
	int wpos;
	int cpos;
	int weight_shift;

	weight_shift = 0 - ((x_contrib_size - 1) / 2);

	if (xstep > 1) {
		/* init arrays */
		for (i = 0; i < width; i++)
			x_out_buff [i] = weight_x_out_buff [i] = 0;

		/* fill the arrays with the scaled data */
		for (rpos = 0; rpos < compressed_width; rpos++) {
			wpos = (rpos * xstep) + weight_shift;
			for (cpos = 0; cpos < x_contrib_size; cpos++) {
				if ((wpos >= 0) && (wpos < width)) {
					weight_x_out_buff [wpos] += x_contrib [cpos];
					x_out_buff [wpos] += x_in_buff [rpos] * x_contrib [cpos];
				}
				wpos++;
			}
		}

		/* normalize the x_out_buff based on weight_x_out_buff */
		for (i = 0; i < width; i++)
			x_out_buff [i] = x_out_buff [i] / weight_x_out_buff [i];

	}

	/* TODO: implement routine for xpos shifting HERE */
}

/* upsamples an array of one component (being R, G, B, Y or anything else) */
/* accepts: xstep/ystep >= 2, xpos/ypos >= 0 */
/* width/height - the final dimensions once upsampled, it also specifies the carray dimensions
 * xpos/ypos xstep/ytem - used to determine the original data */
/* carray is both the input and output buffer */
void upsample_component_lanczos (unsigned char *carray, int width, int height, const int xpos, const int ypos, const int xstep, const int ystep)
{
	float x_out_buff [width];
	float weight_x_out_buff [width];
	float x_in_buff [width];

	float y_out_buff [height];
	float weight_y_out_buff [height];
	float y_in_buff [height];

	int compressed_width;
	int compressed_height;
	
	int linecount;
	unsigned char *datastart;
	float incoming_data;
	int i;

	/* contrib x and y calculation */
	float x_contrib [(RESAMPLER_LANCZOS_RADIUS * 2 * xstep) + 1];
	float y_contrib [(RESAMPLER_LANCZOS_RADIUS * 2 * ystep) + 1];
	int x_contrib_size;
	int y_contrib_size;
	int x_neo_radius = RESAMPLER_LANCZOS_RADIUS * xstep;
	int y_neo_radius = RESAMPLER_LANCZOS_RADIUS * ystep;
	float x;
	// int i;

	/* build x_contrib table */
	x_contrib_size = (x_neo_radius * 2) + 1;
	for (i = 0; i < x_contrib_size; i++) {
		if (i != x_neo_radius) {
			x = ((float) (i - x_neo_radius) / (float)xstep);
			x_contrib [i] = (float) ((RESAMPLER_LANCZOS_RADIUS * sin (M_PIl * (double) x) * sin ((M_PIl / RESAMPLER_LANCZOS_RADIUS) * (double) x)) / (M_PIl * M_PIl * (double) x * (double) x));
		}
	}
	x_contrib [x_neo_radius] = 1;

	/* build y_contrib table */
	y_contrib_size = (y_neo_radius * 2) + 1;
	for (i = 0; i < y_contrib_size; i++) {
		if (i != y_neo_radius) {
			x = ((float) (i - y_neo_radius) / (float)ystep);
			y_contrib [i] = (float) ((RESAMPLER_LANCZOS_RADIUS * sin (M_PIl * (double) x) * sin ((M_PIl / RESAMPLER_LANCZOS_RADIUS) * (double) x)) / (M_PIl * M_PIl * (double) x * (double) x));
		}
	}
	y_contrib [y_neo_radius] = 1;

	compressed_width = return_downscaled_array_len (width, xpos, xstep);
	compressed_height = return_downscaled_array_len (height, ypos, ystep);

	/* horizontal upsampling */
	if ((xstep != 1) || (xpos != 0)) {
		for (linecount = 0; linecount < compressed_height; linecount++) {
			datastart = carray + (linecount * width);

			// 2D array -> line buffer in
			for (i = 0; i < compressed_width; i++)
				*(x_in_buff + i) = (float) *(datastart + i);

			// REMOVEME
			// memcpy (x_in_buff, datastart, compressed_width);

			// REMOVEME
			//x_in_buff[0]=0;
			//x_in_buff[1]=255;
			//x_in_buff[2]=0;

			upsample_component_line_lanczos (x_in_buff, x_out_buff, weight_x_out_buff, x_contrib, x_contrib_size, width, xpos, xstep, compressed_width);

			// line buffer out -> 2D array (and round corners)
			for (i = 0; i < width; i++) {
				incoming_data = *(x_out_buff + i);
				if (incoming_data > 255)
					*(datastart + i) = 255;
				else if (incoming_data < 0)
					*(datastart + i) = 0;
				else
					*(datastart + i) = (unsigned char) incoming_data;
			}

			// REMOVEME
			// memcpy (datastart, x_out_buff, width);	// from resample buffer
		}
	}

	/* vertical upsampling */
	if ((ystep != 1) || (ypos != 0)) {
		for (linecount = 0; linecount < width; linecount++) {
			datastart = carray + linecount;

			// 2D array -> line buffer in
			for (i = 0; i < compressed_height; i++)
				*(y_in_buff + i) = (float) *(datastart + (width * i));
		
			upsample_component_line_lanczos (y_in_buff, y_out_buff, weight_y_out_buff, y_contrib, y_contrib_size, height, ypos, ystep, compressed_height);
		
			// line buffer out -> 2D array (and round corners)
			for (i = 0; i < height; i++) {
				incoming_data = *(y_out_buff + i);
				if (incoming_data > 255)
					*(datastart + (width * i)) = 255;
				else if (incoming_data < 0)
					*(datastart + (width * i)) = 0;
				else
					*(datastart + (width * i)) = (unsigned char) incoming_data;
			}

			// REMOVEME
			//for (i = 0; i < height; i++)
			//	*(datastart + (width * i)) = *(y_out_buff + i);
		}
	}
}



/*
 * / Lanczos resampling routines
 */


