#ifndef SDL_VIDEO_SW_H_
#define SDL_VIDEO_SW_H_

#include "platform.h"
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

/* Returns parameters of the current display pixel format. Used when computing
   lookup tables used for blitting the Atari screen to display surface. */
void SDL_VIDEO_SW_GetPixelFormat(PLATFORM_pixel_format_t *format);
/* Convert a table of RGB values, PALETTE, of size SIZE, to a display's native
   format and store it in the lookup table DEST. */
void SDL_VIDEO_SW_MapRGB(void *dest, int const *palette, int size);

/* Read/write to configuration file. */
int SDL_VIDEO_SW_ReadConfig(char *option, char *parameters);
void SDL_VIDEO_SW_WriteConfig(FILE *fp);

/* Initialisation and processing of command-line arguments. */
int SDL_VIDEO_SW_Initialise(int *argc, char *argv[]);

#endif /* SDL_VIDEO_SW_H_ */
