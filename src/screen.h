#ifndef _SCREEN_H_
#define _SCREEN_H_

#ifdef DIRTYRECT
#ifndef CLIENTUPDATE
extern UBYTE *screen_dirty;
#endif
#endif
void entire_screen_dirty();

extern ULONG *atari_screen;

#ifdef BITPL_SCR
extern ULONG *atari_screen_b;
extern ULONG *atari_screen1;
extern ULONG *atari_screen2;
#endif

extern int screen_visible_x1;
extern int screen_visible_y1;
extern int screen_visible_x2;
extern int screen_visible_y2;

void Screen_DrawAtariSpeed(void);
void Screen_DrawDiskLED(void);
void Screen_FindScreenshotFilename(char *buffer);
int Screen_SaveScreenshot(const char *filename, int interlaced);
void Screen_SaveNextScreenshot(int interlaced);

#endif /* _SCREEN_H_ */
