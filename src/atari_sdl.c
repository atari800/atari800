
/* 
   SDL port of Atari800
   Jacek Poplawski <jacekp@linux.com.pl>

   history:
   
   15-11-2001 - fullscreen switching, scaling only with ratio >= 1.0, more
                keyboard updates  
   14-11-2001 - scaling with any ratio supported, keyboard updates, aspect ratio
   13-11-2001 - audio and better keyboard 
   12-11-2001 - video and joystick emulation
   
   TODO:
   - fix very strange segfaults (when adding/removing code)
   - implement all Atari800 parameters
   - fix audio
   - define joystick(s) emulation keys, use mouse and real joystick
   - turn off fullscreen when error happen
   - Atari_Exit - monitor stuff (debugger?)
   
   USAGE:
   - for now you need makefile, please download:
     ftp://ftp.sophics.cz/pub/Atari800/src/Makefile.SDL
     then "make -f Makefile.SDL"
   - you can turn off sound by changing sound_enabled to 0  
   - you can switch between fullscree/window mode with LALT+f
   - fullscreen switching probably doesn't work in Windows, you need to set
     variable in source code

*/

static int sound_enabled = 1;
int FULLSCREEN = 0;

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "input.h"
#include "colours.h"
#include "platform.h"
#include "ui.h"
#include "ataripcx.h"
#include "pokeysnd.h"

extern int refresh_rate;

// SOUND 
#define FRAGSIZE        10
#define SOUND_VOLUME SDL_MIX_MAXVOLUME/4
static int dsprate = 44100;

// VIDEO
SDL_Surface *MainScreen = NULL;

// KEYBOARD
Uint8 *kbhits;
extern int alt_function;

int done = 0;

void SetVideoMode(int w, int h)
{
	int i, rgb;
	SDL_Color colors[256];		// palette
	printf("Setting Video Mode: %ix%ix8  ", w, h);
	if (FULLSCREEN)
		MainScreen =
			SDL_SetVideoMode(w, h, 8, SDL_FULLSCREEN | SDL_HWPALETTE);
	else
		MainScreen =
			SDL_SetVideoMode(w, h, 8, SDL_RESIZABLE | SDL_HWPALETTE);

	if (MainScreen == NULL) {
		printf("[failed]\n");
		exit(-1);
	}
	printf("[OK]\n");

	for (i = 0; i < 256; i++) {
		rgb = colortable[i];
		colors[i].r = (rgb & 0x00ff0000) >> 16;
		colors[i].g = (rgb & 0x0000ff00) >> 8;
		colors[i].b = (rgb & 0x000000ff) >> 0;
	}

	SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);

	SDL_ShowCursor(SDL_DISABLE);	// hide mouse cursor 
}

void SetNewVideoMode(int w, int h)
{
	if (MainScreen != NULL) {
		if ((h == MainScreen->h) && (w == MainScreen->w))
			return;
	}
	if ((h < ATARI_HEIGHT) || (w < ATARI_WIDTH - 2 * 24)) {
		h = ATARI_HEIGHT;
		w = ATARI_WIDTH - 2 * 24;
	}

	w = w / 8;
	w = w * 8;
	if (w / 1.4 < h)
		h = w / 1.4;
	else
		w = h * 1.4;

	SetVideoMode(w, h);

}

void SwitchFullscreen()
{
	FULLSCREEN = 1 - FULLSCREEN;
	SetVideoMode(MainScreen->w, MainScreen->h);
}

void SDL_Sound_Update(void *userdata, Uint8 * stream, int len)
{
	unsigned char dsp_buffer[1 << FRAGSIZE];
	Pokey_process(dsp_buffer, sizeof(dsp_buffer));
	SDL_MixAudio(stream, dsp_buffer, sizeof(dsp_buffer), SOUND_VOLUME);
}

void SDL_Sound_Initialise(int *argc, char *argv[])
{
	SDL_AudioSpec *desired, *obtained;
	obtained = NULL;
	printf("SDL_Sound_Initialise           ");
	if (sound_enabled) {
		desired = (SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));
		desired->freq = dsprate;
		desired->format = AUDIO_S8;
		desired->samples = 1 << FRAGSIZE;
		desired->callback = SDL_Sound_Update;
		desired->userdata = NULL;
		desired->channels = 1;

		if (SDL_OpenAudio(desired, obtained) < 0) {
			printf("[failed] \nCouldn't open audio: %s\n", SDL_GetError());
			exit(-1);
		}

		// mono
		Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);
		printf("[OK]\n");
	}
	else
		printf("[skipped]\n");
}

int Atari_Keyboard(void)
{
	static int lastkey, key_pressed = 0;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			lastkey = event.key.keysym.sym;
			key_pressed = 1;
			if ((lastkey == SDLK_f) && (kbhits[SDLK_LALT])) {
				SwitchFullscreen();
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

	kbhits = SDL_GetKeyState(NULL);
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

	alt_function = -1;
	if (kbhits[SDLK_LALT]) {
		return AKEY_NONE;
	}

	// really stupid piece of code, I think it should be an array, not switch/case
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
//          return AKEY_;    <-- what is that?
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
		}

	// don't care about shift
	switch (lastkey) {
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

	printf("SDL_Init                       ");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
		printf("[OK]\n");
	else {
		printf("[failed]\n");
		exit(-1);
	}

	SetVideoMode(ATARI_WIDTH - 2 * 24, ATARI_HEIGHT);

	SDL_Sound_Initialise(argc, argv);

}

int Atari_Exit(int run_monitor)
{
	int restart;
	restart = FALSE;

	printf("bye bye\n");
	SDL_Quit();

	return restart;
}

void Atari_DisplayScreen(UBYTE * screen)
{
	int i, j;
	int x, y;
	int dx, dy;
	int w = 0, h = 0;
	char c;

	Uint8 *pos, *start;

	start = MainScreen->pixels;
	SDL_LockSurface(MainScreen);

	if ((w == MainScreen->w) && (h == MainScreen->h)) {
		// no scaling
		screen = screen + 24;
		for (j = 0; j < MainScreen->h; j++) {
			memcpy(start, screen, ATARI_WIDTH - 2 * 24);
			screen = screen + 2 * 24;
			start = start + ATARI_WIDTH;
		}
	}
	else
		// with scaling 
	{

		w = (ATARI_WIDTH - 2 * 24) << 16;
		h = (ATARI_HEIGHT) << 16;
		dx = w / MainScreen->w;
		dy = h / MainScreen->h;

		y = (0) << 16;
		for (j = 0; j < MainScreen->h; j++) {
			x = (24) << 16;
			pos = start;
			for (i = 0; i < MainScreen->w; i++) {
				c = screen[ATARI_WIDTH * (y >> 16) + (x >> 16)];
				*pos = c;
				x = x + dx;
				pos = pos + 1;
			}
			start = start + MainScreen->pitch;
			y = y + dy;
		}

	}

	SDL_UnlockSurface(MainScreen);
	SDL_Flip(MainScreen);
}

int Atari_PORT(int num)
{
	// only joystick 0 for now
	if (num == 0) {
		// arrows only for now
		// TODO: array with joystick emulation keys
		if ((kbhits[SDLK_KP7])
			|| ((kbhits[SDLK_KP4]) && (kbhits[SDLK_KP8])))
			return ~1 & ~4;
		if ((kbhits[SDLK_KP9])
			|| ((kbhits[SDLK_KP8]) && (kbhits[SDLK_KP6])))
			return ~1 & ~8;
		if ((kbhits[SDLK_KP1])
			|| ((kbhits[SDLK_KP4]) && (kbhits[SDLK_KP2])))
			return ~2 & ~4;
		if ((kbhits[SDLK_KP3])
			|| ((kbhits[SDLK_KP2]) && (kbhits[SDLK_KP6])))
			return ~2 & ~8;
		if (kbhits[SDLK_KP4])
			return ~4;
		if (kbhits[SDLK_KP8])
			return ~1;
		if (kbhits[SDLK_KP6])
			return ~8;
		if (kbhits[SDLK_KP2])
			return ~2;
		return 15;
	}

	return 0xff;
}

int Atari_TRIG(int num)
{
	// only joystick 0 and left ctrl as fire
	if (num == 0) {
		if (kbhits[SDLK_LCTRL])
			return 0;
		else
			return 1;
	}
	return 1;
}

int main(int argc, char **argv)
{
	int frametimer;
	int keycode;

	if (!Atari800_Initialise(&argc, argv))
		return 3;

	frametimer = 0;
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
			break;
		default:
			key_break = 0;
			key_code = keycode;
			break;
		}

		frametimer++;
		if (frametimer == refresh_rate) {
			Atari800_Frame(EMULATE_FULL);
			atari_sync();		// TODO: slow computers -> no sync
			Atari_DisplayScreen((UBYTE *) atari_screen);
			frametimer = 0;
		}
		else {
			Atari800_Frame(EMULATE_BASIC);
			atari_sync();
		}
	}
	return 0;
}
