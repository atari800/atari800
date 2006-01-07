#ifndef _SCREEN_WINCE_H_
#define _SCREEN_WINCE_H_

#include "atari.h"

int gron(int *argc, char *argv[]);
void groff(void);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);
void refresh_kbd();

void palette_update();

void gr_suspend();
void gr_resume();

/* meaning: 0 - portrait, 1 - left hand landscape, 2 - right hand landscape */
void set_screen_mode(int mode);
int get_screen_mode();

/* translate physical coordinates to logical keyboard coordinates */
void translate_kbd(short* px, short* py);

extern int smooth_filter;
extern int filter_available;
extern int emulator_active;

#endif /* _SCREEN_WINCE_H_ */
