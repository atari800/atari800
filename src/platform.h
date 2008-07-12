#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "config.h"
#include <stdio.h>

/* This include file defines prototypes for platform-specific functions. */

void PLATFORM_Initialise(int *argc, char *argv[]);
int PLATFORM_Exit(int run_monitor);
int PLATFORM_Keyboard(void);
void PLATFORM_DisplayScreen(void);

int PLATFORM_PORT(int num);
int PLATFORM_TRIG(int num);

#ifdef SUPPORTS_PLATFORM_CONFIGINIT
/* This function sets the configuration parameters to default values */
void PLATFORM_ConfigInit(void);
#endif

#ifdef SUPPORTS_PLATFORM_CONFIGURE
/* This procedure processes lines not recognized by RtConfigLoad. */
int PLATFORM_Configure(char *option, char *parameters);
#endif

#ifdef SUPPORTS_PLATFORM_CONFIGSAVE
/* This function saves additional config lines */
void PLATFORM_ConfigSave(FILE *fp);
#endif

#ifdef SUPPORTS_PLATFORM_PALETTEUPDATE
/* This function updates the palette */
/* If the platform does a conversion of colortable when it initialises
 * and the user changes colortable (such as changing from PAL to NTSC)
 * then this function should update the platform palette */
void PLATFORM_PaletteUpdate(void);
#endif

#ifdef SUPPORTS_PLATFORM_SLEEP
/* This function is for those ports that need their own version of sleep */
void PLATFORM_Sleep(double s);
#endif

#ifdef SDL
/* used in UI to show how the keyboard joystick is mapped */
char *PLATFORM_Joy0Description(char *buffer, int maxsize);
char *PLATFORM_Joy1Description(char *buffer, int maxsize);
extern int PLATFORM_kbd_joy_0_enabled;
extern int PLATFORM_kbd_joy_1_enabled;
#endif

#ifdef XEP80_EMULATION
/* Switch between the Atari and XEP80 screen */
void PLATFORM_SwitchXep80(void);
/* TRUE if the XEP80 screen is visible */
extern int PLATFORM_xep80;
#endif

#endif /* PLATFORM_H_ */
