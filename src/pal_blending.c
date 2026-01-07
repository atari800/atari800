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

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

#define PAL_HI_PHASES 8
#define PAL_HI_Y_TAPS 4
#define PAL_HI_UV_TAPS 12
#define PAL_HI_MAX_WIDTH (Screen_WIDTH + 8)
#define PAL_HI_DELAY_PAD 8
#define PAL_HI_KERNEL_MAX_TAPS 96

typedef struct PAL_HI_Kernel {
	int offset;
	int size;
	float coeffs[PAL_HI_KERNEL_MAX_TAPS];
} PAL_HI_Kernel;

static ULONG pal_high_to_y[2][256][PAL_HI_PHASES][PAL_HI_Y_TAPS];
static ULONG pal_high_to_u[2][256][PAL_HI_PHASES][PAL_HI_UV_TAPS];
static ULONG pal_high_to_v[2][256][PAL_HI_PHASES][PAL_HI_UV_TAPS];
static ULONG pal_high_delay_uv[2][PAL_HI_MAX_WIDTH + PAL_HI_DELAY_PAD];
static int pal_high_tables_ready = FALSE;
static int pal_high_apply_gamma = FALSE;
static const double pal_high_co_vr = 1.1402509;
static const double pal_high_co_ub = 2.0325203;
static const float pal_high_u_scale = 1.20f;

static struct {
	int bpp;
	ULONG rmask;
	ULONG gmask;
	ULONG bmask;
	int rshift;
	int gshift;
	int bshift;
	ULONG rmax;
	ULONG gmax;
	ULONG bmax;
	ULONG avg_mask;
} pal_high_format;

static int PalHigh_FirstBit(ULONG mask)
{
	int shift = 0;
	if (mask == 0)
		return 0;
	while ((mask & 1) == 0) {
		mask >>= 1;
		++shift;
	}
	return shift;
}

static ULONG PalHigh_PackRGB(int r, int g, int b)
{
	ULONG rr = (ULONG)((r * pal_high_format.rmax + 127) / 255) << pal_high_format.rshift;
	ULONG gg = (ULONG)((g * pal_high_format.gmax + 127) / 255) << pal_high_format.gshift;
	ULONG bb = (ULONG)((b * pal_high_format.bmax + 127) / 255) << pal_high_format.bshift;

	return (rr & pal_high_format.rmask) | (gg & pal_high_format.gmask) | (bb & pal_high_format.bmask);
}

static ULONG PalHigh_AveragePixels(ULONG a, ULONG b)
{
	return (a & b) + (((a ^ b) & pal_high_format.avg_mask) >> 1);
}

static void PalHigh_UnpackRGB(ULONG p, int *r, int *g, int *b)
{
	ULONG rr = (p & pal_high_format.rmask) >> pal_high_format.rshift;
	ULONG gg = (p & pal_high_format.gmask) >> pal_high_format.gshift;
	ULONG bb = (p & pal_high_format.bmask) >> pal_high_format.bshift;

	*r = (int)((rr * 255 + pal_high_format.rmax / 2) / pal_high_format.rmax);
	*g = (int)((gg * 255 + pal_high_format.gmax / 2) / pal_high_format.gmax);
	*b = (int)((bb * 255 + pal_high_format.bmax / 2) / pal_high_format.bmax);
}

static void PalHigh_BlurLine(ULONG *dst, const ULONG *src, int n)
{
	int i;
	for (i = 0; i < n; ++i) {
		int r0, g0, b0;
		int r1, g1, b1;
		int r2, g2, b2;
		ULONG prev = (i > 0) ? src[i - 1] : src[i];
		ULONG cur = src[i];
		ULONG next = (i + 1 < n) ? src[i + 1] : src[i];

		PalHigh_UnpackRGB(prev, &r0, &g0, &b0);
		PalHigh_UnpackRGB(cur, &r1, &g1, &b1);
		PalHigh_UnpackRGB(next, &r2, &g2, &b2);

		/* was (1-2-1):
		r0 = (r0 + (r1 << 1) + r2 + 2) >> 2;
		g0 = (g0 + (g1 << 1) + g2 + 2) >> 2;
		b0 = (b0 + (b1 << 1) + b2 + 2) >> 2;
		*/
		r0 = (r0 + (r1 << 2) + r2 + 3) / 6;
		g0 = (g0 + (g1 << 2) + g2 + 3) / 6;
		b0 = (b0 + (b1 << 2) + b2 + 3) / 6;

		dst[i] = PalHigh_PackRGB(r0, g0, b0);
	}
	/* was:
	for (i = 0; i < n; ++i) {
		ULONG prev = (i > 0) ? src[i - 1] : src[i];
		ULONG cur = src[i];
		ULONG next = (i + 1 < n) ? src[i + 1] : src[i];
		ULONG avg1 = PalHigh_AveragePixels(prev, cur);
		ULONG avg2 = PalHigh_AveragePixels(cur, next);
		dst[i] = PalHigh_AveragePixels(avg1, avg2);
	}
	*/
}
static void PalHigh_ApplyGamma(int *r, int *g, int *b)
{
	if (!pal_high_apply_gamma)
		return;
	if (COLOURS_PAL_external.loaded && !COLOURS_PAL_external.adjust)
		return;

	{
		double rf = Colours_Gamma2Linear((double)*r / 255.0, COLOURS_PAL_setup.gamma);
		double gf = Colours_Gamma2Linear((double)*g / 255.0, COLOURS_PAL_setup.gamma);
		double bf = Colours_Gamma2Linear((double)*b / 255.0, COLOURS_PAL_setup.gamma);

		rf = Colours_Linear2sRGB(rf);
		gf = Colours_Linear2sRGB(gf);
		bf = Colours_Linear2sRGB(bf);

		*r = (int)(rf * 255.0 + 0.5);
		*g = (int)(gf * 255.0 + 0.5);
		*b = (int)(bf * 255.0 + 0.5);

		if (*r < 0) *r = 0; else if (*r > 255) *r = 255;
		if (*g < 0) *g = 0; else if (*g > 255) *g = 255;
		if (*b < 0) *b = 0; else if (*b > 255) *b = 255;
	}
}

static void PalHigh_KernelInit(PAL_HI_Kernel *k, int offset, const float *vals, int n)
{
	int i;
	k->offset = offset;
	k->size = n;
	for (i = 0; i < n; ++i)
		k->coeffs[i] = vals[i];
}

static void PalHigh_KernelScale(PAL_HI_Kernel *k, float scale)
{
	int i;
	for (i = 0; i < k->size; ++i)
		k->coeffs[i] *= scale;
}

static void PalHigh_KernelShift(PAL_HI_Kernel *dst, const PAL_HI_Kernel *src, int offset)
{
	*dst = *src;
	dst->offset = src->offset + offset;
}

static void PalHigh_KernelConvolve(PAL_HI_Kernel *dst, const PAL_HI_Kernel *x, const PAL_HI_Kernel *y)
{
	int i, j;
	int n = x->size + y->size - 1;
	if (n > PAL_HI_KERNEL_MAX_TAPS)
		n = PAL_HI_KERNEL_MAX_TAPS;

	dst->offset = x->offset + y->offset;
	dst->size = n;
	for (i = 0; i < n; ++i)
		dst->coeffs[i] = 0.0f;

	for (i = 0; i < x->size; ++i) {
		float s = x->coeffs[i];
		for (j = 0; j < y->size && i + j < n; ++j)
			dst->coeffs[i + j] += s * y->coeffs[j];
	}
}

static void PalHigh_KernelAdd(PAL_HI_Kernel *dst, const PAL_HI_Kernel *x, const PAL_HI_Kernel *y, int subtract)
{
	int i;
	int lo = x->offset < y->offset ? x->offset : y->offset;
	int hi_x = x->offset + x->size;
	int hi_y = y->offset + y->size;
	int hi = hi_x > hi_y ? hi_x : hi_y;
	int n = hi - lo;
	if (n > PAL_HI_KERNEL_MAX_TAPS)
		n = PAL_HI_KERNEL_MAX_TAPS;

	dst->offset = lo;
	dst->size = n;
	for (i = 0; i < n; ++i)
		dst->coeffs[i] = 0.0f;

	for (i = 0; i < x->size && x->offset - lo + i < n; ++i)
		dst->coeffs[x->offset - lo + i] += x->coeffs[i];
	for (i = 0; i < y->size && y->offset - lo + i < n; ++i)
		dst->coeffs[y->offset - lo + i] += subtract ? -y->coeffs[i] : y->coeffs[i];
}

static void PalHigh_KernelModulate(PAL_HI_Kernel *dst, const PAL_HI_Kernel *x, const PAL_HI_Kernel *y)
{
	int i;
	int off = y->offset;
	int n = y->size;
	int mod_pos;

	*dst = *x;
	if (n <= 0)
		return;

	while (off > x->offset)
		off -= n;
	while (off + n <= x->offset)
		off += n;

	mod_pos = x->offset - off;
	for (i = 0; i < dst->size; ++i) {
		dst->coeffs[i] *= y->coeffs[mod_pos];
		++mod_pos;
		if (mod_pos == n)
			mod_pos = 0;
	}
}

static void PalHigh_KernelEvalCubic4(float co[4], float offset, float A)
{
	float t = offset;
	float t2 = t * t;
	float t3 = t2 * t;
	co[0] =     + A * t -        2.0f * A * t2 +        A * t3;
	co[1] = 1.0f      -      (A + 3.0f) * t2 + (A + 2.0f) * t3;
	co[2] =     - A * t + (2.0f * A + 3.0f) * t2 - (A + 2.0f) * t3;
	co[3] =           +             A * t2 -        A * t3;
}

static void PalHigh_KernelSampleBicubic(PAL_HI_Kernel *dst, const PAL_HI_Kernel *src, float offset, float step, float A)
{
	int lo = src->offset;
	int hi = src->offset + src->size;
	float fstart = ceilf(((float)lo - 3.0f - offset) / step);
	float flimit = (float)hi;
	int istart = (int)fstart;
	float fpos = fstart * step + offset;
	int out = 0;
	float co[4];

	dst->offset = istart;
	dst->size = 0;

	while (fpos < flimit && out < PAL_HI_KERNEL_MAX_TAPS) {
		float fposf = floorf(fpos);
		int ipos = (int)fposf;
		float sum = 0.0f;
		int iend = ipos + 4;
		int i;

		PalHigh_KernelEvalCubic4(co, fpos - fposf, A);
		if (ipos >= lo) {
			int ilen = 4;
			if (iend > hi)
				ilen = hi - ipos;
			for (i = 0; i < ilen; ++i)
				sum += co[i] * src->coeffs[(ipos - lo) + i];
		} else if (iend >= lo) {
			if (iend > hi)
				iend = hi;
			for (i = lo; i < iend; ++i)
				sum += co[i - ipos] * src->coeffs[i - lo];
		}

		dst->coeffs[out++] = sum;
		fpos += step;
	}
	dst->size = out;
}

static void PalHigh_KernelSamplePoint(PAL_HI_Kernel *dst, const PAL_HI_Kernel *src, int offset, int step)
{
	int delta = src->offset - offset;
	int pos;
	int out = 0;

	dst->offset = delta / step;
	pos = -delta % step;
	if (pos < 0) {
		++dst->offset;
		pos += step;
	}

	while (pos < src->size && out < PAL_HI_KERNEL_MAX_TAPS) {
		dst->coeffs[out++] = src->coeffs[pos];
		pos += step;
	}

	if (out == 0) {
		dst->coeffs[0] = 0.0f;
		out = 1;
	}
	dst->size = out;
}

static void PalHigh_KernelAccumulateWindow(const PAL_HI_Kernel *k, float *dst, int offset, int limit, float scale)
{
	int start = k->offset + offset;
	int end = start + k->size;
	int lo = start;
	int hi = end;
	int i;

	if (lo < 0)
		lo = 0;
	if (hi > limit)
		hi = limit;

	for (i = lo; i < hi; ++i)
		dst[i] += k->coeffs[i - start] * scale;
}

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
			if (!COLOURS_PAL_external.loaded || COLOURS_PAL_external.adjust) {
				r = Colours_Gamma2Linear(r, COLOURS_PAL_setup.gamma);
				g = Colours_Gamma2Linear(g, COLOURS_PAL_setup.gamma);
				b = Colours_Gamma2Linear(b, COLOURS_PAL_setup.gamma);
				r = Colours_Linear2sRGB(r);
				g = Colours_Linear2sRGB(g);
				b = Colours_Linear2sRGB(b);
			}
			Colours_SetRGB(i, (int) (r * 255), (int) (g * 255), (int) (b * 255), even_pal);
			Colours_YUV2RGB(y, odd_u, odd_v, &r, &g, &b);
			if (!COLOURS_PAL_external.loaded || COLOURS_PAL_external.adjust) {
				r = Colours_Gamma2Linear(r, COLOURS_PAL_setup.gamma);
				g = Colours_Gamma2Linear(g, COLOURS_PAL_setup.gamma);
				b = Colours_Gamma2Linear(b, COLOURS_PAL_setup.gamma);
				r = Colours_Linear2sRGB(r);
				g = Colours_Linear2sRGB(g);
				b = Colours_Linear2sRGB(b);
			}
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

static void PalHigh_RecomputeTables(void)
{
	float ytab[2][256];
	float utab[2][256];
	float vtab[2][256];
	int j;
	int k;
	int phase;

	static const float deg = (float)(2.0 * M_PI / 360.0);
	const float ycphase = (90.0f) * deg;
	const float ycphasec = cosf(ycphase);
	const float ycphases = sinf(ycphase);
	const float scale = 64.0f * 255.0f;

	PAL_HI_Kernel kernbase;
	PAL_HI_Kernel kerncfilt;
	PAL_HI_Kernel kernumod;
	PAL_HI_Kernel kernvmod;
	PAL_HI_Kernel kernysep;
	PAL_HI_Kernel kerncsep;
	PAL_HI_Kernel kerncdemod;

	PAL_HI_Kernel kerny2y[PAL_HI_PHASES];
	PAL_HI_Kernel kernu2y[PAL_HI_PHASES];
	PAL_HI_Kernel kernv2y[PAL_HI_PHASES];
	PAL_HI_Kernel kerny2u[PAL_HI_PHASES];
	PAL_HI_Kernel kernu2u[PAL_HI_PHASES];
	PAL_HI_Kernel kernv2u[PAL_HI_PHASES];
	PAL_HI_Kernel kerny2v[PAL_HI_PHASES];
	PAL_HI_Kernel kernu2v[PAL_HI_PHASES];
	PAL_HI_Kernel kernv2v[PAL_HI_PHASES];

	static const float kernbase_vals[5] = { 1, 1, 1, 1, 1 };
	static const float kerncfilt_vals[11] = {
		1.0f / 1024.0f,
		10.0f / 1024.0f,
		45.0f / 1024.0f,
		120.0f / 1024.0f,
		210.0f / 1024.0f,
		252.0f / 1024.0f,
		210.0f / 1024.0f,
		120.0f / 1024.0f,
		45.0f / 1024.0f,
		10.0f / 1024.0f,
		1.0f / 1024.0f
	};
	static const float kernumod_vals[8] = { 1, 0.70710678f, 0, -0.70710678f, -1, -0.70710678f, 0, 0.70710678f };
	static const float kernvmod_vals[8] = { 0, 0.70710678f, 1, 0.70710678f, 0, -0.70710678f, -1, -0.70710678f };
	static const float kernysep_vals[9] = {
		0.5f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		1.0f / 8.0f,
		0.5f / 8.0f
	};
	static const float kerncsep_vals[33] = {
		1,0,0,0,-2,0,0,0,2,0,0,0,-2,0,0,0,2,0,0,0,-2,0,0,0,2,0,0,0,-2,0,0,0,1
	};
	static const float kerncdemod_vals[8] = { -1, -1, 1, 1, 1, 1, -1, -1 };

	{
		double yuv_table[256 * 5];
		double *ptr = yuv_table;

		COLOURS_PAL_GetYUV(yuv_table);

		for (j = 0; j < 256; ++j) {
			double y = *ptr++;
			double even_u = *ptr++;
			double odd_u = *ptr++;
			double even_v = *ptr++;
			double odd_v = *ptr++;

			ytab[0][j] = (float)y;
			ytab[1][j] = (float)y;
			/* Use per-line chroma; PAL high needs odd/even separation. */
			utab[0][j] = (float)-even_u * pal_high_u_scale;
			utab[1][j] = (float)-odd_u * pal_high_u_scale;
			/* Original V handling:
			vtab[0][j] = (float)even_v;
			vtab[1][j] = (float)odd_v;
			*/
			vtab[0][j] = (float)even_v;
			vtab[1][j] = (float)odd_v;
		}
	}

	PalHigh_KernelInit(&kernbase, 0, kernbase_vals, 5);
	PalHigh_KernelInit(&kerncfilt, -5, kerncfilt_vals, 11);
	PalHigh_KernelInit(&kernumod, 0, kernumod_vals, 8);
	PalHigh_KernelInit(&kernvmod, 0, kernvmod_vals, 8);
	PalHigh_KernelInit(&kernysep, -4, kernysep_vals, 9);
	PalHigh_KernelInit(&kerncsep, -16, kerncsep_vals, 33);
	PalHigh_KernelScale(&kerncsep, 1.0f / 16.0f);
	PalHigh_KernelInit(&kerncdemod, 0, kerncdemod_vals, 8);
	PalHigh_KernelScale(&kerncdemod, 0.5f);

	for (phase = 0; phase < PAL_HI_PHASES; ++phase) {
		PAL_HI_Kernel kernbase2;
		PAL_HI_Kernel kernsignaly;
		PAL_HI_Kernel kernsignalu;
		PAL_HI_Kernel kernsignalv;
		PAL_HI_Kernel tmp;
		PAL_HI_Kernel tmp2;
		PAL_HI_Kernel u;
		PAL_HI_Kernel v;
		PAL_HI_Kernel u_scaled;
		PAL_HI_Kernel v_scaled;

		PalHigh_KernelShift(&kernbase2, &kernbase, 5 * phase);
		kernsignaly = kernbase2;
		PalHigh_KernelConvolve(&kernsignalu, &kernbase2, &kerncfilt);
		PalHigh_KernelModulate(&kernsignalu, &kernsignalu, &kernumod);
		PalHigh_KernelConvolve(&kernsignalv, &kernbase2, &kerncfilt);
		PalHigh_KernelModulate(&kernsignalv, &kernsignalv, &kernvmod);

		PalHigh_KernelConvolve(&tmp, &kernsignaly, &kernysep);
		PalHigh_KernelSampleBicubic(&kerny2y[phase], &tmp, 0.0f, 2.5f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalu, &kernysep);
		PalHigh_KernelSampleBicubic(&kernu2y[phase], &tmp, 0.0f, 2.5f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalv, &kernysep);
		PalHigh_KernelSampleBicubic(&kernv2y[phase], &tmp, 0.0f, 2.5f, -0.75f);

		PalHigh_KernelConvolve(&tmp, &kernsignaly, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 0, 4);
		PalHigh_KernelSampleBicubic(&u, &tmp2, 0.0f, 0.625f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalu, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 0, 4);
		PalHigh_KernelSampleBicubic(&kernu2u[phase], &tmp2, 0.0f, 0.625f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalv, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 0, 4);
		PalHigh_KernelSampleBicubic(&kernv2u[phase], &tmp2, 0.0f, 0.625f, -0.75f);

		PalHigh_KernelConvolve(&tmp, &kernsignaly, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 2, 4);
		PalHigh_KernelSampleBicubic(&v, &tmp2, -0.5f, 0.625f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalu, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 2, 4);
		PalHigh_KernelSampleBicubic(&kernu2v[phase], &tmp2, -0.5f, 0.625f, -0.75f);
		PalHigh_KernelConvolve(&tmp, &kernsignalv, &kerncsep);
		PalHigh_KernelModulate(&tmp, &tmp, &kerncdemod);
		PalHigh_KernelSamplePoint(&tmp2, &tmp, 2, 4);
		PalHigh_KernelSampleBicubic(&kernv2v[phase], &tmp2, -0.5f, 0.625f, -0.75f);

		u_scaled = u;
		v_scaled = v;
		PalHigh_KernelScale(&u_scaled, ycphasec);
		PalHigh_KernelScale(&v_scaled, -ycphases);
		PalHigh_KernelAdd(&kerny2u[phase], &u_scaled, &v_scaled, FALSE);

		u_scaled = u;
		v_scaled = v;
		PalHigh_KernelScale(&u_scaled, ycphases);
		PalHigh_KernelScale(&v_scaled, ycphasec);
		PalHigh_KernelAdd(&kerny2v[phase], &u_scaled, &v_scaled, FALSE);
	}

	for (k = 0; k < 2; ++k) {
		for (j = 0; j < 256; ++j) {
			float y = ytab[k][j];
			float u = utab[k][j];
			float v = vtab[k][j];
			float yerror[8][2] = { { 0 } };
			float uerror[8][2] = { { 0 } };
			float verror[8][2] = { { 0 } };

			for (phase = 0; phase < PAL_HI_PHASES; ++phase) {
				float p2yw[8 + 8] = { 0 };
				float p2uw[24 + 8] = { 0 };
				float p2vw[24 + 8] = { 0 };
				int ypos = 4 - phase * 2;
				int cpos = 13 - phase * 2;
				int offset;

				PalHigh_KernelAccumulateWindow(&kerny2y[phase], p2yw, ypos, 8, y);
				PalHigh_KernelAccumulateWindow(&kernu2y[phase], p2yw, ypos, 8, u);
				PalHigh_KernelAccumulateWindow(&kernv2y[phase], p2yw, ypos, 8, v);

				PalHigh_KernelAccumulateWindow(&kerny2u[phase], p2uw, cpos, 24, y * (float)pal_high_co_ub);
				PalHigh_KernelAccumulateWindow(&kernu2u[phase], p2uw, cpos, 24, u * (float)pal_high_co_ub);
				PalHigh_KernelAccumulateWindow(&kernv2u[phase], p2uw, cpos, 24, v * (float)pal_high_co_ub);

				PalHigh_KernelAccumulateWindow(&kerny2v[phase], p2vw, cpos, 24, y * (float)pal_high_co_vr);
				PalHigh_KernelAccumulateWindow(&kernu2v[phase], p2vw, cpos, 24, u * (float)pal_high_co_vr);
				PalHigh_KernelAccumulateWindow(&kernv2v[phase], p2vw, cpos, 24, v * (float)pal_high_co_vr);

				for (offset = 0; offset < 4; ++offset) {
					float fw0 = p2yw[offset * 2 + 0] * scale + yerror[(offset + phase) & 7][0];
					float fw1 = p2yw[offset * 2 + 1] * scale + yerror[(offset + phase) & 7][1];
					int w0 = (int)floorf(fw0 + 0.5f);
					int w1 = (int)floorf(fw1 + 0.5f);
					yerror[(offset + phase) & 7][0] = fw0 - (float)w0;
					yerror[(offset + phase) & 7][1] = fw1 - (float)w1;
					pal_high_to_y[k][j][phase][offset] = ((ULONG)(w1 & 0xffff) << 16) | (ULONG)(w0 & 0xffff);
				}

				pal_high_to_y[k][j][phase][3] += 0x40004000;

				for (offset = 0; offset < 12; ++offset) {
					float fw0 = p2uw[offset * 2 + 0] * scale + uerror[(offset + phase) & 7][0];
					float fw1 = p2uw[offset * 2 + 1] * scale + uerror[(offset + phase) & 7][1];
					int w0 = (int)floorf(fw0 + 0.5f);
					int w1 = (int)floorf(fw1 + 0.5f);
					uerror[(offset + phase) & 7][0] = fw0 - (float)w0;
					uerror[(offset + phase) & 7][1] = fw1 - (float)w1;
					pal_high_to_u[k][j][phase][offset] = ((ULONG)(w1 & 0xffff) << 16) | (ULONG)(w0 & 0xffff);
				}

				for (offset = 0; offset < 12; ++offset) {
					float fw0 = p2vw[offset * 2 + 0] * scale + verror[(offset + phase) & 7][0];
					float fw1 = p2vw[offset * 2 + 1] * scale + verror[(offset + phase) & 7][1];
					int w0 = (int)floorf(fw0 + 0.5f);
					int w1 = (int)floorf(fw1 + 0.5f);
					verror[(offset + phase) & 7][0] = fw0 - (float)w0;
					verror[(offset + phase) & 7][1] = fw1 - (float)w1;
					pal_high_to_v[k][j][phase][offset] = ((ULONG)(w1 & 0xffff) << 16) | (ULONG)(w0 & 0xffff);
				}
			}
		}
	}

	pal_high_tables_ready = TRUE;
}

void PAL_BLENDING_UpdateLookupHigh(void)
{
	/* was: if (ARTIFACT_mode == ARTIFACT_PAL_HIGH) */
	if (ARTIFACT_mode == ARTIFACT_PAL_HIGH || ARTIFACT_mode == ARTIFACT_PAL_HIGH_BLUR) {
		PLATFORM_pixel_format_t format;
		PLATFORM_GetPixelFormat(&format);

		pal_high_format.bpp = format.bpp;
		pal_high_format.rmask = format.rmask;
		pal_high_format.gmask = format.gmask;
		pal_high_format.bmask = format.bmask;
		pal_high_format.rshift = PalHigh_FirstBit(format.rmask);
		pal_high_format.gshift = PalHigh_FirstBit(format.gmask);
		pal_high_format.bshift = PalHigh_FirstBit(format.bmask);
		pal_high_format.rmax = format.rmask >> pal_high_format.rshift;
		pal_high_format.gmax = format.gmask >> pal_high_format.gshift;
		pal_high_format.bmax = format.bmask >> pal_high_format.bshift;
		pal_high_format.avg_mask = (format.rmask & ~(format.rmask << 1))
			| (format.gmask & ~(format.gmask << 1))
			| (format.bmask & ~(format.bmask << 1));

		pal_high_apply_gamma = TRUE;
		PalHigh_RecomputeTables();
	}
}

static void PalHigh_ResetDelay(int width)
{
	int i;
	int total = width + PAL_HI_DELAY_PAD;
	if (total > PAL_HI_MAX_WIDTH + PAL_HI_DELAY_PAD)
		total = PAL_HI_MAX_WIDTH + PAL_HI_DELAY_PAD;
	for (i = 0; i < total; ++i) {
		pal_high_delay_uv[0][i] = 0x20002000;
		pal_high_delay_uv[1][i] = 0x20002000;
	}
}

static void PalHigh_Luma(ULONG *dst, const UBYTE *src, int n, const ULONG *kernels)
{
	ULONG x0 = 0x40004000;
	ULONG x1 = 0x40004000;
	ULONG x2 = 0x40004000;
	const ULONG *f;

	do {
		f = kernels + 32 * (*src++);
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 4;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 8;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 12;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 16;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 20;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 24;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

		if (!--n)
			break;

		f = kernels + 32 * (*src++) + 28;
		*dst++ = x0 + f[0];
		x0 = x1 + f[1];
		x1 = x2 + f[2];
		x2 = f[3];

	} while (--n);

	*dst++ = x0;
	*dst++ = x1;
	*dst++ = x2;
}

static void PalHigh_Chroma(ULONG *dst, const UBYTE *src, int n, const ULONG *kernels)
{
	ptrdiff_t koffset = 0;

	do {
		const ULONG *f = kernels + 96 * (*src++) + koffset;
		dst[0] += f[0];
		dst[1] += f[1];
		dst[2] += f[2];
		dst[3] += f[3];
		dst[4] += f[4];
		dst[5] += f[5];
		dst[6] += f[6];
		dst[7] += f[7];
		dst[8] += f[8];
		dst[9] += f[9];
		dst[10] += f[10];
		dst[11] += f[11];
		++dst;

		koffset += 12;

		if (koffset == 12 * 8)
			koffset = 0;
	} while (--n);
}

static void PalHigh_Final32(ULONG *dst, const ULONG *ybuf, const ULONG *ubuf, const ULONG *vbuf, ULONG *ulbuf, ULONG *vlbuf, int n)
{
	const double y_scale = 1.0 / (64.0 * 255.0);
	const double u_scale = 1.0 / (64.0 * 255.0 * pal_high_co_ub);
	const double v_scale = 1.0 / (64.0 * 255.0 * pal_high_co_vr);
	int i;

	for (i = 0; i < n; ++i) {
		const ULONG y = ybuf[i];
		ULONG u = ubuf[i + 4];
		ULONG v = vbuf[i + 4];
		const ULONG up = ulbuf[i + 4];
		const ULONG vp = vlbuf[i + 4];

		ulbuf[i + 4] = u;
		vlbuf[i + 4] = v;

		u += up;
		v += vp;

		{
			int y1 = (int)(y & 0xffff);
			int u1 = (int)(u & 0xffff);
			int v1 = (int)(v & 0xffff);
			int y2 = (int)(y >> 16);
			int u2 = (int)(u >> 16);
			int v2 = (int)(v >> 16);

			double y1f = (double)(y1 - 0x4000) * y_scale;
			double u1f = (double)(u1 - 0x4000) * u_scale;
			double v1f = (double)(v1 - 0x4000) * v_scale;
			double y2f = (double)(y2 - 0x4000) * y_scale;
			double u2f = (double)(u2 - 0x4000) * u_scale;
			double v2f = (double)(v2 - 0x4000) * v_scale;
			double r1d, g1d, b1d;
			double r2d, g2d, b2d;

			Colours_YUV2RGB(y1f, u1f, v1f, &r1d, &g1d, &b1d);
			Colours_YUV2RGB(y2f, u2f, v2f, &r2d, &g2d, &b2d);

			int r1 = (int)(r1d * 255.0 + 0.5);
			int g1 = (int)(g1d * 255.0 + 0.5);
			int b1 = (int)(b1d * 255.0 + 0.5);
			int r2 = (int)(r2d * 255.0 + 0.5);
			int g2 = (int)(g2d * 255.0 + 0.5);
			int b2 = (int)(b2d * 255.0 + 0.5);

			if (r1 < 0) r1 = 0; else if (r1 > 255) r1 = 255;
			if (g1 < 0) g1 = 0; else if (g1 > 255) g1 = 255;
			if (b1 < 0) b1 = 0; else if (b1 > 255) b1 = 255;

			if (r2 < 0) r2 = 0; else if (r2 > 255) r2 = 255;
			if (g2 < 0) g2 = 0; else if (g2 > 255) g2 = 255;
			if (b2 < 0) b2 = 0; else if (b2 > 255) b2 = 255;

			PalHigh_ApplyGamma(&r1, &g1, &b1);
			PalHigh_ApplyGamma(&r2, &g2, &b2);

			dst[0] = PalHigh_PackRGB(r1, g1, b1);
			dst[1] = PalHigh_PackRGB(r2, g2, b2);
			dst += 2;
		}
	}
}

static void PalHigh_Final16(ULONG *dst, const ULONG *ybuf, const ULONG *ubuf, const ULONG *vbuf, ULONG *ulbuf, ULONG *vlbuf, int n)
{
	const double y_scale = 1.0 / (64.0 * 255.0);
	const double u_scale = 1.0 / (64.0 * 255.0 * pal_high_co_ub);
	const double v_scale = 1.0 / (64.0 * 255.0 * pal_high_co_vr);
	int i;

	for (i = 0; i < n; ++i) {
		const ULONG y = ybuf[i];
		ULONG u = ubuf[i + 4];
		ULONG v = vbuf[i + 4];
		const ULONG up = ulbuf[i + 4];
		const ULONG vp = vlbuf[i + 4];

		ulbuf[i + 4] = u;
		vlbuf[i + 4] = v;

		u += up;
		v += vp;

		{
			int y1 = (int)(y & 0xffff);
			int u1 = (int)(u & 0xffff);
			int v1 = (int)(v & 0xffff);
			int y2 = (int)(y >> 16);
			int u2 = (int)(u >> 16);
			int v2 = (int)(v >> 16);

			double y1f = (double)(y1 - 0x4000) * y_scale;
			double u1f = (double)(u1 - 0x4000) * u_scale;
			double v1f = (double)(v1 - 0x4000) * v_scale;
			double y2f = (double)(y2 - 0x4000) * y_scale;
			double u2f = (double)(u2 - 0x4000) * u_scale;
			double v2f = (double)(v2 - 0x4000) * v_scale;
			double r1d, g1d, b1d;
			double r2d, g2d, b2d;

			Colours_YUV2RGB(y1f, u1f, v1f, &r1d, &g1d, &b1d);
			Colours_YUV2RGB(y2f, u2f, v2f, &r2d, &g2d, &b2d);

			int r1 = (int)(r1d * 255.0 + 0.5);
			int g1 = (int)(g1d * 255.0 + 0.5);
			int b1 = (int)(b1d * 255.0 + 0.5);
			int r2 = (int)(r2d * 255.0 + 0.5);
			int g2 = (int)(g2d * 255.0 + 0.5);
			int b2 = (int)(b2d * 255.0 + 0.5);

			if (r1 < 0) r1 = 0; else if (r1 > 255) r1 = 255;
			if (g1 < 0) g1 = 0; else if (g1 > 255) g1 = 255;
			if (b1 < 0) b1 = 0; else if (b1 > 255) b1 = 255;

			if (r2 < 0) r2 = 0; else if (r2 > 255) r2 = 255;
			if (g2 < 0) g2 = 0; else if (g2 > 255) g2 = 255;
			if (b2 < 0) b2 = 0; else if (b2 > 255) b2 = 255;

			PalHigh_ApplyGamma(&r1, &g1, &b1);
			PalHigh_ApplyGamma(&r2, &g2, &b2);

			{
				ULONG p0 = PalHigh_PackRGB(r1, g1, b1);
				ULONG p1 = PalHigh_PackRGB(r2, g2, b2);
				*dst++ = (p1 << 16) | (p0 & 0xffff);
			}
		}
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

static void PalHigh_RenderLine32(ULONG *dst, const UBYTE *src, int width, int odd)
{
	ULONG ybuf[PAL_HI_MAX_WIDTH + 32];
	ULONG ubuf[PAL_HI_MAX_WIDTH + 32];
	ULONG vbuf[PAL_HI_MAX_WIDTH + 32];
	UBYTE src_shifted[PAL_HI_MAX_WIDTH + 16];
	const ULONG *kerny = &pal_high_to_y[odd][0][0][0];
	const ULONG *kernu = &pal_high_to_u[odd][0][0][0];
	const ULONG *kernv = &pal_high_to_v[odd][0][0][0];
	int i;
	int n = width;

	if (n > PAL_HI_MAX_WIDTH)
		n = PAL_HI_MAX_WIDTH;

	memset(src_shifted, 0, sizeof(src_shifted));
	memcpy(src_shifted + 2, src, n);

	PalHigh_Luma(ybuf, src_shifted, n + 16, kerny);
	for (i = 0; i < n + 32; ++i) {
		ubuf[i] = 0x20002000;
		vbuf[i] = 0x20002000;
	}
	PalHigh_Chroma(ubuf, src_shifted, n + 16, kernu);
	PalHigh_Chroma(vbuf, src_shifted, n + 16, kernv);
	PalHigh_Final32(dst, ybuf + 4, ubuf + 4, vbuf + 4, pal_high_delay_uv[0], pal_high_delay_uv[1], n);
}

static void PalHigh_RenderLine16(ULONG *dst, const UBYTE *src, int width, int odd)
{
	ULONG ybuf[PAL_HI_MAX_WIDTH + 32];
	ULONG ubuf[PAL_HI_MAX_WIDTH + 32];
	ULONG vbuf[PAL_HI_MAX_WIDTH + 32];
	UBYTE src_shifted[PAL_HI_MAX_WIDTH + 16];
	const ULONG *kerny = &pal_high_to_y[odd][0][0][0];
	const ULONG *kernu = &pal_high_to_u[odd][0][0][0];
	const ULONG *kernv = &pal_high_to_v[odd][0][0][0];
	int i;
	int n = width;

	if (n > PAL_HI_MAX_WIDTH)
		n = PAL_HI_MAX_WIDTH;

	memset(src_shifted, 0, sizeof(src_shifted));
	memcpy(src_shifted + 2, src, n);

	PalHigh_Luma(ybuf, src_shifted, n + 16, kerny);
	for (i = 0; i < n + 32; ++i) {
		ubuf[i] = 0x20002000;
		vbuf[i] = 0x20002000;
	}
	PalHigh_Chroma(ubuf, src_shifted, n + 16, kernu);
	PalHigh_Chroma(vbuf, src_shifted, n + 16, kernv);
	PalHigh_Final16(dst, ybuf + 4, ubuf + 4, vbuf + 4, pal_high_delay_uv[0], pal_high_delay_uv[1], n);
}

void PAL_BLENDING_BlitHigh32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	int odd = start_odd & 1;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (height > 0) {
		PalHigh_RenderLine32(dest, src, width, odd);
		dest += pitch;
		src += Screen_WIDTH;
		odd ^= 1;
		--height;
	}
}

void PAL_BLENDING_BlitHigh16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	int odd = start_odd & 1;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (height > 0) {
		PalHigh_RenderLine16(dest, src, width, odd);
		dest += pitch;
		src += Screen_WIDTH;
		odd ^= 1;
		--height;
	}
}

void PAL_BLENDING_BlitHighBlur32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	ULONG blurbuf[PAL_HI_MAX_WIDTH * 2];
	int odd = start_odd & 1;
	int n = width * 2;
	int i;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (height > 0) {
		PalHigh_RenderLine32(linebuf, src, width, odd);
		PalHigh_BlurLine(blurbuf, linebuf, n);
		for (i = 0; i < n; ++i)
			dest[i] = blurbuf[i];
		dest += pitch;
		src += Screen_WIDTH;
		odd ^= 1;
		--height;
	}
}

void PAL_BLENDING_BlitHighBlur16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	ULONG blurbuf[PAL_HI_MAX_WIDTH * 2];
	int odd = start_odd & 1;
	int n = width * 2;
	int i;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (height > 0) {
		PalHigh_RenderLine32(linebuf, src, width, odd);
		PalHigh_BlurLine(blurbuf, linebuf, n);
		for (i = 0; i < n; i += 2) {
			ULONG p0 = blurbuf[i] & 0xffff;
			ULONG p1 = blurbuf[i + 1] & 0xffff;
			dest[i / 2] = (p1 << 16) | p0;
		}
		dest += pitch;
		src += Screen_WIDTH;
		odd ^= 1;
		--height;
	}
}

void PAL_BLENDING_BlitHighScaled32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	int y = 0x10000;
	int w = (width * 2) << 16;
	int pos;
	int dx = w / dest_width;
	int dy = (height << 16) / dest_height;
	int init_x = w - 0x4000;
	int odd = start_odd & 1;
	int line_ready = FALSE;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (dest_height > 0) {
		int x = init_x;
		if (!line_ready) {
			PalHigh_RenderLine32(linebuf, src, width, odd);
			/* was for blur mode:
			PalHigh_BlurLine(blurbuf, linebuf, width * 2);
			*/
			line_ready = TRUE;
		}

		pos = dest_width - 1;
		while (pos >= 0) {
			dest[pos] = linebuf[x >> 16];
			x -= dx;
			--pos;
		}

		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src += Screen_WIDTH;
			odd ^= 1;
			line_ready = FALSE;
		}
	}
}

void PAL_BLENDING_BlitHighScaled16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	int y = 0x10000;
	int w = (width * 2) << 16;
	int pos;
	int dx = w / dest_width;
	int dy = (height << 16) / dest_height;
	int init_x = w - 0x4000;
	int odd = start_odd & 1;
	int line_ready = FALSE;
	int w1 = dest_width / 2 - 1;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (dest_height > 0) {
		int x = init_x;
		if (!line_ready) {
			PalHigh_RenderLine32(linebuf, src, width, odd);
			/* was for blur mode:
			PalHigh_BlurLine(blurbuf, linebuf, width * 2);
			*/
			line_ready = TRUE;
		}

		pos = w1;
		while (pos >= 0) {
			ULONG p0 = linebuf[x >> 16] & 0xffff;
			x -= dx;
			ULONG p1 = linebuf[x >> 16] & 0xffff;
			x -= dx;
			dest[pos] = (p1 << 16) | p0;
			--pos;
		}

		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src += Screen_WIDTH;
			odd ^= 1;
			line_ready = FALSE;
		}
	}
}

void PAL_BLENDING_BlitHighBlurScaled32(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	ULONG blurbuf[PAL_HI_MAX_WIDTH * 2];
	int y = 0x10000;
	int w = (width * 2) << 16;
	int pos;
	int dx = w / dest_width;
	int dy = (height << 16) / dest_height;
	int init_x = w - 0x4000;
	int odd = start_odd & 1;
	int line_ready = FALSE;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (dest_height > 0) {
		int x = init_x;
		if (!line_ready) {
			PalHigh_RenderLine32(linebuf, src, width, odd);
			PalHigh_BlurLine(blurbuf, linebuf, width * 2);
			line_ready = TRUE;
		}

		pos = dest_width - 1;
		while (pos >= 0) {
			dest[pos] = blurbuf[x >> 16];
			x -= dx;
			--pos;
		}

		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src += Screen_WIDTH;
			odd ^= 1;
			line_ready = FALSE;
		}
	}
}

void PAL_BLENDING_BlitHighBlurScaled16(ULONG *dest, UBYTE *src, int pitch, int width, int height, int dest_width, int dest_height, int start_odd)
{
	ULONG linebuf[PAL_HI_MAX_WIDTH * 2];
	ULONG blurbuf[PAL_HI_MAX_WIDTH * 2];
	int y = 0x10000;
	int w = (width * 2) << 16;
	int pos;
	int dx = w / dest_width;
	int dy = (height << 16) / dest_height;
	int init_x = w - 0x4000;
	int odd = start_odd & 1;
	int line_ready = FALSE;
	int w1 = dest_width / 2 - 1;

	if (!pal_high_tables_ready)
		PalHigh_RecomputeTables();
	PalHigh_ResetDelay(width);

	while (dest_height > 0) {
		int x = init_x;
		if (!line_ready) {
			PalHigh_RenderLine32(linebuf, src, width, odd);
			PalHigh_BlurLine(blurbuf, linebuf, width * 2);
			line_ready = TRUE;
		}

		pos = w1;
		while (pos >= 0) {
			ULONG p0 = blurbuf[x >> 16] & 0xffff;
			x -= dx;
			ULONG p1 = blurbuf[x >> 16] & 0xffff;
			x -= dx;
			dest[pos] = (p1 << 16) | p0;
			--pos;
		}

		dest += pitch;
		y -= dy;
		--dest_height;
		if (y < 0) {
			y += 0x10000;
			src += Screen_WIDTH;
			odd ^= 1;
			line_ready = FALSE;
		}
	}
}
