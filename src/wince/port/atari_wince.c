/* (C) 2001 Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "platform.h"
#include "atari.h"
#include "screen.h"
#include "keyboard.h"
#include "main.h"
#include "sound.h"
#include "monitor.h"
#include "diskled.h"
#include "rt-config.h"

static int usesnd = 1;

extern int refresh_rate;

extern int SHIFT_KEY, KEYPRESSED;
static int kbjoy = 1;

static UBYTE joymask[] =
{
	0,				/* not used */
	~1 & ~4,			/* up/left */
	~1,				/* up */
	~1 & ~8,			/* up/right */
	~4,				/* left */
	~8,				/* right */
	~2 & ~4,			/* down/left */
	~2,				/* down */
	~2 & ~8,			/* down/right */
};

static int trig0;
static int stick0;
static int console;

int Atari_Keyboard(void)
{
	int keycode;
	int i;
	
	prockb();
	
	if (kbjoy)
	{
		trig0 = joyhits[0] ? 0 : 1;
		
		stick0 |= 0xf;
		for(i = 1; i < JOYCODES; i++)
			if(joyhits[i])
				stick0 &= joymask[i];
	}
	
	keycode = get_last_key();
	
	SHIFT_KEY = kbhits[AKEY_SHFT];
	
	if(keycode != AKEY_NONE && keycode != AKEY_UI)
		keycode |= (SHIFT_KEY ? 0x40 : 0) | (kbhits[AKEY_CTRL] ? 0x80 : 0);
	
	console = (console & ~7) | (kbhits[AKEY_F2] ? 0 : 4) | (kbhits[AKEY_F3] ? 0 : 2) | (kbhits[AKEY_F4] ? 0 : 1);
	
	KEYPRESSED = (keycode != AKEY_NONE);
	return keycode;
}

void Atari_Initialise(int *argc, char *argv[])
{
#ifdef SOUND
	if (usesnd)
		Sound_Initialise(argc, argv);
#endif
	if(initinput())
		exit(1);
	if (gron(argc, argv))
		exit(1);
	
#if defined(SET_LED) && !defined(NO_LED_ON_SCREEN)
	LED_lastline = 239;
#endif
	clearkb();
	
	trig0 = 1;
	stick0 = 15;
	console = 7;
	
	/* Port tweaks */
	if(refresh_rate < 2)
		refresh_rate = 2;
}

int Atari_Exit(int run_monitor)
{
	/* monitor is not avaliable in this port */
	if(run_monitor)
		return 1;
	
#ifdef BUFFERED_LOG
	Aflushlog();
#endif
	
	uninitinput();
	groff();
	
#ifdef SOUND
	Sound_Exit();
#endif
	
	return 0;
}

void Atari_DisplayScreen(UBYTE * ascreen)
{
	refreshv(ascreen + 24);
}

int Atari_PORT(int num)
{
	if (num == 0)
		return 0xf0 | stick0;
	else
		return 0xff;
}


int Atari_TRIG(int num)
{
	if (num == 0)
		return trig0;
	else
		return 1;
}

int Atari_POT(int num)
{
	/* The most primitive implementation for the first version */
	if(num == 0) /* first stick hor */
	{
		if(joyhits[4])
			return 1;
		else if(joyhits[5])
			return 228;
		else
			return 113;
	}
	else if(num == 1) /* first stick vert */
	{
		if(joyhits[2])
			return 1;
		else if(joyhits[7])
			return 228;
		else
			return 113;
	}
	else
		return 113;
}

int Atari_CONSOL(void)
{
	return console;
}

int Atari_PEN(int vertical)
{
	return vertical ? 0xff : 0;
}
