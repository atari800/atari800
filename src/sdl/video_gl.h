#ifndef SDL_VIDEO_GL_H_
#define SDL_VIDEO_GL_H_

#include <stdio.h>

#include "platform.h"

#include "videomode.h"

/* Get/set texture pixel format. On some video cards, either of these is faster. */
enum {
	SDL_VIDEO_GL_PIXEL_FORMAT_BGR16,
	SDL_VIDEO_GL_PIXEL_FORMAT_RGB16,
	SDL_VIDEO_GL_PIXEL_FORMAT_BGRA32,
	SDL_VIDEO_GL_PIXEL_FORMAT_ARGB32,
	/* Number of values in enumerator */
	SDL_VIDEO_GL_PIXEL_FORMAT_SIZE
};
/* Call SDL_VIDEO_GL_SetPixelFormat() after changing this variable. */
extern int SDL_VIDEO_GL_pixel_format;
void SDL_VIDEO_GL_SetPixelFormat(int value);
void SDL_VIDEO_GL_TogglePixelFormat(void);

/* Returns parameters of the current display pixel format. Used when computing
   lookup tables used for blitting the Atari screen to display surface. */
void SDL_VIDEO_GL_GetPixelFormat(PLATFORM_pixel_format_t *format);
/* Convert a table of RGB values, PALETTE, of size SIZE, to a display's native
   format and store it in the lookup table DEST. */
void SDL_VIDEO_GL_MapRGB(void *dest, int const *palette, int size);

/* Get/set bilinear filtering. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_GL_SetFiltering() instead. */
extern int SDL_VIDEO_GL_filtering;
void SDL_VIDEO_GL_SetFiltering(int value);
void SDL_VIDEO_GL_ToggleFiltering(void);

/* Get/set usage of Pixel Buffer Objects if available. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_GL_SetPbo() instead. */
extern int SDL_VIDEO_GL_pbo;
/* If PBOs are requested but not available, these functions return FALSE. Note: Testing for
   availibility of PBOs is only possible when OpenGL is active. If the host doesn't support PBOs
   but OpenGL mode is not active, the functions will return TRUE. */
int SDL_VIDEO_GL_SetPbo(int value);
int SDL_VIDEO_GL_TogglePbo(void);

void SDL_VIDEO_GL_ScanlinesPercentageChanged(void);
void SDL_VIDEO_GL_InterpolateScanlinesChanged(void);

/* Called when switching back to software mode. Cleans the OpenGL context and frees structures. */
void SDL_VIDEO_GL_Cleanup(void);

void SDL_VIDEO_GL_DisplayScreen(void);
void SDL_VIDEO_GL_PaletteUpdate(void);
int SDL_VIDEO_GL_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90);
int SDL_VIDEO_GL_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90);

int SDL_VIDEO_GL_ReadConfig(char *option, char *parameters);
void SDL_VIDEO_GL_WriteConfig(FILE *fp);
int SDL_VIDEO_GL_Initialise(int *argc, char *argv[]);

/* Initialise SDL GL subsystem. Must be called after SDL video initialisation
   (SDL_VIDEO_InitSDL()). */
void SDL_VIDEO_GL_InitSDL(void);

#endif /* SDL_VIDEO_GL_H_ */
