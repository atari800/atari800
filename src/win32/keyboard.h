/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#ifndef A800_KEYBOARD_H
#define A800_KEYBOARD_H

#define SHOWKBCODES	0
#define KBCODES 	0x100

extern int pause_hit;
extern int kbcode;
extern UBYTE kbhits[KBCODES];
int prockb(void);
int kbreacquire(void);
int initinput(void);
void uninitinput(void);
void clearkb(void);

#endif
