/*
 * sdl/input.c - SDL library specific port code - input device support
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
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

#include <SDL.h>

#include "config.h"
#include "sdl/input.h"
#include "akey.h"
#include "atari.h"
#include "binload.h"
#include "colours.h"
#include "filter_ntsc.h"
#include "../input.h"
#include "log.h"
#include "platform.h"
#include "pokey.h"
#include "sdl/video.h"
#include "ui.h"
#include "util.h"
#include "videomode.h"
#include "screen.h"
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
#include "ui_basic.h"
#endif

static int grab_mouse = FALSE;
static int swap_joysticks = FALSE;

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

/* Maping for the START, RESET, OPTION, SELECT and EXIT keys */
static int KBD_RESET = SDLK_F5;
static int KBD_OPTION = SDLK_F2;
static int KBD_SELECT = SDLK_F3;
static int KBD_START = SDLK_F4;
static int KBD_EXIT = SDLK_F9;

/* real joysticks */

static int fd_joystick0 = -1;
static int fd_joystick1 = -1;

#define MAX_JOYSTICKS	4
static SDL_Joystick *joystick[MAX_JOYSTICKS] = { NULL, NULL, NULL, NULL };
static int joystick_nbuttons[MAX_JOYSTICKS];
static SDL_INPUT_RealJSConfig_t real_js_configs[MAX_JOYSTICKS];
static int joysticks_found = 0;
static struct js_state {
	unsigned int port;
	unsigned int trig;
} sdl_js_state[MAX_JOYSTICKS];

#define minjoy 10000			/* real joystick tolerancy */

/* keyboard */
static Uint8 *kbhits;

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
static int SDL_controller_kb(void);
static int SDL_consol_keys(void);
int OSK_enabled = 1;
#define OSK_MAX_BUTTONS 6
#define OSK_BUTTON_0 0
#define OSK_BUTTON_1 1
#define OSK_BUTTON_2 2
#define OSK_BUTTON_3 3
#define OSK_BUTTON_4 4
#define OSK_BUTTON_5 5
#define OSK_BUTTON_TRIGGER OSK_BUTTON_0
#define OSK_BUTTON_LEAVE   OSK_BUTTON_1      /* button to exit emulator UI or keyboard emulation screen */
#define OSK_BUTTON_UI      OSK_BUTTON_4      /* button to enter emulator UI */
#define OSK_BUTTON_KEYB    OSK_BUTTON_5      /* button to enter keyboard emulation screen */
#define OSK_BUTTON_START   OSK_BUTTON_LEAVE
#define OSK_BUTTON_SELECT  OSK_BUTTON_2
#define OSK_BUTTON_OPTION  OSK_BUTTON_3
#endif /* #ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD */


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

/*Set real joystick to use hat instead of axis*/
static int set_real_js_use_hat(int joyIndex, const char* parm)
{
    real_js_configs[joyIndex].use_hat = Util_sscandec(parm) != 0 ? TRUE : FALSE;
    return TRUE;
}

/*Reset configurations of the real joysticks*/
static void reset_real_js_configs(void)
{
    int i;
    for (i = 0; i < MAX_JOYSTICKS; i++) {
        real_js_configs[i].use_hat = FALSE;
    }
}

/*Write configurations of real joysticks*/
static void write_real_js_configs(FILE* fp)
{
    int i;
    for (i = 0; i < MAX_JOYSTICKS; i++) {
        fprintf(fp, "SDL_JOY_%d_USE_HAT=%d\n", i, real_js_configs[i].use_hat);
    }
}

/*Get pointer to a real joystick configuration*/
SDL_INPUT_RealJSConfig_t* SDL_INPUT_GetRealJSConfig(int joyIndex)
{
    return &real_js_configs[joyIndex];
}

/* For getting sdl key map out of the config...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
int SDL_INPUT_ReadConfig(char *option, char *parameters)
{
	static int was_config_initialized = FALSE;
    
	if (was_config_initialized == FALSE) {
		reset_real_js_configs();
		was_config_initialized=TRUE;
	}
    
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
	else if (strcmp(option, "SDL_JOY_0_USE_HAT") == 0)
		return set_real_js_use_hat(0,parameters);
	else if (strcmp(option, "SDL_JOY_1_USE_HAT") == 0)
		return set_real_js_use_hat(1,parameters);
	else if (strcmp(option, "SDL_JOY_2_USE_HAT") == 0)
		return set_real_js_use_hat(2,parameters);
	else if (strcmp(option, "SDL_JOY_3_USE_HAT") == 0)
		return set_real_js_use_hat(3,parameters);
	else if (strcmp(option, "SDL_RESET_KEY") == 0)
	        return SDLKeyBind(&KBD_RESET, parameters);
	else if (strcmp(option, "SDL_OPTION_KEY") == 0)
	        return SDLKeyBind(&KBD_OPTION, parameters);
	else if (strcmp(option, "SDL_SELECT_KEY") == 0)
	        return SDLKeyBind(&KBD_SELECT, parameters);
	else if (strcmp(option, "SDL_START_KEY") == 0)
	        return SDLKeyBind(&KBD_START, parameters);
	else if (strcmp(option, "SDL_EXIT_KEY") == 0)
	        return SDLKeyBind(&KBD_EXIT, parameters);
	else
		return FALSE;
}

/* Save the keybindings and the keybindapp options to the config file...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
void SDL_INPUT_WriteConfig(FILE *fp)
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

	fprintf(fp, "SDL_RESET_KEY=%d\n", KBD_RESET);
	fprintf(fp, "SDL_OPTION_KEY=%d\n", KBD_OPTION);
	fprintf(fp, "SDL_SELECT_KEY=%d\n", KBD_SELECT);
	fprintf(fp, "SDL_START_KEY=%d\n", KBD_START);
	fprintf(fp, "SDL_EXIT_KEY=%d\n", KBD_EXIT);
	
	write_real_js_configs(fp);
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
	snprintf(buffer, bufsize, "%11s", key);
}

static void SwapJoysticks(void)
{
	swap_joysticks = 1 - swap_joysticks;
}

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

static int lastkey = SDLK_UNKNOWN, key_pressed = 0, key_control = 0;
static int lastuni = 0;

#if HAVE_WINDOWS_H
/* On Windows 7 rapidly changing the window size invokes a bug in SDL
   which causes the window to be resized to size 0,0. This hack delays
   the resize requests a bit to ensure that no two resizes happen within
   a span of 0.5 second. See also
   http://www.atariage.com/forums/topic/179912-atari800-220-released/page__view__findpost__p__2258092 */
enum { USER_EVENT_RESIZE_DELAY };
static Uint32 ResizeDelayCallback(Uint32 interval, void *param)
{
	SDL_Event event;
	event.user.type = SDL_USEREVENT;
	event.user.code = USER_EVENT_RESIZE_DELAY;
	event.user.data1 = NULL;
	event.user.data2 = NULL;
	SDL_PushEvent(&event);
	return 0;
}
#endif /* HAVE_WINDOWS_H */

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
static unsigned char *atari_screen_backup;
#endif

int PLATFORM_Keyboard(void)
{
	int shiftctrl = 0;
	SDL_Event event;
	int event_found = 0; /* Was there at least one event? */

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
	if (!atari_screen_backup)
		atari_screen_backup = malloc(Screen_HEIGHT * Screen_WIDTH);
#endif

#if HAVE_WINDOWS_H
	/* Used to delay resize events on Windows 7, see above. */
	enum { RESIZE_INTERVAL = 500 };
	static int resize_delayed = FALSE;
	static int resize_needed = FALSE;
	static int resize_w, resize_h;
#endif /* HAVE_WINDOWS_H */

	/* Very ugly fix for SDL CAPSLOCK brokenness.  This will let the user
	 * press CAPSLOCK and get a brief keypress on the Atari but it is not
	 * possible to emulate holding down CAPSLOCK for longer periods with
	 * the broken SDL*/
	if (lastkey == SDLK_CAPSLOCK) {
		lastkey = SDLK_UNKNOWN;
		key_pressed = 0;
 		lastuni = 0;
	}

	while (SDL_PollEvent(&event)) {
		event_found = 1;
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
#if HAVE_WINDOWS_H
			/* Delay resize events on Windows 7, see above. */
			if (resize_delayed) {
				resize_w = event.resize.w;
				resize_h = event.resize.h;
				resize_needed = TRUE;
			} else {
				VIDEOMODE_SetWindowSize(event.resize.w, event.resize.h);
				resize_delayed = TRUE;
				if (SDL_AddTimer(RESIZE_INTERVAL, &ResizeDelayCallback, NULL) == NULL) {
					Log_print("Error: SDL_AddTimer failed: %s", SDL_GetError());
					Log_flushlog();
					exit(-1);
				}
			}
#else
			VIDEOMODE_SetWindowSize(event.resize.w, event.resize.h);
#endif /* HAVE_WINDOWS_H */
			break;
		case SDL_VIDEOEXPOSE:
			/* When window is "uncovered", and we are in the emulator's menu,
			   we need to refresh display manually. */
			PLATFORM_DisplayScreen();
			break;
		case SDL_QUIT:
			return AKEY_EXIT;
			break;
#if HAVE_WINDOWS_H
		case SDL_USEREVENT:
			/* Process delayed video resize on Windows 7, see above. */
			if (event.user.code == USER_EVENT_RESIZE_DELAY) {
				if (resize_needed) {
					SDL_Event events[1];
					resize_needed = FALSE;
					/* If there's a resize event in the queue,
					   wait for it and don't resize now. */
					if (SDL_PeepEvents(events, 1, SDL_PEEKEVENT, SDL_EVENTMASK(SDL_VIDEORESIZE)) != 0)
						resize_delayed = FALSE;
					else {
						VIDEOMODE_SetWindowSize(resize_w, resize_h);
						if (SDL_AddTimer(RESIZE_INTERVAL, &ResizeDelayCallback, NULL) == NULL) {
							Log_print("Error: SDL_AddTimer failed: %s", SDL_GetError());
							Log_flushlog();
							exit(-1);
						}
					}
				} else
					resize_delayed = FALSE;
			}
			break;
#endif /* HAVE_WINDOWS_H */
		}
	}

	if (!event_found && !key_pressed) {
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
		SDL_consol_keys();
		return SDL_controller_kb();
#else
		return AKEY_NONE;
#endif
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
#if defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80) || defined(BIT3)
					VIDEOMODE_Toggle80Column();
#endif
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
			case SDLK_t:
				UI_alt_function = UI_MENU_CASSETTE;
				break;
			case SDLK_BACKSLASH:
				return AKEY_PBI_BB_MENU;
			case SDLK_m:
				grab_mouse = !grab_mouse;
				SDL_WM_GrabInput(grab_mouse ? SDL_GRAB_ON : SDL_GRAB_OFF);
				key_pressed = 0;
				break;
			case SDLK_1:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->hue > COLOURS_HUE_MIN)
						Colours_setup->hue -= 0.02;
				} else {
					if (Colours_setup->hue < COLOURS_HUE_MAX)
						Colours_setup->hue += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_2:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->saturation > COLOURS_SATURATION_MIN)
						Colours_setup->saturation -= 0.02;
				} else {
					if (Colours_setup->saturation < COLOURS_SATURATION_MAX)
						Colours_setup->saturation += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_3:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->contrast > COLOURS_CONTRAST_MIN)
						Colours_setup->contrast -= 0.04;
				} else {
					if (Colours_setup->contrast < COLOURS_CONTRAST_MAX)
					Colours_setup->contrast += 0.04;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_4:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->brightness > COLOURS_BRIGHTNESS_MIN)
						Colours_setup->brightness -= 0.04;
				} else {
					if (Colours_setup->brightness < COLOURS_BRIGHTNESS_MAX)
						Colours_setup->brightness += 0.04;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_5:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->gamma > COLOURS_GAMMA_MIN)
						Colours_setup->gamma -= 0.02;
				} else {
					if (Colours_setup->gamma < COLOURS_GAMMA_MAX)
						Colours_setup->gamma += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_6:
				if (kbhits[SDLK_LSHIFT]) {
					if (Colours_setup->color_delay > COLOURS_DELAY_MIN)
						Colours_setup->color_delay -= 0.4;
				} else {
					if (Colours_setup->color_delay < COLOURS_DELAY_MAX)
						Colours_setup->color_delay += 0.4;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_LEFTBRACKET:
				if (kbhits[SDLK_LSHIFT])
					SDL_VIDEO_SetScanlinesPercentage(SDL_VIDEO_scanlines_percentage - 1);
				else
					SDL_VIDEO_SetScanlinesPercentage(SDL_VIDEO_scanlines_percentage + 1);
				return AKEY_NONE;
			default:
				if(FILTER_NTSC_emu != NULL){
					switch(lastkey){
					case SDLK_7:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.sharpness > FILTER_NTSC_SHARPNESS_MIN)
								FILTER_NTSC_setup.sharpness -= 0.02;
						} else {
							if (FILTER_NTSC_setup.sharpness < FILTER_NTSC_SHARPNESS_MAX)
								FILTER_NTSC_setup.sharpness += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_8:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.resolution > FILTER_NTSC_RESOLUTION_MIN)
								FILTER_NTSC_setup.resolution -= 0.02;
						} else {
							if (FILTER_NTSC_setup.resolution < FILTER_NTSC_RESOLUTION_MAX)
								FILTER_NTSC_setup.resolution += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_9:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.artifacts > FILTER_NTSC_ARTIFACTS_MIN)
								FILTER_NTSC_setup.artifacts -= 0.02;
						} else {
							if (FILTER_NTSC_setup.artifacts < FILTER_NTSC_ARTIFACTS_MAX)
								FILTER_NTSC_setup.artifacts += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_0:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.fringing > FILTER_NTSC_FRINGING_MIN)
								FILTER_NTSC_setup.fringing -= 0.02;
						} else {
							if (FILTER_NTSC_setup.fringing < FILTER_NTSC_FRINGING_MAX)
								FILTER_NTSC_setup.fringing += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_MINUS:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.bleed > FILTER_NTSC_BLEED_MIN)
								FILTER_NTSC_setup.bleed -= 0.02;
						} else {
							if (FILTER_NTSC_setup.bleed < FILTER_NTSC_BLEED_MAX)
								FILTER_NTSC_setup.bleed += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_EQUALS:
						if (kbhits[SDLK_LSHIFT]) {
							if (FILTER_NTSC_setup.burst_phase > FILTER_NTSC_BURST_PHASE_MIN)
								FILTER_NTSC_setup.burst_phase -= 0.02;
						} else {
							if (FILTER_NTSC_setup.burst_phase < FILTER_NTSC_BURST_PHASE_MAX)
								FILTER_NTSC_setup.burst_phase += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_RIGHTBRACKET:
						key_pressed = 0;
						FILTER_NTSC_NextPreset();
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						break;
					}
				}
			break;
			}
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

	BINLOAD_pause_loading = FALSE;

	/* OPTION / SELECT / START keys */
	INPUT_key_consol = INPUT_CONSOL_NONE;
	if (kbhits[KBD_OPTION])
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
	if (kbhits[KBD_SELECT])
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
	if (kbhits[KBD_START])
		INPUT_key_consol &= ~INPUT_CONSOL_START;

	if (key_pressed == 0)
		return AKEY_NONE;

	/* Handle movement and special keys. */

	/* Since KBD_RESET & KBD_EXIT are variables, they can't be cases */
	/* in a switch statement.  So handle them here with if-blocks */
	if (lastkey == KBD_RESET) {
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
	}
	if (lastkey == KBD_EXIT) {
	        return AKEY_EXIT;
	}

	switch (lastkey) {
	case SDLK_F1:
		key_pressed = 0;
		return AKEY_UI;
	case SDLK_F8:
		UI_alt_function = UI_MENU_MONITOR;
		break;
	case SDLK_F10:
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
	case SDLK_F12:
		key_pressed = 0;
		return AKEY_TURBO;
	}

	if (UI_alt_function != -1) {
		key_pressed = 0;
		return AKEY_UI;
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
		if (BINLOAD_wait_active) {
			BINLOAD_pause_loading = TRUE;
			return AKEY_NONE;
		}
		else
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
		return (!UI_is_active && Atari800_f_keys ? AKEY_F3 : (INPUT_key_shift ? AKEY_PLUS : AKEY_LEFT)) ^ shiftctrl;
	case SDLK_RIGHT:
		return (!UI_is_active && Atari800_f_keys ? AKEY_F4 : (INPUT_key_shift ? AKEY_ASTERISK : AKEY_RIGHT)) ^ shiftctrl;
	case SDLK_UP:
		return (!UI_is_active && Atari800_f_keys ? AKEY_F1 : (INPUT_key_shift ? AKEY_MINUS : AKEY_UP)) ^ shiftctrl;
	case SDLK_DOWN:
		return (!UI_is_active && Atari800_f_keys ? AKEY_F2 : (INPUT_key_shift ? AKEY_EQUAL : AKEY_DOWN)) ^ shiftctrl;
	case SDLK_ESCAPE:
		/* Windows takes ctrl+esc and ctrl+shift+esc */
		return AKEY_ESCAPE ^ shiftctrl;
	case SDLK_TAB:
#if HAVE_WINDOWS_H
		/* On Windows, when an SDL window has focus and LAlt+Tab is pressed,
		   a window-switching menu appears, but the LAlt+Tab key sequence is
		   still forwarded to the SDL window. In the effect the user cannot
		   switch with LAlt+Tab without the emulator registering unwanted key
		   presses. On other operating systems (e.g. GNU/Linux/KDE) everything
		   is OK, the key sequence is not registered by the emulator. This
		   hack fixes the behaviour on Windows. */
		if (kbhits[SDLK_LALT]) {
			key_pressed = 0;
			/* 1. In fullscreen software (non-OpenGL) mode, user presses LAlt, then presses Tab.
			      Atari800 window gets minimised and the window-switching menu appears.
			   2. User switches back to Atari800 without releasing LAlt.
			   3. User releases LAlt. Atari800 gets switched back to fullscreen.
			   In the above situation, the emulator would register pressing of LAlt but
			   would not register releasing of the key. It would think that LAlt is still
			   pressed. The hack below fixes the issue by causing SDL to assume LAlt is
			   not pressed. */
#if HAVE_OPENGL
			if (!VIDEOMODE_windowed && !SDL_VIDEO_opengl)
#else
			if (!VIDEOMODE_windowed)
#endif /* HAVE_OPENGL */
				kbhits[SDLK_LALT] = 0;
			return AKEY_NONE;
		}
#endif /* HAVE_WINDOWS_H */
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

void SDL_INPUT_Mouse(void)
{
	Uint8 buttons;

	if(INPUT_direct_mouse) {
		int potx, poty;

		buttons = SDL_GetMouseState(&potx, &poty);
		if(potx < 0) potx = 0;
		if(poty < 0) poty = 0;
		potx = (double)potx * (228.0 / (double)SDL_VIDEO_width);
		poty = (double)poty * (228.0 / (double)SDL_VIDEO_height);
		if(potx > 227) potx = 227;
		if(poty > 227) poty = 227;
		POKEY_POT_input[INPUT_mouse_port << 1] = 227 - potx;
		POKEY_POT_input[(INPUT_mouse_port << 1) + 1] = 227 - poty;
	} else {
		buttons = SDL_GetRelativeMouseState(&INPUT_mouse_delta_x, &INPUT_mouse_delta_y);
	}

	INPUT_mouse_buttons =
		((buttons & SDL_BUTTON(1)) ? 1 : 0) | /* Left button */
		((buttons & SDL_BUTTON(3)) ? 2 : 0) | /* Right button */
		((buttons & SDL_BUTTON(2)) ? 4 : 0); /* Middle button */
}

static void Init_SDL_Joysticks(int first, int second)
{
	int i;
	joysticks_found = 0;
	for(i = 0; i < SDL_NumJoysticks() && i < MAX_JOYSTICKS; i++) {
		joystick[joysticks_found] = SDL_JoystickOpen(i);
		if (joystick[joysticks_found] == NULL)
			Log_print("Joystick %i not found", i);
		else {
			Log_print("Joystick %i found", i);
			joystick_nbuttons[joysticks_found] = SDL_JoystickNumButtons(joystick[i]);
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
			if (joystick_nbuttons[joysticks_found] > OSK_MAX_BUTTONS)
				joystick_nbuttons[joysticks_found] = OSK_MAX_BUTTONS;
#endif
			joysticks_found++;
		}
	}
}

int SDL_INPUT_Initialise(int *argc, char *argv[])
{
	/* TODO check for errors! */
#ifdef LPTJOY
	char *lpt_joy0 = NULL;
	char *lpt_joy1 = NULL;
#endif /* LPTJOY */
	int i;
	int j;
	int no_joystick = FALSE;
	int help_only = FALSE;

	for(i = 0; i < MAX_JOYSTICKS; i++) {
		sdl_js_state[i].port = INPUT_STICK_CENTRE;
		sdl_js_state[i].trig = 0;
	}

	for (i = j = 1; i < *argc; i++) {
#ifdef LPTJOY
		int i_a = (i + 1 < *argc);		/* is argument available? */
#endif /* LPTJOY */
		int a_m = FALSE;			/* error, argument missing! */
		if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = TRUE;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-grabmouse") == 0) {
			grab_mouse = TRUE;
		}

                else if (strcmp(argv[i], "-joy0hat") == 0) {
                        real_js_configs[0].use_hat = TRUE;
                }
                else if (strcmp(argv[i], "-joy1hat") == 0) {
                        real_js_configs[1].use_hat = TRUE;
                }
                else if (strcmp(argv[i], "-joy2hat") == 0) {
                        real_js_configs[2].use_hat = TRUE;
                }
                else if (strcmp(argv[i], "-joy3hat") == 0) {
                        real_js_configs[3].use_hat = TRUE;
                }
#ifdef LPTJOY
		else if (strcmp(argv[i], "-joy0") == 0) {
			if (i_a) {
				lpt_joy0 = argv[++i];
			}
			else a_m = TRUE;
		}
		else if (!strcmp(argv[i], "-joy1")) {
			if (i_a) {
				lpt_joy1 = argv[++i];
			}
			else a_m = TRUE;
		}
#endif /* LPTJOY */
		else if (strcmp(argv[i], "-kbdjoy0") == 0) {
			PLATFORM_kbd_joy_0_enabled = TRUE;
		}
		else if (!strcmp(argv[i], "-kbdjoy1")) {
			PLATFORM_kbd_joy_1_enabled = TRUE;
		}
		else if (strcmp(argv[i], "-no-kbdjoy0") == 0) {
			PLATFORM_kbd_joy_0_enabled = FALSE;
		}
		else if (!strcmp(argv[i], "-no-kbdjoy1")) {
			PLATFORM_kbd_joy_1_enabled = FALSE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-nojoystick      Disable joystick");
                                Log_print("\t-joy0hat         Use hat of joystick 0");
                                Log_print("\t-joy1hat         Use hat of joystick 1");
                                Log_print("\t-joy2hat         Use hat of joystick 1");
                                Log_print("\t-joy3hat         Use hat of joystick 1");
#ifdef LPTJOY
				Log_print("\t-joy0 <pathname> Select LPTjoy0 device");
				Log_print("\t-joy1 <pathname> Select LPTjoy1 device");
#endif /* LPTJOY */
				Log_print("\t-kbdjoy0         enable joystick 0 keyboard emulation");
				Log_print("\t-kbdjoy1         enable joystick 1 keyboard emulation");
				Log_print("\t-no-kbdjoy0      disable joystick 0 keyboard emulation");
				Log_print("\t-no-kbdjoy1      disable joystick 1 keyboard emulation");

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

	if (help_only)
		return TRUE;

	if (!no_joystick) {
#ifdef LPTJOY
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

	if (INPUT_cx85) { /* disable keyboard joystick if using CX85 numpad */
		PLATFORM_kbd_joy_0_enabled = 0;
	}
	if(grab_mouse)
		SDL_WM_GrabInput(SDL_GRAB_ON);

	kbhits = SDL_GetKeyState(NULL);

	if (kbhits == NULL) {
		Log_print("SDL_GetKeyState() failed");
		Log_flushlog();
		return FALSE;
	}

	return TRUE;
}

void SDL_INPUT_Exit(void)
{
	SDL_WM_GrabInput(SDL_GRAB_OFF);
}

void SDL_INPUT_Restart(void)
{
	lastkey = SDLK_UNKNOWN;
	key_pressed = key_control = lastuni = 0;
	if(grab_mouse) SDL_WM_GrabInput(SDL_GRAB_ON);
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

static int get_SDL_joystick_hat_state(SDL_Joystick* joystick)
{
	Uint8 hat = SDL_JoystickGetHat(joystick, 0);

	if ((hat & SDL_HAT_LEFT)==SDL_HAT_LEFT) {
		if ((hat & SDL_HAT_LEFTDOWN)==SDL_HAT_LEFTDOWN) return INPUT_STICK_LL;
		if ((hat & SDL_HAT_LEFTUP)==SDL_HAT_LEFTUP) return INPUT_STICK_UL;
		return INPUT_STICK_LEFT;
	}
	else if ((hat & SDL_HAT_RIGHT)==SDL_HAT_RIGHT) {
		if ((hat & SDL_HAT_RIGHTDOWN)==SDL_HAT_RIGHTDOWN) return INPUT_STICK_LR;
		else if ((hat & SDL_HAT_RIGHTUP)==SDL_HAT_RIGHTUP) return INPUT_STICK_UR;
		return INPUT_STICK_RIGHT;
	}
	else if ((hat & SDL_HAT_UP)==SDL_HAT_UP) {
		return INPUT_STICK_FORWARD;
	}
	else if ((hat & SDL_HAT_DOWN)==SDL_HAT_DOWN) {
		return INPUT_STICK_BACK;
	}

	return INPUT_STICK_CENTRE;
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

static void update_SDL_joysticks(void)
{
	int joy;

	if (! joysticks_found)
		return;

	SDL_JoystickUpdate();

	for(joy = 0; joy < joysticks_found; joy++) {
		int i;

		if (real_js_configs[joy].use_hat == FALSE) {
			sdl_js_state[joy].port = get_SDL_joystick_state(joystick[joy]);
		}
		else {
			sdl_js_state[joy].port = get_SDL_joystick_hat_state(joystick[joy]);
		}

		sdl_js_state[joy].trig = 0;
		for (i = 0; i < joystick_nbuttons[joy]; i++) {
			if (SDL_JoystickGetButton(joystick[joy], i)) {
				sdl_js_state[joy].trig |= 1 << i;
			}
		}
	}
}

static void get_platform_PORT(Uint8 *s0, Uint8 *s1, Uint8 *s2, Uint8 *s3)
{
	int stick0, stick1;
	stick0 = stick1 = INPUT_STICK_CENTRE;

	if (PLATFORM_kbd_joy_0_enabled) {
		if (kbhits[KBD_STICK_0_LEFT])
			stick0 &= INPUT_STICK_LEFT;
		if (kbhits[KBD_STICK_0_RIGHT])
			stick0 &= INPUT_STICK_RIGHT;
		if (kbhits[KBD_STICK_0_UP])
			stick0 &= INPUT_STICK_FORWARD;
		if (kbhits[KBD_STICK_0_DOWN])
			stick0 &= INPUT_STICK_BACK;
	}
	if (PLATFORM_kbd_joy_1_enabled) {
		if (kbhits[KBD_STICK_1_LEFT])
			stick1 &= INPUT_STICK_LEFT;
		if (kbhits[KBD_STICK_1_RIGHT])
			stick1 &= INPUT_STICK_RIGHT;
		if (kbhits[KBD_STICK_1_UP])
			stick1 &= INPUT_STICK_FORWARD;
		if (kbhits[KBD_STICK_1_DOWN])
			stick1 &= INPUT_STICK_BACK;
	}

	if (swap_joysticks) {
		*s1 = stick0;
		*s0 = stick1;
	}
	else {
		*s0 = stick0;
		*s1 = stick1;
	}

	if (fd_joystick0 != -1)
		*s0 &= get_LPT_joystick_state(fd_joystick0);
	else if (joystick[0] != NULL)
		*s0 &= sdl_js_state[0].port;

	if (fd_joystick1 != -1)
		*s1 &= get_LPT_joystick_state(fd_joystick1);
	else if (joystick[1] != NULL)
		*s1 &= sdl_js_state[1].port;

	*s2 = sdl_js_state[2].port;
	*s3 = sdl_js_state[3].port;
}

static void get_platform_TRIG(Uint8 *t0, Uint8 *t1, Uint8 *t2, Uint8 *t3)
{
	int trig0, trig1;
	trig0 = trig1 = 1;

	if (PLATFORM_kbd_joy_0_enabled) {
		trig0 = !kbhits[KBD_TRIG_0];
	}

	if (PLATFORM_kbd_joy_1_enabled) {
		trig1 = !kbhits[KBD_TRIG_1];
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
		*t0 &= ((status & 8) > 0);
#endif /* LPTJOY */
	}
	else if (joystick[0] != NULL) {
		trig0 = 1;
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
		if (OSK_enabled) {
			if (sdl_js_state[0].trig & (1 << OSK_BUTTON_TRIGGER))
				trig0 = 0;
		}
		else
#endif
			if (sdl_js_state[0].trig)
				trig0 = 0;
		*t0 &= trig0;
	}

	if (fd_joystick1 != -1) {
#ifdef LPTJOY
		int status;
		ioctl(fd_joystick1, LPGETSTATUS, &status);
		*t1 &= ((status & 8) > 0);
#endif /* LPTJOY */
	}
	else if (joystick[1] != NULL) {
		trig1 = 1;
		if (sdl_js_state[1].trig)
			trig1 = 0;
		*t1 &= trig1;
	}

	*t2 = sdl_js_state[2].trig ? 0 : 1;
	*t3 = sdl_js_state[3].trig ? 0 : 1;
}

int PLATFORM_PORT(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b, c, d;
	update_SDL_joysticks();
	get_platform_PORT(&a, &b, &c, &d);
	if (num == 0) {
		return (b << 4) | (a & 0x0f);
	}
	else if (num == 1) {
		return (d << 4) | (c & 0x0f);
	}
#endif
	return 0xff;
}

int PLATFORM_TRIG(int num)
{
#ifndef DONT_DISPLAY
	UBYTE a, b, c, d;
	get_platform_TRIG(&a, &b, &c, &d);
	switch (num) {
	case 0:
		return a;
	case 1:
		return b;
	case 2:
		return c;
	case 3:
		return d;
	default:
		break;
	}
#endif
	return 1;
}

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD

#define REPEAT_DELAY 100  /* in ms */
#define REPEAT_INI_DELAY (5 * REPEAT_DELAY)

int UI_BASIC_in_kbui;

static int ui_leave_in_progress;   /* was 'b_ui_leave' */

/*
 * do some basic keyboard emulation using the joystick controller
 */
static int SDL_controller_kb1(void)
{
	static int prev_up = FALSE, prev_down = FALSE, prev_trigger = FALSE,
		prev_keyb = FALSE, prev_left = FALSE, prev_right = FALSE,
		prev_leave = FALSE, prev_ui = FALSE;
	static int repdelay_timeout = REPEAT_DELAY;
	struct js_state *state = &sdl_js_state[0];

	if (! joysticks_found) return(AKEY_NONE);  /* no controller present */

	update_SDL_joysticks();

	if (!UI_is_active && (state->trig & (1 << OSK_BUTTON_UI))) {
		return(AKEY_UI);
	}
	if (!UI_is_active && (state->trig & (1 << OSK_BUTTON_KEYB))) {
		return(AKEY_KEYB);
	}
	/* provide keyboard emulation to enter file name */
	if (UI_is_active && !UI_BASIC_in_kbui && (state->trig & (1 << OSK_BUTTON_KEYB))) {
		int keycode;
		update_SDL_joysticks();
		UI_BASIC_in_kbui = TRUE;
		memcpy(atari_screen_backup, Screen_atari, Screen_HEIGHT * Screen_WIDTH);
		keycode = UI_BASIC_OnScreenKeyboard(NULL, -1);
		memcpy(Screen_atari, atari_screen_backup, Screen_HEIGHT * Screen_WIDTH);
		Screen_EntireDirty();
		PLATFORM_DisplayScreen();
		UI_BASIC_in_kbui = FALSE;
		return keycode;
#if 0 /* @@@ 26-Mar-2013, chris: check this */
		if (inject_key != AKEY_NONE) {
			keycode = inject_key;
			inject_key = AKEY_NONE;
			return(keycode);
		}
		else {
			return(AKEY_NONE);
		}
#endif
	}

	if (UI_is_active || UI_BASIC_in_kbui) {
		if (!(state->port & 1)) {
			prev_down = FALSE;
			if (! prev_up) {
				repdelay_timeout = SDL_GetTicks() + REPEAT_INI_DELAY;
				prev_up = 1;
				return(AKEY_UP);
			}
			else {
				if (SDL_GetTicks() > repdelay_timeout) {
					repdelay_timeout = SDL_GetTicks() + REPEAT_DELAY;
					return(AKEY_UP);
				}
			}
		}
		else {
			prev_up = FALSE;
		}

		if (!(state->port & 2)) {
			prev_up = FALSE;
			if (! prev_down) {
				repdelay_timeout = SDL_GetTicks() + REPEAT_INI_DELAY;
				prev_down = TRUE;
				return(AKEY_DOWN);
			}
			else {
				if (SDL_GetTicks() > repdelay_timeout) {
					repdelay_timeout = SDL_GetTicks() + REPEAT_DELAY;
					return(AKEY_DOWN);
				}
			}
		}
		else {
			prev_down = FALSE;
		}

		if (!(state->port & 4)) {
			prev_right = FALSE;
			if (! prev_left) {
				repdelay_timeout = SDL_GetTicks() + REPEAT_INI_DELAY;
				prev_left = TRUE;
				return(AKEY_LEFT);
			}
			else {
				if (SDL_GetTicks() > repdelay_timeout) {
					repdelay_timeout = SDL_GetTicks() + REPEAT_DELAY;
					return(AKEY_LEFT);
				}
			}
		}
		else {
			prev_left = FALSE;
		}

		if (!(state->port & 8)) {
			prev_left = FALSE;
			if (! prev_right) {
				repdelay_timeout = SDL_GetTicks() + REPEAT_INI_DELAY;
				prev_right = TRUE;
				return(AKEY_RIGHT);
			}
			else {
				if (SDL_GetTicks() > repdelay_timeout) {
					repdelay_timeout = SDL_GetTicks() + REPEAT_DELAY;
					return(AKEY_RIGHT);
				}
			}
		}
		else {
			prev_right = FALSE;
		}


		if ((state->trig & (1 << OSK_BUTTON_TRIGGER))) {
			if (! prev_trigger) {
				prev_trigger = TRUE;
				return(AKEY_RETURN);
			}
		}
		else {
			prev_trigger = FALSE;
		}

		if ((state->trig & (1 << OSK_BUTTON_LEAVE))) {
			if (! prev_leave) {
				prev_leave = TRUE;
				ui_leave_in_progress = TRUE;   /* OSK_BUTTON_LEAVE must be released again */
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_leave = FALSE;
		}

		if ((state->trig & (1 << OSK_BUTTON_UI))) {
			if (! prev_ui && UI_BASIC_in_kbui) {
				prev_ui = TRUE;
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_ui = FALSE;
		}

		if ((state->trig & (1 << OSK_BUTTON_KEYB))) {
			if (! prev_keyb) {
				prev_keyb = TRUE;
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_keyb = FALSE;
		}
	}
	return(AKEY_NONE);
}

static int SDL_controller_kb(void)
{
	int key = SDL_controller_kb1();
#ifdef DEBUG
	if (key != AKEY_NONE) printf("SDL_controller_kb: key = 0x%x\n", key);
#endif
	return key;
}

static int SDL_consol_keys(void)
{
	struct js_state *state = &sdl_js_state[0];

	INPUT_key_consol = INPUT_CONSOL_NONE;

#if OSK_BUTTON_START != OSK_BUTTON_LEAVE
#error FIXME: make button assignments configurable
#endif
	if (Atari800_machine_type != Atari800_MACHINE_5200) {
		if (! (UI_is_active || UI_BASIC_in_kbui)) {
			if ((state->trig & (1 << OSK_BUTTON_START))) {
				if (! ui_leave_in_progress)
					INPUT_key_consol &= ~INPUT_CONSOL_START;
				else
					INPUT_key_consol |= INPUT_CONSOL_START;
			}
			else {
				ui_leave_in_progress = FALSE;
				INPUT_key_consol |= INPUT_CONSOL_START;
			}

			if ((state->trig & (1 << OSK_BUTTON_SELECT)))
				INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
			else
				INPUT_key_consol |= INPUT_CONSOL_SELECT;

			if ((state->trig & (1 << OSK_BUTTON_OPTION)))
				INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
			else
				INPUT_key_consol |= INPUT_CONSOL_OPTION;
		}
	}
	else {
		/* @@@ 5200: TODO @@@ */
	}
	return(AKEY_NONE);
}

#endif /* #ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD */
