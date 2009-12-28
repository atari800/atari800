/*
 * atari_sdl.c - SDL library specific port code
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2009 Atari800 development team (see DOC/CREDITS)
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
   - turn off fullscreen when error happen
*/

#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __linux__
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
#include "akey.h"
#include "colours.h"
#include "colours_ntsc.h"
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
#include "filter_ntsc.h"
#include "pbi_proto80.h"
#include "xep80.h"
#include "xep80_fonts.h"
#include "af80.h"

/* you can set that variables in code, or change it when emulator is running
   I am not sure what to do with sound_enabled (can't turn it on inside
   emulator, probably we need two variables or command line argument) */
#ifdef SOUND
#include "sound.h"
#include "mzpokeysnd.h"
static int sound_enabled = TRUE;
static int sound_flags = POKEYSND_BIT16;
static int sound_bits = 16;
#endif
static int fullscreen = 1;
static int grab_mouse = 0;
static int bw = 0;
static int swap_joysticks = 0;
static int width_mode = 1;
static int scanlines_percentage = 5;
static int scanlinesnoint = FALSE;
static atari_ntsc_t *the_ntscemu = NULL;
#define SHORT_WIDTH_MODE 0
#define DEFAULT_WIDTH_MODE 1
#define FULL_WIDTH_MODE 2

/* joystick emulation
   keys are loaded from config file
   Here the defaults if there is no keymap in the config file... */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int PLATFORM_kbd_joy_0_enabled = TRUE;	/* enabled by default, doesn't hurt */
int PLATFORM_kbd_joy_1_enabled = FALSE;	/* disabled, would steal normal keys */

static int KBD_TRIG_0 = SDLK_RCTRL;
static int KBD_STICK_0_LEFT = SDLK_KP4;
static int KBD_STICK_0_RIGHT = SDLK_KP6;
static int KBD_STICK_0_DOWN = SDLK_KP5;
static int KBD_STICK_0_UP = SDLK_KP8;
static int KBD_TRIG_1 = SDLK_LCTRL;
static int KBD_STICK_1_LEFT = SDLK_a;
static int KBD_STICK_1_RIGHT = SDLK_d;
static int KBD_STICK_1_DOWN = SDLK_s;
static int KBD_STICK_1_UP = SDLK_w;

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
#define FRAGSIZE        10		/* 1<<FRAGSIZE is the maximum size of a sound fragment in samples */
static int frag_samps = (1 << FRAGSIZE);
static int dsp_buffer_bytes;
static Uint8 *dsp_buffer = NULL;
static int dsprate = 44100;
#ifdef SYNCHRONIZED_SOUND
/* latency (in ms) and thus target buffer size */
static int snddelay = 20;
/* allowed "spread" between too many and too few samples in the buffer (ms) */
static int sndspread = 7;
/* estimated gap */
static int gap_est = 0;
/* cumulative audio difference */
static double avg_gap;
/* dsp_write_pos, dsp_read_pos and callbacktick are accessed in two different threads: */
static int dsp_write_pos;
static int dsp_read_pos;
/* tick at which callback occured */
static int callbacktick = 0;
#endif

#endif

/* video */
static SDL_Surface *MainScreen = NULL;
static SDL_Color colors[256];			/* palette */
static Uint16 Palette16[256];			/* 16-bit palette */
static Uint32 Palette32[256];			/* 32-bit palette */
int PLATFORM_show_80 = FALSE; 	/* is the 80 column screen displayed? */
enum PLATFORM_filter_t PLATFORM_filter = PLATFORM_FILTER_NONE;

/* SDL port supports 5 "display modes":
   - normal,
   - rotated 9 degrees,
   - ntscemu - emulation of NTSC composite video,
   - xep80 emulation,
   - proto80 - emulation of 80 column board for 1090XL.
   The structure below is used to hold settings and functions used by a single
   display mode. */
struct display_mode_data_t {
	int w;
	int h;
	int bpp;
	void (*set_video_mode_func)(int, int); /* see below */
	void (*display_screen_func)(Uint8 *); /* see below */
};

/* set_video_mode_func - these functions reset the SDL screen to the given
   width and height. SetNewVideoModeNormal remembers the given W and H
   parameters and sets the screen accordingly, while SetNewVideoModeIgnore
   ignores W and H and sets the screen according to default parameters defined
   in the DISPLAY_MODES table below. */
static void SetNewVideoModeNormal(int w, int h);
static void SetNewVideoModeIgnore(int w, int h);

/* display_screen_func - these functions fill the given SCREEN buffer with
   screen data, differently for each display mode. */
static void DisplayNormal(Uint8 *screen);
static void DisplayRotated240x320(Uint8 *screen);
static void DisplayNTSCEmu640x480(Uint8 *screen);
static void DisplayXEP80(Uint8 *screen);
static void DisplayProto80640x400(Uint8 *screen);
static void DisplayAF80640x500(Uint8 *screen);

/* This table holds default settings for all display modes. */
static struct display_mode_data_t display_modes[] = {
	/* Normal */
	{ Screen_WIDTH, Screen_HEIGHT, 0, /* bpp = 0 - autodetect */
	  &SetNewVideoModeNormal, &DisplayNormal },
	/* Rotated */
	{ 240, 320, 16,
	  &SetNewVideoModeIgnore, &DisplayRotated240x320 },
	/* NTSCEmu */
	{ 640, 480, ATARI_NTSC_OUT_DEPTH,
	  &SetNewVideoModeIgnore, &DisplayNTSCEmu640x480 },
	/* XEP80 */
	{ XEP80_SCRN_WIDTH, XEP80_SCRN_HEIGHT, 8,
	  &SetNewVideoModeIgnore, &DisplayXEP80 },
	/* Proto80 */
	{ 640, 400, 16,
	  &SetNewVideoModeIgnore, &DisplayProto80640x400 },
	/* AF80 */
	{ 640, 500, 16,
	  &SetNewVideoModeIgnore, &DisplayAF80640x500 }
};

/* An enumerator to switch display modes comfortably. */
enum display_mode_t {
	display_normal,
	display_rotated,
	display_ntscemu,
	display_xep80,
	display_proto80,
	display_af80
};
/* Indicates current display mode */
static enum display_mode_t current_display_mode = display_normal;
/* Display mode which should be reverted to, when XEP80 mode is turned off */
static enum display_mode_t xep80_return_display_mode = display_normal;

/* keyboard */
static Uint8 *kbhits;

/* For better handling of the PLATFORM_Configure-recognition...
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
int PLATFORM_Configure(char *option, char *parameters)
{
	if (strcmp(option, "SDL_JOY_0_ENABLED") == 0) {
		PLATFORM_kbd_joy_0_enabled = (parameters != NULL && parameters[0] != '0');
		return TRUE;
	}
	else if (strcmp(option, "SDL_JOY_1_ENABLED") == 0) {
		PLATFORM_kbd_joy_1_enabled = (parameters != NULL && parameters[0] != '0');
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
	else if (strcmp(option, "SDL_JOY_1_TRIGGER") == 0)
		return SDLKeyBind(&KBD_TRIG_1, parameters);
	else
		return FALSE;
}

/* Save the keybindings and the keybindapp options to the config file...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
void PLATFORM_ConfigSave(FILE *fp)
{
	fprintf(fp, "SDL_JOY_0_ENABLED=%d\n", PLATFORM_kbd_joy_0_enabled);
	fprintf(fp, "SDL_JOY_0_LEFT=%d\n", KBD_STICK_0_LEFT);
	fprintf(fp, "SDL_JOY_0_RIGHT=%d\n", KBD_STICK_0_RIGHT);
	fprintf(fp, "SDL_JOY_0_UP=%d\n", KBD_STICK_0_UP);
	fprintf(fp, "SDL_JOY_0_DOWN=%d\n", KBD_STICK_0_DOWN);
	fprintf(fp, "SDL_JOY_0_TRIGGER=%d\n", KBD_TRIG_0);

	fprintf(fp, "SDL_JOY_1_ENABLED=%d\n", PLATFORM_kbd_joy_1_enabled);
	fprintf(fp, "SDL_JOY_1_LEFT=%d\n", KBD_STICK_1_LEFT);
	fprintf(fp, "SDL_JOY_1_RIGHT=%d\n", KBD_STICK_1_RIGHT);
	fprintf(fp, "SDL_JOY_1_UP=%d\n", KBD_STICK_1_UP);
	fprintf(fp, "SDL_JOY_1_DOWN=%d\n", KBD_STICK_1_DOWN);
	fprintf(fp, "SDL_JOY_1_TRIGGER=%d\n", KBD_TRIG_1);
}

void PLATFORM_SetJoystickKey(int joystick, int direction, int value)
{
	if (joystick == 0) {
		switch(direction) {
			case 0: KBD_STICK_0_LEFT = value; break;
			case 1: KBD_STICK_0_UP = value; break;
			case 2: KBD_STICK_0_RIGHT = value; break;
			case 3: KBD_STICK_0_DOWN = value; break;
			case 4: KBD_TRIG_0 = value; break;
		}
	}
	else {
		switch(direction) {
			case 0: KBD_STICK_1_LEFT = value; break;
			case 1: KBD_STICK_1_UP = value; break;
			case 2: KBD_STICK_1_RIGHT = value; break;
			case 3: KBD_STICK_1_DOWN = value; break;
			case 4: KBD_TRIG_1 = value; break;
		}
	}
}

void PLATFORM_GetJoystickKeyName(int joystick, int direction, char *buffer, int bufsize)
{
	char *key = "";
	switch(direction) {
		case 0: key = SDL_GetKeyName((SDLKey)(joystick == 0 ? KBD_STICK_0_LEFT : KBD_STICK_1_LEFT));
			break;
		case 1: key = SDL_GetKeyName((SDLKey)(joystick == 0 ? KBD_STICK_0_UP : KBD_STICK_1_UP));
			break;
		case 2: key = SDL_GetKeyName((SDLKey)(joystick == 0 ? KBD_STICK_0_RIGHT : KBD_STICK_1_RIGHT));
			break;
		case 3: key = SDL_GetKeyName((SDLKey)(joystick == 0 ? KBD_STICK_0_DOWN : KBD_STICK_1_DOWN));
			break;
		case 4: key = SDL_GetKeyName((SDLKey)(joystick == 0 ? KBD_TRIG_0 : KBD_TRIG_1));
			break;
	}
#ifdef HAVE_SNPRINTF
	snprintf(buffer, bufsize, "%11s", key);
#else
	sprintf(buffer, "%11s", key);
#endif
	buffer[bufsize-1] = '\0';
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

#ifdef SYNCHRONIZED_SOUND
/* returns a factor (1.0 by default) to adjust the speed of the emulation
 * so that if the sound buffer is too full or too empty, the emulation
 * slows down or speeds up to match the actual speed of sound output */
double PLATFORM_AdjustSpeed(void)
{
	int bytes_per_sample = (POKEYSND_stereo_enabled ? 2 : 1)*((sound_bits == 16) ? 2:1);

	double alpha = 2.0/(1.0+40.0);
	int gap_too_small;
	int gap_too_large;
	static int inited = FALSE;

	if (!inited) {
		inited = TRUE;
		avg_gap = gap_est;
	}
	else {
		avg_gap = avg_gap + alpha * (gap_est - avg_gap);
	}

	gap_too_small = (snddelay*dsprate*bytes_per_sample)/1000;
	gap_too_large = ((snddelay+sndspread)*dsprate*bytes_per_sample)/1000;
	if (avg_gap < gap_too_small) {
		double speed = 0.95;
		return speed;
	}
	if (avg_gap > gap_too_large) {
		double speed = 1.05;
		return speed;
	}
	return 1.0;
}
#endif /* SYNCHRONIZED_SOUND */

void Sound_Update(void)
{
#ifdef SYNCHRONIZED_SOUND
	int bytes_written = 0;
	int samples_written;
	int gap;
	int newpos;
	int bytes_per_sample;
	double bytes_per_ms;

	if (!sound_enabled) return;
	/* produce samples from the sound emulation */
	samples_written = MZPOKEYSND_UpdateProcessBuffer();
	bytes_per_sample = (POKEYSND_stereo_enabled ? 2 : 1)*((sound_bits == 16) ? 2:1);
	bytes_per_ms = (bytes_per_sample)*(dsprate/1000.0);
	bytes_written = (sound_bits == 8 ? samples_written : samples_written*2);
	SDL_LockAudio();
	/* this is the gap as of the most recent callback */
	gap = dsp_write_pos - dsp_read_pos;
	/* an estimation of the current gap, adding time since then */
	if (callbacktick != 0) {
		gap_est = gap - (bytes_per_ms)*(SDL_GetTicks() - callbacktick);
	}
	/* if there isn't enough room... */
	while (gap + bytes_written > dsp_buffer_bytes) {
		/* then we allow the callback to run.. */
		SDL_UnlockAudio();
		/* and delay until it runs and allows space. */
		SDL_Delay(1);
		SDL_LockAudio();
		/*printf("sound buffer overflow:%d %d\n",gap, dsp_buffer_bytes);*/
		gap = dsp_write_pos - dsp_read_pos;
	}
	/* now we copy the data into the buffer and adjust the positions */
	newpos = dsp_write_pos + bytes_written;
	if (newpos/dsp_buffer_bytes == dsp_write_pos/dsp_buffer_bytes) {
		/* no wrap */
		memcpy(dsp_buffer+(dsp_write_pos%dsp_buffer_bytes), MZPOKEYSND_process_buffer, bytes_written);
	}
	else {
		/* wraps */
		int first_part_size = dsp_buffer_bytes - (dsp_write_pos%dsp_buffer_bytes);
		memcpy(dsp_buffer+(dsp_write_pos%dsp_buffer_bytes), MZPOKEYSND_process_buffer, first_part_size);
		memcpy(dsp_buffer, MZPOKEYSND_process_buffer+first_part_size, bytes_written-first_part_size);
	}
	dsp_write_pos = newpos;
	if (callbacktick == 0) {
		/* Sound callback has not yet been called */
		dsp_read_pos += bytes_written;
	}
	if (dsp_write_pos < dsp_read_pos) {
		/* should not occur */
		printf("Error: dsp_write_pos < dsp_read_pos\n");
	}
	while (dsp_read_pos > dsp_buffer_bytes) {
		dsp_write_pos -= dsp_buffer_bytes;
		dsp_read_pos -= dsp_buffer_bytes;
	}
	SDL_UnlockAudio();
#else /* SYNCHRONIZED_SOUND */
	/* fake function */
#endif /* SYNCHRONIZED_SOUND */
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

void PLATFORM_PaletteUpdate(void)
{
	CalcPalette();
	if (current_display_mode == display_ntscemu)
		FILTER_NTSC_Update(the_ntscemu);
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

static void SetNewVideoModeNormal(int w, int h)
{
	int bpp = display_modes[current_display_mode].bpp;
	float ww, hh;

	if ((h < Screen_HEIGHT) || (w < Screen_WIDTH)) {
		h = Screen_HEIGHT;
		w = Screen_WIDTH;
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
	w /= 8;
	w *= 8;
	h /= 8;
	h *= 8;

	display_modes[current_display_mode].w = w;
	display_modes[current_display_mode].h = h;

	/* Only the BPPs below are supported. Default to 8 bits otherwise. */
	if (bpp != 0 && bpp != 8 && bpp != 16 && bpp != 32) {
		Log_print("BPP %d unsupported, so setting 8bit mode (slow conversion)", bpp);
		bpp = 8;
	}
	SetVideoMode(w, h, bpp);
	if (bpp == 0) {
		bpp = MainScreen->format->BitsPerPixel;
		Log_print("detected %dbpp", bpp);
		if ((bpp != 8) && (bpp != 16)
			&& (bpp != 32)) {
			Log_print("it's unsupported, so setting 8bit mode (slow conversion)");
			bpp = 8;
			SetVideoMode(w, h, 8);
		}
	}

	/* Save bpp in case it changed above */
	display_modes[current_display_mode].bpp = bpp;
}

static void SetNewVideoModeIgnore(int w, int h)
{
	SetVideoMode(display_modes[current_display_mode].w,
	             display_modes[current_display_mode].h,
	             display_modes[current_display_mode].bpp);
}

/* Reinitialises the SDL screen using current_display_mode and its parameters.
 */
static void ResetDisplay(void)
{
	(*display_modes[current_display_mode].set_video_mode_func)(display_modes[current_display_mode].w,
	                                                           display_modes[current_display_mode].h);
	CalcPalette();
	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	ModeInfo();
}

/* Resizes the SDL screen using current_display_mode and the given Width and
   Height. */
static void ResizeDisplay(int w, int h)
{
	(*display_modes[current_display_mode].set_video_mode_func)(w, h);
	SetPalette();

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	ModeInfo();
}

static void SwitchFullscreen(void)
{
	fullscreen = 1 - fullscreen;
	ResetDisplay();
	PLATFORM_DisplayScreen();
}

void PLATFORM_SetFilter(const enum PLATFORM_filter_t filter)
{
	/* Mode to return when filter is turned off */
	static enum display_mode_t saved_display_mode;

	/* When the display is in XEP80 mode, the filter should not be
	   switched on/off immediately. It should be switched only when
	   XEP80 mode is turned off. The variable below indicates which is
	   the case. */
	int change_later = (current_display_mode == display_xep80);

	/* Currently switching filter makes no sense when the emulator
	   is running in PROTO80 mode. */
	if (current_display_mode == display_proto80)
		return;

	PLATFORM_filter = filter;

	if (filter == PLATFORM_FILTER_NONE && the_ntscemu != NULL) {
		/* Turning filter off */
		FILTER_NTSC_Delete(the_ntscemu);
		the_ntscemu = NULL;
		if (change_later)
			xep80_return_display_mode = saved_display_mode;
		else {
			current_display_mode = saved_display_mode;
			ResetDisplay();
		}
	}
	else if (filter == PLATFORM_FILTER_NTSC && the_ntscemu == NULL) {
		/* Turning filter on */
		the_ntscemu = FILTER_NTSC_New();
		FILTER_NTSC_Update(the_ntscemu);
		if (change_later) {
			saved_display_mode = xep80_return_display_mode;
			xep80_return_display_mode = display_ntscemu;
		} else {
			saved_display_mode = current_display_mode;
			current_display_mode = display_ntscemu;
			ResetDisplay();
		}
	}
}

void PLATFORM_Switch80(void)
{
	PLATFORM_show_80 = 1 - PLATFORM_show_80;
	if (PLATFORM_show_80) {
		xep80_return_display_mode = current_display_mode;
		if (AF80_enabled) {
			current_display_mode = display_af80;
		} else if (PBI_PROTO80_enabled) {
			current_display_mode = display_proto80;
		} else if (XEP80_enabled) {
			current_display_mode = display_xep80;
		}
	}
	else
		current_display_mode = xep80_return_display_mode;
	ResetDisplay();
	PLATFORM_DisplayScreen();
}

static void SwitchWidth(void)
{
	width_mode++;
	if (width_mode > FULL_WIDTH_MODE)
		width_mode = SHORT_WIDTH_MODE;

	ResetDisplay();
	PLATFORM_DisplayScreen();
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
#ifndef SYNCHRONIZED_SOUND
	int sndn = (sound_bits == 8 ? len : len/2);
	/* in mono, sndn is the number of samples (8 or 16 bit) */
	/* in stereo, 2*sndn is the number of sample pairs */
	POKEYSND_Process(dsp_buffer, sndn);
	memcpy(stream, dsp_buffer, len);
#else
	int gap;
	int newpos;
	int underflow_amount = 0;
#define MAX_SAMPLE_SIZE 4
	static char last_bytes[MAX_SAMPLE_SIZE];
	int bytes_per_sample = (POKEYSND_stereo_enabled ? 2 : 1)*((sound_bits == 16) ? 2:1);
	gap = dsp_write_pos - dsp_read_pos;
	if (gap < len) {
		underflow_amount = len - gap;
		len = gap;
		/*return;*/
	}
	newpos = dsp_read_pos + len;
	if (newpos/dsp_buffer_bytes == dsp_read_pos/dsp_buffer_bytes) {
		/* no wrap */
		memcpy(stream, dsp_buffer + (dsp_read_pos%dsp_buffer_bytes), len);
	}
	else {
		/* wraps */
		int first_part_size = dsp_buffer_bytes - (dsp_read_pos%dsp_buffer_bytes);
		memcpy(stream,  dsp_buffer + (dsp_read_pos%dsp_buffer_bytes), first_part_size);
		memcpy(stream + first_part_size, dsp_buffer, len - first_part_size);
	}
	/* save the last sample as we may need it to fill underflow */
	if (gap >= bytes_per_sample) {
		memcpy(last_bytes, stream + len - bytes_per_sample, bytes_per_sample);
	}
	/* Just repeat the last good sample if underflow */
	if (underflow_amount > 0 ) {
		int i;
		for (i = 0; i < underflow_amount/bytes_per_sample; i++) {
			memcpy(stream + len +i*bytes_per_sample, last_bytes, bytes_per_sample);
		}
	}
	dsp_read_pos = newpos;
	callbacktick = SDL_GetTicks();
#endif /* SYNCHRONIZED_SOUND */
}

static void SoundSetup(void)
{
	SDL_AudioSpec desired, obtained;
	if (sound_enabled) {
		desired.freq = dsprate;
		if (sound_bits == 8)
			desired.format = AUDIO_U8;
		else if (sound_bits == 16)
			desired.format = AUDIO_S16;
		else {
			Log_print("unknown sound_bits");
			Atari800_Exit(FALSE);
			Log_flushlog();
		}

		desired.samples = frag_samps;
		desired.callback = SoundCallback;
		desired.userdata = NULL;
#ifdef STEREO_SOUND
		desired.channels = POKEYSND_stereo_enabled ? 2 : 1;
#else
		desired.channels = 1;
#endif /* STEREO_SOUND*/

		if (SDL_OpenAudio(&desired, &obtained) < 0) {
			Log_print("Problem with audio: %s", SDL_GetError());
			Log_print("Sound is disabled.");
			sound_enabled = FALSE;
			return;
		}

#ifdef SYNCHRONIZED_SOUND
		{
			/* how many fragments in the dsp buffer */
#define DSP_BUFFER_FRAGS 5
			int specified_delay_samps = (dsprate*snddelay)/1000;
			int dsp_buffer_samps = frag_samps*DSP_BUFFER_FRAGS +specified_delay_samps;
			int bytes_per_sample = (POKEYSND_stereo_enabled ? 2 : 1)*((sound_bits == 16) ? 2:1);
			dsp_buffer_bytes = desired.channels*dsp_buffer_samps*(sound_bits == 8 ? 1 : 2);
			dsp_read_pos = 0;
			dsp_write_pos = (specified_delay_samps+frag_samps)*bytes_per_sample;
			avg_gap = 0.0;
		}
#else
		dsp_buffer_bytes = desired.channels*frag_samps*(sound_bits == 8 ? 1 : 2);
#endif /* SYNCHRONIZED_SOUND */
		free(dsp_buffer);
		dsp_buffer = (Uint8 *)Util_malloc(dsp_buffer_bytes);
		memset(dsp_buffer, 0, dsp_buffer_bytes);
		POKEYSND_Init(POKEYSND_FREQ_17_EXACT, dsprate, desired.channels, sound_flags);
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
			Log_print("16 bit audio enabled");
			sound_flags |= POKEYSND_BIT16;
			sound_bits = 16;
		}
		else if (strcmp(argv[i], "-audio8") == 0) {
			Log_print("8 bit audio enabled");
			sound_flags &= ~POKEYSND_BIT16;
			sound_bits = 8;
		}
		else if (strcmp(argv[i], "-dsprate") == 0)
			dsprate = Util_sscandec(argv[++i]);
#ifdef SYNCHRONIZED_SOUND
		else if (strcmp(argv[i], "-snddelay") == 0)
			snddelay = Util_sscandec(argv[++i]);
#endif
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-sound           Enable sound");
				Log_print("\t-nosound         Disable sound");
				Log_print("\t-audio16         Use 16-bit sound output");
				Log_print("\t-audio8          Use 8 bit sound output");
				Log_print("\t-dsprate <rate>  Set DSP rate in Hz");
				Log_print("\t-snddelay <ms>   Set audio latency in ms");
				sound_enabled = FALSE;
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	SoundSetup();
	SDL_PauseAudio(0);
}
#endif /* SOUND */

int PLATFORM_GetRawKey(void)
{
	while(TRUE)
	{
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				return event.key.keysym.sym;
			}
		}
	}
}

int PLATFORM_Keyboard(void)
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
			ResizeDisplay(event.resize.w, event.resize.h);
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

	UI_alt_function = -1;
	if (kbhits[SDLK_LALT]) {
		if (key_pressed) {
			switch (lastkey) {
			case SDLK_f:
				key_pressed = 0;
				SwitchFullscreen();
				break;
			case SDLK_x:
				if (INPUT_key_shift && (AF80_enabled || XEP80_enabled || PBI_PROTO80_enabled)) {
					key_pressed = 0;
					PLATFORM_Switch80();
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
				UI_alt_function = UI_MENU_RUN;
				break;
			case SDLK_y:
				UI_alt_function = UI_MENU_SYSTEM;
				break;
			case SDLK_o:
				UI_alt_function = UI_MENU_SOUND;
				break;
			case SDLK_w:
				UI_alt_function = UI_MENU_SOUND_RECORDING;
				break;
			case SDLK_a:
				UI_alt_function = UI_MENU_ABOUT;
				break;
			case SDLK_s:
				UI_alt_function = UI_MENU_SAVESTATE;
				break;
			case SDLK_d:
				UI_alt_function = UI_MENU_DISK;
				break;
			case SDLK_l:
				UI_alt_function = UI_MENU_LOADSTATE;
				break;
			case SDLK_c:
				UI_alt_function = UI_MENU_CARTRIDGE;
				break;
			case SDLK_BACKSLASH:
				return AKEY_PBI_BB_MENU;
			case SDLK_m:
				grab_mouse = !grab_mouse;
				SDL_WM_GrabInput(grab_mouse ? SDL_GRAB_ON : SDL_GRAB_OFF);
				key_pressed = 0;
				break;
			case SDLK_1:
				key_pressed = 0;
				if (Atari800_tv_mode == Atari800_TV_NTSC) {
					if (kbhits[SDLK_LSHIFT])
						COLOURS_NTSC_specific_setup.hue-=0.02;
					else
						COLOURS_NTSC_specific_setup.hue+=0.02;
					Colours_Update();
				}
				break;
			case SDLK_2:
				key_pressed = 0;
				if (kbhits[SDLK_LSHIFT])
					Colours_setup->saturation-=0.02;
				else
					Colours_setup->saturation+=0.02;
				Colours_Update();
				break;
			case SDLK_3:
				key_pressed = 0;
				if (kbhits[SDLK_LSHIFT])
					Colours_setup->contrast-=0.02;
				else
					Colours_setup->contrast+=0.02;
				Colours_Update();
				break;
			case SDLK_4:
				key_pressed = 0;
				if (kbhits[SDLK_LSHIFT])
					Colours_setup->brightness-=0.02;
				else
					Colours_setup->brightness+=0.02;
				Colours_Update();
				break;
			case SDLK_5:
				key_pressed = 0;
				if (kbhits[SDLK_LSHIFT])
					Colours_setup->gamma-=0.02;
				else
					Colours_setup->gamma+=0.02;
				Colours_Update();
				break;
			case SDLK_6:
				key_pressed = 0;
				if (Atari800_tv_mode == Atari800_TV_NTSC) {
					if (kbhits[SDLK_LSHIFT])
						COLOURS_NTSC_specific_setup.color_delay-=0.1;
					else
						COLOURS_NTSC_specific_setup.color_delay+=0.1;
					Colours_Update();
				}
				break;
			default:
				if(current_display_mode == display_ntscemu){
					switch(lastkey){
					case SDLK_7:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.sharpness-=0.1;
						else
							FILTER_NTSC_setup.sharpness+=0.1;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_8:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.resolution-=.05;
						else
							FILTER_NTSC_setup.resolution+=.05;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_9:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.artifacts-=.05;
						else
							FILTER_NTSC_setup.artifacts+=.05;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_0:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.fringing-=.05;
						else
							FILTER_NTSC_setup.fringing+=.05;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_MINUS:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.bleed-=.05;
						else
							FILTER_NTSC_setup.bleed+=.05;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_EQUALS:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							FILTER_NTSC_setup.burst_phase-=0.1;
						else
							FILTER_NTSC_setup.burst_phase+=0.1;
						FILTER_NTSC_Update(the_ntscemu);
						break;
					case SDLK_LEFTBRACKET:
						key_pressed = 0;
						if (kbhits[SDLK_LSHIFT])
							scanlines_percentage -= 5*(scanlines_percentage>=5);
						else
							scanlines_percentage += 5*(scanlines_percentage<=100-5);
						Log_print("scanlines percentage:%d",scanlines_percentage);
						break;
					case SDLK_RIGHTBRACKET:
						key_pressed = 0;
						FILTER_NTSC_NextPreset();
						FILTER_NTSC_Update(the_ntscemu);
						break;
					}
				}
			break;
			}
		}
		if (UI_alt_function != -1) {
			key_pressed = 0;
			return AKEY_UI;
		}
	}

	/* SHIFT STATE */
	if ((kbhits[SDLK_LSHIFT]) || (kbhits[SDLK_RSHIFT]))
		INPUT_key_shift = 1;
	else
		INPUT_key_shift = 0;

    /* CONTROL STATE */
	if ((kbhits[SDLK_LCTRL]) || (kbhits[SDLK_RCTRL]))
		key_control = 1;
	else
		key_control = 0;

	/*
	if (event.type == 2 || event.type == 3) {
		Log_print("E:%x S:%x C:%x K:%x U:%x M:%x",event.type,INPUT_key_shift,key_control,lastkey,event.key.keysym.unicode,event.key.keysym.mod);
	}
	*/

	/* OPTION / SELECT / START keys */
	INPUT_key_consol = INPUT_CONSOL_NONE;
	if (kbhits[SDLK_F2])
		INPUT_key_consol &= (~INPUT_CONSOL_OPTION);
	if (kbhits[SDLK_F3])
		INPUT_key_consol &= (~INPUT_CONSOL_SELECT);
	if (kbhits[SDLK_F4])
		INPUT_key_consol &= (~INPUT_CONSOL_START);

	if (key_pressed == 0)
		return AKEY_NONE;

	/* Handle movement and special keys. */
	switch (lastkey) {
	case SDLK_F1:
		key_pressed = 0;
		return AKEY_UI;
	case SDLK_F5:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	case SDLK_F8:
		key_pressed = 0;
		return (PLATFORM_Exit(1) ? AKEY_NONE : AKEY_EXIT);
	case SDLK_F9:
		return AKEY_EXIT;
	case SDLK_F10:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT_INTERLACE;
	}

	/* keyboard joysticks: don't pass the keypresses to emulation
	 * as some games pause on a keypress (River Raid, Bruce Lee)
	 */
	if (!UI_is_active && PLATFORM_kbd_joy_0_enabled) {
		if (lastkey == KBD_STICK_0_LEFT || lastkey == KBD_STICK_0_RIGHT ||
			lastkey == KBD_STICK_0_UP || lastkey == KBD_STICK_0_DOWN || lastkey == KBD_TRIG_0) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (!UI_is_active && PLATFORM_kbd_joy_1_enabled) {
		if (lastkey == KBD_STICK_1_LEFT || lastkey == KBD_STICK_1_RIGHT ||
			lastkey == KBD_STICK_1_UP || lastkey == KBD_STICK_1_DOWN || lastkey == KBD_TRIG_1) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (INPUT_key_shift)
		shiftctrl ^= AKEY_SHFT;

	if (Atari800_machine_type == Atari800_MACHINE_5200 && !UI_is_active) {
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
		if (INPUT_key_shift)
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
		if (INPUT_key_shift)
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
		return (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT) ^ shiftctrl;
	case SDLK_RIGHT:
		return (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT) ^ shiftctrl;
	case SDLK_UP:
		return (INPUT_key_shift ? AKEY_MINUS : AKEY_UP) ^ shiftctrl;
	case SDLK_DOWN:
		return (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN) ^ shiftctrl;
	case SDLK_ESCAPE:
		/* Windows takes ctrl+esc and ctrl+shift+esc */
		return AKEY_ESCAPE ^ shiftctrl;
	case SDLK_TAB:
		return AKEY_TAB ^ shiftctrl;
	case SDLK_DELETE:
		if (INPUT_key_shift)
			return AKEY_DELETE_LINE|shiftctrl;
		else
			return AKEY_DELETE_CHAR;
	case SDLK_INSERT:
		if (INPUT_key_shift)
			return AKEY_INSERT_LINE|shiftctrl;
		else
			return AKEY_INSERT_CHAR;
	}
	if (INPUT_cx85) switch (lastkey) {
	case SDLK_KP1:
		return AKEY_CX85_1;
	case SDLK_KP2:
		return AKEY_CX85_2;
	case SDLK_KP3:
		return AKEY_CX85_3;
	case SDLK_KP4:
		return AKEY_CX85_4;
	case SDLK_KP5:
		return AKEY_CX85_5;
	case SDLK_KP6:
		return AKEY_CX85_6;
	case SDLK_KP7:
		return AKEY_CX85_7;
	case SDLK_KP8:
		return AKEY_CX85_8;
	case SDLK_KP9:
		return AKEY_CX85_9;
	case SDLK_KP0:
		return AKEY_CX85_0;
	case SDLK_KP_PERIOD:
		return AKEY_CX85_PERIOD;
	case SDLK_KP_MINUS:
		return AKEY_CX85_MINUS;
	case SDLK_KP_ENTER:
		return AKEY_CX85_PLUS_ENTER;
	case SDLK_KP_DIVIDE:
		return (key_control ? AKEY_CX85_ESCAPE : AKEY_CX85_NO);
	case SDLK_KP_MULTIPLY:
		return AKEY_CX85_DELETE;
	case SDLK_KP_PLUS:
		return AKEY_CX85_YES;
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
    if(lastuni>='A' && lastuni <= 'Z' && !INPUT_key_shift) lastuni += 0x20;
    if(lastuni>='a' && lastuni <= 'z' && INPUT_key_shift) lastuni -= 0x20;
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

static void PLATFORM_Mouse(void)
{
	Uint8 buttons;

	if(INPUT_direct_mouse) {
		int potx, poty;

		buttons = SDL_GetMouseState(&potx, &poty);
		if(potx < 0) potx = 0;
		if(poty < 0) poty = 0;
		potx = (double)potx * (228.0 / (double)MainScreen->w);
		poty = (double)poty * (228.0 / (double)MainScreen->h);
		if(potx > 227) potx = 227;
		if(poty > 227) poty = 227;
		POKEY_POT_input[INPUT_mouse_port << 1] = 227 - potx;
		POKEY_POT_input[(INPUT_mouse_port << 1) + 1] = 227 - poty;
	} else {
		buttons = SDL_GetRelativeMouseState(&INPUT_mouse_delta_x, &INPUT_mouse_delta_y);
	}

	INPUT_mouse_buttons =
		((buttons & SDL_BUTTON(1)) ? 1 : 0) |
		((buttons & SDL_BUTTON(2)) ? 2 : 0) |
		((buttons & SDL_BUTTON(3)) ? 4 : 0);
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
			swap_joysticks = 1;
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
	if (INPUT_cx85) { /* disable keyboard joystick if using CX85 numpad */
		PLATFORM_kbd_joy_0_enabled = 0;
	}
}


int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i, j;
	int no_joystick;
	int help_only = FALSE;

	no_joystick = 0;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */
		
		if (strcmp(argv[i], "-ntscemu") == 0) {
			PLATFORM_filter = PLATFORM_FILTER_NTSC;
		}
		else if (strcmp(argv[i], "-scanlines") == 0) {
			if (i_a) {
				scanlines_percentage  = Util_sscandec(argv[++i]);
				Log_print("scanlines percentage set");
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-scanlinesnoint") == 0) {
			scanlinesnoint = TRUE;
			Log_print("scanlines interpolation disabled");
		}
		else if (strcmp(argv[i], "-rotate90") == 0) {
			current_display_mode = display_rotated;
			no_joystick = 1;
			Log_print("rotate90 mode");
		}
		else if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = 1;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-width") == 0) {
			if (i_a) {
				display_modes[display_normal].w = Util_sscandec(argv[++i]);
				Log_print("width set");
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-height") == 0) {
			if (i_a) {
				display_modes[display_normal].h = Util_sscandec(argv[++i]);
				Log_print("height set");
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-bpp") == 0) {
			if (i_a) {
				display_modes[display_normal].bpp = Util_sscandec(argv[++i]);
				Log_print("bpp set");
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-fullscreen") == 0) {
			fullscreen = 1;
		}
		else if (strcmp(argv[i], "-windowed") == 0) {
			fullscreen = 0;
		}
		else if (strcmp(argv[i], "-grabmouse") == 0) {
			grab_mouse = 1;
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
				Log_print("\t-grabmouse       Prevent mouse pointer from leaving window");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

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
	SDL_WM_SetCaption(Atari800_TITLE, "Atari800");

#ifdef SOUND
	SoundInitialise(argc, argv);
#endif

	if (help_only)
		return TRUE;		/* return before changing the gfx mode */

	/* 80 column devices */
	if (PBI_PROTO80_enabled || AF80_enabled || XEP80_enabled) {
		PLATFORM_Switch80();
	}

	if (PLATFORM_filter != PLATFORM_FILTER_NONE)
		PLATFORM_SetFilter(PLATFORM_filter);
	ResetDisplay();

	Log_print("video initialized");

	if (no_joystick == 0)
		Init_Joysticks(argc, argv);

	SDL_EnableUNICODE(1);

	if(grab_mouse)
		SDL_WM_GrabInput(SDL_GRAB_ON);

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	int restart;
	int original_fullscreen = fullscreen;

	SDL_WM_GrabInput(SDL_GRAB_OFF);
	if (run_monitor) {
		/* disable graphics, set alpha mode */
		if (fullscreen) {
			SwitchFullscreen();
		}
#ifdef SOUND
		Sound_Pause();
#endif
		restart = MONITOR_Run();
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
		if(grab_mouse) SDL_WM_GrabInput(SDL_GRAB_ON);
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
	enum { atari_width = overscan ? atari_ntsc_640_in_width : atari_ntsc_min_in_width };
	enum { atari_height = 240 -overscan_lines };

	/* Output size */
	enum { out_width = overscan ? atari_ntsc_640_out_width : atari_ntsc_min_out_width };
	enum { out_height = atari_height * 2 };
	enum { left_border_adj = ((640 - out_width)/2) & 0xfffffffc };
	int const raw_width = Screen_WIDTH; /* raw image has extra data */

	int jumped = 24;
	unsigned short *pixels = (unsigned short*)MainScreen->pixels + overscan_lines / 2 * MainScreen->pitch + left_border_adj;
	/* blit atari image, doubled vertically */
	atari_ntsc_blit( the_ntscemu,
			 (ATARI_NTSC_IN_T *) (screen + jumped + overscan_lines / 2 * Screen_WIDTH),
			 raw_width,
			 atari_width,
			 atari_height,
			 pixels,
			 MainScreen->pitch * 2 );
	
	if (!scanlinesnoint) {
		scanLines_16_interp((void *)pixels, out_width, out_height, MainScreen->pitch, scanlines_percentage);
	} else {
		scanLines_16((void *)pixels, out_width, out_height, MainScreen->pitch, scanlines_percentage);
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
			pixels = PBI_PROTO80_GetPixels(scanline,column);
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

static void DisplayAF80640x500(UBYTE *screen)
{
	UWORD black = 0x0000;
	int skip = MainScreen->pitch - 80*8;
	Uint16 *start16 = (Uint16 *) MainScreen->pixels;

	int scanline, column;
	UBYTE pixels;

	static int AF80Frame = 0;
	int blink;
	AF80Frame++;
	if (AF80Frame == 60) AF80Frame = 0;
	if (AF80Frame > 29) {
		blink = TRUE;
	}
	else {
		blink = FALSE;
	}

	for (scanline = 0; scanline < 10*25; scanline++) {
		for (column = 0; column < 80; column++) {
			int i;
			int colour;
			pixels = AF80_GetPixels(scanline, column, &colour, blink);
			for (i = 0; i < 8; i++) {
				if (pixels & 0x01) {
					*start16++ = SDL_MapRGB(MainScreen->format, (colour>>16), ((colour>>8)&0xff), (colour&0xff));
				}
				else {
					*start16++ = black;
				}
				pixels >>= 1;
			}
		}
		start16 += skip;
	}
	scanLines_16_interp((void *)MainScreen->pixels, 640, 500, MainScreen->pitch, scanlines_percentage);
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
			Uint8 left = screen[Screen_WIDTH * (i * 2) + 32 + 320 - j];
			Uint8 right = screen[Screen_WIDTH * (i * 2 + 1) + 32 + 320 - j];
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
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * SetNewVideoModeNormal() function. */
	case 8:
		while (i > 0) {
			memcpy(start32, screen, width);
			screen += Screen_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	case 16:
		while (i > 0) {
			pos = width;
			do {
				pos--;
				c = screen[pos];
				quad = Palette16[c] << 16;
				pos--;
				c = screen[pos];
				quad += Palette16[c];
				start32[pos >> 1] = quad;
			} while (pos > 0);
			screen += Screen_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	default:
		/* MainScreen->format->BitsPerPixel = 32 */
		while (i > 0) {
			pos = width;
			do {
				pos--;
				c = screen[pos];
				quad = Palette32[c];
				start32[pos] = quad;
			} while (pos > 0);
			screen += Screen_WIDTH;
			start32 += pitch4;
			i--;
		}
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
	h = (Screen_HEIGHT) << 16;
	dx = w / MainScreen->w;
	dy = h / MainScreen->h;
	w1 = MainScreen->w - 1;
	w2 = MainScreen->w / 2 - 1;
	w4 = MainScreen->w / 4 - 1;
	ss = screen;
	y = (0) << 16;
	i = MainScreen->h;

	switch (MainScreen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * SetNewVideoModeNormal() function. */
	case 8:
		while (i > 0) {
			x = (width + jumped) << 16;
			pos = w4;
			yy = Screen_WIDTH * (y >> 16);
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
			yy = Screen_WIDTH * (y >> 16);
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
	default:
		/* MainScreen->format->BitsPerPixel = 32 */
		while (i > 0) {
			x = (width + jumped) << 16;
			pos = w1;
			yy = Screen_WIDTH * (y >> 16);
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
	}
}

void DisplayNormal(Uint8 *screen)
{
	int width, jumped;

	switch (width_mode) {
	case SHORT_WIDTH_MODE:
		width = Screen_WIDTH - 2 * 24 - 2 * 8;
		jumped = 24 + 8;
		break;
	case DEFAULT_WIDTH_MODE:
		width = Screen_WIDTH - 2 * 24;
		jumped = 24;
		break;
	case FULL_WIDTH_MODE:
		width = Screen_WIDTH;
		jumped = 0;
		break;
	default:
		Log_print("unsupported width_mode");
		Log_flushlog();
		exit(-1);
		break;
	}
	if (MainScreen->w == width && MainScreen->h == Screen_HEIGHT) {
		DisplayWithoutScaling(screen, jumped, width);
	}
	else {
		DisplayWithScaling(screen, jumped, width);
	}
}

void PLATFORM_DisplayScreen(void)
{
	/* Use function corresponding to the current_display_mode. */
	(*display_modes[current_display_mode].display_screen_func)((UBYTE *)Screen_atari);
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
			return INPUT_STICK_UR;
		else if (y > minjoy)
			return INPUT_STICK_LR;
		else
			return INPUT_STICK_RIGHT;
	}
	else if (x < -minjoy) {
		if (y < -minjoy)
			return INPUT_STICK_UL;
		else if (y > minjoy)
			return INPUT_STICK_LL;
		else
			return INPUT_STICK_LEFT;
	}
	else {
		if (y < -minjoy)
			return INPUT_STICK_FORWARD;
		else if (y > minjoy)
			return INPUT_STICK_BACK;
		else
			return INPUT_STICK_CENTRE;
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
			return INPUT_STICK_UR;
		}
		else if (status & 0x20) {	/* down */
			return INPUT_STICK_LR;
		}
		else {
			return INPUT_STICK_RIGHT;
		}
	}
	else if (status & 0x80) {		/* left */
		if (status & 0x10) {		/* up */
			return INPUT_STICK_UL;
		}
		else if (status & 0x20) {	/* down */
			return INPUT_STICK_LL;
		}
		else {
			return INPUT_STICK_LEFT;
		}
	}
	else {
		if (status & 0x10) {		/* up */
			return INPUT_STICK_FORWARD;
		}
		else if (status & 0x20) {	/* down */
			return INPUT_STICK_BACK;
		}
		else {
			return INPUT_STICK_CENTRE;
		}
	}
#else
	return 0;
#endif /* LPTJOY */
}

static void get_platform_PORT(Uint8 *s0, Uint8 *s1)
{
	int stick0, stick1;
	stick0 = stick1 = INPUT_STICK_CENTRE;

	if (PLATFORM_kbd_joy_0_enabled) {
		if (kbhits[KBD_STICK_0_LEFT])
			stick0 = INPUT_STICK_LEFT;
		if (kbhits[KBD_STICK_0_RIGHT])
			stick0 = INPUT_STICK_RIGHT;
		if (kbhits[KBD_STICK_0_UP])
			stick0 = INPUT_STICK_FORWARD;
		if (kbhits[KBD_STICK_0_DOWN])
			stick0 = INPUT_STICK_BACK;
		if ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_UP]))
			stick0 = INPUT_STICK_UL;
		if ((kbhits[KBD_STICK_0_LEFT]) && (kbhits[KBD_STICK_0_DOWN]))
			stick0 = INPUT_STICK_LL;
		if ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_UP]))
			stick0 = INPUT_STICK_UR;
		if ((kbhits[KBD_STICK_0_RIGHT]) && (kbhits[KBD_STICK_0_DOWN]))
			stick0 = INPUT_STICK_LR;
	}
	if (PLATFORM_kbd_joy_1_enabled) {
		if (kbhits[KBD_STICK_1_LEFT])
			stick1 = INPUT_STICK_LEFT;
		if (kbhits[KBD_STICK_1_RIGHT])
			stick1 = INPUT_STICK_RIGHT;
		if (kbhits[KBD_STICK_1_UP])
			stick1 = INPUT_STICK_FORWARD;
		if (kbhits[KBD_STICK_1_DOWN])
			stick1 = INPUT_STICK_BACK;
		if ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_UP]))
			stick1 = INPUT_STICK_UL;
		if ((kbhits[KBD_STICK_1_LEFT]) && (kbhits[KBD_STICK_1_DOWN]))
			stick1 = INPUT_STICK_LL;
		if ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_UP]))
			stick1 = INPUT_STICK_UR;
		if ((kbhits[KBD_STICK_1_RIGHT]) && (kbhits[KBD_STICK_1_DOWN]))
			stick1 = INPUT_STICK_LR;
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

static void get_platform_TRIG(Uint8 *t0, Uint8 *t1)
{
	int trig0, trig1, i;
	trig0 = trig1 = 1;

	if (PLATFORM_kbd_joy_0_enabled) {
		trig0 = kbhits[KBD_TRIG_0] ? 0 : 1;
	}

	if (PLATFORM_kbd_joy_1_enabled) {
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

int PLATFORM_PORT(int num)
{
#ifndef DONT_DISPLAY
	if (num == 0) {
		UBYTE a, b;
		get_platform_PORT(&a, &b);
		return (b << 4) | (a & 0x0f);
	}
#endif
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b;
	get_platform_TRIG(&a, &b);
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
		INPUT_key_code = PLATFORM_Keyboard();
		PLATFORM_Mouse();
		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
	}
}

/*
vim:ts=4:sw=4:
*/
