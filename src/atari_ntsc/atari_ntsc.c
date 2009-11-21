/* Based on nes_ntsc 0.2.2. http://www.slack.net/~ant/ */

#include "atari_ntsc.h"
#include "../log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Copyright (C) 2006-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

/* Atari change: removal and addition of structure fields.
   Values of resolution and sharpness adjusted to make NTSC artifacts look better.
   Values 16 and 235 for black_level and white_level are taken from
   http://en.wikipedia.org/wiki/Rec._601 */
atari_ntsc_setup_t const atari_ntsc_monochrome = { 0, -1,   0, 0, -.3,  .3,  .2, -.2, -.2, -1, 0, 0, 0, 0.f, 21.f, 16, 235 };
atari_ntsc_setup_t const atari_ntsc_composite  = { 0, -0.5, 0, 0, -.5,  .3, -.1,   0,   0,  0, 0, 0, 0, 0.f, 21.f, 16, 235 };
atari_ntsc_setup_t const atari_ntsc_svideo     = { 0, -0.5, 0, 0, -.3,  .3,  .2,  -1,  -1,  0, 0, 0, 0, 0.f, 21.f, 16, 235 };
atari_ntsc_setup_t const atari_ntsc_rgb        = { 0, -0.5, 0, 0, -.3,  .3,  .7,  -1,  -1, -1, 0, 0, 0, 0.f, 21.f, 16, 235 };

#define alignment_count 4
#define burst_count     1
#define rescale_in      8
#define rescale_out     7

#define artifacts_mid   1.0f
#define fringing_mid    1.0f
/* Atari change: default palette is already at correct hue.
#define std_decoder_hue -15 */
#define std_decoder_hue 0

/* Atari change: only one palette - remove base_palete field. */
#define STD_HUE_CONDITION( setup ) !(setup->palette)

#include "atari_ntsc_impl.h"

/* Atari change: adapted to 4/7 pixel ratio. */
/* 4 input pixels -> 8 composite samples */
pixel_info_t const atari_ntsc_pixels [alignment_count] = {
	{ PIXEL_OFFSET( -6, -6 ), { 0, 0, 1, 1 } },
	{ PIXEL_OFFSET( -4, -4 ), { 0, 0, 1, 1 } },
	{ PIXEL_OFFSET( -2, -2 ), { 0, 0, 1, 1 } },
	{ PIXEL_OFFSET(  0,  0 ), { 0, 0, 1, 1 } },
};

/* Atari change: no alternating burst phases - removed merge_kernel_fields function. */

static void correct_errors( atari_ntsc_rgb_t color, atari_ntsc_rgb_t* out )
{
	int n;
	for ( n = burst_count; n; --n )
	{
		unsigned i;
		for ( i = 0; i < rgb_kernel_size / 2; i++ )
		{
			/* Atari change: adapted to 4/7 pixel ratio */
			atari_ntsc_rgb_t error = color -
					out [i    ] - out [(i+12)%14+14] - out [(i+10)%14+28] - out[(i+8)%14+42] -
					out [i + 7] - out [i + 5    +14] - out [i + 3    +28] - out [ i+1    +42];
			DISTRIBUTE_ERROR( i+1+42, i+3+28, i+5+14, i+7 );
		}
		out += alignment_count * rgb_kernel_size;
	}
}

void atari_ntsc_init( atari_ntsc_t* ntsc, atari_ntsc_setup_t const* setup )
{
	/* Atari change: no alternating burst phases - remove merge_fields variable. */
	int entry;
	init_t impl;
	float gamma_factor;

	/* Atari change:
	   NTSC coloburst frequency is (315.0/88.0)MHz
	   (see http://en.wikipedia.org/wiki/Colorburst). By dividing 1000 by
	   this fraction, we get NTSC color cycle duration in nanoseconds. */
	static const float color_cycle_length = 1000.0f * 88.0f / 315.0f;
	/* Atari change: additional variables for palette generation. */
	float burst_phase;
	float color_diff;
	float scaled_black_level;
	float scaled_white_level;

	if ( !setup )
		setup = &atari_ntsc_composite;

	burst_phase = setup->burst_phase * PI;
	/* This computes difference between two consecutive chrominances, in
	   radians. */
	color_diff = setup->color_delay / color_cycle_length * 2 * PI;
	scaled_black_level = (float)setup->black_level / 255.0f;
	scaled_white_level = (float)setup->white_level / 255.0f;

	init( &impl, setup );
	
	/* setup fast gamma */
	{
		float gamma = (float) setup->gamma * -0.5f;
		if ( STD_HUE_CONDITION( setup ) )
			gamma += 0.1333f;
		
		gamma_factor = (float) pow( (float) fabs( gamma ), 0.73f );
		if ( gamma < 0 )
			gamma_factor = -gamma_factor;
	}

	/* Atari change: no alternating burst phases - remove code for merge_fields. */

	for ( entry = 0; entry < atari_ntsc_palette_size; entry++ )
	{
		/* Atari change: Atari palette generation instead of the NES one;
		   the NES color emphasis code also got removed. */
		/* NTSC luma multipliers from CGIA.PDF */
		float luma_mult[16]={
			0.6941, 0.7091, 0.7241, 0.7401, 
			0.7560, 0.7741, 0.7931, 0.8121,
			0.8260, 0.8470, 0.8700, 0.8930,
			0.9160, 0.9420, 0.9690, 1.0000};
		/* calculate yiq for color entry */
		int color = entry >> 4;
		float angle = burst_phase - (color - 1) * color_diff;
		float y = ( luma_mult[entry & 0x0f] - luma_mult[0] )/( luma_mult[15] - luma_mult[0] );
		float saturation = (color ? 0.35f: 0.0f);
		float i = sin( angle ) * saturation;
		float q = cos( angle ) * saturation;

		/* Optionally use palette instead */
		if ( setup->palette )
		{
			unsigned char const* in = &setup->palette [entry * 3];
			static float const to_float = 1.0f / 0xFF;
			float r = to_float * in [0];
			float g = to_float * in [1];
			float b = to_float * in [2];
			q = RGB_TO_YIQ( r, g, b, y, i );
		}

		/* Apply brightness, contrast, and gamma */
		y *= (float) setup->contrast * 0.5f + 1;
		/* adjustment reduces error when using input palette */
		y += (float) setup->brightness * 0.5f - 0.5f / 256;

		/* Atari change: support for black_ and white_level. */
		y = y * (scaled_white_level - scaled_black_level) + scaled_black_level;
		if (y < scaled_black_level)
			y = scaled_black_level;
		else if (y > scaled_white_level)
			y = scaled_white_level;

		{
			float r, g, b = YIQ_TO_RGB( y, i, q, default_decoder, float, r, g );

			/* fast approximation of n = pow( n, gamma ) */
			r = (r * gamma_factor - gamma_factor) * r + r;
			g = (g * gamma_factor - gamma_factor) * g + g;
			b = (b * gamma_factor - gamma_factor) * b + b;

			q = RGB_TO_YIQ( r, g, b, y, i );
		}

		i *= rgb_unit;
		q *= rgb_unit;
		y *= rgb_unit;
		y += rgb_offset;

		/* Generate kernel */
		{
			int r, g, b = YIQ_TO_RGB( y, i, q, impl.to_rgb, int, r, g );
			/* blue tends to overflow, so clamp it */
			atari_ntsc_rgb_t rgb = PACK_RGB( r, g, (b < 0x3E0 ? b: 0x3E0) );
			
			if ( setup->palette_out )
				RGB_PALETTE_OUT( rgb, &setup->palette_out [entry * 3] );
			
			if ( ntsc )
			{
				atari_ntsc_rgb_t* kernel = ntsc->table [entry];
				gen_kernel( &impl, y, i, q, kernel );
				/* Atari change: no alternating burst phases - remove code for merge_fields. */
				correct_errors( rgb, kernel );
			}
		}
	}
	Log_print("atari_ntsc_init(): hue:%f saturation:%f contrast:%f brightness:%f sharpness:%f gamma:%f res:%f artif:%f fring:%f bleed:%f burst_phase:%f color_delay:%f\n",
		  setup->hue,setup->saturation,setup->contrast,setup->brightness,setup->sharpness,
		  setup->gamma,setup->resolution,setup->artifacts,setup->fringing,setup->bleed,
		  setup->burst_phase,setup->color_delay);
}

#ifndef ATARI_NTSC_NO_BLITTERS

/* Atari change: no alternating burst phases - remove burst_phase parameter. */
void atari_ntsc_blit( atari_ntsc_t const* ntsc, ATARI_NTSC_IN_T const* input, long in_row_width,
		int in_width, int in_height, void* rgb_out, long out_pitch )
{
	int chunk_count = (in_width - 1) / atari_ntsc_in_chunk;
	for ( ; in_height; --in_height )
	{
		ATARI_NTSC_IN_T const* line_in = input;
		/* Atari change: no alternating burst phases - remove burst_phase parameter; adjust to 4/7 pixel ratio. */
		ATARI_NTSC_BEGIN_ROW( ntsc,
				atari_ntsc_black, atari_ntsc_black, atari_ntsc_black, ATARI_NTSC_ADJ_IN( *line_in ) );
		atari_ntsc_out_t* restrict line_out = (atari_ntsc_out_t*) rgb_out;
		int n;
		++line_in;

		for ( n = chunk_count; n; --n )
		{
			/* order of input and output pixels must not be altered */
			ATARI_NTSC_COLOR_IN( 0, ATARI_NTSC_ADJ_IN( line_in [0] ) );
			ATARI_NTSC_RGB_OUT( 0, line_out [0], ATARI_NTSC_OUT_DEPTH );
			ATARI_NTSC_RGB_OUT( 1, line_out [1], ATARI_NTSC_OUT_DEPTH );

			ATARI_NTSC_COLOR_IN( 1, ATARI_NTSC_ADJ_IN( line_in [1] ) );
			ATARI_NTSC_RGB_OUT( 2, line_out [2], ATARI_NTSC_OUT_DEPTH );
			ATARI_NTSC_RGB_OUT( 3, line_out [3], ATARI_NTSC_OUT_DEPTH );

			ATARI_NTSC_COLOR_IN( 2, ATARI_NTSC_ADJ_IN( line_in [2] ) );
			ATARI_NTSC_RGB_OUT( 4, line_out [4], ATARI_NTSC_OUT_DEPTH );
			ATARI_NTSC_RGB_OUT( 5, line_out [5], ATARI_NTSC_OUT_DEPTH );

			ATARI_NTSC_COLOR_IN( 3, ATARI_NTSC_ADJ_IN( line_in [3] ) );
			ATARI_NTSC_RGB_OUT( 6, line_out [6], ATARI_NTSC_OUT_DEPTH );

			line_in  += 4;
			line_out += 7;
		}

		/* finish final pixels */
		ATARI_NTSC_COLOR_IN( 0, atari_ntsc_black );
		ATARI_NTSC_RGB_OUT( 0, line_out [0], ATARI_NTSC_OUT_DEPTH );
		ATARI_NTSC_RGB_OUT( 1, line_out [1], ATARI_NTSC_OUT_DEPTH );

		ATARI_NTSC_COLOR_IN( 1, atari_ntsc_black );
		ATARI_NTSC_RGB_OUT( 2, line_out [2], ATARI_NTSC_OUT_DEPTH );
		ATARI_NTSC_RGB_OUT( 3, line_out [3], ATARI_NTSC_OUT_DEPTH );

		ATARI_NTSC_COLOR_IN( 2, atari_ntsc_black );
		ATARI_NTSC_RGB_OUT( 4, line_out [4], ATARI_NTSC_OUT_DEPTH );
		ATARI_NTSC_RGB_OUT( 5, line_out [5], ATARI_NTSC_OUT_DEPTH );

		ATARI_NTSC_COLOR_IN( 3, atari_ntsc_black );
		ATARI_NTSC_RGB_OUT( 6, line_out [6], ATARI_NTSC_OUT_DEPTH );

		input += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}

#endif

/* Atari800-specific: */

atari_ntsc_setup_t atari_ntsc_setup;

int atari_ntsc_Initialise(int *argc, char *argv[])
{
	int i, j;

	/* Set the default palette setup. */
	atari_ntsc_setup = atari_ntsc_composite;
/*	memcpy(&atari_ntsc_setup, &atari_ntsc_composite, sizeof(atari_ntsc_setup_t));*/

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = 0;			/* error, argument missing! */
		
		if (strcmp(argv[i], "-ntsc_hue") == 0) {
			if (i_a)
				atari_ntsc_setup.hue = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_sat") == 0) {
			if (i_a)
				atari_ntsc_setup.saturation = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_cont") == 0) {
			if (i_a)
				atari_ntsc_setup.contrast = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_bright") == 0) {
			if (i_a)
				atari_ntsc_setup.brightness = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_sharp") == 0) {
			if (i_a)
				atari_ntsc_setup.sharpness = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_gamma") == 0) {
			if (i_a)
				atari_ntsc_setup.gamma = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_res") == 0) {
			if (i_a)
				atari_ntsc_setup.resolution = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_artif") == 0) {
			if (i_a)
				atari_ntsc_setup.artifacts = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_fring") == 0) {
			if (i_a)
				atari_ntsc_setup.fringing = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_bleed") == 0) {
			if (i_a)
				atari_ntsc_setup.bleed = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_burst") == 0) {
			if (i_a)
				atari_ntsc_setup.burst_phase = atof(argv[++i]);
			else a_m = 1;
		} else if (strcmp(argv[i], "-ntsc_delay") == 0) {
			if (i_a)
				atari_ntsc_setup.color_delay = atof(argv[++i]);
			else a_m = 1;
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-ntsc_hue <n>          Set NTSC hue -1..1 (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.hue);
				Log_print("\t-ntsc_sat <n>          Set NTSC saturation (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.saturation);
				Log_print("\t-ntsc_cont <n>         Set NTSC contrast (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.contrast);
				Log_print("\t-ntsc_bright <n>       Set NTSC brightness (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.brightness);
				Log_print("\t-ntsc_sharp <n>        Set NTSC sharpness (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.sharpness);
				Log_print("\t-ntsc_gamma <n>        Set NTSC gamma adjustment (default %.2g)", atari_ntsc_setup.gamma);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_res <n>          Set NTSC resolution (default %.2g)", atari_ntsc_setup.resolution);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_artif <n>        Set NTSC artifacts (default %.2g)", atari_ntsc_setup.artifacts);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_fring <n>        Set NTSC fringing (default %.2g)", atari_ntsc_setup.fringing);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_bleed <n>        Set NTSC bleed (default %.2g)", atari_ntsc_setup.bleed);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_burst <n>        Set NTSC burst phase -1..1 (artif colours)(def: %.2g)", atari_ntsc_setup.burst_phase);
				Log_print("\t                       (-ntsc_emu only)");
				Log_print("\t-ntsc_delay <n>        Set NTSC GTIA color delay (default %.2g)", atari_ntsc_setup.color_delay);
				Log_print("\t                       (-ntsc_emu only)");
			}

			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return 0;
		}
	}
	*argc = j;

	return 1;
}
