#ifndef _UI_H
#define _UI_H
#include "atari.h"	/* for UBYTE */

int SelectCartType(UBYTE *screen, int k);
void ui(UBYTE *screen);

extern int ui_is_active;

extern unsigned char ascii_to_screen[128];

#endif
