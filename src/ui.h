#ifndef _UI_H
#define _UI_H
#include	"atari.h"	/* for UBYTE */
void ui(UBYTE * screen);

extern int ui_is_active;

extern unsigned char ascii_to_screen[128];

extern int mach_xlxe;
extern int Ram256;
#endif
