#ifndef _SCREEN_H_
#define _SCREEN_H_

#include "atari.h"  /* UBYTE */

#ifdef DIRTYRECT
#ifndef CLIENTUPDATE
extern UBYTE *Screen_dirty;
#endif /* CLIENTUPDATE */
#endif /* DIRTYRECT */

extern ULONG *Screen_atari;

#ifdef BITPL_SCR
extern ULONG *Screen_atari_b;
extern ULONG *Screen_atari1;
extern ULONG *Screen_atari2;
#endif

extern int Screen_visible_x1;
extern int Screen_visible_y1;
extern int Screen_visible_x2;
extern int Screen_visible_y2;

extern int Screen_show_atari_speed;
extern int Screen_show_disk_led;
extern int Screen_show_sector_counter;

void Screen_Initialise(int *argc, char *argv[]);
void Screen_DrawAtariSpeed(double);
void Screen_DrawDiskLED(void);
void Screen_FindScreenshotFilename(char *buffer);
int Screen_SaveScreenshot(const char *filename, int interlaced);
void Screen_SaveNextScreenshot(int interlaced);
void Screen_EntireDirty(void);

#endif /* _SCREEN_H_ */
