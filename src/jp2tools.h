/* jp2tools.h
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

#ifndef SRC_JP2TOOLS_H
#define SRC_JP2TOOLS_H

/* types used externally */
enum enum_upsampler {UPS_LINEAR, UPS_LANCZOS};
#define t_upsampler enum enum_upsampler

// FIXME: this shouldn't be necessary as it's redundant, but won't compile otherwise
extern t_upsampler JP2Upsampler;



int return_downscaled_array_len (const int width, const int xpos, const int xstep);
void jp2_downsample_image_components (unsigned char *bitmap, const int width, const int height, const int bpp, const int *csampling, int *csampling_effective, int *cwidth, int *cheight);
void jp2_upsample_image_components (unsigned char *bitmap, const int width, const int height, const int bpp, const int *csampling, const int *cwidth, const int *cheight);

/* PRIVATE */
void upsample_component_line_lanczos (const float *x_in_buff, float *x_out_buff, float *weight_x_out_buff, const float *x_contrib, const int x_contrib_size, const int width, const int xpos, const int xstep, const int compressed_width);
void upsample_component_lanczos (unsigned char *carray, int width, int height, const int xpos, const int ypos, const int xstep, const int ystep);

#endif

