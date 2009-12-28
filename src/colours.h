#ifndef COLOURS_H_
#define COLOURS_H_

#include "colours_external.h"

extern int Colours_table[256];

/* Contains controls for palette adjustment. These controls are available for
   NTSC and PAL palettes. */
typedef struct Colours_setup_t {
	double saturation;
	double contrast;
	double brightness;
	double gamma;
	int black_level; /* 0..255. ITU-R Recommendation BT.601 advises it to be 16. */
	int white_level; /* 0..255. ITU-R Recommendation BT.601 advises it to be 235. */
} Colours_setup_t;

/* Pointer to the current palette setup. Depending on the current TV system,
   it points to the NTSC setup, or the PAL setup. (See COLOURS_NTSC_setup and
   COLOURS_PAL_setup.) */
extern Colours_setup_t *Colours_setup;

#define Colours_GetR(x) ((UBYTE) (Colours_table[x] >> 16))
#define Colours_GetG(x) ((UBYTE) (Colours_table[x] >> 8))
#define Colours_GetB(x) ((UBYTE) Colours_table[x])
/* Packs R, G, B into palette COLORTABLE_PTR for colour number I. */
void Colours_SetRGB(int i, int r, int g, int b, int *colortable_ptr);

/* Called when the TV system changes, it updates the current palette
   accordingly. */
void Colours_SetVideoSystem(int mode);
/* Called during initialisation - updates palette if TV system has changed. */
void Colours_InitialiseMachine(void);

/* Updates the current palette - should be called after changing palette setup
   or loading/unloading an external palette. */
void Colours_Update(void);
/* Restores default setup for the current palette (NTSC or PAL one).
   Colours_Update should be called afterwards to apply changes. */
void Colours_RestoreDefaults(void);
/* Save the current colours, including adjustments, to a palette file.
   Returns TRUE on success or FALSE on error. */
int Colours_Save(const char *filename);

/* Pointer to an externally-loaded palette. Depending on the current TV
   system, it points to the external NTSC or PAL palette - they can be loaded
   independently. (See COLOURS_NTSC_external and COLOURS_PAL_external.) */
extern COLOURS_EXTERNAL_t *Colours_external;

/* Colours initialisation and processing of command-line arguments. */
int Colours_Initialise(int *argc, char *argv[]);

#endif /* COLOURS_H_ */
