#include "config.h"

#ifdef SET_LED

/* 0 = off, 1..9 = read leds, 10..18 = write leds */
extern int LED_status;

/* 1 = turn led off next frame
   we do not turn led off immediately, so we can see it lightning
   if there was any transmission last frame */
extern int LED_off_delay;

#define Set_LED_Read(unit) (LED_status = 1 + (unit))
#define Set_LED_Write(unit) (LED_status = 10 + (unit))
#define Set_LED_Off() (LED_off_delay = 1)

/* this should be called between Antic_RunDisplayList()
   and Atari_DisplayScreen, so it can draw LED on screen */
void Update_LED(void);

#ifndef NO_LED_ON_SCREEN

/* this should contain number of last visible line of Atari screen
   it defaults to 239 - in 320x200 with middle lines displayed
   this should be 219
 */
extern int LED_lastline;

#endif /* NO_LED_ON_SCREEN */

#else /* SET_LED */

/* no leds at all - ignore setting and update */
#define Set_LED_Read(unit)
#define Set_LED_Write(unit)
#define Set_LED_Off()
#define Update_LED()

#endif /* SET_LED */
