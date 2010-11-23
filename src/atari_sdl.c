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
#include "videomode.h"
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
static int swap_joysticks = 0;
static int scanlines_percentage = 5;
static int scanlinesnoint = FALSE;
static atari_ntsc_t *the_ntscemu = NULL;

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

static VIDEOMODE_resolution_t desktop_resolution;

/* SDL port supports 5 "display modes":
   - normal,
   - ntscemu - emulation of NTSC composite video,
   - xep80 emulation,
   - proto80 - emulation of 80 column board for 1090XL,
   - af80 - Austin Franklin 80 column card.
   The structure below is used to hold settings and functions used by a single
   display mode. */
struct display_mode_data_t {
	int bpp;
	void (*display_screen_func)(Uint8 *); /* see below */
};

/* display_screen_func - these functions fill the given SCREEN buffer with
   screen data, differently for each display mode. */
static void DisplayWithoutScaling(Uint8 *screen);
static void DisplayWithScaling(Uint8 *screen);
static void DisplayRotated(Uint8 *screen);
static void DisplayNTSCEmu(Uint8 *screen);
static void DisplayXEP80(Uint8 *screen);
static void DisplayProto80(Uint8 *screen);
static void DisplayAF80(Uint8 *screen);

/* This table holds default settings for all display modes. */
static struct display_mode_data_t display_modes[VIDEOMODE_MODE_SIZE] = {
	/* Normal */
	{ 0, /* bpp = 0 - autodetect */
	  &DisplayWithoutScaling },
	/* NTSC Filter */
	{ ATARI_NTSC_OUT_DEPTH,
	  &DisplayNTSCEmu },
	/* XEP80 */
	{ 8,
	  &DisplayXEP80 },
	/* Proto80 */
	{ 16,
	  &DisplayProto80 },
	/* AF80 */
	{ 16,
	  &DisplayAF80 }
};

/* Indicates current display mode */
static VIDEOMODE_MODE_t current_display_mode = VIDEOMODE_MODE_NORMAL;

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

	if (!sound_enabled || Atari800_turbo) return;
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
	Uint32 c;
	for (i = 0; i < 256; i++) {
		rgb = Colours_table[i];
		colors[i].r = (rgb & 0x00ff0000) >> 16;
		colors[i].g = (rgb & 0x0000ff00) >> 8;
		colors[i].b = (rgb & 0x000000ff) >> 0;
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
	if (current_display_mode == VIDEOMODE_MODE_NTSC_FILTER)
		FILTER_NTSC_Update(the_ntscemu);
	SetPalette();
}

static void ModeInfo(void)
{
	char fullflag, width, joyflag;
	if (fullscreen)
		fullflag = '*';
	else
		fullflag = ' ';
	switch (VIDEOMODE_horizontal_area) {
	case VIDEOMODE_HORIZONTAL_FULL:
		width = 'f';
		break;
	case VIDEOMODE_HORIZONTAL_NORMAL:
		width = 'd';
		break;
	case VIDEOMODE_HORIZONTAL_NARROW:
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
	Log_print("[%c] Full screen  [%c] Width Mode  [%c] Joysticks Swapped",
		 fullflag, width, joyflag);
}

static void SetVideoMode(int w, int h, int bpp)
{
	Uint32 flags = fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;
	int update_palette = (MainScreen == NULL || MainScreen->format->BitsPerPixel != bpp);
	MainScreen = SDL_SetVideoMode(w, h, bpp, flags);
	if (MainScreen == NULL) {
		Log_print("Setting Video Mode: %dx%dx%d FAILED", w, h, bpp);
		Log_flushlog();
		exit(-1);
	}
	/* Clear the screen. */
	SDL_FillRect(MainScreen, NULL, 0);
	SDL_Flip(MainScreen);

	if (update_palette) {
		CalcPalette();
		SetPalette();
	}
}

static void UpdateNtscFilter(VIDEOMODE_MODE_t mode)
{
	if (mode != VIDEOMODE_MODE_NTSC_FILTER && the_ntscemu != NULL) {
		/* Turning filter off */
		FILTER_NTSC_Delete(the_ntscemu);
		the_ntscemu = NULL;
	}
	else if (mode == VIDEOMODE_MODE_NTSC_FILTER && the_ntscemu == NULL) {
		/* Turning filter on */
		the_ntscemu = FILTER_NTSC_New();
		FILTER_NTSC_Update(the_ntscemu);
	}
}

void PLATFORM_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90)
{
	int bpp = (rotate90 ? 16 : display_modes[mode].bpp);
	fullscreen = !windowed;
	UpdateNtscFilter(mode);
	current_display_mode = mode;

	SetVideoMode(res->width, res->height, bpp);
	if (bpp == 0) {
		bpp = MainScreen->format->BitsPerPixel;
		Log_print("detected %dbpp", bpp);
		if ((bpp != 8) && (bpp != 16)
			&& (bpp != 32)) {
			Log_print("it's unsupported, so setting 8bit mode (slow conversion)");
			bpp = 8;
			SetVideoMode(res->width, res->height, 8);
		}
		display_modes[mode].bpp = bpp;
	}

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */
	ModeInfo();

	if (current_display_mode == VIDEOMODE_MODE_NORMAL) {
		if (rotate90)
			display_modes[0].display_screen_func = &DisplayRotated;
		else if (VIDEOMODE_src_width == VIDEOMODE_dest_width && VIDEOMODE_src_height == VIDEOMODE_dest_height)
			display_modes[0].display_screen_func = &DisplayWithoutScaling;
		else
			display_modes[0].display_screen_func = &DisplayWithScaling;
	}
	PLATFORM_DisplayScreen();
}

VIDEOMODE_resolution_t *PLATFORM_AvailableResolutions(unsigned int *size)
{
	SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
	VIDEOMODE_resolution_t *resolutions;
	unsigned int num_modes;
	unsigned int i;
	if (modes == (SDL_Rect**)0 || modes == (SDL_Rect**)-1)
		return NULL;
	/* Determine number of available modes. */
	for (num_modes = 0; modes[num_modes] != NULL; ++num_modes);

	resolutions = Util_malloc(num_modes * sizeof(VIDEOMODE_resolution_t));
	for (i = 0; i < num_modes; i++) {
		resolutions[i].width = modes[i]->w;
		resolutions[i].height = modes[i]->h;
	}
	*size = num_modes;

	return resolutions;
}

VIDEOMODE_resolution_t *PLATFORM_DesktopResolution(void)
{
	return &desktop_resolution;
}

int PLATFORM_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90)
{
	if (mode == VIDEOMODE_MODE_NORMAL) {
		/* Normal mode doesn't support stretching together with rotation. */
		return !(stretch && rotate90);
	} else
		/* Other modes don't support stretching or rotation at all. */
		return !stretch && !rotate90;
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
	int help_only = FALSE;

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
				help_only = TRUE;
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

	if (help_only)
		return;

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
			VIDEOMODE_SetWindowSize(event.resize.w, event.resize.h);
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
				VIDEOMODE_ToggleWindowed();
				break;
			case SDLK_x:
				if (INPUT_key_shift) {
					key_pressed = 0;
					VIDEOMODE_Toggle80Column();
				}
				break;
			case SDLK_g:
				key_pressed = 0;
				VIDEOMODE_ToggleHorizontalArea();
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
				if(current_display_mode == VIDEOMODE_MODE_NTSC_FILTER){
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
	case SDLK_F12:
		key_pressed = 0;
		return AKEY_TURBO;
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
		
		if (strcmp(argv[i], "-scanlines") == 0) {
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
		else if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = 1;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-bpp") == 0) {
			if (i_a) {
				int bpp = display_modes[VIDEOMODE_MODE_NORMAL].bpp = Util_sscandec(argv[++i]);
				if (bpp != 0 && bpp != 8 && bpp != 16 && bpp != 32) {
					Log_print("Invalid BPP value %d", bpp);
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-grabmouse") == 0) {
			grab_mouse = 1;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-scanlines       Specify scanlines percentage (ntscemu only)");
				Log_print("\t-scanlinesnoint  Disable scanlines interpolation (ntscemu only)");
				Log_print("\t-nojoystick      Disable joystick");
#ifdef LPTJOY
				Log_print("\t-joy0 <pathname> Select LPTjoy0 device");
				Log_print("\t-joy1 <pathname> Select LPTjoy0 device");
#endif /* LPTJOY */
				Log_print("\t-bpp <num>       Host color depth (0 = autodetect)");
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

	if (!help_only) {
		i = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
#ifdef SOUND
		i |= SDL_INIT_AUDIO;
#endif
#ifdef HAVE_WINDOWS_H
		/* Windows SDL version 1.2.10+ uses windib as the default, but it is slower.
		   Additionally on some graphic cards it doesn't work properly in low
		   resolutions like Atari800's default of 336x240. */
		if (SDL_getenv("SDL_VIDEODRIVER")==NULL)
			SDL_putenv("SDL_VIDEODRIVER=directx");
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
	}

#ifdef SOUND
	SoundInitialise(argc, argv);
#endif

	if (help_only)
		return TRUE; /* return before initialising SDL */

	/* Get the desktop resolution */
	{
		SDL_VideoInfo const * const info = SDL_GetVideoInfo();
		desktop_resolution.width = info->current_w;
		desktop_resolution.height = info->current_h;
	}

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

	SDL_WM_GrabInput(SDL_GRAB_OFF);
	if (run_monitor) {
		/* disable graphics, set alpha mode */
		VIDEOMODE_ForceWindowed(TRUE);
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
		VIDEOMODE_ForceWindowed(FALSE);
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
	start32 = (Uint32 *) MainScreen->pixels + pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 4;

	i = VIDEOMODE_src_height;
	while (i > 0) {
		memcpy(start32, screen, VIDEOMODE_src_width);
		screen += XEP80_SCRN_WIDTH;
		start32 += pitch4;
		i--;
	}
}

static void DisplayNTSCEmu(UBYTE *screen)
{
	/* Output size */
	unsigned int out_width = ATARI_NTSC_OUT_WIDTH(VIDEOMODE_src_width);
	unsigned int out_height = VIDEOMODE_src_height * 2;

	unsigned short *pixels = (unsigned short*)MainScreen->pixels + MainScreen->pitch / 2 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
	/* blit atari image, doubled vertically */
	atari_ntsc_blit(the_ntscemu,
	                (ATARI_NTSC_IN_T *) (screen + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
	                Screen_WIDTH,
	                VIDEOMODE_src_width,
	                VIDEOMODE_src_height,
	                pixels,
	                MainScreen->pitch * 2);
	
	if (!scanlinesnoint) {
		scanLines_16_interp((void *)pixels, out_width, out_height, MainScreen->pitch, scanlines_percentage);
	} else {
		scanLines_16((void *)pixels, out_width, out_height, MainScreen->pitch, scanlines_percentage);
	}
}

static void DisplayProto80(UBYTE *screen)
{
	UWORD white = 0xffff;
	UWORD black = 0x0000;
	unsigned int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	unsigned int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int skip = MainScreen->pitch - (last_column - first_column)*8;
	Uint16 *start16 = (Uint16 *) MainScreen->pixels + MainScreen->pitch / 2 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
	Uint16 *copy_start16 = start16;

	unsigned int scanline, column;
	UBYTE pixels;
	unsigned int last_scanline = VIDEOMODE_src_offset_top + VIDEOMODE_src_height;
	for (scanline = VIDEOMODE_src_offset_top; scanline < last_scanline; scanline++) {
		for (column = first_column; column < last_column; column++) {
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
	scanLines_16_interp((void *)copy_start16, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, scanlines_percentage);
}

static void DisplayAF80(UBYTE *screen)
{
	UWORD black = 0x0000;
	unsigned int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	unsigned int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int skip = MainScreen->pitch - (last_column - first_column)*8;
	Uint16 *start16 = (Uint16 *) MainScreen->pixels + MainScreen->pitch / 2 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
	Uint16 *copy_start16 = start16;

	unsigned int scanline, column;
	UBYTE pixels;
	unsigned int last_scanline = VIDEOMODE_src_offset_top + VIDEOMODE_src_height;

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

	for (scanline = VIDEOMODE_src_offset_top; scanline < last_scanline; scanline++) {
		for (column = first_column; column < last_column; column++) {
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
	scanLines_16_interp((void *)copy_start16, VIDEOMODE_dest_width, VIDEOMODE_dest_height, MainScreen->pitch, scanlines_percentage);
}

static void DisplayRotated(Uint8 *screen)
{
	unsigned int x, y;
	register Uint32 *start32 = (Uint32 *) MainScreen->pixels + MainScreen->pitch / 4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
	int pitch4 = MainScreen->pitch / 4 - VIDEOMODE_dest_width / 2;
	screen += Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	for (y = 0; y < VIDEOMODE_dest_height; y++) {
		for (x = 0; x < VIDEOMODE_dest_width / 2; x++) {
			Uint8 left = screen[Screen_WIDTH * (x * 2) + VIDEOMODE_src_width - y];
			Uint8 right = screen[Screen_WIDTH * (x * 2 + 1) + VIDEOMODE_src_width - y];
#ifdef WORDS_BIGENDIAN
			*start32++ = (Palette16[left] << 16) + Palette16[right];
#else
			*start32++ = (Palette16[right] << 16) + Palette16[left];
#endif
		}
		start32 += pitch4;
	}
}

static void DisplayWithoutScaling(Uint8 *screen)
{
	register Uint32 quad;
	register Uint32 *start32;
	register Uint8 c;
	register int pos;
	register int pitch4;
	int i;

	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	screen += Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	i = VIDEOMODE_src_height;
	switch (MainScreen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 4;
		while (i > 0) {
			memcpy(start32, screen, VIDEOMODE_src_width);
			screen += Screen_WIDTH;
			start32 += pitch4;
			i--;
		}
		break;
	case 16:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
		while (i > 0) {
			pos = VIDEOMODE_src_width;
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
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
		while (i > 0) {
			pos = VIDEOMODE_src_width;
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

static void DisplayWithScaling(Uint8 *screen)
{
	register Uint32 quad;
	register int x;
	register int dx;
	register int yy;
	register Uint8 *ss = screen;
	register Uint32 *start32;
	int i;
	int y;
	int w1;
	int w, h;
	int pos;
	int pitch4;
	int dy;
	int init_x = ((VIDEOMODE_src_width + VIDEOMODE_src_offset_left) << 16) - 0x4000;

	Uint8 c;
	pitch4 = MainScreen->pitch / 4;
	start32 = (Uint32 *) MainScreen->pixels;

	w = (VIDEOMODE_src_width) << 16;
	h = (VIDEOMODE_src_height) << 16;
	dx = w / VIDEOMODE_dest_width;
	dy = h / VIDEOMODE_dest_height;
	y = (VIDEOMODE_src_offset_top) << 16;
	i = VIDEOMODE_dest_height;

	switch (MainScreen->format->BitsPerPixel) {
	/* Possible values are 8, 16 and 32, as checked earlier in the
	 * PLATFORM_SetVideoMode() function. */
	case 8:
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 4;
		w1 = VIDEOMODE_dest_width / 4 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
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
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left / 2;
		w1 = VIDEOMODE_dest_width / 2 - 1;
		while (i > 0) {
			x = init_x;
			pos = w1;
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
		start32 += pitch4 * VIDEOMODE_dest_offset_top + VIDEOMODE_dest_offset_left;
		w1 = VIDEOMODE_dest_width - 1;
		/* MainScreen->format->BitsPerPixel = 32 */
		while (i > 0) {
			x = init_x;
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

void PLATFORM_DisplayScreen(void)
{
	/* Use function corresponding to the current_display_mode. */
	(*display_modes[current_display_mode].display_screen_func)((UBYTE *)Screen_atari);
	/* SDL_UpdateRect is faster than SDL_Flip, because it updates only
	   the relevant part of the screen. */
/*	SDL_Flip(MainScreen);*/
	SDL_UpdateRect(MainScreen, VIDEOMODE_dest_offset_left, VIDEOMODE_dest_offset_top, VIDEOMODE_dest_width, VIDEOMODE_dest_height);
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
