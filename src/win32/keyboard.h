#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#define SHOWKBCODES	0
#define KBCODES 	0x100

#include <dinput.h>

extern int pause_hit;
extern int kbcode;
extern UBYTE kbhits[KBCODES];
int prockb(void);
int kbreacquire(void);
int initinput(void);
void uninitinput(void);
void clearkb(void);
HRESULT
SetDIDwordProperty(LPDIRECTINPUTDEVICE pdev, REFGUID guidProperty,
		   DWORD dwObject, DWORD dwHow, DWORD dwValue);

#endif /* KEYBOARD_H_ */
