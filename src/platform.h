#ifndef __PLATFORM__
#define __PLATFORM__

#include "atari.h"

/*
 * This include file defines prototypes for platforms specific functions.
 */

void Atari_Initialise(int *argc, char *argv[]);
int Atari_Exit(int run_monitor);
int Atari_Keyboard(void);
#ifdef WIN32
void (*Atari_DisplayScreen)(UBYTE *screen);
#else
void Atari_DisplayScreen (UBYTE *screen);
#endif

int Atari_PORT(int num);
int Atari_TRIG(int num);
int Atari_POT(int num);
int Atari_CONSOL(void);
int Atari_PEN(int vertical);
#if defined(SET_LED) && defined(NO_LED_ON_SCREEN)
void Atari_Set_LED(int how);
#endif

#endif
