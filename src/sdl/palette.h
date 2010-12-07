#ifndef SDL_PALETTE_H_
#define SDL_PALETTE_H_

#include "videomode.h"

struct SDL_PALETTE_tab_t {
	int *palette;
	int size;
};

/* Contains pointers to palettes used by various display modes, and their sizes.
   The table is indexed by a VIDEOMODE_MODE_t value. */
extern struct SDL_PALETTE_tab_t const SDL_PALETTE_tab[VIDEOMODE_MODE_SIZE];

#endif /* SDL_PALETTE_H_ */
