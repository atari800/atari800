/* 
   SDL port of Atari800
   Jacek Poplawski <jacekp@linux.com.pl>

   06-11-2001 - replaced SHORTER_MODE with WIDTH_MODE, now there are 3 states:
		DEFAULT_WIDTH_MODE (ATARI_WIDTH-2*24), SHORTER_WIDTH_MODE
		(ATARI_WIDTH-2*24-2*8), and FULL_WIDTH_MODE (ATARI_WIDTH)
	      - checking if bpp is supported (8,16,32 only), if not - switching
		to 8bit	
	      - moved fps stuff to CountFPS		
	      - moved "bug reports" message up, so user could see that when
		init fails
   04-11-2001 - split Atari_DisplayScreen (DisplayWithoutScaling and
                DisplayWithScaling)
	      - put Aflushlog everywhere	
   03-11-2001 - 32 bit support
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
#include "memory-d.h"
#include "pia.h"
#include "log.h"

// you can set that variables in code, or change it when emulator is running
// I am not sure what to do with sound_enabled (can't turn it on inside
// emulator, probably we need two variables or command line argument)
static int sound_enabled = 1;
static int SDL_ATARI_BPP = 0;	// 0 - autodetect
static int FULLSCREEN = 0;
static int BW = 0;
static int SWAP_JOYSTICKS = 0;
static int WIDTH_MODE = 1;
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

SDL_Joystick *joystick0, *joystick1;
int joystick0_nbuttons, joystick1_nbuttons;

#define minjoy 10000			// real joystick tolerancy

extern int refresh_rate;

extern int alt_function;

// sound 
#define FRAGSIZE        10		// 1<<FRAGSIZE is size of sound buffer
static int SOUND_VOLUME = SDL_MIX_MAXVOLUME / 4;
#define dsprate 44100

// video
SDL_Surface *MainScreen = NULL;
SDL_Color colors[256];			// palette
Uint16 Palette16[256];			// 16-bit palette
Uint32 Palette32[256];			// 32-bit palette
#define CurrentBPP -1

// keyboard
Uint8 *kbhits;
static int last_key_break = 0;
static int last_key_code = AKEY_NONE;

int Sound_Update(void) { return 0; }	// fake function

void SetPalette()
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
	Aflushlog();
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

	if (bpp == CurrentBPP)
		bpp = MainScreen->format->BitsPerPixel;

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
		Aflushlog();
	}

	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	// hide mouse cursor 

	ModeInfo();

}

void SwitchFullscreen()
{
	FULLSCREEN = 1 - FULLSCREEN;
	SetNewVideoMode(MainScreen->w, MainScreen->h, CurrentBPP);
	Atari_DisplayScreen((UBYTE *) atari_screen);
}

void SwitchWidth()
{
	WIDTH_MODE++;
	if (WIDTH_MODE > FULL_WIDTH_MODE)
		WIDTH_MODE = SHORT_WIDTH_MODE;
	SetNewVideoMode(MainScreen->w, MainScreen->h, CurrentBPP);
	Atari_DisplayScreen((UBYTE *) atari_screen);
}

void SwitchBPP()
{
	SDL_ATARI_BPP *= 2;
	if (SDL_ATARI_BPP > 32)
		SDL_ATARI_BPP = 8;
	SetNewVideoMode(MainScreen->w, MainScreen->h, SDL_ATARI_BPP);
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

void SDL_Sound_Update(void *userdata, Uint8 * stream, int len)
{
	Uint8 dsp_buffer[1 << FRAGSIZE];
	if (len > 1 << FRAGSIZE)
		len = 1 << FRAGSIZE;
	Pokey_process(dsp_buffer, len);
	SDL_MixAudio(stream, dsp_buffer, len, SOUND_VOLUME);
}

void SDL_Sound_Initialise(int *argc, char *argv[])
{
	SDL_AudioSpec desired, obtained;
	if (sound_enabled) {
		desired.freq = dsprate;
		desired.format = AUDIO_S8;
		desired.samples = 1 << FRAGSIZE;
		desired.callback = SDL_Sound_Update;
		desired.userdata = NULL;
		desired.channels = 1;

		if (SDL_OpenAudio(&desired, &obtained) < 0) {
			Aprint("Problem with audio: %s", SDL_GetError());
			Aprint("You can disable sound by setting sound_enabled=0");
			Aflushlog();
			exit(-1);
		}

		// mono
		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);
		Aprint("sound initialized");
		Aflushlog();
	}
	else {
		Aprint
			("Audio is off, you can turn it on by setting sound_enabled=1");
		Aflushlog();
	}
}

int Atari_Keyboard(void)
{
	static int lastkey = AKEY_NONE, key_pressed = 0;

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
			SetNewVideoMode(event.resize.w, event.resize.h, CurrentBPP);
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
			case SDLK_e:
				SwitchBPP();
				break;
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
//      case SDLK_6:
//          return AKEY_;    ^ <-- what is that?
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
		return AKEY_TAB;
	}
	return AKEY_NONE;
}

void Init_Joysticks()
{
	joystick0 = SDL_JoystickOpen(0);
	if (joystick0 == NULL)
		Aprint("joystick 0 not found");
	else {
		Aprint("joystick 0 found!");
		joystick0_nbuttons = SDL_JoystickNumButtons(joystick0);
	}
	joystick1 = SDL_JoystickOpen(1);
	if (joystick1 == NULL)
		Aprint("joystick 1 not found");
	else {
		Aprint("joystick 1 found!");
		joystick1_nbuttons = SDL_JoystickNumButtons(joystick1);
	}
	Aflushlog();
}

void Atari_Initialise(int *argc, char *argv[])
{
	Aprint
		("please report SDL port bugs to Jacek Poplawski <jacekp@linux.com.pl>");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
		Aprint("SDL_Init FAILED");
		Aprint(SDL_GetError());
		Aflushlog();
		exit(-1);
	}

	SetNewVideoMode(ATARI_WIDTH - 2 * 24, ATARI_HEIGHT, SDL_ATARI_BPP);
	CalcPalette();
	SetPalette();

	Aprint("video initialized");

	SDL_Sound_Initialise(argc, argv);

	Init_Joysticks();

}

int Atari_Exit(int run_monitor)
{
	int restart;
	restart = FALSE;

	SDL_Quit();

	return restart;
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
	int w2, w4;
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
			pos = w2;
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

void SDL_Atari_PORT(Uint8 * s0, Uint8 * s1)
{
	int stick0, stick1;
	int x, y;
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
	if (joystick0 != NULL) {
		x = SDL_JoystickGetAxis(joystick0, 0);
		y = SDL_JoystickGetAxis(joystick0, 1);
		if (x > minjoy) {
			if (y < -minjoy)
				stick0 = STICK_UR;
			else if (y > minjoy)
				stick0 = STICK_LR;
			else
				stick0 = STICK_RIGHT;
		}
		else if (x < -minjoy) {
			if (y < -minjoy)
				stick0 = STICK_UL;
			else if (y > minjoy)
				stick0 = STICK_LL;
			else
				stick0 = STICK_LEFT;
		}
		else {
			if (y < -minjoy)
				stick0 = STICK_FORWARD;
			else if (y > minjoy)
				stick0 = STICK_BACK;
			else
				stick0 = STICK_CENTRE;
		}
		*s0 = stick0;
	}
	if (joystick1 != NULL) {
		x = SDL_JoystickGetAxis(joystick1, 0);
		y = SDL_JoystickGetAxis(joystick1, 1);
		if (x > minjoy) {
			if (y < -minjoy)
				stick0 = STICK_UR;
			else if (y > minjoy)
				stick0 = STICK_LR;
			else
				stick0 = STICK_RIGHT;
		}
		else if (x < -minjoy) {
			if (y < -minjoy)
				stick0 = STICK_UL;
			else if (y > minjoy)
				stick0 = STICK_LL;
			else
				stick0 = STICK_LEFT;
		}
		else {
			if (y < -minjoy)
				stick0 = STICK_FORWARD;
			else if (y > minjoy)
				stick0 = STICK_BACK;
			else
				stick0 = STICK_CENTRE;
		}
		*s1 = stick1;
	}

}

void SDL_Atari_TRIG(Uint8 * t0, Uint8 * t1)
{
	int trig0, trig1, i;

	if (joystick0 == NULL) {
		if ((kbhits[SDL_TRIG_0]) || (kbhits[SDL_TRIG_0_B]))
			trig0 = 0;
		else
			trig0 = 1;
	}
	else {
		trig0 = 1;
		for (i = 0; i < joystick0_nbuttons; i++) {
			if (SDL_JoystickGetButton(joystick0, i))
				trig0 = 0;
		}
	}
	if (joystick1 == NULL) {
		if ((kbhits[SDL_TRIG_1]) || (kbhits[SDL_TRIG_1_B]))
			trig1 = 0;
		else
			trig1 = 1;
	}
	else {
		trig1 = 1;
		for (i = 0; i < joystick1_nbuttons; i++) {
			if (SDL_JoystickGetButton(joystick1, i))
				trig1 = 0;
		}
	}


	if (SWAP_JOYSTICKS) {
		*t1 = trig0;
		*t0 = trig1;
	}
	else {
		*t0 = trig0;
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
		Aflushlog();
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

	refresh_rate = 1;			// ;-)  

	if (sound_enabled)
		SDL_PauseAudio(0);
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
			if (sound_enabled)
				SDL_PauseAudio(1);
			ui((UBYTE *) atari_screen);
			if (sound_enabled)
				SDL_PauseAudio(0);
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
				IRQST &= ~0x80;
				if (IRQEN & 0x80) {
					GenerateIRQ();
				}
				break;
		default:
				key_break = 0;
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
				IRQST &= ~0x40;
				if (IRQEN & 0x40) {
					GenerateIRQ();
				}
			}
		}

		SDL_Atari_PORT(&STICK[0], &STICK[1]);
		SDL_Atari_TRIG(&TRIG[0], &TRIG[1]);

		PORT_input[0] = (STICK[1] << 4) | STICK[0];

		Device_Frame();
		GTIA_Frame();
		ANTIC_Frame(TRUE);
		LED_Frame();
		POKEY_Frame();
		nframes++;
		atari_sync();
		Atari_DisplayScreen((UBYTE *) atari_screen);
#ifdef FPS_COUNTER
		CountFPS();
#endif
	}
	Atari800_Exit(FALSE);
	return 0;
}
