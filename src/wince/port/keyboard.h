/* (C) 2001  Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */
#ifndef A800_KEYBOARD_H
#define A800_KEYBOARD_H

#define KBCODES 	0x100
#define JOYCODES  9

extern int pause_hit;
extern int kbcode;

extern UBYTE kbhits[KBCODES];
/* joycodes are: trigger, up/left, up, up/right, left, right, down/left, down, down/right */
extern UBYTE joyhits[JOYCODES];

int prockb(void);
int initinput(void);
void uninitinput(void);
void clearkb(void);

void hitbutton(short);
void releasebutton(short);

void tapscreen(short x, short y);
void untapscreen(short x, short y);

void push_key(short akey);
void release_key(short akey);
short get_last_key();

/* 0 to 3 - overlay in landscape, 4 - not visible or portrait mode*/
extern int currentKeyboardMode;
/* 1 for negative image */
extern int currentKeyboardColor;

/* Packed bitmap 240x80 - five rows of buttons */
extern unsigned long* kbd_image;

#endif
