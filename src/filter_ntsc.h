#ifndef FILTER_NTSC_H_
#define FILTER_NTSC_H_

#include "atari_ntsc/atari_ntsc.h"

/* Contains controls used to adjust the palette in the NTSC filter. */
extern atari_ntsc_setup_t FILTER_NTSC_setup;

/* Allocates memory for a new NTSC filter. */
atari_ntsc_t *FILTER_NTSC_New(void);
/* Frees memory used by an NTSC filter, FILTER. */
void FILTER_NTSC_Delete(atari_ntsc_t *filter);
/* Reinitialises an NTSC filter, FILTER. Should be called after changing
   palette setup or loading/unloading an external palette. */
void FILTER_NTSC_Update(atari_ntsc_t *filter);
/* Restores default values for NTSC-filter-specific colour controls.
   FILTER_NTSC_Update should be called afterwards to apply changes. */
void FILTER_NTSC_RestoreDefaults(void);
/* Switches between available preset settings: Composite, S-Video, RGB,
   Monochrome. FILTER_NTSC_Update should be called afterwards to apply
   changes. */
void FILTER_NTSC_NextPreset(void);

/* NTSC filter initialisation and processing of command-line arguments. */
int FILTER_NTSC_Initialise(int *argc, char *argv[]);

#endif /* FILTER_NTSC_H_ */
