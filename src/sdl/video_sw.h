#ifndef SDL_VIDEO_SW_H_
#define SDL_VIDEO_SW_H_

#include "videomode.h"

void SDL_VIDEO_SW_DisplayScreen(void);
void SDL_VIDEO_SW_PaletteUpdate(void);
void SDL_VIDEO_SW_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90);
int SDL_VIDEO_SW_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90);

#endif /* SDL_VIDEO_SW_H_ */
