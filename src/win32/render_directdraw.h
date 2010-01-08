#ifndef _RENDER_DIRECTDRAW_H_
#define _RENDER_DIRECTDRAW_H_

#include "screen_win32.h" 

int startupdirectdraw(BOOL bltgfx, INT width);
void shutdowndirectdraw(void);
void refreshv_directdraw(UBYTE *scr_ptr, BOOL bltgfx);

#endif

