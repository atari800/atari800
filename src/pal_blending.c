/*
 * pal_blending.c - blitting functions that emulate PAL delay line accurately
 *
 * Copyright (C) 2013 Tomasz Krasuski
 * Copyright (C) 2013 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.

 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with Atari800; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pal_blending.h"

#include "artifact.h"
#include "atari.h"
#include "colours.h"
#include "colours_pal.h"
#include "platform.h"
#include "screen.h"

#if SUPPORTS_CHANGE_VIDEOMODE
#include "videomode.h"
#endif /* SUPPORTS_CHANGE_VIDEOMODE */

static union {
	UWORD bpp16[2][256];	/* 16-bit palette */
	ULONG bpp32[2][256];	/* 32-bit palette */
} palette;

static ULONG shift_mask;

void PAL_BLENDING_UpdateLookup(void)
{
	if (ARTIFACT_mode == ARTIFACT_PAL_BLEND) {
		double yuv_table[256*5];
		int even_pal[256];
		int odd_pal[256];
		int i;
		double *ptr = yuv_table;
		PLATFORM_pixel_format_t format;

		COLOURS_PAL_GetYUV(yuv_table);

		for (i = 0; i < 256; ++i) {
			double y = *ptr++;
			double even_u = *ptr++;
			double odd_u = *ptr++;
			double even_v = *ptr++;
			double odd_v = *ptr++;
			double r, g, b;
			Colours_YUV2RGB(y, even_u, even_v, &r, &g, &b);
			Colours_SetRGB(i, (int) (r * 255), (int) (g * 255), (int) (b * 255), even_pal);
			Colours_YUV2RGB(y, odd_u, odd_v, &r, &g, &b);
			Colours_SetRGB(i, (int) (r * 255), (int) (g * 255), (int) (b * 255), odd_pal);
		}
		PLATFORM_GetPixelFormat(&format);
		shift_mask = (format.rmask & ~(format.rmask << 1)) | (format.gmask & ~(format.gmask << 1)) | (format.bmask & ~(format.bmask << 1));
		switch (format.bpp) {
		case 16:
			PLATFORM_MapRGB(palette.bpp16[0], even_pal, 256);
			PLATFORM_MapRGB(palette.bpp16[1], odd_pal, 256);
			shift_mask |= shift_mask << 16;
			break;
		case 32:
			PLATFORM_MapRGB(palette.bpp32[0], even_pal, 256);
			PLATFORM_MapRGB(palette.bpp32[1], odd_pal, 256);
		}
		shift_mask = ~shift_mask;
	}
}

void PAL_BLENDING_Blit16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	register ULONG quad, quad_prev;
	register UBYTE c;
	register int pos;
	UBYTE *src_prev = src;
	int odd_prev = start_odd ^ 1;
	int width_32;
	if (width & 0x01)
		width_32 = width + 1;
	else
		width_32 = width;
	while (height > 0) {
		pos = width_32;
		do {
			pos--;
			c = src[pos];
			/* Make QUAD_PREV have the same Y component as the current line's pixel. */
			quad_prev = palette.bpp16[odd_prev][(src_prev[pos] & 0xf0) | (c & 0x0f)] << 16;
			quad = palette.bpp16[start_odd][c] << 16;
			pos--;
			c = src[pos];
			quad_prev |= palette.bpp16[odd_prev][(src_prev[pos] & 0xf0) | (c & 0x0f)];
			quad |= palette.bpp16[start_odd][c];
			/* Since QUAD_PREV and QUAD have the same Y component, computing
			   averages of even U/V and odd U/V is equal to computing averages
			   of even and odd RGB components. */
			/* dest[pos >> 1] = ((quad+quad_prev) & shift_mask)/2; */
			dest[pos >> 1] = (quad & quad_prev) + (((quad ^ quad_prev) & shift_mask) >> 1);
		} while (pos > 0);
		src_prev = src;
		src += Screen_WIDTH;
		dest += pitch;
		height--;
		start_odd ^= 1;
		odd_prev ^= 1;
	}
}

void PAL_BLENDING_Blit32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	register ULONG quad, quad_prev;
	register UBYTE c;
	register int pos;
	UBYTE *src_prev = src;
	int odd_prev = start_odd ^ 1;
	while (height > 0) {
		pos = width;
		do {
			pos--;
			c = src[pos];
			/* Make QUAD_PREV have the same Y component as the current line's pixel. */
			quad_prev = palette.bpp32[odd_prev][(src_prev[pos] & 0xf0) | (c & 0x0f)];
			quad = palette.bpp32[start_odd][c];
			/* Since QUAD_PREV and QUAD have the same Y component, computing
			   averages of even U/V and odd U/V is equal to computing averages
			   of even and odd RGB components. */
			/* dest[pos] = ((quad+quad_prev) & shift_mask)/2; */
			dest[pos] = (quad & quad_prev) + (((quad ^ quad_prev) & shift_mask) >> 1);
		} while (pos > 0);
		src_prev = src;
		src += Screen_WIDTH;
		dest += pitch;
		height--;
		start_odd ^= 1;
		odd_prev ^= 1;
	}
}

void PAL_BLENDING_BlitScaled16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	register ULONG quad, quad_prev;
	register int x;
	int y = 0x10000;
	int w1 = dest_width / 2 - 1;
	int w = width << 16;
	int h = height << 16;
	int pos;
	register int dx = w / dest_width;
	int dy = h / dest_height;
	int init_x = (width << 16) - 0x4000;
	UBYTE *src_prev = src;
	int odd_prev = start_odd ^ 1;

	UBYTE c;

	while (dest_height > 0) {
		x = init_x;
		pos = w1;
		while (pos >= 0) {
			c = src[x >> 16];
			/* Make QUAD_PREV have the same Y component as the current line's pixel. */
			quad_prev = palette.bpp16[odd_prev][(src_prev[x >> 16] & 0xf0) | (c & 0x0f)] << 16;
			quad = palette.bpp16[start_odd][c] << 16;
			x -= dx;
			c = src[x >> 16];
			quad_prev |= palette.bpp16[odd_prev][(src_prev[x >> 16] & 0xf0) | (c & 0x0f)];
			quad |= palette.bpp16[start_odd][c];
			x -= dx;
			/* Since QUAD_PREV and QUAD have the same Y component, computing
			   averages of even U/V and odd U/V is equal to computing averages
			   of even and odd RGB components. */
			/* dest[pos] = ((quad+quad_prev) & shift_mask)/2; */
			dest[pos] = (quad & quad_prev) + (((quad ^ quad_prev) & shift_mask) >> 1);
			pos--;
		}
		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src_prev = src;
			src += Screen_WIDTH;
			start_odd ^= 1;
			odd_prev ^= 1;
		}
	}
}

void PAL_BLENDING_BlitScaled32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	register ULONG quad, quad_prev;
	register int x;
	int y = 0x10000;
	int w1 = dest_width - 1;
	int w = width << 16;
	int h = height << 16;
	int pos;
	register int dx = w / dest_width;
	int dy = h / dest_height;
	int init_x = w - 0x4000;
	UBYTE *src_prev = src;
	int odd_prev = start_odd ^ 1;

	UBYTE c;

	while (dest_height > 0) {
		x = init_x;
		pos = w1;
		while (pos >= 0) {
			c = src[x >> 16];
			/* Make QUAD_PREV have the same Y component as the current line's pixel. */
			quad_prev = palette.bpp32[odd_prev][(src_prev[x >> 16] & 0xf0) | (c & 0x0f)];
			quad = palette.bpp32[start_odd][c];
			x -= dx;
			/* Since QUAD_PREV and QUAD have the same Y component, computing
			   averages of even U/V and odd U/V is equal to computing averages
			   of even and odd RGB components. */
			/* dest[pos] = ((quad+quad_prev) & shift_mask)/2; */
			dest[pos] = (quad & quad_prev) + (((quad ^ quad_prev) & shift_mask) >> 1);
			pos--;
		}
		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src_prev = src;
			src += Screen_WIDTH;
			start_odd ^= 1;
			odd_prev ^= 1;
		}
	}
}
