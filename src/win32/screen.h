/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#ifndef A800_SCREEN_H
#define A800_SCREEN_H

#include "atari.h"

#define CLR_BACK	0x44

int gron(int *argc, char *argv[]);
void groff(void);
void palupd(int beg, int cnt);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);

#endif
