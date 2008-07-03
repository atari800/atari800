/*
 * atari_sdl.c - SDL library specific port code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
   Thanks to David Olofson for scaling tips!

   TODO:
   - implement all Atari800 parameters
   - use mouse and real joystick
   - turn off fullscreen when error happen
*/

#include "config.h"
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

/* Atari800 includes */
#include "input.h"
#include "colours.h"
#include "monitor.h"
#include "platform.h"
#include "ui.h"
#include "screen.h"
#include "pokeysnd.h"
#include "gtia.h"
#include "antic.h"
#include "devices.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "log.h"
#include "util.h"
#include "atari_ntsc.h"
#include "pbi_proto80.h"
#include "xep80.h"
#include "xep80_fonts.h"

/* you can set that variables in code, or change it when emulator is running
   I am not sure what to do with sound_enabled (can't turn it on inside
   emulator, probably we need two variables or command line argument) */
#ifdef SOUND
#include "sound.h"
static int sound_enabled = TRUE;
static int sound_flags = 0;
static int sound_bits = 8;
#endif
static int default_bpp = 0;	/* 0 - autodetect */
static int fullscreen = 1;
static int bw = 0;
static int swap_joysticks = 0;
static int width_mode = 1;
static int rotate90 = 0;
static int ntscemu = 0;
static int scanlines_percentage = 5;
static int scanlinesnoint = FALSE;
static atari_ntsc_t *the_ntscemu;
/* making setup static conveniently clears all fields to 0 */
static atari_ntsc_setup_t atari_ntsc_setup;
#define SHORT_WIDTH_MODE 0
#define DEFAULT_WIDTH_MODE 1
#define FULL_WIDTH_MODE 2

/* joystick emulation
   keys are loaded from config file
   Here the defaults if there is no keymap in the config file... */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int kbd_joy_0_enabled = TRUE;	/* enabled by default, doesn't hurt */
int kbd_joy_1_enabled = FALSE;	/* disabled, would steal normal keys */

static int KBD_TRIG_0 = SDLK_LCTRL;
static int KBD_STICK_0_LEFT = SDLK_KP4;
static int KBD_STICK_0_RIGHT = SDLK_KP6;
static int KBD_STICK_0_DOWN = SDLK_KP2;
static int KBD_STICK_0_UP = SDLK_KP8;
static int KBD_STICK_0_LEFTUP = SDLK_KP7;
static int KBD_STICK_0_RIGHTUP = SDLK_KP9;
static int KBD_STICK_0_LEFTDOWN = SDLK_KP1;
static int KBD_STICK_0_RIGHTDOWN = SDLK_KP3;

static int KBD_TRIG_1 = SDLK_TAB;
static int KBD_STICK_1_LEFT = SDLK_a;
static int KBD_STICK_1_RIGHT = SDLK_d;
static int KBD_STICK_1_DOWN = SDLK_x;
static int KBD_STICK_1_UP = SDLK_w;
static int KBD_STICK_1_LEFTUP = SDLK_q;
static int KBD_STICK_1_RIGHTUP = SDLK_e;
static int KBD_STICK_1_LEFTDOWN = SDLK_z;
static int KBD_STICK_1_RIGHTDOWN = SDLK_c;

/* real joysticks */

static int fd_joystick0 = -1;
static int fd_joystick1 = -1;

static SDL_Joystick *joystick0 = NULL;
static SDL_Joystick *joystick1 = NULL;
static int joystick0_nbuttons;
static int joystick1_nbuttons;

#define minjoy 10000			/* real joystick tolerancy */

#ifdef SOUND
/* sound */
#define FRAGSIZE        10		/* 1<<FRAGSIZE is size of sound buffer in samples */
static int dsp_buffer_samps = (1 << FRAGSIZE);
static int dsp_buffer_bytes;
static Uint8 *dsp_buffer = NULL;
static int dsprate = 44100;
#endif

/* video */
static SDL_Surface *MainScreen = NULL;
static SDL_Color colors[256];			/* palette */
static Uint16 Palette16[256];			/* 16-bit palette */
static Uint32 Palette32[256];			/* 32-bit palette */
int Atari_xep80 = FALSE; 	/* is the XEP80 screen displayed? */

/* keyboard */
static Uint8 *kbhits;

/* For better handling of the Atari_Configure-recognition...
   Takes a keySym as integer-string and fills the value
   into the retval referentially.
   Authors: B.Schreiber, A.Martinez
   fixed and cleaned up by joy */
static int SDLKeyBind(int *retval, char *sdlKeySymIntStr)
{
	int ksym;

	if (retval == NULL || sdlKeySymIntStr == NULL) {
		return FALSE;
	}

	/* make an int out of the keySymIntStr... */
	ksym = Util_sscandec(sdlKeySymIntStr);

	if (ksym > SDLK_FIRST && ksym < SDLK_LAST) {
		*retval = ksym;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/* For getting sdl key map out of the config...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
int Atari_Configure(char *option, char *parameters)
{
	if (strcmp(option, "SDL_JOY_0_ENABLED") == 0) {
		kbd_joy_0_enabled = (parameters != NULL && parameters[0] != '0');
		return TRUE;
	}
	else if (strcmp(option, "SDL_JOY_1_ENABLED") == 0) {
		kbd_joy_1_enabled = (parameters != NULL && parameters[0] != '0');
		return TRUE;
	}
	else if (strcmp(option, "SDL_JOY_0_LEFT") == 0)
		return SDLKeyBind(&KBD_STICK_0_LEFT, parameters);
	else if (strcmp(option, "SDL_JOY_0_RIGHT") == 0)
		return SDLKeyBind(&KBD_STICK_0_RIGHT, parameters);
	else if (strcmp(option, "SDL_JOY_0_DOWN") == 0)
		return SDLKeyBind(&KBD_STICK_0_DOWN, parameters);
	else if (strcmp(option, "SDL_JOY_0_UP") == 0)
		return SDLKeyBind(&KBD_STICK_0_UP, parameters);
	else if (strcmp(option, "SDL_JOY_0_LEFTUP") == 0)
		return SDLKeyBind(&KBD_STICK_0_LEFTUP, parameters);
	else if (strcmp(option, "SDL_JOY_0_RIGHTUP") == 0)
		return SDLKeyBind(&KBD_STICK_0_RIGHTUP, parameters);
	else if (strcmp(option, "SDL_JOY_0_LEFTDOWN") == 0)
		return SDLKeyBind(&KBD_STICK_0_LEFTDOWN, parameters);
	else if (strcmp(option, "SDL_JOY_0_RIGHTDOWN") == 0)
		return SDLKeyBind(&KBD_STICK_0_RIGHTDOWN, parameters);
	else if (strcmp(option, "SDL_TRIG_0") == 0) /* obsolete */
		return SDLKeyBind(&KBD_TRIG_0, parameters);
	else if (strcmp(option, "SDL_JOY_0_TRIGGER") == 0)
		return SDLKeyBind(&KBD_TRIG_0, parameters);
	else if (strcmp(option, "SDL_JOY_1_LEFT") == 0)
		return SDLKeyBind(&KBD_STICK_1_LEFT, parameters);
	else if (strcmp(option, "SDL_JOY_1_RIGHT") == 0)
		return SDLKeyBind(&KBD_STICK_1_RIGHT, parameters);
	else if (strcmp(option, "SDL_JOY_1_DOWN") == 0)
		return SDLKeyBind(&KBD_STICK_1_DOWN, parameters);
	else if (strcmp(option, "SDL_JOY_1_UP") == 0)
		return SDLKeyBind(&KBD_STICK_1_UP, parameters);
	else if (strcmp(option, "SDL_JOY_1_LEFTUP") == 0)
		return SDLKeyBind(&KBD_STICK_1_LEFTUP, parameters);
	else if (strcmp(option, "SDL_JOY_1_RIGHTUP") == 0)
		return SDLKeyBind(&KBD_STICK_1_RIGHTUP, parameters);
	else if (strcmp(option, "SDL_JOY_1_LEFTDOWN") == 0)
		return SDLKeyBind(&KBD_STICK_1_LEFTDOWN, parameters);
	else if (strcmp(option, "SDL_JOY_1_RIGHTDOWN") == 0)
		return SDLKeyBind(&KBD_STICK_1_RIGHTDOWN, parameters);
	else if (strcmp(option, "SDL_TRIG_1") == 0) /* obsolete */
		return SDLKeyBind(&KBD_TRIG_1, parameters);
	else if (strcmp(option, "SDL_JOY_1_TRIGGER") == 0)
		return SDLKeyBind(&KBD_TRIG_1, parameters);
	else
		return FALSE;
}

/* Save the keybindings and the keybindapp options to the config file...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
void Atari_ConfigSave(FILE *fp)
{
	fprintf(fp, "SDL_JOY_0_ENABLED=%d\n", kbd_joy_0_enabled);
	fprintf(fp, "SDL_JOY_0_LEFT=%d\n", KBD_STICK_0_LEFT);
	fprintf(fp, "SDL_JOY_0_RIGHT=%d\n", KBD_STICK_0_RIGHT);
	fprintf(fp, "SDL_JOY_0_UP=%d\n", KBD_STICK_0_UP);
	fprintf(fp, "SDL_JOY_0_DOWN=%d\n", KBD_STICK_0_DOWN);
	fprintf(fp, "SDL_JOY_0_LEFTUP=%d\n", KBD_STICK_0_LEFTUP);
	fprintf(fp, "SDL_JOY_0_RIGHTUP=%d\n", KBD_STICK_0_RIGHTUP);
	fprintf(fp, "SDL_JOY_0_LEFTDOWN=%d\n", KBD_STICK_0_LEFTDOWN);
	fprintf(fp, "SDL_JOY_0_RIGHTDOWN=%d\n", KBD_STICK_0_RIGHTDOWN);
	fprintf(fp, "SDL_JOY_0_TRIGGER=%d\n", KBD_TRIG_0);

	fprintf(fp, "SDL_JOY_1_ENABLED=%d\n", kbd_joy_1_enabled);
	fprintf(fp, "SDL_JOY_1_LEFT=%d\n", KBD_STICK_1_LEFT);
	fprintf(fp, "SDL_JOY_1_RIGHT=%d\n", KBD_STICK_1_RIGHT);
	fprintf(fp, "SDL_JOY_1_UP=%d\n", KBD_STICK_1_UP);
	fprintf(fp, "SDL_JOY_1_DOWN=%d\n", KBD_STICK_1_DOWN);
	fprintf(fp, "SDL_JOY_1_LEFTUP=%d\n", KBD_STICK_1_LEFTUP);
	fprintf(fp, "SDL_JOY_1_RIGHTUP=%d\n", KBD_STICK_1_RIGHTUP);
	fprintf(fp, "SDL_JOY_1_LEFTDOWN=%d\n", KBD_STICK_1_LEFTDOWN);
	fprintf(fp, "SDL_JOY_1_RIGHTDOWN=%d\n", KBD_STICK_1_RIGHTDOWN);
	fprintf(fp, "SDL_JOY_1_TRIGGER=%d\n", KBD_TRIG_1);
}

/* used in UI to show how the keyboard joystick is mapped */
char *joy_0_description(char *buffer, int maxsize)
{
	snprintf(buffer, maxsize, " (L=%s R=%s U=%s D=%s B=%s)",
			SDL_GetKeyName((SDLKey)KBD_STICK_0_LEFT),
			SDL_GetKeyName((SDLKey)KBD_STICK_0_RIGHT),
			SDL_GetKeyName((SDLKey)KBD_STICK_0_UP),
			SDL_GetKeyName((SDLKey)KBD_STICK_0_DOWN),
			SDL_GetKeyName((SDLKey)KBD_TRIG_0)
	);
	return buffer;
}

char *joy_1_description(char *buffer, int maxsize)
{
	snprintf(buffer, maxsize, " (L=%s R=%s U=%s D=%s B=%s)",
			SDL_GetKeyName((SDLKey)KBD_STICK_1_LEFT),
			SDL_GetKeyName((SDLKey)KBD_STICK_1_RIGHT),
			SDL_GetKeyName((SDLKey)KBD_STICK_1_UP),
			SDL_GetKeyName((SDLKey)KBD_STICK_1_DOWN),
			SDL_GetKeyName((SDLKey)KBD_TRIG_1)
	);
	return buffer;
}

#ifdef SOUND

void Sound_Pause(void)
{
	if (sound_enabled) {
		/* stop audio output */
		SDL_PauseAudio(1);
	}
}

void Sound_Continue(void)
{
	if (sound_enabled) {
		/* start audio output */
		SDL_PauseAudio(0);
	}
}

void Sound_Update(void)
{
	/* fake function */
}

#endif /* SOUND */

static void SetPalette(void)
{
	SDL_SetPalette(MainScreen, SDL_LOGPAL | SDL_PHYSPAL, colors, 0, 256);
}

static void CalcPalette(void)
{
	int i, rgb;
	float y;
	Uint32 c;
	if (bw == 0)
		for (i = 0; i < 256; i++) {
			rgb = Colours_table[i];
			colors[i].r = (rgb & 0x00ff0000) >> 16;
			colors[i].g = (rgb & 0x0000ff00) >> 8;
			colors[i].b = (rgb & 0x000000ff) >> 0;
		}
	else
		for (i = 0; i < 256; i++) {
			rgb = Colours_table[i];
			y = 0.299 * ((rgb & 0x00ff0000) >> 16) +
				0.587 * ((rgb & 0x0000ff00) >> 8) +
				0.114 * ((rgb & 0x000000ff) >> 0);
			colors[i].r = (Uint8)y;
			colors[i].g = (Uint8)y;
			colors[i].b = (Uint8)y;
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

void Atari_PaletteUpdate(void)
{
	CalcPalette();
}

static void ModeInfo(void)
{
	char bwflag, fullflag, width, joyflag;
	if (bw)
		bwflag = '*';
	else
		bwflag = ' ';
	if (fullscreen)
		fullflag = '*';
	else
		fullflag = ' ';
	switch (width_mode) {
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
	if (swap_joysticks)
		joyflag = '*';
	else
		joyflag = ' ';
	Log_print("Video Mode: %dx%dx%d", MainScreen->w, MainScreen->h,
		   MainScreen->format->BitsPerPixel);
	Log_print("[%c] Full screen  [%c] BW  [%c] Width Mode  [%c] Joysticks Swapped",
		 fullflag, bwflag, width, joyflag);
}

static void SetVideoMode(int w, int h, int bpp)
{
	if (fullscreen)
		MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_FULLSCREEN);
	else
		MainScreen = SDL_SetVideoMode(w, h, bpp, SDL_RESIZABLE);
	if (MainScreen == NULL) {
		Log_print("Setting Video Mode: %dx%dx%d FAILED", w, h, bpp);
		Log_flushlog();
		exit(-1);
	}
}

static void SetNewVideoMode(int w, int h, int bpp)
{
	float ww, hh;

	if ((rotate90||ntscemu||PBI_PROTO80_enabled||Atari_xep80))
	{
		SetVideoMode(w, h, bpp);
	}
	else {

		if ((h < ATARI_HEIGHT) || (w < ATARI_WIDTH)) {
			h = ATARI_HEIGHT;
			w = ATARI_WIDTH;
		}

		/* aspect ratio, floats needed */
		ww = w;
		hh = h;
		switch (width_mode) {
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
		w = (int)ww;
		h = (int)hh;
		w = w / 8;
		w = w * 8;
		h = h / 8;
		h = h * 8;

		SetVideoMode(w, h, bpp);

		default_bpp = MainScreen->format->BitsPerPixel;
		if (bpp == 0) {
			Log_print("detected %dbpp", default_bpp);
			if ((default_bpp != 8) && (default_bpp != 16)
				&& (default_bpp != 32)) {
				Log_print("it's unsupported, so setting 8bit mode (slow conversion)");
				SetVideoMode(w, h, 8);
			}
		}
	}

	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	ModeInfo();

}

static void SwitchFullscreen(void)
{
	fullscreen = 1 - fullscreen;
	SetNewVideoMode(MainScreen->w, MainScreen->h,
					MainScreen->format->BitsPerPixel);
	Atari_DisplayScreen();
}

void Atari_SwitchXep80(void)
{
	static int saved_w, saved_h, saved_bpp;
	Atari_xep80 = 1 - Atari_xep80;
	if (Atari_xep80) {
		saved_w = MainScreen->w;
		saved_h = MainScreen->h;
		saved_bpp = MainScreen->format->BitsPerPixel;
		SetNewVideoMode(XEP80_SCRN_WIDTH, XEP80_SCRN_HEIGHT, 8);
	}
	else {
		SetNewVideoMode(saved_w, saved_h, saved_bpp);
	}
	Atari_DisplayScreen();
}

static void SwitchWidth(void)
{
	width_mode++;
	if (width_mode > FULL_WIDTH_MODE)
		width_mode = SHORT_WIDTH_MODE;
	SetNewVideoMode(MainScreen->w, MainScreen->h,
					MainScreen->format->BitsPerPixel);
	Atari_DisplayScreen();
}

static void SwitchBW(void)
{
	bw = 1 - bw;
	CalcPalette();
	SetPalette();
	ModeInfo();
}

static void SwapJoysticks(void)
{
	swap_joysticks = 1 - swap_joysticks;
	ModeInfo();
}

#ifdef SOUND
static void SoundCallback(void *userdata, Uint8 *stream, int len)
{
	int sndn = (sound_bits == 8 ? len : len/2);
	/* in mono, sndn is the number of samples (8 or 16 bit) */
	/* in stereo, 2*sndn is the number of sample pairs */
	Pokey_process(dsp_buffer, sndn);
	memcpy(stream, dsp_buffer, len);
}

static void SoundSetup(void)
{
	SDL_AudioSpec desired, obtained;
	if (sound_enabled) {
		desired.freq = dsprate;
		if (sound_bits == 8)
			desired.format = AUDIO_U8;
		else if (sound_bits == 16)
			desired.format = AUDIO_U16;
		else {
			Log_print("unknown sound_bits");
			Atari800_Exit(FALSE);
			Log_flushlog();
		}

		desired.samples = dsp_buffer_samps;
		desired.callback = SoundCallback;
		desired.userdata = NULL;
#ifdef STEREO_SOUND
		desired.channels = stereo_enabled ? 2 : 1;
#else
		desired.channels = 1;
#endif /* STEREO_SOUND*/

		if (SDL_OpenAudio(&desired, &obtained) < 0) {
			Log_print("Problem with audio: %s", SDL_GetError());
			Log_print("Sound is disabled.");
			sound_enabled = FALSE;
			return;
		}

		free(dsp_buffer);
		dsp_buffer_bytes = desired.channels*dsp_buffer_samps*(sound_bits == 8 ? 1 : 2);
		dsp_buffer = (Uint8 *)Util_malloc(dsp_buffer_bytes);
		Pokey_sound_init(FREQ_17_EXACT, dsprate, desired.channels, sound_flags);
		SDL_PauseAudio(0);
	}
}

void Sound_Reinit(void)
{
	SDL_CloseAudio();
	SoundSetup();
}

static void SoundInitialise(int *argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-audio16") == 0) {
			Log_print("audio 16bit enabled");
			sound_flags |= SND_BIT16;
			sound_bits = 16;
		}
		else if (strcmp(argv[i], "-dsprate") == 0)
			dsprate = Util_sscandec(argv[++i]);
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-sound           Enable sound");
				Log_print("\t-nosound         Disable sound");
				Log_print("\t-dsprate <rate>  Set DSP rate in Hz");
				sound_enabled = FALSE;
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	SoundSetup();
}
#endif /* SOUND */

int Atari_Keyboard(void)
{
	static int lastkey = SDLK_UNKNOWN, key_pressed = 0, key_control = 0;
 	static int lastuni = 0;
	int shiftctrl = 0;
	SDL_Event event;

	/* Very ugly fix for SDL CAPSLOCK brokenness.  This will let the user
	 * press CAPSLOCK and get a brief keypress on the Atari but it is not
	 * possible to emulate holding down CAPSLOCK for longer periods with
	 * the broken SDL*/
	if (lastkey == SDLK_CAPSLOCK) {
		lastkey = SDLK_UNKNOWN;
	   	key_pressed = 0;
 		lastuni = 0;
	}
	if (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			lastkey = event.key.keysym.sym;
 			lastuni = event.key.keysym.unicode;
			key_pressed = 1;
			break;
		case SDL_KEYUP:
			lastkey = event.key.keysym.sym;
 			lastuni = 0; /* event.key.keysym.unicode is not defined for KEYUP */
			key_pressed = 0;
			/* ugly hack to fix broken SDL CAPSLOCK*/
			/* Because SDL is only sending Keydown and keyup for every change
			 * of state of the CAPSLOCK status, rather than the actual key.*/
			if(lastkey == SDLK_CAPSLOCK) {
				key_pressed = 1;
			}
			break;
		case SDL_VIDEORESIZE:
			if (!(rotate90||ntscemu||PBI_PROTO80_enabled||Atari_xep80)) {
				SetNewVideoMode(event.resize.w, event.resize.h, MainScreen->format->BitsPerPixel);
			}
			else {
				/* resizing TODO */
				SetNewVideoMode(MainScreen->w, MainScreen->h,
					MainScreen->format->BitsPerPixel);
			}
			break;
		case SDL_QUIT:
			return AKEY_EXIT;
			break;
		}
	}
	else if (!key_pressed)
		return AKEY_NONE;

	kbhits = SDL_GetKeyState(NULL);

	if (kbhits == NULL) {
		Log_print("oops, kbhits is NULL!");
		Log_flushlog();
		exit(-1);
	}

	alt_function = -1;
	if (kbhits[SDLK_LALT]) {
		if (key_pressed) {
			switch (lastkey) {
			case SDLK_f:
				key_pressed = 0;
				SwitchFullscreen();
				break;
			case SDLK_x:
				if (key_shift && XEP80_enabled) {
					key_pressed = 0;
					Atari_SwitchXep80();
				}
				break;
			case SDLK_g:
				key_pressed = 0;
				SwitchWidth();
				break;
			case SDLK_b:
				key_pressed = 0;
				SwitchBW();
				break;
			case SDLK_j:
				key_pressed = 0;
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
			case SDLK_BACKSLASH:
				return AKEY_PBI_BB_MENU;
			default:
				if(ntscemu){
					switch(lastkey){
					case SDLK_1:
						key_pressed = 0;
						atari_ntsc_setup.sharpness-=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_2:
						key_pressed = 0;
						atari_ntsc_setup.sharpness+=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_3:
						key_pressed = 0;
						atari_ntsc_setup.saturation-=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_4:
						key_pressed = 0;
						atari_ntsc_setup.saturation+=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_5:
						key_pressed = 0;
						atari_ntsc_setup.brightness-=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_6:
						key_pressed = 0;
						atari_ntsc_setup.brightness+=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_7:
						key_pressed = 0;
						atari_ntsc_setup.contrast-=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_8:
						key_pressed = 0;
						atari_ntsc_setup.contrast+=0.1;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_9:
						key_pressed = 0;
						atari_ntsc_setup.burst_phase-=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_0:
						key_pressed = 0;
						atari_ntsc_setup.burst_phase+=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_MINUS:
						key_pressed = 0;
						atari_ntsc_setup.gaussian_factor-=.2;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_EQUALS:
						key_pressed = 0;
						atari_ntsc_setup.gaussian_factor+=.2;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_LEFTBRACKET:
						key_pressed = 0;
						scanlines_percentage -= 5*(scanlines_percentage>=5);
						Log_print("scanlines percentage:%d",scanlines_percentage);
						break;
					case SDLK_RIGHTBRACKET:
						key_pressed = 0;
						scanlines_percentage += 5*(scanlines_percentage<=100-5);
						Log_print("scanlines percentage:%d",scanlines_percentage);
						break;
					case SDLK_SEMICOLON:
						key_pressed = 0;
						atari_ntsc_setup.hue-=.01;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_QUOTE:
						key_pressed = 0;
						atari_ntsc_setup.hue+=.01;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_COMMA:
						key_pressed = 0;
						atari_ntsc_setup.gamma_adj-=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_PERIOD:
						key_pressed = 0;
						atari_ntsc_setup.gamma_adj+=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_INSERT:
						key_pressed = 0;
						atari_ntsc_setup.saturation_ramp-=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					case SDLK_DELETE:
						key_pressed = 0;
						atari_ntsc_setup.saturation_ramp+=.05;
						atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
						break;
					}
				}
			break;
			}
		}
		if (alt_function != -1) {
			key_pressed = 0;
			return AKEY_UI;
		}
	}

	/* SHIFT STATE */
	if ((kbhits[SDLK_LSHIFT]) || (kbhits[SDLK_RSHIFT]))
		key_shift = 1;
	else
		key_shift = 0;

    /* CONTROL STATE */
	if ((kbhits[SDLK_LCTRL]) || (kbhits[SDLK_RCTRL]))
		key_control = 1;
	else
		key_control = 0;

	/*
	if (event.type == 2 || event.type == 3) {
		Log_print("E:%x S:%x C:%x K:%x U:%x M:%x",event.type,key_shift,key_control,lastkey,event.key.keysym.unicode,event.key.keysym.mod);
	}
	*/

	/* OPTION / SELECT / START keys */
	key_consol = CONSOL_NONE;
	if (kbhits[SDLK_F2])
		key_consol &= (~CONSOL_OPTION);
	if (kbhits[SDLK_F3])
		key_consol &= (~CONSOL_SELECT);
	if (kbhits[SDLK_F4])
		key_consol &= (~CONSOL_START);

	if (key_pressed == 0)
		return AKEY_NONE;

	/* Handle movement and special keys. */
	switch (lastkey) {
	case SDLK_F1:
		key_pressed = 0;
		return AKEY_UI;
	case SDLK_F5:
		key_pressed = 0;
		return key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case SDLK_F8:
		key_pressed = 0;
		return (Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT);
	case SDLK_F9:
		return AKEY_EXIT;
	case SDLK_F10:
		key_pressed = 0;
		return key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT_INTERLACE;
	}

	/* keyboard joysticks: don't pass the keypresses to emulation
	 * as some games pause on a keypress (River Raid, Bruce Lee)
	 */
	if (!ui_is_active && kbd_joy_0_enabled) {
		if (lastkey == KBD_STICK_0_LEFT || lastkey == KBD_STICK_0_RIGHT ||
			lastkey == KBD_STICK_0_UP || lastkey == KBD_STICK_0_DOWN ||
			lastkey == KBD_STICK_0_LEFTUP || lastkey == KBD_STICK_0_LEFTDOWN ||
			lastkey == KBD_STICK_0_RIGHTUP || lastkey == KBD_STICK_0_RIGHTDOWN ||
			lastkey == KBD_TRIG_0) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (!ui_is_active && kbd_joy_1_enabled) {
		if (lastkey == KBD_STICK_1_LEFT || lastkey == KBD_STICK_1_RIGHT ||
			lastkey == KBD_STICK_1_UP || lastkey == KBD_STICK_1_DOWN ||
			lastkey == KBD_STICK_1_LEFTUP || lastkey == KBD_STICK_1_LEFTDOWN ||
			lastkey == KBD_STICK_1_RIGHTUP || lastkey == KBD_STICK_1_RIGHTDOWN ||
			lastkey == KBD_TRIG_1) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (key_shift)
		shiftctrl ^= AKEY_SHFT;

	if (machine_type == MACHINE_5200 && !ui_is_active) {
		if (lastkey == SDLK_F4)
			return AKEY_5200_START ^ shiftctrl;
		switch (lastuni) {
		case 'p':
			return AKEY_5200_PAUSE ^ shiftctrl;
		case 'r':
			return AKEY_5200_RESET ^ shiftctrl;
		case '0':
			return AKEY_5200_0 ^ shiftctrl;
		case '1':
			return AKEY_5200_1 ^ shiftctrl;
		case '2':
			return AKEY_5200_2 ^ shiftctrl;
		case '3':
			return AKEY_5200_3 ^ shiftctrl;
		case '4':
			return AKEY_5200_4 ^ shiftctrl;
		case '5':
			return AKEY_5200_5 ^ shiftctrl;
		case '6':
			return AKEY_5200_6 ^ shiftctrl;
		case '7':
			return AKEY_5200_7 ^ shiftctrl;
		case '8':
			return AKEY_5200_8 ^ shiftctrl;
		case '9':
			return AKEY_5200_9 ^ shiftctrl;
		case '#':
		case '=':
			return AKEY_5200_HASH ^ shiftctrl;
		case '*':
			return AKEY_5200_ASTERISK ^ shiftctrl;
		}
		return AKEY_NONE;
	}

	if (key_control)
		shiftctrl ^= AKEY_CTRL;

	switch (lastkey) {
	case SDLK_BACKQUOTE: /* fallthrough */
		/* These are the "Windows" keys, but they don't work on Windows*/
	case SDLK_LSUPER:
		return AKEY_ATARI ^ shiftctrl;
	case SDLK_RSUPER:
		if (key_shift)
			return AKEY_CAPSLOCK;
		else
			return AKEY_CAPSTOGGLE;
	case SDLK_END:
	case SDLK_F6:
		return AKEY_HELP ^ shiftctrl;
	case SDLK_PAGEDOWN:
		return AKEY_F2 | AKEY_SHFT;
	case SDLK_PAGEUP:
		return AKEY_F1 | AKEY_SHFT;
	case SDLK_HOME:
		return key_control ? AKEY_LESS|shiftctrl : AKEY_CLEAR;
	case SDLK_PAUSE:
	case SDLK_F7:
		return AKEY_BREAK;
	case SDLK_CAPSLOCK:
		if (key_shift)
			return AKEY_CAPSLOCK|shiftctrl;
		else
			return AKEY_CAPSTOGGLE|shiftctrl;
	case SDLK_SPACE:
		return AKEY_SPACE ^ shiftctrl;
	case SDLK_BACKSPACE:
		return AKEY_BACKSPACE|shiftctrl;
	case SDLK_RETURN:
		return AKEY_RETURN ^ shiftctrl;
	case SDLK_LEFT:
		return (key_shift ? AKEY_PLUS : AKEY_LEFT) ^ shiftctrl;
	case SDLK_RIGHT:
		return (key_shift ? AKEY_ASTERISK : AKEY_RIGHT) ^ shiftctrl;
	case SDLK_UP:
		return (key_shift ? AKEY_MINUS : AKEY_UP) ^ shiftctrl;
	case SDLK_DOWN:
		return (key_shift ? AKEY_EQUAL : AKEY_DOWN) ^ shiftctrl;
	case SDLK_ESCAPE:
		/* Windows takes ctrl+esc and ctrl+shift+esc */
		return AKEY_ESCAPE ^ shiftctrl;
	case SDLK_TAB:
		return AKEY_TAB ^ shiftctrl;
	case SDLK_DELETE:
		if (key_shift)
			return AKEY_DELETE_LINE|shiftctrl;
		else
			return AKEY_DELETE_CHAR;
	case SDLK_INSERT:
		if (key_shift)
			return AKEY_INSERT_LINE|shiftctrl;
		else
			return AKEY_INSERT_CHAR;
	}

	/* Handle CTRL-0 to CTRL-9 and other control characters */
	if (key_control) {
		switch(lastuni) {
		case '.':
			return AKEY_FULLSTOP|shiftctrl;
		case ',':
			return AKEY_COMMA|shiftctrl;
		case ';':
			return AKEY_SEMICOLON|shiftctrl;
		}
		switch (lastkey) {
		case SDLK_PERIOD:
			return AKEY_FULLSTOP|shiftctrl;
		case SDLK_COMMA:
			return AKEY_COMMA|shiftctrl;
		case SDLK_SEMICOLON:
			return AKEY_SEMICOLON|shiftctrl;
		case SDLK_SLASH:
			return AKEY_SLASH|shiftctrl;
		case SDLK_BACKSLASH:
			/* work-around for Windows */
			return AKEY_ESCAPE|shiftctrl;
		case SDLK_0:
			return AKEY_CTRL_0|shiftctrl;
		case SDLK_1:
			return AKEY_CTRL_1|shiftctrl;
		case SDLK_2:
			return AKEY_CTRL_2|shiftctrl;
		case SDLK_3:
			return AKEY_CTRL_3|shiftctrl;
		case SDLK_4:
			return AKEY_CTRL_4|shiftctrl;
		case SDLK_5:
			return AKEY_CTRL_5|shiftctrl;
		case SDLK_6:
			return AKEY_CTRL_6|shiftctrl;
		case SDLK_7:
			return AKEY_CTRL_7|shiftctrl;
		case SDLK_8:
			return AKEY_CTRL_8|shiftctrl;
		case SDLK_9:
			return AKEY_CTRL_9|shiftctrl;
		}
	}

	/* Host Caps Lock will make lastuni switch case, so prevent this*/
    if(lastuni>='A' && lastuni <= 'Z' && !key_shift) lastuni += 0x20;
    if(lastuni>='a' && lastuni <= 'z' && key_shift) lastuni -= 0x20;
	/* Uses only UNICODE translation, no shift states (this was added to
	 * support non-US keyboard layouts)*/
	/* input.c takes care of removing invalid shift+control keys */
	switch (lastuni) {
	case 1:
		return AKEY_CTRL_a|shiftctrl;
	case 2:
		return AKEY_CTRL_b|shiftctrl;
	case 3:
		return AKEY_CTRL_c|shiftctrl;
	case 4:
		return AKEY_CTRL_d|shiftctrl;
	case 5:
		return AKEY_CTRL_e|shiftctrl;
	case 6:
		return AKEY_CTRL_f|shiftctrl;
	case 7:
		return AKEY_CTRL_g|shiftctrl;
	case 8:
		return AKEY_CTRL_h|shiftctrl;
	case 9:
		return AKEY_CTRL_i|shiftctrl;
	case 10:
		return AKEY_CTRL_j|shiftctrl;
	case 11:
		return AKEY_CTRL_k|shiftctrl;
	case 12:
		return AKEY_CTRL_l|shiftctrl;
	case 13:
		return AKEY_CTRL_m|shiftctrl;
	case 14:
		return AKEY_CTRL_n|shiftctrl;
	case 15:
		return AKEY_CTRL_o|shiftctrl;
	case 16:
		return AKEY_CTRL_p|shiftctrl;
	case 17:
		return AKEY_CTRL_q|shiftctrl;
	case 18:
		return AKEY_CTRL_r|shiftctrl;
	case 19:
		return AKEY_CTRL_s|shiftctrl;
	case 20:
		return AKEY_CTRL_t|shiftctrl;
	case 21:
		return AKEY_CTRL_u|shiftctrl;
	case 22:
		return AKEY_CTRL_v|shiftctrl;
	case 23:
		return AKEY_CTRL_w|shiftctrl;
	case 24:
		return AKEY_CTRL_x|shiftctrl;
	case 25:
		return AKEY_CTRL_y|shiftctrl;
	case 26:
		return AKEY_CTRL_z|shiftctrl;
	case 'A':
		return AKEY_A;
	case 'B':
		return AKEY_B;
	case 'C':
		return AKEY_C;
	case 'D':
		return AKEY_D;
	case 'E':
		return AKEY_E;
	case 'F':
		return AKEY_F;
	case 'G':
		return AKEY_G;
	case 'H':
		return AKEY_H;
	case 'I':
		return AKEY_I;
	case 'J':
		return AKEY_J;
	case 'K':
		return AKEY_K;
	case 'L':
		return AKEY_L;
	case 'M':
		return AKEY_M;
	case 'N':
		return AKEY_N;
	case 'O':
		return AKEY_O;
	case 'P':
		return AKEY_P;
	case 'Q':
		return AKEY_Q;
	case 'R':
		return AKEY_R;
	case 'S':
		return AKEY_S;
	case 'T':
		return AKEY_T;
	case 'U':
		return AKEY_U;
	case 'V':
		return AKEY_V;
	case 'W':
		return AKEY_W;
	case 'X':
		return AKEY_X;
	case 'Y':
		return AKEY_Y;
	case 'Z':
		return AKEY_Z;
	case ':':
		return AKEY_COLON;
	case '!':
		return AKEY_EXCLAMATION;
	case '@':
		return AKEY_AT;
	case '#':
		return AKEY_HASH;
	case '$':
		return AKEY_DOLLAR;
	case '%':
		return AKEY_PERCENT;
	case '^':
		return AKEY_CARET;
	case '&':
		return AKEY_AMPERSAND;
	case '*':
		return AKEY_ASTERISK;
	case '(':
		return AKEY_PARENLEFT;
	case ')':
		return AKEY_PARENRIGHT;
	case '+':
		return AKEY_PLUS;
	case '_':
		return AKEY_UNDERSCORE;
	case '"':
		return AKEY_DBLQUOTE;
	case '?':
		return AKEY_QUESTION;
	case '<':
		return AKEY_LESS;
	case '>':
		return AKEY_GREATER;
	case 'a':
		return AKEY_a;
	case 'b':
		return AKEY_b;
	case 'c':
		return AKEY_c;
	case 'd':
		return AKEY_d;
	case 'e':
		return AKEY_e;
	case 'f':
		return AKEY_f;
	case 'g':
		return AKEY_g;
	case 'h':
		return AKEY_h;
	case 'i':
		return AKEY_i;
	case 'j':
		return AKEY_j;
	case 'k':
		return AKEY_k;
	case 'l':
		return AKEY_l;
	case 'm':
		return AKEY_m;
	case 'n':
		return AKEY_n;
	case 'o':
		return AKEY_o;
	case 'p':
		return AKEY_p;
	case 'q':
		return AKEY_q;
	case 'r':
		return AKEY_r;
	case 's':
		return AKEY_s;
	case 't':
		return AKEY_t;
	case 'u':
		return AKEY_u;
	case 'v':
		return AKEY_v;
	case 'w':
		return AKEY_w;
	case 'x':
		return AKEY_x;
	case 'y':
		return AKEY_y;
	case 'z':
		return AKEY_z;
	case ';':
		return AKEY_SEMICOLON;
	case '0':
		return AKEY_0;
	case '1':
		return AKEY_1;
	case '2':
		return AKEY_2;
	case '3':
		return AKEY_3;
	case '4':
		return AKEY_4;
	case '5':
		return AKEY_5;
	case '6':
		return AKEY_6;
	case '7':
		return AKEY_7;
	case '8':
		return AKEY_8;
	case '9':
		return AKEY_9;
	case ',':
		return AKEY_COMMA;
	case '.':
		return AKEY_FULLSTOP;
	case '=':
		return AKEY_EQUAL;
	case '-':
		return AKEY_MINUS;
	case '\'':
		return AKEY_QUOTE;
	case '/':
		return AKEY_SLASH;
	case '\\':
		return AKEY_BACKSLASH;
	case '[':
		return AKEY_BRACKETLEFT;
	case ']':
		return AKEY_BRACKETRIGHT;
	case '|':
		return AKEY_BAR;
	}

	return AKEY_NONE;
}

static void Init_SDL_Joysticks(int first, int second)
{
	if (first) {
		joystick0 = SDL_JoystickOpen(0);
		if (joystick0 == NULL)
			Log_print("joystick 0 not found");
		else {
			Log_print("joystick 0 found!");
			joystick0_nbuttons = SDL_JoystickNumButtons(joystick0);
			swap_joysticks = 1;		/* real joy is STICK(0) and numblock is STICK(1) */
		}
	}

	if (second) {
		joystick1 = SDL_JoystickOpen(1);
		if (joystick1 == NULL)
			Log_print("joystick 1 not found");
		else {
			Log_print("joystick 1 found!");
			joystick1_nbuttons = SDL_JoystickNumButtons(joystick1);
		}
	}
}

static void Init_Joysticks(int *argc, char *argv[])
{
#ifdef LPTJOY
	char *lpt_joy0 = NULL;
	char *lpt_joy1 = NULL;
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (!strcmp(argv[i], "-joy0")) {
			if (i == *argc - 1) {
				Log_print("joystick device path missing!");
				break;
			}
			lpt_joy0 = argv[++i];
		}
		else if (!strcmp(argv[i], "-joy1")) {
			if (i == *argc - 1) {
				Log_print("joystick device path missing!");
				break;
			}
			lpt_joy1 = argv[++i];
		}
		else {
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (lpt_joy0 != NULL) {				/* LPT1 joystick */
		fd_joystick0 = open(lpt_joy0, O_RDONLY);
		if (fd_joystick0 == -1)
			perror(lpt_joy0);
	}
	if (lpt_joy1 != NULL) {				/* LPT2 joystick */
		fd_joystick1 = open(lpt_joy1, O_RDONLY);
		if (fd_joystick1 == -1)
			perror(lpt_joy1);
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
	bpp = default_bpp;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-ntscemu") == 0) {
			ntscemu = TRUE;
			width = 640;
			height = 480;
			bpp = 16;
			/* allocate memory for atari_ntsc and initialize */
			the_ntscemu = (atari_ntsc_t*) malloc( sizeof (atari_ntsc_t) );
			Log_print("ntscemu mode");
		}
		else if (strcmp(argv[i], "-scanlines") == 0) {
			scanlines_percentage  = Util_sscandec(argv[++i]);
			Log_print("scanlines percentage set");
		}
		else if (strcmp(argv[i], "-scanlinesnoint") == 0) {
			scanlinesnoint = TRUE;
			Log_print("scanlines interpolation disabled");
		}
		else if (strcmp(argv[i], "-rotate90") == 0) {
			rotate90 = 1;
			width = 240;
			height = 320;
			bpp = 16;
			no_joystick = 1;
			Log_print("rotate90 mode");
		}
		else if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = 1;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-width") == 0) {
			width = Util_sscandec(argv[++i]);
			Log_print("width set", width);
		}
		else if (strcmp(argv[i], "-height") == 0) {
			height = Util_sscandec(argv[++i]);
			Log_print("height set");
		}
		else if (strcmp(argv[i], "-bpp") == 0) {
			bpp = Util_sscandec(argv[++i]);
			Log_print("bpp set");
		}
		else if (strcmp(argv[i], "-fullscreen") == 0) {
			fullscreen = 1;
		}
		else if (strcmp(argv[i], "-windowed") == 0) {
			fullscreen = 0;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-ntscemu         Emulate NTSC composite video (640x480x16)");
				Log_print("\t-scanlines       Specify scanlines percentage (ntscemu only)");
				Log_print("\t-scanlinesnoint  Disable scanlines interpolation (ntscemu only)");
				Log_print("\t-rotate90        Display 240x320 screen");
				Log_print("\t-nojoystick      Disable joystick");
#ifdef LPTJOY
				Log_print("\t-joy0 <pathname> Select LPTjoy0 device");
				Log_print("\t-joy1 <pathname> Select LPTjoy0 device");
#endif /* LPTJOY */
				Log_print("\t-width <num>     Host screen width");
				Log_print("\t-height <num>    Host screen height");
				Log_print("\t-bpp <num>       Host color depth");
				Log_print("\t-fullscreen      Run fullscreen");
				Log_print("\t-windowed        Run in window");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	if (ntscemu || help_only){
			ATARI_NTSC_DEFAULTS_Initialise(argc, argv, &atari_ntsc_setup);
			if(ntscemu) atari_ntsc_init( the_ntscemu, &atari_ntsc_setup );
	}

	i = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
#ifdef SOUND
	if (!help_only)
		i |= SDL_INIT_AUDIO;
#endif
#ifdef WIN32
	/*Windows SDL version 1.2.10+ uses windib as the default, but it is slower*/
	if (getenv("SDL_VIDEODRIVER")==NULL) {
#ifdef __STRICT_ANSI__
		extern int putenv(char *string); /* suppress -ansi -pedantic warning */
#endif
		putenv("SDL_VIDEODRIVER=directx");
	}
#endif
	if (SDL_Init(i) != 0) {
		Log_print("SDL_Init FAILED");
		Log_print(SDL_GetError());
		Log_flushlog();
		exit(-1);
	}
	atexit(SDL_Quit);

	/* SDL_WM_SetIcon("/usr/local/atari800/atarixe.ICO"), NULL); */
	SDL_WM_SetCaption(ATARI_TITLE, "Atari800");

#ifdef SOUND
	SoundInitialise(argc, argv);
#endif

	if (help_only)
		return;		/* return before changing the gfx mode */

	/* Prototype 80 column adaptor */
	if (PBI_PROTO80_enabled) {
		width = 640;
		height = 400;
		bpp = 16;
		Log_print("proto80 mode");
	}

	SetNewVideoMode(width, height, bpp);
	CalcPalette();
	SetPalette();

	Log_print("video initialized");

	if (no_joystick == 0)
		Init_Joysticks(argc, argv);

	SDL_EnableUNICODE(1);
}

int Atari_Exit(int run_monitor)
{
	int restart;
	int original_fullscreen = fullscreen;

	if (run_monitor) {
		/* disable graphics, set alpha mode */
		if (fullscreen) {
			SwitchFullscreen();
		}
#ifdef SOUND
		Sound_Pause();
#endif
		restart = monitor();
#ifdef SOUND
		Sound_Continue();
#endif
	}
	else {
		restart = FALSE;
	}

	if (restart) {
		/* set up graphics and all the stuff */
		if (original_fullscreen != fullscreen) {
			SwitchFullscreen();
		}
		return 1;
	}

	SDL_Quit();

	Log_flushlog();

	return restart;
}
/* License of scanLines_16():*/
/* This function has been altered from its original version */
/* This license is a verbatim copy of the license of ZLib 
 * http://www.gnu.org/licenses/license-list.html#GPLCompatibleLicenses
 * This is a free software license, and compatible with the GPL. */
/*****************************************************************************
 ** Original Source: /cvsroot/bluemsx/blueMSX/Src/VideoRender/VideoRender.c,v 
 **
 ** Original Revision: 1.25 
 **
 ** Original Date: 2006/01/17 08:49:34 
 **
 ** More info: http://www.bluemsx.com
 **
 ** Copyright (C) 2003-2004 Daniel Vik
 **
 **  This software is provided 'as-is', without any express or implied
 **  warranty.  In no event will the authors be held liable for any damages
 **  arising from the use of this software.
 **
 **  Permission is granted to anyone to use this software for any purpose,
 **  including commercial applications, and to alter it and redistribute it
 **  freely, subject to the following restrictions:
 **
 **  1. The origin of this software must not be misrepresented; you must not
 **     claim that you wrote the original software. If you use this software
 **     in a product, an acknowledgment in the product documentation would be
 **     appreciated but is not required.
 **  2. Altered source versions must be plainly marked as such, and must not be
 **     misrepresented as being the original software.
 **  3. This notice may not be removed or altered from any source distribution.
 **
 ******************************************************************************
 */

static void scanLines_16(void* pBuffer, int width, int height, int pitch, int scanLinesPct)
{
    Uint32* pBuf = (Uint32*)(pBuffer)+pitch/sizeof(Uint32);
	Uint32* sBuf = (Uint32*)(pBuffer);
    int w, h;
	static int prev_scanLinesPct;

    pitch = pitch * 2 / (int)sizeof(Uint32);
    height /= 2;
    width /= 2;

	if (scanLinesPct < 0) scanLinesPct = 0;
	if (scanLinesPct > 100) scanLinesPct = 100;

    if (scanLinesPct == 100) {
        if (prev_scanLinesPct != 100) {
			/*clean dirty blank scanlines*/
			prev_scanLinesPct = 100;
			for (h = 0; h < height; h++) {
				memset(pBuf, 0, width * sizeof(Uint32));
				pBuf += pitch;
			}
		}
		return;
    }
	prev_scanLinesPct = scanLinesPct;


    if (scanLinesPct == 0) {
	/* fill in blank scanlines */
		for (h = 0; h < height; h++) {
			memcpy(pBuf, sBuf, width * sizeof(Uint32));
			sBuf += pitch;
			pBuf += pitch;
		}
		return;
    }
    scanLinesPct = (100-scanLinesPct) * 32 / 100;

    for (h = 0; h < height; h++) {
		for (w = 0; w < width; w++) {
			Uint32 pixel = sBuf[w];
			Uint32 a = (((pixel & 0x07e0f81f) * scanLinesPct) & 0xfc1f03e0) >> 5;
			Uint32 b = (((pixel >> 5) & 0x07c0f83f) * scanLinesPct) & 0xf81f07e0;
			pBuf[w] = a | b;
		}
		sBuf += pitch;
		pBuf += pitch;
	}
}

/* Modified version of the above, which uses interpolation (slower but better)*/
static void scanLines_16_interp(void* pBuffer, int width, int height, int pitch, int scanLinesPct)
{
    Uint32* pBuf = (Uint32*)(pBuffer)+pitch/sizeof(Uint32);
	Uint32* sBuf = (Uint32*)(pBuffer);
	Uint32* tBuf = (Uint32*)(pBuffer)+pitch*2/sizeof(Uint32);
    int w, h;
	static int prev_scanLinesPct;

    pitch = pitch * 2 / (int)sizeof(Uint32);
    height /= 2;
    width /= 2;

	if (scanLinesPct < 0) scanLinesPct = 0;
	if (scanLinesPct > 100) scanLinesPct = 100;

    if (scanLinesPct == 100) {
        if (prev_scanLinesPct != 100) {
			/*clean dirty blank scanlines*/
			prev_scanLinesPct = 100;
			for (h = 0; h < height; h++) {
				memset(pBuf, 0, width * sizeof(Uint32));
				pBuf += pitch;
			}
		}
		return;
    }
	prev_scanLinesPct = scanLinesPct;


    if (scanLinesPct == 0) {
	/* fill in blank scanlines */
		for (h = 0; h < height; h++) {
			memcpy(pBuf, sBuf, width * sizeof(Uint32));
			sBuf += pitch;
			pBuf += pitch;
		}
		return;
    }
    scanLinesPct = (100-scanLinesPct) * 32 / 200;

    for (h = 0; h < height-1; h++) {
		for (w = 0; w < width; w++) {
			Uint32 pixel = sBuf[w];
			Uint32 pixel2 = tBuf[w];
			Uint32 a = ((((pixel & 0x07e0f81f)+(pixel2 & 0x07e0f81f)) * scanLinesPct) & 0xfc1f03e0) >> 5;
			Uint32 b = ((((pixel >> 5) & 0x07c0f83f)+((pixel2 >> 5) & 0x07c0f83f)) * scanLinesPct) & 0xf81f07e0;
			pBuf[w] = a | b;
		}
		sBuf += pitch;
		tBuf += pitch;
		pBuf += pitch;
	}
}

static void DisplayXEP80(UBYTE *screen)
{
	static int xep80Frame = 0;
	Uint32 *start32;
	int pitch4;
	int i;
	xep80Frame++;
	if (xep80Frame == 60) xep80Frame = 0;
	if (xep80Frame > 29) {
		screen = XEP80_screen_1;
	}
	else {
		screen = XEP80_screen_2;
	}

	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	i = MainScreen->h;
	while (i > 0) {
		memcpy(start32, screen, XEP80_SCRN_WIDTH);
		screen += XEP80_SCRN_WIDTH;
		start32 += pitch4;
		i--;
	}
}

static void DisplayNTSCEmu640x480(UBYTE *screen)
{
	/* Number of overscan lines not shown */
	enum { overscan_lines = 24 };
	/* Change to 0 to clip the 8-pixel overscan borders off */
	enum { overscan = 1 };

	/* Atari active pixel area */
	enum { atari_width = overscan ? atari_ntsc_full_in_width : atari_ntsc_min_in_width };
	enum { atari_height = 240 -overscan_lines };

	/* Output size */
	enum { width = overscan ? atari_ntsc_full_out_width : atari_ntsc_min_out_width };
	enum { height = atari_height * 2 };
	enum { left_border_adj = ((640 - width)/2) & 0xfffffffc };
	int const raw_width = ATARI_WIDTH; /* raw image has extra data */

	int jumped = 24;
	unsigned short *pixels = (unsigned short*)MainScreen->pixels + overscan_lines / 2 * MainScreen->pitch + left_border_adj;
	/* blit atari image, doubled vertically */
	atari_ntsc_blit( the_ntscemu, screen + jumped + overscan_lines / 2 * ATARI_WIDTH, raw_width, width, height / 2, pixels, MainScreen->pitch * 2 );
	
	if (!scanlinesnoint) {
		scanLines_16_interp((void *)pixels, width, height, MainScreen->pitch, scanlines_percentage);
	} else {
		scanLines_16((void *)pixels, width, height, MainScreen->pitch, scanlines_percentage);
	}
}

static void DisplayProto80640x400(UBYTE *screen)
{
	UWORD white = 0xffff;
	UWORD black = 0x0000;
	int skip = MainScreen->pitch - 80*8;
	Uint16 *start16 = (Uint16 *) MainScreen->pixels;

	int scanline, column;
	UBYTE pixels;
	for (scanline = 0; scanline < 8*24; scanline++) {
		for (column = 0; column < 80; column++) {
			int i;
			pixels = PBI_PROTO80_Pixels(scanline,column);
			for (i = 0; i < 8; i++) {
				if (pixels & 0x80) {
					*start16++ = white;
				}
				else {
					*start16++ = black;
				}
				pixels <<= 1;
			}
		}
		start16 += skip;
	}
	scanLines_16_interp((void *)MainScreen->pixels, 640, 400, MainScreen->pitch, scanlines_percentage);
}

static void DisplayRotated240x320(Uint8 *screen)
{
	int i, j;
	register Uint32 *start32;
	if (MainScreen->format->BitsPerPixel != 16) {
		Log_print("rotated display works only for bpp=16 right now");
		Log_flushlog();
		exit(-1);
	}
	start32 = (Uint32 *) MainScreen->pixels;
	for (j = 0; j < MainScreen->h; j++)
		for (i = 0; i < MainScreen->w / 2; i++) {
			Uint8 left = screen[ATARI_WIDTH * (i * 2) + 32 + 320 - j];
			Uint8 right = screen[ATARI_WIDTH * (i * 2 + 1) + 32 + 320 - j];
#ifdef WORDS_BIGENDIAN
			*start32++ = (Palette16[left] << 16) + Palette16[right];
#else
			*start32++ = (Palette16[right] << 16) + Palette16[left];
#endif
		}
}

static void DisplayWithoutScaling(Uint8 *screen, int jumped, int width)
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
		Log_print("unsupported color depth %d", MainScreen->format->BitsPerPixel);
		Log_print("please set default_bpp to 8 or 16 and recompile atari_sdl");
		Log_flushlog();
		exit(-1);
	}
}

static void DisplayWithScaling(Uint8 *screen, int jumped, int width)
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
		Log_print("unsupported color depth %d", MainScreen->format->BitsPerPixel);
		Log_print("please set default_bpp to 8 or 16 or 32 and recompile atari_sdl");
		Log_flushlog();
		exit(-1);
	}
}

void Atari_DisplayScreen(void)
{
	int width, jumped;

	switch (width_mode) {
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
		Log_print("unsupported width_mode");
		Log_flushlog();
		exit(-1);
		break;
	}
	if (Atari_xep80) {
		DisplayXEP80((UBYTE *)Screen_atari);
	}
	else if (ntscemu) {
  		DisplayNTSCEmu640x480((UBYTE *)Screen_atari);
  	}
	else if (PBI_PROTO80_enabled) {
  		DisplayProto80640x400((UBYTE *)Screen_atari);
  	}
  	else if (rotate90) {
		DisplayRotated240x320((UBYTE *) Screen_atari);
	}
	else if (MainScreen->w == width && MainScreen->h == ATARI_HEIGHT) {
		DisplayWithoutScaling((UBYTE *) Screen_atari, jumped, width);
	}
	else {
		DisplayWithScaling((UBYTE *) Screen_atari, jumped, width);
	}
	SDL_Flip(MainScreen);
}

static int get_SDL_joystick_state(SDL_Joystick *joystick)
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
	}
	else if (x < -minjoy) {
		if (y < -minjoy)
			return STICK_UL;
		else if (y > minjoy)
			return STICK_LL;
		else
			return STICK_LEFT;
	}
	else {
		if (y < -minjoy)
			return STICK_FORWARD;
		else if (y > minjoy)
			return STICK_BACK;
		else
			return STICK_CENTRE;
	}
}

static int get_LPT_joystick_state(int fd)
{
#ifdef LPTJOY
	int status;

	ioctl(fd, LPGETSTATUS, &status);
	status ^= 0x78;

	if (status & 0x40) {			/* right */
		if (status & 0x10) {		/* up */
			return STICK_UR;
		}
		else if (status & 0x20) {	/* down */
			return STICK_LR;
		}
		else {
			return STICK_RIGHT;
		}
	}
	else if (status & 0x80) {		/* left */
		if (status & 0x10) {		/* up */
			return STICK_UL;
		}
		else if (status & 0x20) {	/* down */
			return STICK_LL;
		}
		else {
			return STICK_LEFT;
		}
	}
	else {
		if (status & 0x10) {		/* up */
			return STICK_FORWARD;
		}
		else if (status & 0x20) {	/* down */
			return STICK_BACK;
		}
		else {
			return STICK_CENTRE;
		}
	}
#else
	return 0;
#endif /* LPTJOY */
}

static void get_Atari_PORT(Uint8 *s0, Uint8 *s1)
{
	int stick0, stick1;
	stick0 = stick1 = STICK_CENTRE;

	if (kbd_joy_0_enabled) {
		if (kbhits[KBD_STICK_0_LEFT])
			stick0 = STICK_LEFT;
		if (kbhits[KBD_STICK_0_RIGHT])
			stick0 = STICK_RIGHT;
		if (kbhits[KBD_STICK_0_UP])
			stick0 = STICK_FORWARD;
		if (kbhits[KBD_STICK_0_DOWN])
			stick0 = STICK_BACK;
		if ((kbhits[KBD_STICK_0_LEFTUP])
			|| ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_UP])))
			stick0 = STICK_UL;
		if ((kbhits[KBD_STICK_0_LEFTDOWN])
			|| ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_DOWN])))
			stick0 = STICK_LL;
		if ((kbhits[KBD_STICK_0_RIGHTUP])
			|| ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_UP])))
			stick0 = STICK_UR;
		if ((kbhits[KBD_STICK_0_RIGHTDOWN])
			|| ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_DOWN])))
			stick0 = STICK_LR;
	}
	if (kbd_joy_1_enabled) {
		if (kbhits[KBD_STICK_1_LEFT])
			stick1 = STICK_LEFT;
		if (kbhits[KBD_STICK_1_RIGHT])
			stick1 = STICK_RIGHT;
		if (kbhits[KBD_STICK_1_UP])
			stick1 = STICK_FORWARD;
		if (kbhits[KBD_STICK_1_DOWN])
			stick1 = STICK_BACK;
		if ((kbhits[KBD_STICK_1_LEFTUP])
			|| ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_UP])))
			stick1 = STICK_UL;
		if ((kbhits[KBD_STICK_1_LEFTDOWN])
			|| ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_DOWN])))
			stick1 = STICK_LL;
		if ((kbhits[KBD_STICK_1_RIGHTUP])
			|| ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_UP])))
			stick1 = STICK_UR;
		if ((kbhits[KBD_STICK_1_RIGHTDOWN])
			|| ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_DOWN])))
			stick1 = STICK_LR;
	}

	if (swap_joysticks) {
		*s1 = stick0;
		*s0 = stick1;
	}
	else {
		*s0 = stick0;
		*s1 = stick1;
	}

	if ((joystick0 != NULL) || (joystick1 != NULL))	/* can only joystick1!=NULL ? */
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

static void get_Atari_TRIG(Uint8 *t0, Uint8 *t1)
{
	int trig0, trig1, i;
	trig0 = trig1 = 1;

	if (kbd_joy_0_enabled) {
		trig0 = kbhits[KBD_TRIG_0] ? 0 : 1;
	}

	if (kbd_joy_1_enabled) {
		trig1 = kbhits[KBD_TRIG_1] ? 0 : 1;
	}

	if (swap_joysticks) {
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
	}
	else if (joystick0 != NULL) {
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
	}
	else if (joystick1 != NULL) {
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

int Atari_PORT(int num)
{
#ifndef DONT_DISPLAY
	if (num == 0) {
		UBYTE a, b;
		get_Atari_PORT(&a, &b);
		return (b << 4) | (a & 0x0f);
	}
#endif
	return 0xff;
}

int Atari_TRIG(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b;
	get_Atari_TRIG(&a, &b);
	switch (num) {
	case 0:
		return a;
	case 1:
		return b;
	default:
		break;
	}
#endif
	return 1;
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		key_code = Atari_Keyboard();
		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen();
	}
}

/*
vim:ts=4:sw=4:
*/
