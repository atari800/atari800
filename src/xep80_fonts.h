
#ifndef _XEP80_FONTS_H_
#define _XEP80_FONTS_H_

#include "config.h"
#include "atari.h"
#include "xep80.h"

#define XEP_NUM_FONT_SETS   3
#define XEP_NUM_FONTS       8
#define XEP80_CHAR_COUNT    256

#define REV_FONT_BIT        0x1
#define UNDER_FONT_BIT      0x2
#define BLK_FONT_BIT        0x4

#define XEP80_UNDER_ROW     10

extern UBYTE atari_fonts[XEP_NUM_FONT_SETS][XEP_NUM_FONTS][XEP80_CHAR_COUNT][XEP80_CHAR_HEIGHT][XEP80_CHAR_WIDTH];
extern UBYTE xep80_oncolor;
extern UBYTE xep80_offcolor;

extern int xep80_fonts_inited;

void XEP80_InitFonts(void);

#endif /* _XEP80_FONTS_H_ */
