#ifndef SDL_PALETTE_H_
#define SDL_PALETTE_H_

#include <SDL.h>

#include "videomode.h"

typedef struct SDL_PALETTE_tab_t {
	int *palette;
	int size;
} SDL_PALETTE_tab_t;

/* Contains pointers to palettes used by various display modes, and their sizes.
   The table is indexed by a VIDEOMODE_MODE_t value. */
extern SDL_PALETTE_tab_t const SDL_PALETTE_tab[VIDEOMODE_MODE_SIZE];

typedef union SDL_PALETTE_buffer_t {
	Uint16 bpp16[256];	/* 16-bit palette */
	Uint32 bpp32[256];	/* 32-bit palette */
	
} SDL_PALETTE_buffer_t;

/* Holds all palette values for the currently-used pixel format (BGR, RGB,
   ARGB etc.) */
extern SDL_PALETTE_buffer_t SDL_PALETTE_buffer;

void SDL_PALETTE_Calculate32_A8R8G8B8(VIDEOMODE_MODE_t mode);
void SDL_PALETTE_Calculate32_B8G8R8A8(VIDEOMODE_MODE_t mode);
void SDL_PALETTE_Calculate16_R5G6B5(VIDEOMODE_MODE_t mode);
void SDL_PALETTE_Calculate16_B5G6R5(VIDEOMODE_MODE_t mode);

#endif /* SDL_PALETTE_H_ */
