/*
 * Heart of The Alien Redux: Pixel-art upscalers
 *
 * Scale2x/Scale3x, based on the AdvanceMAME filters by Andrea Mazzoleni
 * and the stripped-down version used by the original Heart of The Alien
 * Redux (Gil Megidish).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "upscale.h"

#define MAX2(a, b) (((a) > (b)) ? (a) : (b))
#define MIN2(a, b) (((a) < (b)) ? (a) : (b))

void upscale_nearest(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height,
	int factor)
{
	int x, y;

	if (factor < 1)
	{
		return;
	}

	for (y = 0; y < height; y++)
	{
		const unsigned char *row = src + y * src_pitch;
		unsigned char *out = dst + (y * factor) * dst_pitch;
		int n;

		for (x = 0; x < width; x++)
		{
			unsigned char value = row[x];
			int i;

			for (i = 0; i < factor; i++)
			{
				out[x * factor + i] = value;
			}
		}

		for (n = 1; n < factor; n++)
		{
			memcpy(out + n * dst_pitch, out, width * factor);
		}
	}
}

void upscale2x(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		const unsigned char *row = src + y * src_pitch;
		const unsigned char *up = src + MAX2(0, y - 1) * src_pitch;
		const unsigned char *down = src + MIN2(height - 1, y + 1) * src_pitch;
		unsigned char *out0 = dst + (y * 2) * dst_pitch;
		unsigned char *out1 = out0 + dst_pitch;

		for (x = 0; x < width; x++)
		{
			unsigned char b, d, e, f, h;
			unsigned char e0, e1, e2, e3;

			b = up[x];
			d = row[MAX2(0, x - 1)];
			e = row[x];
			f = row[MIN2(width - 1, x + 1)];
			h = down[x];

			e0 = (d == b && b != f && d != h) ? d : e;
			e1 = (b == f && b != d && f != h) ? f : e;
			e2 = (d == h && d != b && h != f) ? d : e;
			e3 = (h == f && d != h && b != f) ? f : e;

			out0[x * 2 + 0] = e0;
			out0[x * 2 + 1] = e1;
			out1[x * 2 + 0] = e2;
			out1[x * 2 + 1] = e3;
		}
	}
}

void upscale3x(unsigned char *dst, int dst_pitch,
	const unsigned char *src, int src_pitch, int width, int height)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		const unsigned char *row = src + y * src_pitch;
		const unsigned char *up = src + MAX2(0, y - 1) * src_pitch;
		const unsigned char *down = src + MIN2(height - 1, y + 1) * src_pitch;
		unsigned char *out0 = dst + (y * 3) * dst_pitch;
		unsigned char *out1 = out0 + dst_pitch;
		unsigned char *out2 = out1 + dst_pitch;

		for (x = 0; x < width; x++)
		{
			unsigned char a, b, c, d, e, f, g, h, i;
			unsigned char e0, e1, e2, e3, e4, e5, e6, e7, e8;

			int xm = MAX2(0, x - 1);
			int xp = MIN2(width - 1, x + 1);

			a = up[xm];
			b = up[x];
			c = up[xp];
			d = row[xm];
			e = row[x];
			f = row[xp];
			g = down[xm];
			h = down[x];
			i = down[xp];

			e0 = (d == b && b != f && d != h) ? d : e;
			e1 = ((d == b && b != f && d != h && e != c) ||
				(b == f && b != d && f != h && e != a)) ? b : e;
			e2 = (b == f && b != d && f != h) ? f : e;
			e3 = ((d == b && b != f && d != h && e != g) ||
				(b == f && b != d && f != h && e != a)) ? d : e;
			e4 = e;
			e5 = ((b == f && b != d && f != h && e != i) ||
				(d == h && d != b && h != f && e != c)) ? f : e;
			e6 = (d == h && d != b && h != f) ? d : e;
			e7 = ((d == h && d != b && h != f && e != i) ||
				(h == f && d != h && b != f && e != g)) ? h : e;
			e8 = (h == f && d != h && b != f) ? f : e;

			out0[x * 3 + 0] = e0;
			out0[x * 3 + 1] = e1;
			out0[x * 3 + 2] = e2;
			out1[x * 3 + 0] = e3;
			out1[x * 3 + 1] = e4;
			out1[x * 3 + 2] = e5;
			out2[x * 3 + 0] = e6;
			out2[x * 3 + 1] = e7;
			out2[x * 3 + 2] = e8;
		}
	}
}
