#ifndef _RENDER_GDIPLUS_H_
#define _RENDER_GDIPLUS_H_

#ifdef __cplusplus 
extern "C" { 
#endif 

#include "screen_win32.h" 

void startupgdiplus(void);
void shutdowngdiplus(void);
void refreshv_gdiplus(UBYTE *scr_ptr, FRAMEPARAMS *fp);

#ifdef __cplusplus 
} 
#endif 


#endif /* _RENDER_GDIPLUS_H_ */

