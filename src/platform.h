/* $Id$ */
#ifndef __PLATFORM__
#define __PLATFORM__

#include "atari.h"

/*
 * This include file defines prototypes for platforms specific functions.
 */

void Atari_Initialise(int *argc, char *argv[]);
int Atari_Exit(int run_monitor);
int Atari_Keyboard(void);
void Atari_DisplayScreen (UBYTE *screen);

int Atari_PORT(int num);
int Atari_TRIG(int num);

#endif

/*
$Log$
Revision 1.5  2001/10/03 16:52:38  fox
removed Atari_POT and Atari_Set_LED

Revision 1.4  2001/09/26 10:48:13  fox
removed Atari_PEN

Revision 1.3  2001/09/22 09:22:37  fox
Atari_CONSOL -> key_consol

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
