#ifndef _SCREEN_WIN32_H_
#define _SCREEN_WIN32_H_

#include "atari.h"

#define CLR_BACK	0x44

int gron(int *argc, char *argv[]);
void groff(void);
void palupd(int beg, int cnt);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);
extern int windowed;

#endif /* _SCREEN_H_ */
