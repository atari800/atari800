/* 
   SDL port of Atari800
   Jacek Poplawski <jacekp@linux.com.pl>

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
   - fix memory related bugs (probably not in my code)
   - implement all Atari800 parameters
   - fix audio
   - use mouse and real joystick
   - turn off fullscreen when error happen
   - rewrite keyboard stuff, switch/case is bad(slow) idea
   - Atari_Exit - monitor stuff (debugger?)
   
   USAGE:
   - for now you need makefile, please download:
     ftp://ftp.sophics.cz/pub/Atari800/src/Makefile.SDL
     then "make -f Makefile.SDL"
   - you can turn off sound by changing sound_enabled to 0  
   - you can switch between fullscreen/window mode with LALT+f
   - you can switch between color/bw mode with LALT+b - FEEL THE POWER OF BW
     MONITOR!
   - you can swap joysticks with LALT+j  
   - fullscreen switching probably doesn't work in Windows, you need to set
     variable in source code

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

// you can set that variables in code, or change it when emulator is running
// I am not sure what to do with sound_enabled (can't turn it on inside
// emulator, probably we need two variables or command line argument)
static int sound_enabled = 1;
static int FULLSCREEN = 0;
static int BW = 0;
static int SWAP_JOYSTICKS = 0;

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

// I still don't know when it's not 1
extern int refresh_rate;

// SOUND 
#define FRAGSIZE        10		// 1<<FRAGSIZE is size of sound buffer
static int SOUND_VOLUME = SDL_MIX_MAXVOLUME / 4;
#define dsprate 44100

// VIDEO
SDL_Surface *MainScreen = NULL;
SDL_Color colors[256];			// palette

// KEYBOARD
Uint8 *kbhits;
static int last_key_break = 0;
static int last_key_code = AKEY_NONE;

// main loop variable (TODO: kill all that global variables!) 
int done = 0;

void SetPalette()
{
	SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);
}

void CalcPalette()
{
	int i, rgb;
	float y;
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

}


void SetVideoMode(int w, int h)
{
	if (FULLSCREEN)
		MainScreen = SDL_SetVideoMode(w, h, 8, SDL_FULLSCREEN);
	else
		MainScreen = SDL_SetVideoMode(w, h, 8, SDL_RESIZABLE);

	if (MainScreen == NULL) {
		printf("Setting Video Mode: %ix%ix8 FAILED", w, h);
		exit(-1);
	}

	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	// hide mouse cursor 
}

void SetNewVideoMode(int w, int h)
{
	if ((h < ATARI_HEIGHT) || (w < ATARI_WIDTH - 2 * 24)) {
		h = ATARI_HEIGHT;
		w = ATARI_WIDTH - 2 * 24;
	}

	// aspect ratio
	if (w / 1.4 < h)
		h = w / 1.4;
	else
		w = h * 1.4;
	w = w / 8;
	w = w * 8;
	h = h / 8;
	h = h * 8;
	if (MainScreen != NULL) {
		if ((h == MainScreen->h) && (w == MainScreen->w))
			return;
	};

	SetVideoMode(w, h);
}

void SwitchFullscreen()
{
	FULLSCREEN = 1 - FULLSCREEN;
	SetVideoMode(MainScreen->w, MainScreen->h);
	Atari_DisplayScreen((UBYTE *) atari_screen);
}

void SwitchBW()
{
	BW = 1 - BW;
	CalcPalette();
	SetVideoMode(MainScreen->w, MainScreen->h);
}

void SwapJoysticks()
{
	SWAP_JOYSTICKS = 1 - SWAP_JOYSTICKS;
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
			printf("Problem with audio: %s\n", SDL_GetError());
			printf("You can disable sound by setting sound_enabled=0\n");
			exit(-1);
		}

		// mono
		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);
	}
	else
		printf
			("Audio is off, you can turn it on by setting sound_enabled=1\n");
}

int Atari_Keyboard(void)
{
	static int lastkey = AKEY_NONE, key_pressed = 0;

	SDL_Event event;
	kbhits = SDL_GetKeyState(NULL);
	if (kbhits == NULL) {
		printf("oops, kbhits is NULL!\n");
		exit(-1);
	}
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			lastkey = event.key.keysym.sym;
			key_pressed = 1;
			if ((lastkey == SDLK_f) && (kbhits[SDLK_LALT])) {
				SwitchFullscreen();
			}
			if ((lastkey == SDLK_b) && (kbhits[SDLK_LALT])) {
				SwitchBW();
			}
			if ((lastkey == SDLK_j) && (kbhits[SDLK_LALT])) {
				SwapJoysticks();
			}
			break;
		case SDL_KEYUP:
			key_pressed = 0;
			break;
		case SDL_VIDEORESIZE:
			SetNewVideoMode(event.resize.w, event.resize.h);
			break;
		case SDL_QUIT:
			done = 1;
			break;
		}
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

	// TODO: when you push two keys, and release one - key_pressed will be
	// 0, so it will return AKEY_NONE, but key will be pressed!
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

	// don't care about shift
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
	default:
		return AKEY_NONE;
	}
}

void Atari_Initialise(int *argc, char *argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		printf("SDL_Init FAILED\n");
		exit(-1);
	}

	CalcPalette();

	SetVideoMode(ATARI_WIDTH - 2 * 24, ATARI_HEIGHT);

	SDL_Sound_Initialise(argc, argv);

	printf
		("please report SDL port bugs to Jacek Poplawski <jacekp@linux.com.pl>\n");

}

int Atari_Exit(int run_monitor)
{
	int restart;
	restart = FALSE;

	SDL_Quit();

	return restart;
}

void Atari_DisplayScreen(UBYTE * screen)
{
	register Uint32 quad;
	register int x;
	register int dx;
	register int yy;
	register Uint8 *ss;
	int i;
	int y;
	int dy;
	int w, h;
	int w4;
	int pos;
	int pitch4;
	Uint32 *start32;

	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	if ((MainScreen->w == ATARI_WIDTH - 2 * 24)
		&& (MainScreen->h == ATARI_HEIGHT)) {
		// no scaling
		screen = screen + 24;
		i = MainScreen->h;
		while (i > 0) {
			memcpy(start32, screen, ATARI_WIDTH - 2 * 24);
			screen += ATARI_WIDTH;
			start32 += pitch4;
			i--;
		}
	}
	else {
		// optimized scaling (I analized gcc -S output, please remember
		// about gcc options, I use:
		// " -O2 -march=k6 -fomit-frame-pointer ")
		// it works only for 8bit surfaces, I am not sure - maybe it's
		// worth to have 16bit? SDL converts it anyway

		w = (ATARI_WIDTH - 2 * 24) << 16;
		h = (ATARI_HEIGHT) << 16;
		dx = w / MainScreen->w;
		dy = h / MainScreen->h;
		w4 = MainScreen->w / 4 - 1;
		ss = screen;

		y = (0) << 16;
		i = MainScreen->h;
		while (i > 0) {
			x = (ATARI_WIDTH - 24) << 16;
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

// two joysticks at once, it's just for testing, should be written better (more
// joysticks, mouse/real joystick support, etc)

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
}

void SDL_Atari_TRIG(Uint8 * t1, Uint8 * t2)
{
	if ((kbhits[SDL_TRIG_0]) || (kbhits[SDL_TRIG_0_B]))
		*t1 = 0;
	else
		*t1 = 1;
	if ((kbhits[SDL_TRIG_1]) || (kbhits[SDL_TRIG_1_B]))
		*t2 = 0;
	else
		*t2 = 1;
}

int main(int argc, char **argv)
{
	int keycode;
	static UBYTE STICK[4];

	if (!Atari800_Initialise(&argc, argv))
		return 3;

	refresh_rate = 1;			// ;-)  

	if (sound_enabled)
		SDL_PauseAudio(0);
	while (!done) {
		keycode = Atari_Keyboard();

		switch (keycode) {
		case AKEY_EXIT:
			Atari800_Exit(FALSE);
			return 0;
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

	}
	return 0;
}
