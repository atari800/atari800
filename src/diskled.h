#ifndef _DISKLED_H_
#define _DISKLED_H_

/* 0 = off, 1..9 = read leds, 10..18 = write leds */
extern int led_status;

/* 1 = turn led off next frame
   we do not turn led off immediately, so we can see it lightning
   if there was any transmission last frame */
extern int led_off_delay;

#define LED_SetRead(unit,delay) (led_status = 1 + (unit), led_off_delay = delay)
#define LED_SetWrite(unit, delay) (led_status = 10 + (unit), led_off_delay = delay)

void LED_Frame(void);

#endif /* _DISKLED_H_ */
