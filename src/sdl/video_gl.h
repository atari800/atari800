#ifndef SDL_VIDEO_GL_H_
#define SDL_VIDEO_GL_H_

#include <stdio.h>

#include "videomode.h"

/* Get/set bilinear filtering. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_GL_SetFiltering() instead. */
extern int SDL_VIDEO_GL_filtering;
void SDL_VIDEO_GL_SetFiltering(int value);
void SDL_VIDEO_GL_ToggleFiltering(void);

void SDL_VIDEO_GL_ScanlinesPercentageChanged(void);
void SDL_VIDEO_GL_InterpolateScanlinesChanged(void);

/* Get/set synchronization of blitting with screen vertical retrace. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_GL_SetSync() instead. */
/* FIXME: Disabled, currently doesn't work.
extern int SDL_VIDEO_GL_sync;
int SDL_VIDEO_GL_SetSync(int value);
int SDL_VIDEO_GL_ToggleSync(void);*/

/* Called when switching back to software mode. Cleans the OpenGL context and frees structures. */
void SDL_VIDEO_GL_Cleanup(void);

void SDL_VIDEO_GL_DisplayScreen(void);
void SDL_VIDEO_GL_PaletteUpdate(void);
int SDL_VIDEO_GL_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90, int window_resized);
int SDL_VIDEO_GL_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90);

int SDL_VIDEO_GL_ReadConfig(char *option, char *parameters);
void SDL_VIDEO_GL_WriteConfig(FILE *fp);
int SDL_VIDEO_GL_Initialise(int *argc, char *argv[]);

#endif /* SDL_VIDEO_GL_H_ */
