/* (C) 2001  Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */
#ifndef A800_SCREEN_H
#define A800_SCREEN_H

#include "atari.h"

int gron(int *argc, char *argv[]);
void groff(void);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);
void refresh_kbd();

void gr_suspend();
void gr_resume();

/* meaning: 0 - portrain, 1 - left hand landscape, 2 - right hand landscape */
void set_screen_mode(int mode);
int get_screen_mode();

/* translate physical coordinates to logical keyboard coordinates */
void translate_kbd(short* px, short* py);

#endif
