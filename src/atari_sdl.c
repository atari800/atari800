/* 
 * atari_sdl.c - SDL library specific port code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
   Changelog:

   09-02-2002 - "-mzpokey", "-oldpokey" and "-stereo" removed

   14-12-2002 - added support for new audio stuff:
   		"-mzpokey" - enable new (better) pokey code
		"-oldpokey" - disable new pokey code (default)
		"-stereo" - enable stereo mode
		"-audio16" - enable 16bit audio mode (recommended)
   23-04-2002 - added command line options:
		"--nojoystick", "--width x", "--height y", "--bpp b",
		"--fullscreen", "--windowed"
   06-12-2001 - replaced SHORTER_MODE with WIDTH_MODE, now there are 3 states:
		DEFAULT_WIDTH_MODE (ATARI_WIDTH-2*24), SHORTER_WIDTH_MODE
		(ATARI_WIDTH-2*24-2*8), and FULL_WIDTH_MODE (ATARI_WIDTH)
	      - checking if bpp is supported (8,16,32 only), if not - switching
		to 8bit	
	      - moved fps stuff to CountFPS		
	      - moved "bug reports" message up, so user could see that when
		init fails
   04-12-2001 - split Atari_DisplayScreen (DisplayWithoutScaling and
                DisplayWithScaling)
	      - put Aflushlog everywhere	
   03-12-2001 - 32 bit support
   	      - rewrited Atari_DisplayScreen function a little
	      - LALT+j swaps only keyboard emulated joysticks now
   29-11-2001 - real joystick(s) support
   	      - fixed bug with triggers, when joysticks was swapped
   23-11-2001 - 16bpp support - it's faster, becouse SDL doesn't need to
		convert surface, of course if your video mode is 24 or 32bpp -
		conversion still will be done, 8bpp should be faster in that
		case
	      - optional fsp counter
	      - changing bpp in realtime with ALT+E
	      - I am not unrolling loop in Atari_DisplayScreen anymore, fps
		counter show that it doesn't help	
	      - "shorter mode" (changed with ALT+G) - without 8 columns on left
		and right side, so you can set 320x240 or 640x480 mode now	
	      - mode info	
   20-11-2001 - ui shortcuts 
   	      - fix small problem with LALT+b
	      - removed global variable "done" 
   19-11-2001 - I still don't have any testers, so I started to experiment a
		little. Changed Atari_PORT/TRIG to SDL_Atari_PORT/TRIG, don't
		use Atari800_Frame, don't use INPUT_Frame, copied some code
		from there. Result? Fixed kikstart and starquake behaviour!
		Warning! Only 2 joysticks support and much reduced input.
	      - swapping joysticks with LALT+j
   18-11-2001 - couldn't work with code last few days, but I am back and alive!
 	      - I found memory related bug in ui.c, searching for more	
	      - fixed problem with switching fullscreen when in ui
	      - fully configurable two joysticks emulation (I checked it in
		basic and zybex, second trig work bad in kikstart, don't know
		why, yet)
	      - now using len in SDL_Sound_Update	
   15-11-2001 - fullscreen switching
              - scaling only with ratio >= 1.0
	      - more keyboard updates 
	      - scaling should be LIGHTSPEED now, but please compile emulator
		with optimization CFLAGS like "-O2 -march=your_cpu_type" and
		"-fomit-frame-pointer" - so compiler can use pentium
		instructions and one additional register 
	      - fantastic new feature "Black-and-White video", you can switch
		BW mode with ALT+b	
   14-11-2001 - scaling with any ratio supported
   	      - keyboard updates
	      - aspect ratio
   13-11-2001 - audio
              - better keyboard support
   12-11-2001 - video
              - joystick emulation
   
   TODO:
   - implement all Atari800 parameters
   - use mouse and real joystick
   - turn off fullscreen when error happen
   - Atari_Exit - monitor stuff (debugger?)
   
   USAGE:
   - you can turn off sound by changing sound_enabled to 0  
   - you can switch between fullscreen/window mode with LALT+f
   - you can switch between color/bw mode with LALT+b - FEEL THE POWER OF BW
     MONITOR!
   - you can swap joysticks with LALT+j  
   - you can switch "width modes" with LALT+g - so you can set 320x240 or
     640x480
   - you can switch color depth (for now - 8/16) with LALT+e   
   - fullscreen switching probably doesn't work in Windows, you need to set
     variable in source code
   - if you are using XFree86 - try to set low videomode, like 400x300 - so you
     will play without scaling (speed!)  

   Thanks to David Olofson for scaling tips!  
*/

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef linux
#define LPTJOY	1
#endif

#ifdef LPTJOY
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/lp.h>
#endif /* LPTJOY */

// Atari800 includes
#include "config.h"
#include "input.h"
#include "colours.h"
#include "platform.h"
#include "ui.h"
#include "ataripcx.h"
#include "pokeysnd.h"
#include "gtia.h"
#include "antic.h"
#include "diskled.h"
#include "devices.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "log.h"

// you can set that variables in code, or change it when emulator is running
// I am not sure what to do with sound_enabled (can't turn it on inside
// emulator, probably we need two variables or command line argument)
#ifdef SOUND
static int sound_enabled = TRUE;
static int sound_flags = 0;
static int sound_bits = 8;
#endif
static int SDL_ATARI_BPP = 0;	// 0 - autodetect
static int FULLSCREEN = 1;
static int BW = 0;
static int SWAP_JOYSTICKS = 0;
static int WIDTH_MODE = 1;
static int ROTATE90 =0;
#define SHORT_WIDTH_MODE 0
#define DEFAULT_WIDTH_MODE 1
#define FULL_WIDTH_MODE 2

// you need to uncomment this to turn on fps counter

// #define FPS_COUNTER = 1

// joystick emulation
// keys should be loaded from config file

int SDL_TRIG_0 = SDLK_LCTRL;
int SDL_TRIG_0_B = SDLK_KP0;
int SDL_JOY_0_LEFT = SDLK_KP4;
int SDL_JOY_0_RIGHT = SDLK_KP6;
int SDL_JOY_0_DOWN = SDLK_KP2;
int SDL_JOY_0_UP = SDLK_KP8;
int SDL_JOY_0_LEFTUP = SDLK_KP7;
int SDL_JOY_0_RIGHTUP = SDLK_KP9;
int SDL_JOY_0_LEFTDOWN = SDLK_KP1;
int SDL_JOY_0_RIGHTDOWN = SDLK_KP3;

int SDL_TRIG_1 = SDLK_TAB;
int SDL_TRIG_1_B = SDLK_LSHIFT;
int SDL_JOY_1_LEFT = SDLK_a;
int SDL_JOY_1_RIGHT = SDLK_d;
int SDL_JOY_1_DOWN = SDLK_x;
int SDL_JOY_1_UP = SDLK_w;
int SDL_JOY_1_LEFTUP = SDLK_q;
int SDL_JOY_1_RIGHTUP = SDLK_e;
int SDL_JOY_1_LEFTDOWN = SDLK_z;
int SDL_JOY_1_RIGHTDOWN = SDLK_c;

// real joysticks

int fd_joystick0 = -1;
int fd_joystick1 = -1;

SDL_Joystick *joystick0 = NULL;
SDL_Joystick *joystick1 = NULL;
int joystick0_nbuttons, joystick1_nbuttons;

#define minjoy 10000			// real joystick tolerancy

extern int alt_function;

#ifdef SOUND
// sound 
#define FRAGSIZE        10		// 1<<FRAGSIZE is size of sound buffer
static int SOUND_VOLUME = SDL_MIX_MAXVOLUME / 4;
static int dsprate = 44100;
#endif

// video
SDL_Surface *MainScreen = NULL;
SDL_Color colors[256];			// palette
Uint16 Palette16[256];			// 16-bit palette
Uint32 Palette32[256];			// 32-bit palette

// keyboard
Uint8 *kbhits;
static int last_key_break = 0;
static int last_key_code = AKEY_NONE;

#ifdef SOUND
int Sound_Update(void)
{
	return 0;
}								// fake function
#endif

static void SetPalette()
{
	SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);
}

void CalcPalette()
{
	int i, rgb;
	float y;
	Uint32 c;
	if (BW == 0)
		for (i = 0; i < 256; i++) {
			rgb = colortable[i];
			colors[i].r = (rgb & 0x00ff0000) >> 16;
			colors[i].g = (rgb & 0x0000ff00) >> 8;
			colors[i].b = (rgb & 0x000000ff) >> 0;
		}
	else
		for (i = 0; i < 256; i++) {
			rgb = colortable[i];
			y = 0.299 * ((rgb & 0x00ff0000) >> 16) +
				0.587 * ((rgb & 0x0000ff00) >> 8) +
				0.114 * ((rgb & 0x000000ff) >> 0);
			colors[i].r = y;
			colors[i].g = y;
			colors[i].b = y;
		}
	for (i = 0; i < 256; i++) {
		c =
			SDL_MapRGB(MainScreen->format, colors[i].r, colors[i].g,
					   colors[i].b);
		switch (MainScreen->format->BitsPerPixel) {
		case 16:
			Palette16[i] = (Uint16) c;
			break;
		case 32:
			Palette32[i] = (Uint32) c;
			break;
		}
	}

}

void ModeInfo()
{
	char bwflag, fullflag, width, joyflag;
	if (BW)
		bwflag = '*';
	else
		bwflag = ' ';
	if (FULLSCREEN)
		fullflag = '*';
	else
		fullflag = ' ';
	switch (WIDTH_MODE) {
	case FULL_WIDTH_MODE:
		width = 'f';
		break;
	case DEFAULT_WIDTH_MODE:
		width = 'd';
		break;
	case SHORT_WIDTH_MODE:
		width = 's';
		break;
	default:
		width = '?';
		break;
	}
	if (SWAP_JOYSTICKS)
		joyflag = '*';
	else
		joyflag = ' ';
	Aprint("Video Mode: %ix%ix%i", MainScreen->w, MainScreen->h,
		   MainScreen->format->BitsPerPixel);
	Aprint
		("[%c] FULLSCREEN  [%c] BW  [%c] WIDTH MODE  [%c] JOYSTICKS SWAPPED",
		 fullflag, bwflag, width, joyflag);
}

void SetVideoMode(int w, int h, int bpp)
{
	if (FULLSCREEN)
		MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_FULLSCREEN);
	else
		MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_RESIZABLE);
	if (MainScreen == NULL) {
		Aprint("Setting Video Mode: %ix%ix%i FAILED", w, h, bpp);
		Aflushlog();
		exit(-1);
	}
}

void SetNewVideoMode(int w, int h, int bpp)
{
	float ww, hh;

	if (ROTATE90)
	{
		SetVideoMode(w,h,bpp);
	}
	else
	{
	

	if ((h < ATARI_HEIGHT) || (w < ATARI_WIDTH)) {
		h = ATARI_HEIGHT;
		w = ATARI_WIDTH;
	}

	// aspect ratio, floats needed
	ww = w;
	hh = h;
	switch (WIDTH_MODE) {
	case SHORT_WIDTH_MODE:
		if (ww * 0.75 < hh)
			hh = ww * 0.75;
		else
			ww = hh / 0.75;
		break;
	case DEFAULT_WIDTH_MODE:
		if (ww / 1.4 < hh)
			hh = ww / 1.4;
		else
			ww = hh * 1.4;
		break;
	case FULL_WIDTH_MODE:
		if (ww / 1.6 < hh)
			hh = ww / 1.6;
		else
			ww = hh * 1.6;
		break;
	}
	w = ww;
	h = hh;
	w = w / 8;
	w = w * 8;
	h = h / 8;
	h = h * 8;

	SetVideoMode(w, h, bpp);

	SDL_ATARI_BPP = MainScreen->format->BitsPerPixel;
	if (bpp == 0) {
		Aprint("detected %ibpp", SDL_ATARI_BPP);
		if ((SDL_ATARI_BPP != 8) && (SDL_ATARI_BPP != 16)
			&& (SDL_ATARI_BPP != 32)) {
			Aprint
				("it's unsupported, so setting 8bit mode (slow conversion)");
			SetVideoMode(w, h, 8);
		}
	}
	}

	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	// hide mouse cursor 

	ModeInfo();

}

void SwitchFullscreen()
{
	FULLSCREEN = 1 - FULLSCREEN;
	SetNewVideoMode(MainScreen->w, MainScreen->h,
					MainScreen->format->BitsPerPixel);
	Atari_DisplayScreen((UBYTE *) atari_screen);
}

void SwitchWidth()
{
	WIDTH_MODE++;
	if (WIDTH_MODE > FULL_WIDTH_MODE)
		WIDTH_MODE = SHORT_WIDTH_MODE;
	SetNewVideoMode(MainScreen->w, MainScreen->h,
					MainScreen->format->BitsPerPixel);
	Atari_DisplayScreen((UBYTE *) atari_screen);
}

void SwitchBW()
{
	BW = 1 - BW;
	CalcPalette();
	SetPalette();
	ModeInfo();
}

void SwapJoysticks()
{
	SWAP_JOYSTICKS = 1 - SWAP_JOYSTICKS;
	ModeInfo();
}

#ifdef SOUND
void SDL_Sound_Update(void *userdata, Uint8 * stream, int len)
{
	Uint8 dsp_buffer[2 << FRAGSIZE]; // x2, because 16bit buffers
	if (len > 1 << FRAGSIZE)
		len = 1 << FRAGSIZE;
	Pokey_process(dsp_buffer, len);
	if (sound_bits==8)
		SDL_MixAudio(stream, dsp_buffer, len, SOUND_VOLUME);
	else	
		SDL_MixAudio(stream, dsp_buffer, 2*len, SOUND_VOLUME);
}

void SDL_Sound_Initialise(int *argc, char *argv[])
{
	int i, j;
	SDL_AudioSpec desired, obtained;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-audio16") == 0)
		{	
			Aprint("audio 16bit enabled");
			sound_flags|=SND_BIT16;
			sound_bits=16;
		}	
		else if (strcmp(argv[i], "-dsprate") == 0)
			sscanf(argv[++i], "%d", &dsprate);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-sound           Enable sound\n"
				       "\t-nosound         Disable sound\n"
				       "\t-dsprate <rate>  Set DSP rate in Hz\n"
				      );
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (sound_enabled) {
		desired.freq = dsprate;
		if (sound_bits==8)
			desired.format = AUDIO_U8;
		else
			if (sound_bits==16)
				desired.format = AUDIO_U16;
		else
		{
			Aprint ("unknown sound_bits");
			Atari800_Exit(FALSE);
			Aflushlog();
		};	
			
		desired.samples = 1 << FRAGSIZE;
		desired.callback = SDL_Sound_Update;
		desired.userdata = NULL;
		desired.channels = 1;

		if (SDL_OpenAudio(&desired, &obtained) < 0) {
			Aprint("Problem with audio: %s", SDL_GetError());
			Aprint("Sound is disabled.");
			sound_enabled = FALSE;
			return;
		}

		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1, sound_flags);
	}
	else {
		Aprint
			("Audio is off, you can turn it by -sound");
	}
}
#endif

int Atari_Keyboard(void)
{
	static int lastkey = AKEY_NONE, key_pressed = 0, key_control = 0;

	SDL_Event event;
	if (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			lastkey = event.key.keysym.sym;
			key_pressed = 1;
			break;
		case SDL_KEYUP:
			lastkey = event.key.keysym.sym;
			key_pressed = 0;
			break;
		case SDL_VIDEORESIZE:
			SetNewVideoMode(event.resize.w, event.resize.h,
							MainScreen->format->BitsPerPixel);
			break;
		case SDL_QUIT:
			return AKEY_EXIT;
			break;
		}
	}
	kbhits = SDL_GetKeyState(NULL);

	if (kbhits == NULL) {
		Aprint("oops, kbhits is NULL!");
		Aflushlog();
		exit(-1);
	}

	alt_function = -1;
	if (kbhits[SDLK_LALT]) {
		if (key_pressed) {
			switch (lastkey) {
			case SDLK_f:
				SwitchFullscreen();
				break;
			case SDLK_g:
				SwitchWidth();
				break;
			case SDLK_b:
				SwitchBW();
				break;
			case SDLK_j:
				SwapJoysticks();
				break;
			case SDLK_r:
				alt_function = MENU_RUN;
				break;
			case SDLK_y:
				alt_function = MENU_SYSTEM;
				break;
			case SDLK_o:
				alt_function = MENU_SOUND;
				break;
			case SDLK_w:
				alt_function = MENU_SOUND_RECORDING;
				break;
			case SDLK_a:
				alt_function = MENU_ABOUT;
				break;
			case SDLK_s:
				alt_function = MENU_SAVESTATE;
				break;
			case SDLK_d:
				alt_function = MENU_DISK;
				break;
			case SDLK_l:
				alt_function = MENU_LOADSTATE;
				break;
			case SDLK_c:
				alt_function = MENU_CARTRIDGE;
				break;
			}
		}
		key_pressed = 0;
		if (alt_function != -1)
			return AKEY_UI;
		else
			return AKEY_NONE;
	}


	// SHIFT STATE
	if ((kbhits[SDLK_LSHIFT]) || (kbhits[SDLK_RSHIFT]))
		key_shift = 1;
	else
		key_shift = 0;

    // CONTROL STATE
	if ((kbhits[SDLK_LCTRL]) || (kbhits[SDLK_RCTRL]))
		key_control = 1;
	else
		key_control = 0;

	// OPTION / SELECT / START keys
	key_consol = CONSOL_NONE;
	if (kbhits[SDLK_F2])
		key_consol &= (~CONSOL_OPTION);
	if (kbhits[SDLK_F3])
		key_consol &= (~CONSOL_SELECT);
	if (kbhits[SDLK_F4])
		key_consol &= (~CONSOL_START);

	if (key_pressed == 0) {
		return AKEY_NONE;
	}

	// really stupid piece of code, I think it should be an array, not switch/case
	// IDEA: it will be nice to define keyboard, like joystick emulation keys
	if (key_shift)
		switch (lastkey) {
		case SDLK_a:
			return AKEY_A;
		case SDLK_b:
			return AKEY_B;
		case SDLK_c:
			return AKEY_C;
		case SDLK_d:
			return AKEY_D;
		case SDLK_e:
			return AKEY_E;
		case SDLK_f:
			return AKEY_F;
		case SDLK_g:
			return AKEY_G;
		case SDLK_h:
			return AKEY_H;
		case SDLK_i:
			return AKEY_I;
		case SDLK_j:
			return AKEY_J;
		case SDLK_k:
			return AKEY_K;
		case SDLK_l:
			return AKEY_L;
		case SDLK_m:
			return AKEY_M;
		case SDLK_n:
			return AKEY_N;
		case SDLK_o:
			return AKEY_O;
		case SDLK_p:
			return AKEY_P;
		case SDLK_q:
			return AKEY_Q;
		case SDLK_r:
			return AKEY_R;
		case SDLK_s:
			return AKEY_S;
		case SDLK_t:
			return AKEY_T;
		case SDLK_u:
			return AKEY_U;
		case SDLK_v:
			return AKEY_V;
		case SDLK_w:
			return AKEY_W;
		case SDLK_x:
			return AKEY_X;
		case SDLK_y:
			return AKEY_Y;
		case SDLK_z:
			return AKEY_Z;
		case SDLK_SEMICOLON:
			return AKEY_COLON;
		case SDLK_F5:
			return AKEY_COLDSTART;
		case SDLK_1:
			return AKEY_EXCLAMATION;
		case SDLK_2:
			return AKEY_AT;
		case SDLK_3:
			return AKEY_HASH;
		case SDLK_4:
			return AKEY_DOLLAR;
		case SDLK_5:
			return AKEY_PERCENT;
		case SDLK_6:
			return AKEY_CARET;
		case SDLK_7:
			return AKEY_AMPERSAND;
		case SDLK_8:
			return AKEY_ASTERISK;
		case SDLK_9:
			return AKEY_PARENLEFT;
		case SDLK_0:
			return AKEY_PARENRIGHT;
		case SDLK_EQUALS:
			return AKEY_PLUS;
		case SDLK_MINUS:
			return AKEY_UNDERSCORE;
		case SDLK_QUOTE:
			return AKEY_DBLQUOTE;
		case SDLK_SLASH:
			return AKEY_QUESTION;
		case SDLK_COMMA:
			return AKEY_LESS;
		case SDLK_PERIOD:
			return AKEY_GREATER;
		case SDLK_F10:
			key_pressed = 0;
			return AKEY_SCREENSHOT_INTERLACE;
		case SDLK_INSERT:
			return AKEY_INSERT_LINE;
		}
	else
		switch (lastkey) {
		case SDLK_a:
			return AKEY_a;
		case SDLK_b:
			return AKEY_b;
		case SDLK_c:
			return AKEY_c;
		case SDLK_d:
			return AKEY_d;
		case SDLK_e:
			return AKEY_e;
		case SDLK_f:
			return AKEY_f;
		case SDLK_g:
			return AKEY_g;
		case SDLK_h:
			return AKEY_h;
		case SDLK_i:
			return AKEY_i;
		case SDLK_j:
			return AKEY_j;
		case SDLK_k:
			return AKEY_k;
		case SDLK_l:
			return AKEY_l;
		case SDLK_m:
			return AKEY_m;
		case SDLK_n:
			return AKEY_n;
		case SDLK_o:
			return AKEY_o;
		case SDLK_p:
			return AKEY_p;
		case SDLK_q:
			return AKEY_q;
		case SDLK_r:
			return AKEY_r;
		case SDLK_s:
			return AKEY_s;
		case SDLK_t:
			return AKEY_t;
		case SDLK_u:
			return AKEY_u;
		case SDLK_v:
			return AKEY_v;
		case SDLK_w:
			return AKEY_w;
		case SDLK_x:
			return AKEY_x;
		case SDLK_y:
			return AKEY_y;
		case SDLK_z:
			return AKEY_z;
		case SDLK_SEMICOLON:
			return AKEY_SEMICOLON;
		case SDLK_F5:
			return AKEY_WARMSTART;
		case SDLK_0:
			return AKEY_0;
		case SDLK_1:
			return AKEY_1;
		case SDLK_2:
			return AKEY_2;
		case SDLK_3:
			return AKEY_3;
		case SDLK_4:
			return AKEY_4;
		case SDLK_5:
			return AKEY_5;
		case SDLK_6:
			return AKEY_6;
		case SDLK_7:
			return AKEY_7;
		case SDLK_8:
			return AKEY_8;
		case SDLK_9:
			return AKEY_9;
		case SDLK_COMMA:
			return AKEY_COMMA;
		case SDLK_PERIOD:
			return AKEY_FULLSTOP;
		case SDLK_EQUALS:
			return AKEY_EQUAL;
		case SDLK_MINUS:
			return AKEY_MINUS;
		case SDLK_QUOTE:
			return AKEY_QUOTE;
		case SDLK_SLASH:
			return AKEY_SLASH;
		case SDLK_BACKSLASH:
			return AKEY_BACKSLASH;
		case SDLK_LEFTBRACKET:
			return AKEY_BRACKETLEFT;
		case SDLK_RIGHTBRACKET:
			return AKEY_BRACKETRIGHT;
		case SDLK_F10:
			key_pressed = 0;
			return AKEY_SCREENSHOT;
		case SDLK_INSERT:
			return AKEY_INSERT_CHAR;
		}

	switch (lastkey) {
	case SDLK_END:
		return AKEY_HELP;
	case SDLK_PAGEDOWN:
		return AKEY_HELP;
	case SDLK_PAGEUP:
		return AKEY_CAPSLOCK;
	case SDLK_HOME:
		return AKEY_CLEAR;
	case SDLK_PAUSE:
		return AKEY_BREAK;
	case SDLK_CAPSLOCK:
		return AKEY_CAPSLOCK;
	case SDLK_SPACE:
		return AKEY_SPACE;
	case SDLK_BACKSPACE:
		return AKEY_BACKSPACE;
	case SDLK_RETURN:
		return AKEY_RETURN;
	case SDLK_F9:
		return AKEY_EXIT;
	case SDLK_F1:
		return AKEY_UI;
	case SDLK_LEFT:
		return AKEY_LEFT;
	case SDLK_RIGHT:
		return AKEY_RIGHT;
	case SDLK_UP:
		return AKEY_UP;
	case SDLK_DOWN:
		return AKEY_DOWN;
	case SDLK_ESCAPE:
		return AKEY_ESCAPE;
	case SDLK_TAB:
		if (key_shift)
			return AKEY_SETTAB;
		else if (key_control)
			return AKEY_CLRTAB;
		else
			return AKEY_TAB;
	case SDLK_DELETE:
		if (key_shift)
			return AKEY_DELETE_LINE;
		else
			return AKEY_DELETE_CHAR;
	case SDLK_INSERT:
		if (key_shift)
			return AKEY_INSERT_LINE;
		else
			return AKEY_INSERT_CHAR;
	}
	return AKEY_NONE;
}

void Init_SDL_Joysticks(int first, int second)
{
	if (first) {
		joystick0 = SDL_JoystickOpen(0);
		if (joystick0 == NULL)
			Aprint("joystick 0 not found");
		else {
			Aprint("joystick 0 found!");
			joystick0_nbuttons = SDL_JoystickNumButtons(joystick0);
			SWAP_JOYSTICKS = 1;		// real joy is STICK(0) and numblock is STICK(1)
		}
	}

	if (second) {
		joystick1 = SDL_JoystickOpen(1);
		if (joystick1 == NULL)
			Aprint("joystick 1 not found");
		else {
			Aprint("joystick 1 found!");
			joystick1_nbuttons = SDL_JoystickNumButtons(joystick1);
		}
	}
}

void Init_Joysticks(int *argc, char *argv[])
{
#ifdef LPTJOY
	char *lpt_joy0 = NULL;
	char *lpt_joy1 = NULL;
	int i;
	int j;
	
	for (i=j=1; i<*argc; i++) {
		if (!strcmp(argv[i], "-joy0")) {
			if (i == *argc - 1) {
				Aprint("joystick device path missing!");
				break;
			}
			lpt_joy0 = argv[++i];
		} else if (!strcmp(argv[i], "-joy1")) {
			if (i == *argc - 1) {
				Aprint("joystick device path missing!");
				break;
			}
			lpt_joy1 = argv[++i];
		} else {
			argv[j++] = argv[i];
		}
	}
	*argc = j;
	
	if (lpt_joy0 != NULL) {				// LPT1 joystick
		fd_joystick0 = open(lpt_joy0, O_RDONLY);
		if (fd_joystick0 == -1) perror(lpt_joy0);
	}
	if (lpt_joy1 != NULL) {				// LPT2 joystick
		fd_joystick1 = open(lpt_joy1, O_RDONLY);
		if (fd_joystick1 == -1) perror(lpt_joy1);
	}
#endif /* LPTJOY */
	Init_SDL_Joysticks(fd_joystick0 == -1, fd_joystick1 == -1);
}


void Atari_Initialise(int *argc, char *argv[])
{
	int i, j;
	int no_joystick;
	int width, height, bpp;
	int help_only = FALSE;

	no_joystick = 0;
	width = ATARI_WIDTH;
	height = ATARI_HEIGHT;
	bpp = SDL_ATARI_BPP;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-rotate90") == 0) {
			ROTATE90 = 1;
			width=240;
			height=320;
			bpp=16;
			no_joystick = 1;
			Aprint("rotate90 mode");
			i++;
		}
		else if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = 1;
			Aprint("no joystick");
			i++;
		}
		else if (strcmp(argv[i], "-width") == 0) {
			sscanf(argv[++i], "%i", &width);
			Aprint("width set", width);
		}
		else if (strcmp(argv[i], "-height") == 0) {
			sscanf(argv[++i], "%i", &height);
			Aprint("height set");
		}
		else if (strcmp(argv[i], "-bpp") == 0) {
			sscanf(argv[++i], "%i", &bpp);
			Aprint("bpp set");
		}
		else if (strcmp(argv[i], "-fullscreen") == 0) {
			FULLSCREEN = 1;
		}
		else if (strcmp(argv[i], "-windowed") == 0) {
			FULLSCREEN = 0;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Aprint("\t-rotate90        Display 240x320 screen");
				Aprint("\t-nojoystick      Disable joystick");
#ifdef LPTJOY
				Aprint("\t-joy0 <pathname> Select LPTjoy0 device");
				Aprint("\t-joy1 <pathname> Select LPTjoy0 device");
#endif /* LPTJOY */
				Aprint("\t-width <num>     Host screen width");
				Aprint("\t-height <num>    Host screen height");
				Aprint("\t-bpp <num>       Host color depth");
				Aprint("\t-fullscreen      Run fullscreen");
				Aprint("\t-windowed        Run in window");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	i = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
#ifdef SOUND
	i |= SDL_INIT_AUDIO;
#endif
	if (SDL_Init(i) != 0) {
		Aprint("SDL_Init FAILED");
		Aprint(SDL_GetError());
		Aflushlog();
		exit(-1);
	}
	atexit(SDL_Quit);

	/* SDL_WM_SetIcon("/usr/local/atari800/atarixe.ICO"), NULL); */
	SDL_WM_SetCaption(ATARI_TITLE, "Atari800");

#ifdef SOUND
	SDL_Sound_Initialise(argc, argv);
#endif

	if (help_only)
		return;		/* return before changing the gfx mode */

	SetNewVideoMode(width, height, bpp);
	CalcPalette();
	SetPalette();

	Aprint
		("please report SDL port bugs to Jacek Poplawski <jacekp@linux.com.pl>");
	Aprint("video initialized");

	if (no_joystick == 0)
		Init_Joysticks(argc, argv);

}

int Atari_Exit(int run_monitor)
{
	int restart;

#if 0
	if (run_monitor)
		/* disable graphics, set alpha mode */
		restart = monitor();
	else
#endif
		restart = FALSE;

	if (restart) {
		/* set up graphics and all the stuff */
		return 1;
	}

	SDL_Quit();

	Aflushlog();

	return restart;
}

void DisplayRotated240x320(Uint8 * screen)
{
	int i,j;
	Uint8 c;
	register Uint32 *start32;
	Uint32 quad;
	if (MainScreen->format->BitsPerPixel!=16) 
	{
		Aprint("rotated display works only for bpp=16 right now");
		Aflushlog();
		exit(-1);
	};
	start32 = (Uint32 *) MainScreen->pixels;
	for (j=0;j<MainScreen->h;j++)
	  for (i=0;i<MainScreen->w/2;i++)
	  {
		c=screen[ATARI_WIDTH*(i*2)+32+320-j];
		quad=Palette16[c]<<16;
		c=screen[ATARI_WIDTH*(i*2+1)+32+320-j];
		quad+=Palette16[c];
	  	*start32=quad;
		start32++;
	  }	
}

void DisplayWithoutScaling(Uint8 * screen, int jumped, int width)
{
	register Uint32 quad;
	register Uint32 *start32;
	register Uint8 c;
	register int pos;
	register int pitch4;
	int i;

	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	screen = screen + jumped;
	i = MainScreen->h;
	switch (MainScreen->format->BitsPerPixel) {
	case 8:
		while (i > 0) {
			memcpy(start32, screen, width);
			screen += ATARI_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	case 16:
		while (i > 0) {
			pos = width - 1;
			while (pos > 0) {
				c = screen[pos];
				quad = Palette16[c] << 16;
				pos--;
				c = screen[pos];
				quad += Palette16[c];
				start32[pos >> 1] = quad;
				pos--;
			}
			screen += ATARI_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	case 32:
		while (i > 0) {
			pos = width - 1;
			while (pos > 0) {
				c = screen[pos];
				quad = Palette32[c];
				start32[pos] = quad;
				pos--;
			}
			screen += ATARI_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	default:
		Aprint("unsupported color depth %i",
			   MainScreen->format->BitsPerPixel);
		Aprint
			("please set SDL_ATARI_BPP to 8 or 16 and recompile atari_sdl");
		Aflushlog();
		exit(-1);
	}
}

void DisplayWithScaling(Uint8 * screen, int jumped, int width)
{
	register Uint32 quad;
	register int x;
	register int dx;
	register int yy;
	register Uint8 *ss;
	register Uint32 *start32;
	int i;
	int y;
	int w1, w2, w4;
	int w, h;
	int pos;
	int pitch4;
	int dy;
	Uint8 c;
	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	w = (width) << 16;
	h = (ATARI_HEIGHT) << 16;
	dx = w / MainScreen->w;
	dy = h / MainScreen->h;
	w1 = MainScreen->w - 1;
	w2 = MainScreen->w / 2 - 1;
	w4 = MainScreen->w / 4 - 1;
	ss = screen;
	y = (0) << 16;
	i = MainScreen->h;

	switch (MainScreen->format->BitsPerPixel) {
	case 8:
		while (i > 0) {
			x = (width + jumped) << 16;
			pos = w4;
			yy = ATARI_WIDTH * (y >> 16);
			while (pos >= 0) {
				quad = (ss[yy + (x >> 16)] << 24);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 16);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 8);
				x = x - dx;
				quad += (ss[yy + (x >> 16)] << 0);
				x = x - dx;

				start32[pos] = quad;
				pos--;

			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}
		break;
	case 16:
		while (i > 0) {
			x = (width + jumped) << 16;
			pos = w2;
			yy = ATARI_WIDTH * (y >> 16);
			while (pos >= 0) {

				c = ss[yy + (x >> 16)];
				quad = Palette16[c] << 16;
				x = x - dx;
				c = ss[yy + (x >> 16)];
				quad += Palette16[c];
				x = x - dx;
				start32[pos] = quad;
				pos--;

			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}
		break;
	case 32:
		while (i > 0) {
			x = (width + jumped) << 16;
			pos = w1;
			yy = ATARI_WIDTH * (y >> 16);
			while (pos >= 0) {

				c = ss[yy + (x >> 16)];
				quad = Palette32[c];
				x = x - dx;
				start32[pos] = quad;
				pos--;

			}
			start32 += pitch4;
			y = y + dy;
			i--;
		}

		break;
	default:
		Aprint("unsupported color depth %i",
			   MainScreen->format->BitsPerPixel);
		Aprint
			("please set SDL_ATARI_BPP to 8 or 16 or 32 and recompile atari_sdl");
		Aflushlog();
		exit(-1);
	}
}

void Atari_DisplayScreen(UBYTE * screen)
{
	int width, jumped;

	switch (WIDTH_MODE) {
	case SHORT_WIDTH_MODE:
		width = ATARI_WIDTH - 2 * 24 - 2 * 8;
		jumped = 24 + 8;
		break;
	case DEFAULT_WIDTH_MODE:
		width = ATARI_WIDTH - 2 * 24;
		jumped = 24;
		break;
	case FULL_WIDTH_MODE:
		width = ATARI_WIDTH;
		jumped = 0;
		break;
	default:
		Aprint("unsupported WIDTH_MODE");
		Aflushlog();
		exit(-1);
		break;
	}

	if (ROTATE90)
	{
		DisplayRotated240x320(screen);
	}
	else
	if ((MainScreen->w == width)
		&& (MainScreen->h == ATARI_HEIGHT)) {
		DisplayWithoutScaling(screen, jumped, width);
	}

	else {
		DisplayWithScaling(screen, jumped, width);
	}
	SDL_Flip(MainScreen);
}

// two empty functions, needed by input.c and platform.h

int Atari_PORT(int num)
{
	return 0;
}

int Atari_TRIG(int num)
{
	return 0;
}

int get_SDL_joystick_state(SDL_Joystick *joystick)
{
	int x;
	int y;

	x = SDL_JoystickGetAxis(joystick, 0);
	y = SDL_JoystickGetAxis(joystick, 1);

	if (x > minjoy) {
		if (y < -minjoy)
			return STICK_UR;
		else if (y > minjoy)
			return STICK_LR;
		else
			return STICK_RIGHT;
	} else if (x < -minjoy) {
		if (y < -minjoy)
			return STICK_UL;
		else if (y > minjoy)
			return STICK_LL;
		else
			return STICK_LEFT;
	} else {
		if (y < -minjoy)
			return STICK_FORWARD;
		else if (y > minjoy)
			return STICK_BACK;
		else
			return STICK_CENTRE;
	}
}

int get_LPT_joystick_state(int fd)
{
#ifdef LPTJOY
	int status;

	ioctl(fd, LPGETSTATUS, &status);
	status ^= 0x78;

	if (status & 0x40) {			// right
		if (status & 0x10) {		// up
			return STICK_UR;
		} else if (status & 0x20) {	// down
			return STICK_LR;
		} else {
			return STICK_RIGHT;
		}
	} else if (status & 0x80) {		// left
		if (status & 0x10) {		// up
			return STICK_UL;
		} else if (status & 0x20) {	// down
			return STICK_LL;
		} else {
			return STICK_LEFT;
		}
	} else {
		if (status & 0x10) {		// up
			return STICK_FORWARD;
		} else if (status & 0x20) {	// down
			return STICK_BACK;
		} else {
			return STICK_CENTRE;
		}
	}
#else
	return 0;
#endif /* LPTJOY */
}

void SDL_Atari_PORT(Uint8 * s0, Uint8 * s1)
{
	int stick0, stick1;
	stick0 = STICK_CENTRE;
	stick1 = STICK_CENTRE;

	if (kbhits[SDL_JOY_0_LEFT])
		stick0 = STICK_LEFT;
	if (kbhits[SDL_JOY_0_RIGHT])
		stick0 = STICK_RIGHT;
	if (kbhits[SDL_JOY_0_UP])
		stick0 = STICK_FORWARD;
	if (kbhits[SDL_JOY_0_DOWN])
		stick0 = STICK_BACK;
	if ((kbhits[SDL_JOY_0_LEFTUP])
		|| ((kbhits[SDL_JOY_0_LEFT]) && (kbhits[SDL_JOY_0_UP])))
		stick0 = STICK_UL;
	if ((kbhits[SDL_JOY_0_LEFTDOWN])
		|| ((kbhits[SDL_JOY_0_LEFT]) && (kbhits[SDL_JOY_0_DOWN])))
		stick0 = STICK_LL;
	if ((kbhits[SDL_JOY_0_RIGHTUP])
		|| ((kbhits[SDL_JOY_0_RIGHT]) && (kbhits[SDL_JOY_0_UP])))
		stick0 = STICK_UR;
	if ((kbhits[SDL_JOY_0_RIGHTDOWN])
		|| ((kbhits[SDL_JOY_0_RIGHT]) && (kbhits[SDL_JOY_0_DOWN])))
		stick0 = STICK_LR;
	if (kbhits[SDL_JOY_1_LEFT])
		stick1 = STICK_LEFT;
	if (kbhits[SDL_JOY_1_RIGHT])
		stick1 = STICK_RIGHT;
	if (kbhits[SDL_JOY_1_UP])
		stick1 = STICK_FORWARD;
	if (kbhits[SDL_JOY_1_DOWN])
		stick1 = STICK_BACK;
	if ((kbhits[SDL_JOY_1_LEFTUP])
		|| ((kbhits[SDL_JOY_1_LEFT]) && (kbhits[SDL_JOY_1_UP])))
		stick1 = STICK_UL;
	if ((kbhits[SDL_JOY_1_LEFTDOWN])
		|| ((kbhits[SDL_JOY_1_LEFT]) && (kbhits[SDL_JOY_1_DOWN])))
		stick1 = STICK_LL;
	if ((kbhits[SDL_JOY_1_RIGHTUP])
		|| ((kbhits[SDL_JOY_1_RIGHT]) && (kbhits[SDL_JOY_1_UP])))
		stick1 = STICK_UR;
	if ((kbhits[SDL_JOY_1_RIGHTDOWN])
		|| ((kbhits[SDL_JOY_1_RIGHT]) && (kbhits[SDL_JOY_1_DOWN])))
		stick1 = STICK_LR;

	if (SWAP_JOYSTICKS) {
		*s1 = stick0;
		*s0 = stick1;
	}
	else {
		*s0 = stick0;
		*s1 = stick1;
	}

	if ((joystick0 != NULL) || (joystick1 != NULL))	// can only joystick1!=NULL ?
	{
		SDL_JoystickUpdate();
	}
	
	if (fd_joystick0 != -1)
		*s0 = get_LPT_joystick_state(fd_joystick0);
	else if (joystick0 != NULL)
		*s0 = get_SDL_joystick_state(joystick0);

	if (fd_joystick1 != -1)
		*s1 = get_LPT_joystick_state(fd_joystick1);
	else if (joystick1 != NULL)
		*s1 = get_SDL_joystick_state(joystick1);
}

void SDL_Atari_TRIG(Uint8 * t0, Uint8 * t1)
{
	int trig0, trig1, i;

	if ((kbhits[SDL_TRIG_0]) || (kbhits[SDL_TRIG_0_B]))
		trig0 = 0;
	else
		trig0 = 1;

	if ((kbhits[SDL_TRIG_1]) || (kbhits[SDL_TRIG_1_B]))
		trig1 = 0;
	else
		trig1 = 1;

	if (SWAP_JOYSTICKS) {
		*t1 = trig0;
		*t0 = trig1;
	}
	else {
		*t0 = trig0;
		*t1 = trig1;
	}

	if (fd_joystick0 != -1) {
#ifdef LPTJOY
		int status;
		ioctl(fd_joystick0, LPGETSTATUS, &status);
		if (status & 8)
			*t0 = 1;
		else
			*t0 = 0;
#endif /* LPTJOY */
	} else if (joystick0 != NULL) {
		trig0 = 1;
		for (i = 0; i < joystick0_nbuttons; i++) {
			if (SDL_JoystickGetButton(joystick0, i)) {
				trig0 = 0;
				break;
			}
		}
		*t0 = trig0;
	}
	
	if (fd_joystick1 != -1) {
#ifdef LPTJOY
		int status;
		ioctl(fd_joystick1, LPGETSTATUS, &status);
		if (status & 8)
			*t1 = 1;
		else
			*t1 = 0;
#endif /* LPTJOY */
	} else if (joystick1 != NULL) {
		trig1 = 1;
		for (i = 0; i < joystick1_nbuttons; i++) {
			if (SDL_JoystickGetButton(joystick1, i)) {
				trig1 = 0;
				break;
			}
		}
		*t1 = trig1;
	}
}

void CountFPS()
{
	static int ticks1 = 0, ticks2, shortframes;
	if (ticks1 == 0)
		ticks1 = SDL_GetTicks();
	ticks2 = SDL_GetTicks();
	shortframes++;
	if (ticks2 - ticks1 > 1000) {
		ticks1 = ticks2;
		Aprint("%i fps", shortframes);
		shortframes = 0;
	}
}

int main(int argc, char **argv)
{
	int keycode;
	static UBYTE STICK[4];
	int done = 0;

	if (!Atari800_Initialise(&argc, argv))
		return 3;

#ifdef SOUND
	if (sound_enabled)
		SDL_PauseAudio(0);
#endif
	while (!done) {
		keycode = Atari_Keyboard();

		switch (keycode) {
		case AKEY_EXIT:
			done = 1;
			break;
		case AKEY_COLDSTART:
			Coldstart();
			break;
		case AKEY_WARMSTART:
			Warmstart();
			break;
		case AKEY_UI:
#ifdef SOUND
			if (sound_enabled)
				SDL_PauseAudio(1);
#endif
			ui((UBYTE *) atari_screen);
#ifdef SOUND
			if (sound_enabled)
				SDL_PauseAudio(0);
#endif
			break;
		case AKEY_SCREENSHOT:
			Save_PCX_file(FALSE, Find_PCX_name());
			break;
		case AKEY_SCREENSHOT_INTERLACE:
			Save_PCX_file(TRUE, Find_PCX_name());
			break;
		case AKEY_BREAK:
			key_break = 1;
			if (!last_key_break) {
				if (IRQEN & 0x80) {
					IRQST &= ~0x80;
					GenerateIRQ();
				}
				break;
		default:
				key_break = 0;
				if (keycode != SDL_JOY_0_LEFT	// plain hack
					&& keycode != SDL_JOY_0_RIGHT	// see atari_vga.c
					&& keycode != SDL_JOY_0_UP	// for better way
					&& keycode != SDL_JOY_0_DOWN
					&& keycode != SDL_JOY_0_LEFTDOWN
					&& keycode != SDL_JOY_0_RIGHTDOWN
					&& keycode != SDL_JOY_0_LEFTUP
					&& keycode != SDL_JOY_0_RIGHTUP
					&& keycode != SDL_TRIG_0 && keycode != SDL_TRIG_0_B)
					key_code = keycode;
				break;
			}
		}

		last_key_break = key_break;

		SKSTAT |= 0xc;
		if (key_shift)
			SKSTAT &= ~8;
		if (key_code == AKEY_NONE)
			last_key_code = AKEY_NONE;
		if (key_code != AKEY_NONE) {
			SKSTAT &= ~4;
			if ((key_code ^ last_key_code) & ~AKEY_SHFTCTRL) {
				last_key_code = key_code;
				KBCODE = (UBYTE) key_code;
				if (IRQEN & 0x40) {
					if (IRQST & 0x40) {
						IRQST &= ~0x40;
						GenerateIRQ();
					}
					else {
						/* keyboard over-run */
						SKSTAT &= ~0x40;
						/* assert(IRQ != 0); */
					}
 				}
			}
		}

		SDL_Atari_PORT(&STICK[0], &STICK[1]);
		SDL_Atari_TRIG(&TRIG[0], &TRIG[1]);

		PORT_input[0] = (STICK[1] << 4) | STICK[0];

		Device_Frame();
		GTIA_Frame();
		ANTIC_Frame(TRUE);
#ifdef SHOW_DISK_LED
		LED_Frame();
#endif
		POKEY_Frame();
		nframes++;
		atari_sync();
		Atari_DisplayScreen((UBYTE *) atari_screen);
#ifdef FPS_COUNTER
		CountFPS();
#endif
	}
	Atari800_Exit(FALSE);
	Aflushlog();
	return 0;
}

/*
 $Log$
 Revision 1.38  2003/10/25 19:07:48  joy
 keyboard overrun fix in SDL

 Revision 1.37  2003/08/05 13:22:37  joy
 removed unused stuff

 Revision 1.36  2003/03/03 09:57:32  joy
 Ed improved autoconf again plus fixed some little things

 Revision 1.35  2003/02/24 09:32:42  joy
 header cleanup

 Revision 1.34  2003/02/18 09:07:52  joy
 more Atari keys supported: CARET, HELP, CAPSLOCK, SETTAB, CLRTAB, DELETE_LINE, DELETE_CHAR, INSERT_LINE, INSERT_CHAR

 Revision 1.33  2003/02/17 14:21:33  joy
 window title set

 Revision 1.32  2003/02/09 21:22:39  joy
 removed obsolete and unnecessary cmdline switches

 Revision 1.31  2003/02/08 20:06:09  joy
 input.c had a bug that Piotr fixed, I just copied the bugfix here.

 Revision 1.30  2003/01/27 14:13:02  joy
 Perry's cycle-exact ANTIC/GTIA emulation

 Revision 1.29  2002/12/14 11:02:56  jacek_poplawski

 updated audio stuff (mzpokey, stereo, 16bit audio)

 Revision 1.28  2002/08/18 20:24:12  joy
 MacOSX needs SetPalette private

 Revision 1.27  2002/08/07 08:54:34  joy
 when sound init failed Atari800 continues to run with sound disabled

 Revision 1.26  2002/08/07 07:26:58  joy
 SDL cleanup thanks to atexit, -nosound supported, -disable-SOUND supported, -dsprate added, -help fixed

 Revision 1.25  2002/06/27 21:50:25  joy
 LPTjoy in SDL works under Linux only

 Revision 1.24  2002/06/27 21:20:08  joy
 LPT joy in SDL

 Revision 1.23  2002/06/23 21:52:49  joy
 "-help" added
 options handling fixed.
 "--option" converted to "-option" to conform to the rest of application

 Revision 1.22  2002/04/23 00:26:54  jacek_poplawski

 added command line "--rotate90" for Sharp PDA

 Revision 1.21  2002/04/22 23:37:17  jacek_poplawski

 added command line options to SDL port

 Revision 1.20  2002/02/23 05:12:36  jacek_poplawski
 *** empty log message ***

 Revision 1.19  2002/02/13 08:38:39  joy
 fixed sound init (should have beeen unsigned = _U8)
 screen offset in 32bpp fixed

 Revision 1.18  2001/12/31 08:44:53  joy
 updated

 Revision 1.17  2001/12/29 21:48:05  joy
 HWSURFACE removed (I just tried something and forgot to remove it before commit)

 Revision 1.16  2001/12/29 21:34:08  joy
 fullscreen by default
 define NVIDIA if you have nVIDIA card that can't set 336x240 fullscreen
 Aflushlog() calls removed - it should be called only after the screen is back to text mode. For X11 compile it with BUFFERED_LOG disabled.
 If there's just one real joystick then SWAP_JOYSTICK is enabled automatically so that the other player can use numeric block for playing.
 STICK1 fixed.
 TRIG0 and TRIG1 fixed. Swapping of TRIGs fixed.
 joy keys must not be passed to the emulator otherwise some games will stop (RallySpeedway and others)
 dates in changelog fixed (it was December for sure)

 */
