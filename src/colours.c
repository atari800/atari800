/*
 * colours.c - colour palette emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <string.h>	/* for strcmp() */
#include <math.h>
#include "atari.h"
#include "colours.h"
#include "log.h"

#define COLINTENS	100

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

typedef struct {
  UBYTE r;
  UBYTE g;
  UBYTE b;
} pal_t[0x100];

static int read_palette(char *filename);

static int palette_loaded = FALSE;
static int min_y = 0, max_y = 0xf0;
static int colintens = COLINTENS;
static int colshift = 30;
static const double redf = 0.30;
static const double greenf = 0.59;
static const double bluef = 0.11;

static int basicpal[256];
int colortable[256];

#define CLIP_VAR(x) \
  if (x > 0xff) \
    x = 0xff; \
  if (x < 0) \
    x = 0


static int real_pal[256] =
{
  0x323132, 0x3f3e3f, 0x4d4c4d, 0x5b5b5b,
  0x6a696a, 0x797879, 0x888788, 0x979797,
  0xa1a0a1, 0xafafaf, 0xbebebe, 0xcecdce,
  0xdbdbdb, 0xebeaeb, 0xfafafa, 0xffffff,
  0x612e00, 0x6c3b00, 0x7a4a00, 0x885800,
  0x94670c, 0xa5761b, 0xb2842a, 0xc1943a,
  0xca9d43, 0xdaad53, 0xe8bb62, 0xf8cb72,
  0xffd87f, 0xffe88f, 0xfff79f, 0xffffae,
  0x6c2400, 0x773000, 0x844003, 0x924e11,
  0x9e5d22, 0xaf6c31, 0xbc7b41, 0xcc8a50,
  0xd5935b, 0xe4a369, 0xf2b179, 0xffc289,
  0xffcf97, 0xffdfa6, 0xffedb5, 0xfffdc4,
  0x751618, 0x812324, 0x8f3134, 0x9d4043,
  0xaa4e50, 0xb85e60, 0xc66d6f, 0xd57d7f,
  0xde8787, 0xed9596, 0xfca4a5, 0xffb4b5,
  0xffc2c4, 0xffd1d3, 0xffe0e1, 0xffeff0,
  0x620e71, 0x6e1b7c, 0x7b2a8a, 0x8a3998,
  0x9647a5, 0xa557b5, 0xb365c3, 0xc375d1,
  0xcd7eda, 0xdc8de9, 0xea97f7, 0xf9acff,
  0xffbaff, 0xffc9ff, 0xffd9ff, 0xffe8ff,
  0x560f87, 0x611d90, 0x712c9e, 0x7f3aac,
  0x8d48ba, 0x9b58c7, 0xa967d5, 0xb877e5,
  0xc280ed, 0xd090fc, 0xdf9fff, 0xeeafff,
  0xfcbdff, 0xffccff, 0xffdbff, 0xffeaff,
  0x461695, 0x5122a0, 0x6032ac, 0x6e41bb,
  0x7c4fc8, 0x8a5ed6, 0x996de3, 0xa87cf2,
  0xb185fb, 0xc095ff, 0xcfa3ff, 0xdfb3ff,
  0xeec1ff, 0xfcd0ff, 0xffdfff, 0xffefff,
  0x212994, 0x2d359f, 0x3d44ad, 0x4b53ba,
  0x5961c7, 0x686fd5, 0x777ee2, 0x878ef2,
  0x9097fa, 0x96a6ff, 0xaeb5ff, 0xbfc4ff,
  0xcdd2ff, 0xdae3ff, 0xeaf1ff, 0xfafeff,
  0x0f3584, 0x1c418d, 0x2c509b, 0x3a5eaa,
  0x486cb7, 0x587bc5, 0x678ad2, 0x7699e2,
  0x80a2eb, 0x8fb2f9, 0x9ec0ff, 0xadd0ff,
  0xbdddff, 0xcbecff, 0xdbfcff, 0xeaffff,
  0x043f70, 0x114b79, 0x215988, 0x2f6896,
  0x3e75a4, 0x4d83b2, 0x5c92c1, 0x6ca1d2,
  0x74abd9, 0x83bae7, 0x93c9f6, 0xa2d8ff,
  0xb1e6ff, 0xc0f5ff, 0xd0ffff, 0xdeffff,
  0x005918, 0x006526, 0x0f7235, 0x1d8144,
  0x2c8e50, 0x3b9d60, 0x4aac6f, 0x59bb7e,
  0x63c487, 0x72d396, 0x82e2a5, 0x92f1b5,
  0x9ffec3, 0xaeffd2, 0xbeffe2, 0xcefff1,
  0x075c00, 0x146800, 0x227500, 0x328300,
  0x3f910b, 0x4fa01b, 0x5eae2a, 0x6ebd3b,
  0x77c644, 0x87d553, 0x96e363, 0xa7f373,
  0xb3fe80, 0xc3ff8f, 0xd3ffa0, 0xe3ffb0,
  0x1a5600, 0x286200, 0x367000, 0x457e00,
  0x538c00, 0x629b07, 0x70a916, 0x80b926,
  0x89c22f, 0x99d13e, 0xa8df4d, 0xb7ef5c,
  0xc5fc6b, 0xd5ff7b, 0xe3ff8b, 0xf3ff99,
  0x334b00, 0x405700, 0x4d6500, 0x5d7300,
  0x6a8200, 0x7a9100, 0x889e0f, 0x98ae1f,
  0xa1b728, 0xbac638, 0xbfd548, 0xcee458,
  0xdcf266, 0xebff75, 0xfaff85, 0xffff95,
  0x4b3c00, 0x584900, 0x655700, 0x746500,
  0x817400, 0x908307, 0x9f9116, 0xaea126,
  0xb7aa2e, 0xc7ba3e, 0xd5c74d, 0xe5d75d,
  0xf2e56b, 0xfef47a, 0xffff8b, 0xffff9a,
  0x602e00, 0x6d3a00, 0x7a4900, 0x895800,
  0x95670a, 0xa4761b, 0xb2832a, 0xc2943a,
  0xcb9d44, 0xdaac53, 0xe8ba62, 0xf8cb73,
  0xffd77f, 0xffe791, 0xfff69f, 0xffffaf,
};


static void Palette_Format(int black, int white, int colors)
/* format loaded palette */
{
  double white_in, black_in, brightfix;
  pal_t rgb;
  int i, j;

  for (i = 0; i < 0x100; i++)
  {
    int c = basicpal[i];

    rgb[i].r = (c >> 16) & 0xff;
    rgb[i].g = (c >> 8) & 0xff;
    rgb[i].b = c & 0xff;
  }

  black_in = redf * rgb[0].r + greenf * rgb[0].g + bluef * rgb[0].b;
  white_in = redf * rgb[15].r + greenf * rgb[15].g + bluef * rgb[15].b;
  brightfix = (double)(white - black) / (white_in - black_in);

  for (i = 0; i < 0x10; i++)
  {
    for (j = 0; j < 0x10; j++)
    {
      double y, r, g, b;
      int r1, g1, b1;

      y = redf * rgb[i * 16 + j].r
	+ greenf * rgb[i * 16 + j].g
	+ bluef * rgb[i * 16 + j].b;
      r = (double)rgb[i * 16 + j].r - y;
      g = (double)rgb[i * 16 + j].g - y;
      b = (double)rgb[i * 16 + j].b - y;

      y = ((y - black_in) * brightfix) + black;
      r *= (double)colors * brightfix / (double)COLINTENS;
      g *= (double)colors * brightfix / (double)COLINTENS;
      b *= (double)colors * brightfix / (double)COLINTENS;

      r1 = y + r;
      g1 = y + g;
      b1 = y + b;

      CLIP_VAR(r1);
      CLIP_VAR(g1);
      CLIP_VAR(b1);

      rgb[i * 16 + j].r = r1;
      rgb[i * 16 + j].g = g1;
      rgb[i * 16 + j].b = b1;
    }
  }
  for (i = 0; i < 0x100; i++)
  {
    colortable[i] = (rgb[i].r << 16) + (rgb[i].g << 8) + (rgb[i].b << 0);
  }
}

static void makepal(int *pal)
{
  pal_t rgb;
  int i, j;
  const double colf = 0.08;

  for (i = 0; i < 0x10; i++)
  {
    int r, g, b;
    double angle;

    if (i == 0)
    {
      r = g = b = 0;
    }
    else
    {
      angle = M_PI * ((double) i * (1.0 / 7) - (double) colshift * 0.01);
      r = (colf / redf) * cos(angle) * (max_y - min_y);
      g = (colf / greenf) * cos(angle + M_PI * (2.0 / 3)) * (max_y - min_y);
      b = (colf / bluef) * cos(angle + M_PI * (4.0 / 3)) * (max_y - min_y);
    }
    for (j = 0; j < 0x10; j++)
    {
      int y, r1, g1, b1;

      y = (min_y * 0xf + max_y * j) / 0xf;
      r1 = y + r;
      g1 = y + g;
      b1 = y + b;
      CLIP_VAR(r1);
      CLIP_VAR(g1);
      CLIP_VAR(b1);
      rgb[i * 16 + j].r = r1;
      rgb[i * 16 + j].g = g1;
      rgb[i * 16 + j].b = b1;
    }
  }
  for (i = 0; i < 0x100; i++)
  {
    pal[i] = (rgb[i].r << 16) + (rgb[i].g << 8) + (rgb[i].b << 0);
  }
}

void Palette_Initialise(int *argc, char *argv[])
{
	int i, j;
	int generate_palette = FALSE;

	/* use real palette by default */
	memcpy(basicpal, real_pal, sizeof(basicpal));

	for (i = j = 1; i < *argc; i++)
	{
		if (strcmp(argv[i], "-black") == 0)
			sscanf(argv[++i], "%d", &min_y);
		else if (strcmp(argv[i], "-white") == 0)
			sscanf(argv[++i], "%d", &max_y);
		else if (strcmp(argv[i], "-colors") == 0)
			sscanf(argv[++i], "%d", &colintens);
		else if (strcmp(argv[i], "-colshift") == 0)
			sscanf(argv[++i], "%d", &colshift);
		else if (strcmp(argv[i], "-genpal") == 0)
			generate_palette = TRUE;
		else if (strcmp(argv[i], "-palette") == 0)
			read_palette(argv[++i]);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-palette <file>  Use external palette");
				Aprint("\t-black <0-255>   Set black level");
				Aprint("\t-white <0-255>   Set white level");
				Aprint("\t-colors <num>    Set color intensity");
				Aprint("\t-genpal <num>    Generate artificial palette");
				Aprint("\t-colshift <num>  Set color shift (-genpal only)");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (generate_palette)
		makepal(basicpal);

	if (! palette_loaded)
		Palette_Format(min_y, max_y, colintens);
}

/* returns TRUE if successful */
static int read_palette(char *filename) {
	FILE *fp;
	int i;
	if ((fp = fopen(filename,"rb")) == NULL)
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
	palette_loaded = TRUE;
	return TRUE;
}

/*
$Log$
Revision 1.10  2003/02/24 09:32:49  joy
header cleanup

Revision 1.9  2002/12/16 17:57:56  knik
corrected color/BW conversion (Vasyl was right)
palette generating code moved to separate function
removed unused tables

Revision 1.8  2002/06/23 21:50:51  joy
"-palette" moved from atari.c to colours.c
other changes to get "-palette" working correctly.

Revision 1.6  2001/11/27 19:11:21  knik
real palette used by default so COMPILED_PALETTE conditional not needed
palette adjusting code improved and moved to Palette_Format function

*/
