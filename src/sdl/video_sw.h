#ifndef SDL_VIDEO_SW_H_
#define SDL_VIDEO_SW_H_

#include "videomode.h"

void SDL_VIDEO_SW_DisplayScreen(void);
void SDL_VIDEO_SW_PaletteUpdate(void);
void SDL_VIDEO_SW_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90);
int SDL_VIDEO_SW_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90);

/* Get/set videomode bits per pixel. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_SW_SetBpp() instead. */
extern int SDL_VIDEO_SW_bpp;
int SDL_VIDEO_SW_SetBpp(int value);
int SDL_VIDEO_SW_ToggleBpp(void);

/* Read/write to configuration file. */
int SDL_VIDEO_SW_ReadConfig(char *option, char *parameters);
void SDL_VIDEO_SW_WriteConfig(FILE *fp);

/* Initialisation and processing of command-line arguments. */
int SDL_VIDEO_SW_Initialise(int *argc, char *argv[]);

#endif /* SDL_VIDEO_SW_H_ */
