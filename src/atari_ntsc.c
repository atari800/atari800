
/* http://www.slack.net/~ant/ */

/* compilable in C or C++; just change the file extension */

#include "atari_ntsc.h"
#include "log.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
/* Based on algorithm by NewRisingSun */
/* License note by Perry: Expat License. 
 * http://www.gnu.org/licenses/license-list.html#GPLCompatibleLicenses
 * "This is a simple, permissive non-copyleft free software license, compatible with the GNU GPL."*/
/* Copyright (C) 2006 Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

enum { composite_border = 6 };
enum { center_offset = 1 };
enum { alignment_count = 4 }; /* different pixel alignments with respect to yiq quads */
enum { rgb_kernel_size = atari_ntsc_entry_size / alignment_count };

/* important to use + and not | since values are signed */
#define MAKE_KRGB( r, g, b ) \
	( ((r + 16) >> 5 << 20) + ((g + 16) >> 5 << 10) + ((b + 16) >> 5) )
static float const rgb_unit = 0x1000;

#define MAKE_KMASK( x ) (((x) << 20) | ((x) << 10) | (x))

/* clamp each RGB component to 0 to 0x7F range (low two bits are trashed) */
#define CLAMP_RGB( io, adj ) {\
	ntsc_rgb_t sub = (io) >> (7 + adj) & MAKE_KMASK( 3 );\
	ntsc_rgb_t clamp = MAKE_KMASK( 0x202 ) - sub;\
	io = ((io) | clamp) & (clamp - sub);\
}

enum { composite_size = composite_border + 8 + composite_border };
enum { rgb_pad = (center_offset + composite_border + 7) / 8 * 8 - center_offset - composite_border };
enum { rgb_size = (rgb_pad + composite_size + 7) / 8 * 8 };
enum { rescaled_size = rgb_size / 8 * 7 };
enum { ntsc_kernel_size = composite_size * 2 };

typedef struct ntsc_to_rgb_t
{
	float composite [composite_size];
	float to_rgb [6];
	float brightness;
	float contrast;
	float sharpness;
	short rgb [rgb_size] [3];
	short rescaled [rescaled_size + 1] [3]; /* extra space for sharpen */
	float kernel [ntsc_kernel_size];
} ntsc_to_rgb_t;

static float const pi = 3.14159265358979323846f;

static void rotate_matrix( float const* in, float s, float c, float* out )
{
	int n = 3;
	while ( n-- )
	{
		float i = *in++;
		float q = *in++;
		*out++ = i * c - q * s;
		*out++ = i * s + q * c;
	}
}

static void ntsc_to_rgb_init( ntsc_to_rgb_t* ntsc, atari_ntsc_setup_t const* setup, float hue )
{
	static float const to_rgb [6] = { 0.956, 0.621, -0.272, -0.647, -1.105, 1.702 };
	float gaussian_factor = 1+setup->gaussian_factor; /* 1 = normal, > 1 reduces echoes of bright objects (was 2.1)*/
	float const brightness_bias = rgb_unit / 128; /* reduces vert bands on artifact colored areas */
	int i;
	/* ranges need to be scaled a bit to avoid pixels overflowing at extremes */
	ntsc->brightness = (setup->brightness-0.0f) * (0.4f * rgb_unit) + brightness_bias;
	ntsc->contrast = (setup->contrast-0.0f) * 0.4f + 1;
	ntsc->sharpness = 1 + (setup->sharpness < 0 ? setup->sharpness * 0.5f : setup->sharpness);
	
	for ( i = 0; i < composite_size; i++ )
		ntsc->composite [i] = 0;
	
	/* Generate gaussian kernel, padded with zero */
	for ( i = 0; i < ntsc_kernel_size; i++ )
		ntsc->kernel [i] = 0;
	for ( i = -composite_border; i <= composite_border; i++ )
		ntsc->kernel [ntsc_kernel_size / 2 + i] = exp( i * i * (-0.03125f * gaussian_factor) );
	
	/* normalize kernel totals of every fourth sample (at all four phases) to 0.5, otherwise */
	/* i/q low-pass will favor one of the four alignments and cause repeating spots */
	for ( i = 0; i < 4; i++ )
	{
		double sum = 0;
		float scale;
		int x;
		for ( x = i; x < ntsc_kernel_size; x += 4 )
			sum += ntsc->kernel [x];
		scale = 0.5 / sum;
		for ( x = i; x < ntsc_kernel_size; x += 4 )
			ntsc->kernel [x] *= scale;
	}
	
	/* adjust decoder matrix */
	{
		float sat = setup->saturation + 1;
		rotate_matrix( to_rgb, sin( hue ) * sat, cos( hue ) * sat, ntsc->to_rgb );
	}
	
	memset( ntsc->rgb, 0, sizeof ntsc->rgb );
}

/* Convert NTSC composite signal to RGB, where composite signal contains only four
   non-zero samples beginning at offset */
static void ntsc_to_rgb( ntsc_to_rgb_t const* ntsc, int offset, short* out )
{
	float const* kernel = &ntsc->kernel [ntsc_kernel_size / 2 - offset];
	float f0 = ntsc->composite [offset];
	float f1 = ntsc->composite [offset + 1];
	float f2 = ntsc->composite [offset + 2];
	float f3 = ntsc->composite [offset + 3];
	int x = 0;
	while ( x < composite_size )
	{
		#define PIXEL( get_y ) \
		{\
			float i = kernel [ 0] * f0 + kernel [-2] * f2;\
			float q = kernel [-1] * f1 + kernel [-3] * f3;\
			float y = get_y;\
			float r = y + i * ntsc->to_rgb [0] + q * ntsc->to_rgb [1];\
			float g = y + i * ntsc->to_rgb [2] + q * ntsc->to_rgb [3];\
			float b = y + i * ntsc->to_rgb [4] + q * ntsc->to_rgb [5];\
			kernel++;\
			out [0] = (int) r;\
			out [1] = (int) g;\
			out [2] = (int) b;\
			out += 3;\
		}
		
		PIXEL( i - ntsc->composite [x + 0] )
		PIXEL( q - ntsc->composite [x + 1] )
		PIXEL( ntsc->composite [x + 2] - i )
		PIXEL( ntsc->composite [x + 3] - q )
		x += 4;
		
		#undef PIXEL
	}
}

/* Rescale pixels to NTSC aspect ratio using linear interpolation,
   with 7 output pixels for every 8 input pixels, linear interpolation */
static void rescale( short const* in, int count, short* out )
{
	do
	{
		int const accuracy = 16;
		int const unit = 1 << accuracy;
		int const step = unit / 8;
		int left = unit - step;
		int right = step;
		int n = 7;
		while ( n-- )
		{
			int r = (in [0] * left + in [3] * right) >> accuracy;
			int g = (in [1] * left + in [4] * right) >> accuracy;
			int b = (in [2] * left + in [5] * right) >> accuracy;
			*out++ = r;
			*out++ = g;
			*out++ = b;
			left  -= step;
			right += step;
			in += 3;
		}
		in += 3;
	}
	while ( (count -= 7) > 0 );
}

/* sharpen image using (level-1)/2, level, (level-1)/2 convolution kernel */
static void sharpen( short const* in, float level, int count, short* out )
{
	/* to do: sharpen luma only? */
	int const accuracy = 16;
	int const middle = (int) (level * (1 << accuracy));
	int const side   = (middle - (1 << accuracy)) >> 1;
	
	*out++ = *in++;
	*out++ = *in++;
	*out++ = *in++;
	
	for ( count = (count - 2) * 3; count--; in++ )
		*out++ = (in [0] * middle - in [-3] * side - in [3] * side) >> accuracy;
	*out++ = *in++;
	*out++ = *in++;
	*out++ = *in++;
}

/* Generate pixel and capture into table */
static ntsc_rgb_t* gen_pixel( ntsc_to_rgb_t* ntsc, int ntsc_pos, int rescaled_pos, ntsc_rgb_t* out )
{
	ntsc_to_rgb( ntsc, composite_border + ntsc_pos, ntsc->rgb [rgb_pad] );
	rescale( ntsc->rgb [0], rescaled_size, ntsc->rescaled [1] );
	sharpen( ntsc->rescaled [1], ntsc->sharpness, rescaled_size, ntsc->rescaled [0] );
	
	{
		short const* in = ntsc->rescaled [rescaled_pos];
		int n = rgb_kernel_size;
		while ( n-- )
		{
			*out++ = MAKE_KRGB( in [0], in [1], in [2] );
			in += 3;
		}
	}
	return out;
}
void atari_ntsc_init( atari_ntsc_t* emu, atari_ntsc_setup_t const* setup )
{

	/* init pixel renderer */
	int entry;
	float burst_phase = (setup->burst_phase) * pi;
	ntsc_to_rgb_t ntsc;
	ntsc_to_rgb_init( &ntsc, setup, setup->hue * pi - burst_phase );
	
	for ( entry = 0; entry < atari_ntsc_color_count; entry++ )
	{
		/*NTSC PHASE CONSTANTS.*/
		float ntsc_phase_constant[16]={0,2.27,1.87,1.62,
				1.22,0.62,-0.31,-0.855,
				-1.18,-1.43,-1.63,-1.93,
       				-2.38,-3.43,2.52,2.07};
		/*NTSC luma multipliers from gtia.pdf*/
				float luma_mult[16]={
				0.6941, 0.7091, 0.7241, 0.7401, 
				0.7560, 0.7741, 0.7931, 0.8121,
			       	0.8260, 0.8470, 0.8700, 0.8930,
			       	0.9160, 0.9420, 0.9690, 1.0000};
		/* calculate yiq for color entry */
		int color = entry >> 4;
		float angle = burst_phase + ntsc_phase_constant[color];
		float lumafactor = ( luma_mult[ entry & 0x0f] - luma_mult[0] )/( luma_mult[15] - luma_mult[0] );
		float adj_lumafactor = pow(lumafactor, 1 +setup->gamma_adj);
		float y = adj_lumafactor  * rgb_unit;
		float base_saturation = (color ? rgb_unit * 0.35f: 0.0f);
		float saturation_ramp = setup->saturation_ramp;
		float saturation = base_saturation * ( 1 - saturation_ramp*lumafactor );
		float i = sin( angle ) * saturation;
		float q = cos( angle ) * saturation;
		ntsc_rgb_t* out = emu->table [entry];
		y = y * ntsc.contrast + ntsc.brightness;
		
		/* generate at four alignments with respect to output */
		ntsc.composite [composite_border + 0] = i + y;
		ntsc.composite [composite_border + 1] = q + y;
		out = gen_pixel( &ntsc, 0, 0, out );
		
		ntsc.composite [composite_border + 0] = 0;
		ntsc.composite [composite_border + 1] = 0;
		ntsc.composite [composite_border + 2] = i - y;
		ntsc.composite [composite_border + 3] = q - y;
		out = gen_pixel( &ntsc, 2, 2, out );
		
		ntsc.composite [composite_border + 2] = 0;
		ntsc.composite [composite_border + 3] = 0;
		ntsc.composite [composite_border + 4] = i + y;
		ntsc.composite [composite_border + 5] = q + y;
		out = gen_pixel( &ntsc, 4, 4, out );
		
		ntsc.composite [composite_border + 4] = 0;
		ntsc.composite [composite_border + 5] = 0;
		ntsc.composite [composite_border + 6] = i - y;
		ntsc.composite [composite_border + 7] = q - y;
		out = gen_pixel( &ntsc, 6, 6, out );
		
		ntsc.composite [composite_border + 6] = 0;
		ntsc.composite [composite_border + 7] = 0;
		
		/* correct roundoff errors that would cause vertical bands in solid areas */
		{
			float r = y + i * ntsc.to_rgb [0] + q * ntsc.to_rgb [1];
			float g = y + i * ntsc.to_rgb [2] + q * ntsc.to_rgb [3];
			float b = y + i * ntsc.to_rgb [4] + q * ntsc.to_rgb [5];
			ntsc_rgb_t correct = MAKE_KRGB( (int) r, (int) g, (int) b ) + MAKE_KMASK( 0x100 );
			int i;
			out = emu->table [entry];
			for ( i = 0; i < rgb_kernel_size / 2; i++ )
			{
				/* sum as would occur when outputting run of pixels using same color,
				but don't sum first kernel; the difference between this and the correct
				color is what the first kernel's entry should be */
				ntsc_rgb_t sum = out [(i+12)%14+14]+out[(i+10)%14+28]+out[(i+8)%14+42]+
						out[i+7]+out [ i+ 5    +14]+out[ i+ 3    +28]+out[ i+1    +42];
				out [i] = correct - sum;
			}
		}
	}
	Log_print("atari_ntsc_init(): sharpness:%f saturation:%f brightness:%f contrast:%f gaussian_factor:%f burst_phase:%f, hue:%f gamma_adj:%f saturation_ramp:%f\n",setup->sharpness,setup->saturation,setup->brightness,setup->contrast,setup->gaussian_factor,setup->burst_phase,setup->hue,setup->gamma_adj,setup->saturation_ramp);
}

void atari_ntsc_blit( atari_ntsc_t const* emu, unsigned char const* in, long in_pitch,
		int width, int height, unsigned short* out, long out_pitch )
{
	int const chunk_count = (width - 10) / 7;
	long next_in_line = in_pitch - chunk_count * 4;
	long next_out_line = out_pitch - (chunk_count + 1) * (7 * sizeof *out);
	while ( height-- )
	{
		#define ENTRY( n ) emu->table [n]
		ntsc_rgb_t const* k1 = ENTRY( 0 );
		ntsc_rgb_t const* k2 = k1;
		ntsc_rgb_t const* k3 = k1;
		ntsc_rgb_t const* k4 = k1;
		ntsc_rgb_t const* k5 = k4;
		ntsc_rgb_t const* k6 = k4;
		ntsc_rgb_t const* k7 = k4;
		int n;
		
		#if ATARI_NTSC_RGB_BITS == 16
			#define TO_RGB( in ) ((in >> 11 & 0xF800) | (in >> 6 & 0x07C0) | (in >> 2 & 0x001F))
		#elif ATARI_NTSC_RGB_BITS == 15
			#define TO_RGB( in ) ((in >> 12 & 0x7C00) | (in >> 7 & 0x03E0) | (in >> 2 & 0x001F))
		#endif
		
		#define PIXEL( a ) {\
			ntsc_rgb_t temp =\
					k0 [a  ] + k1 [(a+5)%7+14] + k2 [(a+3)%7+28] + k3 [(a+1)%7+42] +\
					k4 [a+7] + k5 [(a+5)%7+21] + k6 [(a+3)%7+35] + k7 [(a+1)%7+49];\
			if ( a ) out [a-1] = rgb;\
			CLAMP_RGB( temp, 0 );\
			rgb = TO_RGB( temp );\
		}
		
		for ( n = chunk_count; n; --n )
		{
			ntsc_rgb_t const* k0 = ENTRY( in [0] );
			int rgb;
			PIXEL( 0 );
			PIXEL( 1 );
			k5 = k1;
			k1 = ENTRY( in [1] );
			PIXEL( 2 );
			PIXEL( 3 );
			k6 = k2;
			k2 = ENTRY( in [2] );
			PIXEL( 4 );
			PIXEL( 5 );
			k7 = k3;
			k3 = ENTRY( in [3] );
			PIXEL( 6 );
			out [6] = rgb;
			k4 = k0;
			in += 4;
			out += 7;
		}
		{
			ntsc_rgb_t const* k0 = ENTRY( 0 );
			int rgb;
			PIXEL( 0 );
			PIXEL( 1 );
			k5 = k1;
			k1 = k0;
			PIXEL( 2 );
			PIXEL( 3 );
			k6 = k2;
			k2 = k0;
			PIXEL( 4 );
			PIXEL( 5 );
			k7 = k3;
			k3 = k0;
			PIXEL( 6 );
			k4 = k0;
			out [6] = rgb;
			out += 7;
			PIXEL( 0 );
			PIXEL( 1 );
			k5 = k0;
			PIXEL( 2 );
			out [2] = rgb;
		}
		#undef PIXEL
		
		in += next_in_line;
		out = (unsigned short*) ((char*) out + next_out_line);
	}
}

/* Atari800-specific: */

/* Adjust default values here */
atari_ntsc_setup_t atari_ntsc_setup = {
	0.f, /* hue */
	-0.1f, /* saturation */
	0.f, /* contrast */
	-0.1f, /* brightness */
	-0.5f, /* sharpness */
	-0.6f, /* burst phase */
	0.f, /* gaussian factor */
	-0.15, /* gamma adjustment */
	0.25 /* satutration ramp */
};

void atari_ntsc_Initialise(int *argc, char *argv[])
{
	int i, j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-ntsc_hue") == 0) {
			atari_ntsc_setup.hue = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_sat") == 0){
			atari_ntsc_setup.saturation = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_cont") == 0){
			atari_ntsc_setup.contrast = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_bright") == 0){
			atari_ntsc_setup.brightness = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_sharp") == 0){
			atari_ntsc_setup.sharpness = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_burst") == 0){
			atari_ntsc_setup.burst_phase = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_gauss") == 0){
			atari_ntsc_setup.gaussian_factor = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_gamma") == 0){
			atari_ntsc_setup.gamma_adj = atof(argv[++i]);
		}else if (strcmp(argv[i], "-ntsc_ramp") == 0){
			atari_ntsc_setup.saturation_ramp = atof(argv[++i]);
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-ntsc_hue <n>    Set NTSC hue -1..1 (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.hue);
				Log_print("\t-ntsc_sat <n>    Set NTSC saturation (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.saturation);
				Log_print("\t-ntsc_cont <n>   Set NTSC contrast (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.contrast);
				Log_print("\t-ntsc_bright <n> Set NTSC brightness (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.brightness);
				Log_print("\t-ntsc_sharp <n>  Set NTSC sharpness (default %.2g) (-ntsc_emu only)", atari_ntsc_setup.sharpness);
				Log_print("\t-ntsc_burst <n>  Set NTSC burst phase -1..1 (artif colours)(def: %.2g)", atari_ntsc_setup.burst_phase);
				Log_print("\t                 (-ntsc_emu only)");
				Log_print("\t-ntsc_gauss <n>  Set NTSC Gaussian factor (default %.2g)", atari_ntsc_setup.gaussian_factor);
				Log_print("\t                 (-ntsc_emu only)");
				Log_print("\t-ntsc_gamma <n>  Set NTSC gamma adjustment (default %.2g)", atari_ntsc_setup.gamma_adj);
				Log_print("\t                 (-ntsc_emu only)");
				Log_print("\t-ntsc_ramp <n>   Set NTSC saturation ramp factor (default %.2g)", atari_ntsc_setup.saturation_ramp);
				Log_print("\t                 (-ntsc_emu only)");
			}

			argv[j++] = argv[i];
		}

	}
	*argc = j;
}

