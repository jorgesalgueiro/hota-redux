/*
 * Heart of The Alien Redux: Pixel-art upscalers
 *
 * Scale2x/Scale3x (AdvanceMAME) on 8-bit palette indices.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef UPSCALE_H
#define UPSCALE_H

/** Replicates each pixel by an integer factor (hard, nearest-neighbor).
    The source and destination may not overlap.
    @param factor    integer scale factor (1..8)
*/
void upscale_nearest(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height,
	int factor);

/** Doubles an 8-bit indexed image using the Scale2x algorithm.
    The source and destination may not overlap.
    @param dst       destination pixels
    @param dst_pitch destination bytes per row
    @param src       source pixels
    @param src_pitch source bytes per row
    @param width     source width in pixels
    @param height    source height in pixels
*/
void upscale2x(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height);

/** Triples an 8-bit indexed image using the Scale3x algorithm.
    The source and destination may not overlap.
*/
void upscale3x(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height);

#endif // UPSCALE_H
