#ifndef _RENDER_DIRECT3D_H_
#define _RENDER_DIRECT3D_H_

#ifdef __cplusplus 
extern "C" { 
#endif 

#include "atari.h"
#include "screen_win32.h" 

void startupdirect3d(int screenwidth, int screenheight, BOOL windowed, FRAMEPARAMS *fp); 
void shutdowndirect3d(void);
void refreshv_direct3d(UBYTE *scr_ptr, FRAMEPARAMS *fp);
void init_vertices(float x, float y, float z);
void refresh_frame(UBYTE *scr_ptr, FRAMEPARAMS *fp);
void initpresentparams(int screenwidth, int screenheight, BOOL windowed);
void resetdevice(int screenwidth, int screenheight, BOOL windowed, FRAMEPARAMS *fp);
void initdevice(FRAMEPARAMS *fp);

#ifdef __cplusplus 
} 

#endif
#endif

