#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "config.h"
#include <stdio.h>

/* This include file defines prototypes for platform-specific functions. */

void Atari_Initialise(int *argc, char *argv[]);
int Atari_Exit(int run_monitor);
int Atari_Keyboard(void);
void Atari_DisplayScreen(void);

int Atari_PORT(int num);
int Atari_TRIG(int num);

#ifdef SUPPORTS_ATARI_CONFIGINIT
/* This function sets the configuration parameters to default values */
void Atari_ConfigInit(void);
#endif

#ifdef SUPPORTS_ATARI_CONFIGURE
/* This procedure processes lines not recognized by RtConfigLoad. */
int Atari_Configure(char *option, char *parameters);
#endif

#ifdef SUPPORTS_ATARI_CONFIGSAVE
/* This function saves additional config lines */
void Atari_ConfigSave(FILE *fp);
#endif

#endif /* _PLATFORM_H_ */

/*
$Log$
Revision 1.7  2005/10/19 21:34:34  pfusik
moved things from rt-config; removed Atari_DisplayScreen's argument

Revision 1.6  2003/02/24 09:33:07  joy
header cleanup

Revision 1.5  2001/10/03 16:52:38  fox
removed Atari_POT and Atari_Set_LED

Revision 1.4  2001/09/26 10:48:13  fox
removed Atari_PEN

Revision 1.3  2001/09/22 09:22:37  fox
Atari_CONSOL -> key_consol

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
