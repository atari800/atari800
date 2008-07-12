/*
 * colours.c - Atari colour palette
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>	/* for strcmp() */
#include <math.h>
#include <stdlib.h>
#include "atari.h"
#include "colours.h"
#include "log.h"
#include "util.h"
#include "platform.h"
#ifdef __PLUS
#include "display_win.h"
#include "misc_win.h"
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#define PALETTE_R(x,c) ((UBYTE) (c[x] >> 16))
#define PALETTE_G(x,c) ((UBYTE) (c[x] >> 8))
#define PALETTE_B(x,c) ((UBYTE) c[x])
#define PALETTE_Y(x,c) (0.30 * PALETTE_R(x,c) + 0.59 * PALETTE_G(x,c) + 0.11 * PALETTE_B(x,c))

int *Colours_table;

static int colortable_ntsc[256] = {
 0x000000, 0x101010, 0x1f1f1f, 0x2f2f2f,
 0x3e3e3e, 0x4e4e4e, 0x5e5e5e, 0x6e6e6e,
 0x797979, 0x8a8a8a, 0x9c9c9c, 0xadadad,
 0xbfbfbf, 0xd2d2d2, 0xe6e6e6, 0xfcfcfc,
 0x110900, 0x241d00, 0x342c00, 0x433c00,
 0x514a00, 0x615a00, 0x716a00, 0x817a00,
 0x8c850c, 0x9c951f, 0xaea734, 0xbfb847,
 0xd0c95b, 0xe3dc71, 0xf6f087, 0xffffa1,
 0x2a0000, 0x3d0b00, 0x4c1b00, 0x5b2b00,
 0x693a00, 0x794a00, 0x885a04, 0x976a16,
 0xa27522, 0xb28634, 0xc39848, 0xd4aa5c,
 0xe4bb6f, 0xf7cf84, 0xffe29a, 0xfff9b3,
 0x360000, 0x490100, 0x581100, 0x672100,
 0x752f00, 0x844008, 0x93501a, 0xa2602b,
 0xad6c37, 0xbd7d49, 0xcd8f5c, 0xdea16f,
 0xeeb282, 0xffc697, 0xffdaac, 0xfff1c4,
 0x420000, 0x540000, 0x630204, 0x721214,
 0x802123, 0x8f3233, 0x9e4244, 0xad5354,
 0xb75e60, 0xc77071, 0xd88284, 0xe89496,
 0xf8a6a7, 0xffbabb, 0xffced0, 0xffe5e7,
 0x3e002a, 0x51003e, 0x60004d, 0x6f065c,
 0x7d156a, 0x8c2679, 0x9b3789, 0xaa4798,
 0xb453a3, 0xc464b3, 0xd577c4, 0xe589d4,
 0xf59ce5, 0xffb0f7, 0xffc4ff, 0xffdcff,
 0x0f0077, 0x220089, 0x320097, 0x410fa5,
 0x501eb2, 0x5f2ec1, 0x6f3fcf, 0x7f50dd,
 0x8a5be7, 0x9a6df5, 0xac7fff, 0xbd91ff,
 0xcea3ff, 0xe1b7ff, 0xf4ccff, 0xffe3ff,
 0x000076, 0x000288, 0x0c1297, 0x1c22a5,
 0x2b31b2, 0x3b41c0, 0x4c51ce, 0x5c61dc,
 0x686de6, 0x797ef5, 0x8b90ff, 0x9da2ff,
 0xafb3ff, 0xc2c7ff, 0xd6dbff, 0xedf2ff,
 0x000065, 0x001077, 0x002086, 0x082f94,
 0x173ea1, 0x284eb0, 0x395ebe, 0x496ecc,
 0x5579d7, 0x668ae6, 0x799cf6, 0x8baeff,
 0x9ebfff, 0xb2d2ff, 0xc6e6ff, 0xddfcff,
 0x00074f, 0x001b62, 0x002a71, 0x003a7f,
 0x0b488d, 0x1b589c, 0x2c68ab, 0x3d78b9,
 0x4983c4, 0x5b94d3, 0x6ea5e3, 0x80b7f3,
 0x93c8ff, 0xa7dbff, 0xbceeff, 0xd3ffff,
 0x000f3b, 0x00234e, 0x00325d, 0x00426c,
 0x035079, 0x146089, 0x257098, 0x367fa7,
 0x428ab1, 0x539bc1, 0x67acd2, 0x79bee2,
 0x8ccff2, 0xa0e1ff, 0xb5f5ff, 0xcdffff,
 0x001a17, 0x002d2b, 0x003d3a, 0x004c49,
 0x005a58, 0x0c6a67, 0x1e7a77, 0x2f8987,
 0x3b9492, 0x4da4a2, 0x60b6b3, 0x73c7c4,
 0x86d7d5, 0x9aeae8, 0xb0fdfb, 0xc8ffff,
 0x002500, 0x003800, 0x004803, 0x005713,
 0x006522, 0x0d7432, 0x1e8443, 0x2f9353,
 0x3b9e5f, 0x4dae70, 0x60bf83, 0x73d095,
 0x86e0a7, 0x9bf3bb, 0xb0ffcf, 0xc8ffe6,
 0x001e00, 0x003100, 0x0c4100, 0x1c5000,
 0x2a5e00, 0x3b6e00, 0x4b7d00, 0x5b8d00,
 0x67970c, 0x78a81f, 0x8ab933, 0x9cca47,
 0xaedb5b, 0xc2ed71, 0xd6ff87, 0xedffa0,
 0x001300, 0x132600, 0x233600, 0x324500,
 0x415400, 0x516300, 0x617300, 0x718300,
 0x7c8e08, 0x8d9e1b, 0x9fb02f, 0xb0c143,
 0xc1d257, 0xd5e46d, 0xe8f883, 0xfeff9d,
 0x1e0000, 0x311400, 0x412400, 0x503300,
 0x5e4200, 0x6e5200, 0x7d6200, 0x8d7209,
 0x977d15, 0xa88e28, 0xb9a03c, 0xcab150,
 0xdbc263, 0xedd679, 0xffe98f, 0xffffa8
};

static int colortable_pal[256] = {
 0x2d2d2d, 0x3b3b3b, 0x494949, 0x575757,
 0x656565, 0x737373, 0x818181, 0x8f8f8f,
 0x9d9d9d, 0xababab, 0xb9b9b9, 0xc7c7c7,
 0xd5d5d5, 0xe3e3e3, 0xf1f1f1, 0xffffff,
 0x5c2300, 0x6a3100, 0x783f00, 0x864d0a,
 0x945b18, 0xa26926, 0xb07734, 0xbe8542,
 0xcc9350, 0xdaa15e, 0xe8af6c, 0xf6bd7a,
 0xffcb88, 0xffd996, 0xffe7a4, 0xfff5b2,
 0x691409, 0x772217, 0x853025, 0x933e33,
 0xa14c41, 0xaf5a4f, 0xbd685d, 0xcb766b,
 0xd98479, 0xe79287, 0xf5a095, 0xffaea3,
 0xffbcb1, 0xffcabf, 0xffd8cd, 0xffe6db,
 0x6c0a38, 0x7a1846, 0x882654, 0x963462,
 0xa44270, 0xb2507e, 0xc05e8c, 0xce6c9a,
 0xdc7aa8, 0xea88b6, 0xf896c4, 0xffa4d2,
 0xffb2e0, 0xffc0ee, 0xffcefc, 0xffdcff,
 0x640565, 0x721373, 0x802181, 0x8e2f8f,
 0x9c3d9d, 0xaa4bab, 0xb859b9, 0xc667c7,
 0xd475d5, 0xe283e3, 0xf091f1, 0xfe9fff,
 0xffadff, 0xffbbff, 0xffc9ff, 0xffd7ff,
 0x520789, 0x601597, 0x6e23a5, 0x7c31b3,
 0x8a3fc1, 0x984dcf, 0xa65bdd, 0xb469eb,
 0xc277f9, 0xd085ff, 0xde93ff, 0xeca1ff,
 0xfaafff, 0xffbdff, 0xffcbff, 0xffd9ff,
 0x3a109c, 0x481eaa, 0x562cb8, 0x643ac6,
 0x7248d4, 0x8056e2, 0x8e64f0, 0x9c72fe,
 0xaa80ff, 0xb88eff, 0xc69cff, 0xd4aaff,
 0xe2b8ff, 0xf0c6ff, 0xfed4ff, 0xffe2ff,
 0x1f1e9c, 0x2d2caa, 0x3b3ab8, 0x4948c6,
 0x5756d4, 0x6564e2, 0x7372f0, 0x8180fe,
 0x8f8eff, 0x9d9cff, 0xabaaff, 0xb9b8ff,
 0xc7c6ff, 0xd5d4ff, 0xe3e2ff, 0xf1f0ff,
 0x072e89, 0x153c97, 0x234aa5, 0x3158b3,
 0x3f66c1, 0x4d74cf, 0x5b82dd, 0x6990eb,
 0x779ef9, 0x85acff, 0x93baff, 0xa1c8ff,
 0xafd6ff, 0xbde4ff, 0xcbf2ff, 0xd9ffff,
 0x003e65, 0x034c73, 0x115a81, 0x1f688f,
 0x2d769d, 0x3b84ab, 0x4992b9, 0x57a0c7,
 0x65aed5, 0x73bce3, 0x81caf1, 0x8fd8ff,
 0x9de6ff, 0xabf4ff, 0xb9ffff, 0xc7ffff,
 0x004b38, 0x005946, 0x096754, 0x177562,
 0x258370, 0x33917e, 0x419f8c, 0x4fad9a,
 0x5dbba8, 0x6bc9b6, 0x79d7c4, 0x87e5d2,
 0x95f3e0, 0xa3ffee, 0xb1fffc, 0xbfffff,
 0x005209, 0x006017, 0x0c6e25, 0x1a7c33,
 0x288a41, 0x36984f, 0x44a65d, 0x52b46b,
 0x60c279, 0x6ed087, 0x7cde95, 0x8aeca3,
 0x98fab1, 0xa6ffbf, 0xb4ffcd, 0xc2ffdb,
 0x005300, 0x0b6100, 0x196f00, 0x277d0a,
 0x358b18, 0x439926, 0x51a734, 0x5fb542,
 0x6dc350, 0x7bd15e, 0x89df6c, 0x97ed7a,
 0xa5fb88, 0xb3ff96, 0xc1ffa4, 0xcfffb2,
 0x134e00, 0x215c00, 0x2f6a00, 0x3d7800,
 0x4b8600, 0x59940b, 0x67a219, 0x75b027,
 0x83be35, 0x91cc43, 0x9fda51, 0xade85f,
 0xbbf66d, 0xc9ff7b, 0xd7ff89, 0xe5ff97,
 0x2d4300, 0x3b5100, 0x495f00, 0x576d00,
 0x657b00, 0x738901, 0x81970f, 0x8fa51d,
 0x9db32b, 0xabc139, 0xb9cf47, 0xc7dd55,
 0xd5eb63, 0xe3f971, 0xf1ff7f, 0xffff8d,
 0x463300, 0x544100, 0x624f00, 0x705d00,
 0x7e6b00, 0x8c790b, 0x9a8719, 0xa89527,
 0xb6a335, 0xc4b143, 0xd2bf51, 0xe0cd5f,
 0xeedb6d, 0xfce97b, 0xfff789, 0xffff97
};

void Colours_SetRGB(int i, int r, int g, int b, int *colortable_ptr)
{
	if (r < 0)
		r = 0;
	else if (r > 255)
		r = 255;
	if (g < 0)
		g = 0;
	else if (g > 255)
		g = 255;
	if (b < 0)
		b = 0;
	else if (b > 255)
		b = 255;
	colortable_ptr[i] = (r << 16) + (g << 8) + b;
}

int Colours_Read(const char *filename, int *colortable_ptr)
{
	FILE *fp;
	int i;
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return FALSE;
	for (i = 0; i < 256; i++) {
		int j;
		colortable_ptr[i] = 0;
		for (j = 16; j >= 0; j -= 8) {
			int c = fgetc(fp);
			if (c == EOF) {
				fclose(fp);
				return FALSE;
			}
			colortable_ptr[i] |= c << j;
		}
	}
	fclose(fp);
	return TRUE;
}

void Colours_Generate(int black, int white, int colshift, int *colortable_ptr)
{
	int i;
	int j;
	for (i = 0; i < 16; i++) {
		double angle;
		int r;
		int g;
		int b;
		if (i == 0) {
			r = g = b = 0;
		}
		else {
			angle = M_PI * (i / 7.0 - colshift * 0.01);
			r = (int) (0.08 / 0.30 * cos(angle) * (white - black));
			g = (int) (0.08 / 0.59 * cos(angle + M_PI * 2 / 3) * (white - black));
			b = (int) (0.08 / 0.11 * cos(angle + M_PI * 4 / 3) * (white - black));
		}
		for (j = 0; j < 16; j++) {
			int y;
			y = black + white * j / 15;
			Colours_SetRGB(i * 16 + j, y + r, y + g, y + b, colortable_ptr);
		}
	}
}

void Colours_Adjust(int black, int white, int colintens, int *colortable_ptr)
{
	double black_in;
	double white_in;
	double brightfix;
	double colorfix;
	int i;
	black_in = PALETTE_Y(0, colortable_ptr);
	white_in = PALETTE_Y(15, colortable_ptr);
	brightfix = (white - black) / (white_in - black_in);
	colorfix = brightfix * colintens / 100;
	for (i = 0; i < 256; i++) {
		double y;
		double r;
		double g;
		double b;
		y = PALETTE_Y(i, colortable_ptr);
		r = PALETTE_R(i, colortable_ptr) - y;
		g = PALETTE_G(i, colortable_ptr) - y;
		b = PALETTE_B(i, colortable_ptr) - y;
		y = (y - black_in) * brightfix + black;
		r = y + r * colorfix;
		g = y + g * colorfix;
		b = y + b * colorfix;
		Colours_SetRGB(i, (int) r, (int) g, (int) b, colortable_ptr);
	}
}

void Colours_SetVideoSystem(int mode) {
	if (mode == Atari800_TV_NTSC) {
		Colours_table = colortable_ntsc;
	}
       	else if (mode == Atari800_TV_PAL) {
		Colours_table = colortable_pal;
	}
	else {
		Atari800_Exit(FALSE);
		Log_print("Interal error: Invalid Atari800_tv_mode\n");
		exit(1);
	}
}

void Colours_InitialiseMachine(void)
{
	static int saved_tv_mode = Atari800_TV_UNSET;
	if (saved_tv_mode == Atari800_TV_UNSET) {
		/* Colours_SetVideoSystem was already called in initialisation*/
		saved_tv_mode = Atari800_tv_mode;
	}
	else if (saved_tv_mode != Atari800_tv_mode) {
		/* Atari800_tv_mode has changed */
		saved_tv_mode = Atari800_tv_mode;
		Colours_SetVideoSystem(Atari800_tv_mode);
#if SUPPORTS_PLATFORM_PALETTEUPDATE
		PLATFORM_PaletteUpdate();
#endif
	}
}

void Colours_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
#ifndef __PLUS
	/* NTSC */
	int black_ntsc = 0;
	int white_ntsc = 255;
	int colintens_ntsc = 100;
	int colshift_ntsc = 30;
	/* PAL */
	int black_pal = 0;
	int white_pal = 255;
	int colintens_pal = 100;
	int colshift_pal = 30;
#else /* __PLUS */
	/*XXX FIXME: add pal/ntsc*/
	int black = g_Screen.Pal.nBlackLevel;
	int white = g_Screen.Pal.nWhiteLevel;
	int colintens = g_Screen.Pal.nSaturation;
	int colshift = g_Screen.Pal.nColorShift;
#endif /* __PLUS */
	int generate_ntsc = FALSE;
	int generate_pal = FALSE;
	int adjust_ntsc = FALSE;
	int adjust_pal = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-blackn") == 0) {
			black_ntsc = Util_sscandec(argv[++i]);
			adjust_ntsc = TRUE;
		}
		else if (strcmp(argv[i], "-blackp") == 0) {
			black_pal = Util_sscandec(argv[++i]);
			adjust_pal = TRUE;
		}
		else if (strcmp(argv[i], "-whiten") == 0) {
			white_ntsc = Util_sscandec(argv[++i]);
			adjust_ntsc = TRUE;
		}
		else if (strcmp(argv[i], "-whitep") == 0) {
			white_pal = Util_sscandec(argv[++i]);
			adjust_pal = TRUE;
		}
		else if (strcmp(argv[i], "-colorsn") == 0) {
			colintens_ntsc = Util_sscandec(argv[++i]);
			adjust_ntsc = TRUE;
		}
		else if (strcmp(argv[i], "-colorsp") == 0) {
			colintens_pal = Util_sscandec(argv[++i]);
			adjust_pal = TRUE;
		}
		else if (strcmp(argv[i], "-colshiftn") == 0)
			colshift_ntsc = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-colshiftp") == 0)
			colshift_pal = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-genpaln") == 0)
			generate_ntsc = TRUE;
		else if (strcmp(argv[i], "-genpalp") == 0)
			generate_pal = TRUE;
		else if (strcmp(argv[i], "-paletten") == 0)
			Colours_Read(argv[++i], colortable_ntsc);
		else if (strcmp(argv[i], "-palettep") == 0)
			Colours_Read(argv[++i], colortable_pal);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-paletten <file> Use external NTSC palette");
				Log_print("\t-palettep <file> Use external PAL palette");
				Log_print("\t-blackn <0-255>  Set NTSC palette black level");
				Log_print("\t-blackp <0-255>  Set PAL palette black level");
				Log_print("\t-whiten <0-255>  Set NTSC palette white level");
				Log_print("\t-whitep <0-255>  Set PAL palette white level");
				Log_print("\t-colorsn <num>   Set NTSC palette colors saturation");
				Log_print("\t-colorsp <num>   Set PAL palette colors saturation");
				Log_print("\t-genpaln         Generate artificial NTSC palette");
				Log_print("\t-genpalp         Generate artificial PAL palette");
				Log_print("\t-colshiftn <num> Set NTSC palette color shift (-genpaln only)");
				Log_print("\t-colshiftp <num> Set PAL palette color shift (-genpalp only)");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (generate_ntsc) {
		Colours_Generate(black_ntsc, white_ntsc, colshift_ntsc, colortable_ntsc);
	}
	if (generate_pal) {
		Colours_Generate(black_pal, white_pal, colshift_pal, colortable_pal);
	}
	if (adjust_ntsc) {
		Colours_Adjust(black_ntsc, white_ntsc, colintens_ntsc, colortable_ntsc);
	}
	if (adjust_pal) {
		Colours_Adjust(black_pal, white_pal, colintens_pal, colortable_pal);
	}
	Colours_SetVideoSystem(Atari800_tv_mode); /* Atari800_tv_mode is set before calling Colours_Initialise */
}
