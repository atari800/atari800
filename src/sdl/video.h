#ifndef SDL_VIDEO_H_
#define SDL_VIDEO_H_

#include <stdio.h>
#include <SDL.h>

#include "config.h"
#include "videomode.h"

/* Native BPP of the desktop. OpenGL modes can be opened only
   in the native BPP. */
extern int SDL_VIDEO_native_bpp;

/* Current width/height of the screen/window. */
extern int SDL_VIDEO_width;
extern int SDL_VIDEO_height;

/* Indicates current display mode */
extern VIDEOMODE_MODE_t SDL_VIDEO_current_display_mode;

#if HAVE_OPENGL
/* Indicates whenther OpenGL is available on the host machine. */
extern int SDL_VIDEO_opengl_available;
/* Get/set video hardware acceleration. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_SetOpengl() instead. */
extern int SDL_VIDEO_opengl;
int SDL_VIDEO_SetOpengl(int value);
int SDL_VIDEO_ToggleOpengl(void);
#endif /* HAVE_OPENGL */

/* Get/set the vertical synchronisation feature. */
/* Call VIDEOMODE_Update() after changing this variable, or use SDL_VIDEO_SetVsync() instead. */
extern int SDL_VIDEO_vsync;
/* If Vsync is requested but not available in the current video mode, these
   functions return FALSE and set SDL_VIDEO_vsync_available to FALSE; else the returned and
   set values are TRUE. */
int SDL_VIDEO_SetVsync(int value);
int SDL_VIDEO_ToggleVsync(void);
extern int SDL_VIDEO_vsync_available;

/* Get/set brightness of scanlines. (0=none, 100=completely black). */
/* Use SDL_VIDEO_SetScanlinesPercentage() to set this value. */
extern int SDL_VIDEO_scanlines_percentage;
void SDL_VIDEO_SetScanlinesPercentage(int value);

/* Get/set scanlines interplation, both in sowtfare and in OpenGL mode. */
/* Use SDL_VIDEO_SetInterpolateScanlines() to set this value. */
extern int SDL_VIDEO_interpolate_scanlines;
void SDL_VIDEO_SetInterpolateScanlines(int value);
void SDL_VIDEO_ToggleInterpolateScanlines(void);

int SDL_VIDEO_ReadConfig(char *option, char *parameters);
void SDL_VIDEO_WriteConfig(FILE *fp);
int SDL_VIDEO_Initialise(int *argc, char *argv[]);
void SDL_VIDEO_Exit(void);

/* Write the screen data into DEST. */
void SDL_VIDEO_BlitNormal8(Uint32 *dest, Uint8 *src, int pitch, int width, int height);
void SDL_VIDEO_BlitNormal16(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint16 *palette16);
void SDL_VIDEO_BlitNormal32(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint32 *palette32);
void SDL_VIDEO_BlitXEP80_8(Uint32 *dest, Uint8 *src, int pitch, int width, int height);
void SDL_VIDEO_BlitXEP80_16(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint16 *palette16);
void SDL_VIDEO_BlitXEP80_32(Uint32 *dest, Uint8 *src, int pitch, int width, int height, Uint32 *palette32);
void SDL_VIDEO_BlitProto80_8(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line);
void SDL_VIDEO_BlitProto80_16(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, Uint16 *palette16);
void SDL_VIDEO_BlitProto80_32(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, Uint32 *palette32);
void SDL_VIDEO_BlitAF80_8(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink);
void SDL_VIDEO_BlitAF80_16(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink, Uint16 *palette16);
void SDL_VIDEO_BlitAF80_32(Uint32 *dest, int first_column, int last_column, int pitch, int first_line, int last_line, int blink, Uint32 *palette32);

#endif /* SDL_VIDEO_H_ */