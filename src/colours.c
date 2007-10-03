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
#include "atari.h"
#include "colours.h"
#include "log.h"
#include "util.h"
#ifdef __PLUS
#include "display_win.h"
#include "misc_win.h"
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

int colortable[256] = {
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

void Palette_SetRGB(int i, int r, int g, int b)
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
	colortable[i] = (r << 16) + (g << 8) + b;
}

int Palette_Read(const char *filename)
{
	FILE *fp;
	int i;
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return FALSE;
	for (i = 0; i < 256; i++) {
		int j;
		colortable[i] = 0;
		for (j = 16; j >= 0; j -= 8) {
			int c = fgetc(fp);
			if (c == EOF) {
				fclose(fp);
				return FALSE;
			}
			colortable[i] |= c << j;
		}
	}
	fclose(fp);
	return TRUE;
}

void Palette_Generate(int black, int white, int colshift)
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
			Palette_SetRGB(i * 16 + j, y + r, y + g, y + b);
		}
	}
}

void Palette_Adjust(int black, int white, int colintens)
{
	double black_in;
	double white_in;
	double brightfix;
	double colorfix;
	int i;
	black_in = Palette_GetY(0);
	white_in = Palette_GetY(15);
	brightfix = (white - black) / (white_in - black_in);
	colorfix = brightfix * colintens / 100;
	for (i = 0; i < 256; i++) {
		double y;
		double r;
		double g;
		double b;
		y = Palette_GetY(i);
		r = Palette_GetR(i) - y;
		g = Palette_GetG(i) - y;
		b = Palette_GetB(i) - y;
		y = (y - black_in) * brightfix + black;
		r = y + r * colorfix;
		g = y + g * colorfix;
		b = y + b * colorfix;
		Palette_SetRGB(i, (int) r, (int) g, (int) b);
	}
}

void Palette_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
#ifndef __PLUS
	int black = 0;
	int white = 255;
	int colintens = 100;
	int colshift = 30;
#else /* __PLUS */
	int black = g_Screen.Pal.nBlackLevel;
	int white = g_Screen.Pal.nWhiteLevel;
	int colintens = g_Screen.Pal.nSaturation;
	int colshift = g_Screen.Pal.nColorShift;
#endif /* __PLUS */
	int generate = FALSE;
	int adjust = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-black") == 0) {
			black = Util_sscandec(argv[++i]);
			adjust = TRUE;
		}
		else if (strcmp(argv[i], "-white") == 0) {
			white = Util_sscandec(argv[++i]);
			adjust = TRUE;
		}
		else if (strcmp(argv[i], "-colors") == 0) {
			colintens = Util_sscandec(argv[++i]);
			adjust = TRUE;
		}
		else if (strcmp(argv[i], "-colshift") == 0)
			colshift = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-genpal") == 0)
			generate = TRUE;
		else if (strcmp(argv[i], "-palette") == 0)
			Palette_Read(argv[++i]);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-palette <file>  Use external palette");
				Aprint("\t-black <0-255>   Set black level");
				Aprint("\t-white <0-255>   Set white level");
				Aprint("\t-colors <num>    Set colors saturation");
				Aprint("\t-genpal          Generate artificial palette");
				Aprint("\t-colshift <num>  Set color shift (-genpal only)");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (generate)
		Palette_Generate(black, white, colshift);

	if (adjust)
		Palette_Adjust(black, white, colintens);
}
