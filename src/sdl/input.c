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

#if SDL2
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#endif

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

#ifndef M_PI
# define M_PI 3.141592653589793
#endif

static int grab_mouse = FALSE;

/* Per-Atari-port input source assignment */
#define MAX_HOST_JOYSTICKS 16
#define MAX_LPT_JOYSTICKS 2
#define JOY_MODE_NONE        0
#define JOY_MODE_KBD0        1
#define JOY_MODE_KBD1        2
#define JOY_MODE_PARALLEL    3
#define JOY_MODE_HOST_JOY    4

static int joy_port_mode[4] = {JOY_MODE_NONE, JOY_MODE_NONE, JOY_MODE_NONE, JOY_MODE_NONE};
static int joy_port_param[4] = {0, 0, 0, 0};
static char joy_port_name[4][256] = {{0}};
static int joy_port_has_name[4] = {0};
static int joy_port_slot[4] = {0};
static int paddle_pot_axis[4][2] = {{2,3},{2,3},{2,3},{2,3}};
static int paddle_fire_btn[4][2] = {{4,5},{4,5},{4,5},{4,5}};
static int kbd_layout_active[2] = {1, 0};

static SDL_Joystick *host_joys[MAX_HOST_JOYSTICKS];
static int n_host_joys = 0;
static int host_joy_slot[MAX_HOST_JOYSTICKS];

#ifdef LPTJOY
static int lpt_fd[MAX_LPT_JOYSTICKS] = {-1, -1};
static int n_lpt_joys = 0;
static char lpt_path[MAX_LPT_JOYSTICKS][FILENAME_MAX];
#endif

/* joystick emulation (via stick_dev.kbd)
   keys are loaded from config file
   Here the defaults if there is no keymap in the config file,
   in the order up, down, left, right, trigger */
static int kbd_stick0[5] = {
#if SDL2
	SDLK_KP_8, SDLK_KP_5, SDLK_KP_4, SDLK_KP_6, SDLK_RCTRL
#else
	SDLK_KP8, SDLK_KP5, SDLK_KP4, SDLK_KP6, SDLK_RCTRL
#endif
};
static int kbd_stick1[5] = {
	SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_LCTRL
};
#define KBD_STICK_0_UP kbd_stick0[0]
#define KBD_STICK_0_DOWN kbd_stick0[1]
#define KBD_STICK_0_LEFT kbd_stick0[2]
#define KBD_STICK_0_RIGHT kbd_stick0[3]
#define KBD_TRIG_0 kbd_stick0[4]
#define KBD_STICK_1_UP kbd_stick1[0]
#define KBD_STICK_1_DOWN kbd_stick1[1]
#define KBD_STICK_1_LEFT kbd_stick1[2]
#define KBD_STICK_1_RIGHT kbd_stick1[3]
#define KBD_TRIG_1 kbd_stick1[4]

/* Maping for the START, RESET, OPTION, SELECT and EXIT keys */
static int KBD_UI = SDLK_F1;
static int KBD_OPTION = SDLK_F2;
static int KBD_SELECT = SDLK_F3;
static int KBD_START = SDLK_F4;
static int KBD_RESET = SDLK_F5;
static int KBD_HELP = SDLK_F6;
static int KBD_BREAK = SDLK_F7;
static int KBD_MON = SDLK_F8;
static int KBD_EXIT = SDLK_F9;
static int KBD_SSHOT = SDLK_F10;
static int KBD_TURBO = SDLK_F12;

/* Each emulated joystick can take its input from host keyboard, an
   LPT joystick, an actual SDL joystick, or a combination thereof. */
#define MAX_JOYSTICKS	4
static struct stick_dev {
	int *kbd;
	int fd_lpt;
	SDL_Joystick *sdl_joy;
	int nbuttons;
	SDL_INPUT_RealJSConfig_t real_config;
} stick_devs[MAX_JOYSTICKS];

#define minjoy 10000			/* real joystick tolerancy */

/* keyboard */
#if SDL2
# define KEYBOARD_CONST	const
#else
# define KEYBOARD_CONST
#endif
static KEYBOARD_CONST Uint8 *kbhits;

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
static struct stick_dev *osk_stick = NULL;
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

static void Init_SDL_Joysticks(void);

static int get_key_state(const Uint8* kbhits, int keycode) {
#if SDL2
	keycode &= ~SDLK_SCANCODE_MASK;
#endif
	return kbhits[keycode];
}

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

#if SDL2
	if (ksym > SDLK_UNKNOWN) {
#else
	if (ksym > SDLK_FIRST && ksym < SDLK_LAST) {
#endif
		*retval = ksym;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/* Fill stick_devs[0..3] from joy_port_mode[] / joy_port_param[].
   Called during init and whenever port config changes via UI. */
static void apply_port_mapping(void) {
	int i;
	kbd_layout_active[0] = 0;
	kbd_layout_active[1] = 0;
	for (i = 0; i < MAX_JOYSTICKS; i++) {
		struct stick_dev *s = &stick_devs[i];
		s->kbd = NULL;
		s->fd_lpt = -1;
		s->sdl_joy = NULL;
		s->nbuttons = 0;
		switch (joy_port_mode[i]) {
		case JOY_MODE_KBD0:
			s->kbd = kbd_stick0;
			kbd_layout_active[0] = 1;
			break;
		case JOY_MODE_KBD1:
			s->kbd = kbd_stick1;
			kbd_layout_active[1] = 1;
			break;
#ifdef LPTJOY
		case JOY_MODE_PARALLEL: {
			int idx = joy_port_param[i];
			if (idx >= 0 && idx < n_lpt_joys)
				s->fd_lpt = lpt_fd[idx];
			break;
		}
#endif
		case JOY_MODE_HOST_JOY:
		case JOY_MODE_PADDLE: {
			int idx = joy_port_param[i];
			if (idx >= 0 && idx < n_host_joys && host_joys[idx] != NULL) {
				s->sdl_joy = host_joys[idx];
				s->nbuttons = SDL_JoystickNumButtons(host_joys[idx]);
			}
			break;
		}
		default:
			break;
		}
	}
}

/*Set real joystick to use hat instead of axis*/
static int set_real_js_use_hat(int joyIndex, const char* parm)
{
    stick_devs[joyIndex].real_config.use_hat = Util_sscandec(parm) != 0 ? TRUE : FALSE;
    return TRUE;
}

static int set_real_js_axes(int joyIndex, const char* parm) {
#if SDL2
    stick_devs[joyIndex].real_config.axes = Util_sscandec(parm) == 0 ? 0 : 2;
    return TRUE;
#else
	return FALSE;
#endif
}

static int set_real_js_diagonals(int joyIndex, const char* parm) {
#if SDL2
	int zone = Util_sscandec(parm);
    stick_devs[joyIndex].real_config.diagonal_zones =
		zone == 1 ? JoystickNarrowDiagonalsZone :
			zone == 2 ? JoystickWideDiagonalsZone : JoystickNoDiagonals;
    return TRUE;
#else
	return FALSE;
#endif
}

static int set_real_js_actions(int joyIndex, char* params) {
#if SDL2
	int btn = 0;
	char* p = strtok(params, ",");
	while (p) {
		if (btn < INPUT_JOYSTICK_MAX_BUTTONS) {
    		stick_devs[joyIndex].real_config.buttons[btn++].action = atoi(p);
		}
		else {
			break;
		}
		p = strtok(NULL, ",");
	}
    return TRUE;
#else
	return FALSE;
#endif
}

static int set_real_js_keys(int joyIndex, char* params) {
#if SDL2
	int btn = 0;
	char* p = strtok(params, ",");
	while (p) {
		if (btn < INPUT_JOYSTICK_MAX_BUTTONS) {
    		stick_devs[joyIndex].real_config.buttons[btn++].key = atoi(p);
		}
		else {
			break;
		}
		p = strtok(NULL, ",");
	}
    return TRUE;
#else
	return FALSE;
#endif
}

/*Reset configurations of the real joysticks*/
static void reset_real_js_configs(void)
{
    int i;
    for (i = 0; i < MAX_JOYSTICKS; i++) {
        stick_devs[i].real_config.use_hat = FALSE;
#if SDL2
        stick_devs[i].real_config.axes = 0;
        stick_devs[i].real_config.diagonal_zones = JoystickNarrowDiagonalsZone;
		for (int btn = 0; btn < INPUT_JOYSTICK_MAX_BUTTONS; ++btn) {
			switch (btn) {
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				stick_devs[i].real_config.buttons[btn].action = JoystickUiAction;
				stick_devs[i].real_config.buttons[btn].key = AKEY_CONTROLLER_BUTTON_TRIGGER;
				break;
			case SDL_CONTROLLER_BUTTON_A:
				stick_devs[i].real_config.buttons[btn].action = JoystickAtariKey;
				stick_devs[i].real_config.buttons[btn].key = AKEY_START;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				stick_devs[i].real_config.buttons[btn].action = JoystickAtariKey;
				stick_devs[i].real_config.buttons[btn].key = AKEY_SELECT;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				stick_devs[i].real_config.buttons[btn].action = JoystickAtariKey;
				stick_devs[i].real_config.buttons[btn].key = AKEY_OPTION;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				stick_devs[i].real_config.buttons[btn].action = JoystickUiAction;
				stick_devs[i].real_config.buttons[btn].key = AKEY_WARMSTART;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
				stick_devs[i].real_config.buttons[btn].action = JoystickUiAction;
				stick_devs[i].real_config.buttons[btn].key = AKEY_TURBO;
				break;
			case SDL_CONTROLLER_BUTTON_START:
				stick_devs[i].real_config.buttons[btn].action = JoystickUiAction;
				stick_devs[i].real_config.buttons[btn].key = UI_MENU_RUN;
				break;
			default:
				stick_devs[i].real_config.buttons[btn].action = JoystickNoAction;
				stick_devs[i].real_config.buttons[btn].key = 0;
				break;
			}
		}
#endif
		paddle_pot_axis[i][0] = 2;
		paddle_pot_axis[i][1] = 3;
		paddle_fire_btn[i][0] = 4;
		paddle_fire_btn[i][1] = 5;
    }
}

/* Configuration for SDL1 and SDL2 keys is separate; SDL2 redefined key codes */
#if SDL2
# define KEY_SDL "SDL2_"
#else
# define KEY_SDL "SDL_"
#endif

/*Write configurations of real joysticks*/
static void write_real_js_configs(FILE* fp)
{
    int i;
    for (i = 0; i < MAX_JOYSTICKS; i++) {
        fprintf(fp, KEY_SDL"JOY_%d_USE_HAT=%d\n", i, stick_devs[i].real_config.use_hat);
#if SDL2
        fprintf(fp, KEY_SDL"JOY_%d_AXES=%d\n", i, stick_devs[i].real_config.axes);
        fprintf(fp, KEY_SDL"JOY_%d_DIAGONALS=%d\n", i, stick_devs[i].real_config.diagonal_zones);
        fprintf(fp, KEY_SDL"JOY_%d_BUTTON_ACTIONS=", i);
		for (int btn = 0; btn < INPUT_JOYSTICK_MAX_BUTTONS; ++btn) {
        	fprintf(fp, "%d,", stick_devs[i].real_config.buttons[btn].action);
		}
        fprintf(fp, "\n");
        fprintf(fp, KEY_SDL"JOY_%d_BUTTON_KEYS=", i);
		for (int btn = 0; btn < INPUT_JOYSTICK_MAX_BUTTONS; ++btn) {
        	fprintf(fp, "%d,", stick_devs[i].real_config.buttons[btn].key);
		}
        fprintf(fp, "\n");
#endif
    }
}

/*Get pointer to a real joystick configuration*/
SDL_INPUT_RealJSConfig_t* SDL_INPUT_GetRealJSConfig(int joyIndex)
{
    return &stick_devs[joyIndex].real_config;
}

/* Return number of detected SDL joysticks opened in host_joys[] */
int SDL_INPUT_GetNumHostJoysticks(void) {
	return n_host_joys;
}

/* Return number of opened parallel port joysticks (Linux LPT only) */
int SDL_INPUT_GetNumLPTJoysticks(void) {
	return n_lpt_joys;
}

/* Return display name of host joystick at given index, or NULL */
const char *SDL_INPUT_GetHostJoystickName(int index) {
	if (index >= 0 && index < n_host_joys && host_joys[index] != NULL)
#if SDL2
		return SDL_JoystickName(host_joys[index]);
#else
		return SDL_JoystickName(index);
#endif
	return NULL;
}

/* Return display name with slot suffix when multiple joysticks share the same name */
const char *SDL_INPUT_GetHostJoystickDisplayName(int index) {
	const char *name = SDL_INPUT_GetHostJoystickName(index);
	if (!name) return "?";
	if (index >= 0 && index < MAX_HOST_JOYSTICKS && host_joy_slot[index] >= 0) {
		static char display_names[MAX_HOST_JOYSTICKS][48];
		snprintf(display_names[index], sizeof(display_names[index]), "%s #%d", name, host_joy_slot[index] + 1);
		return display_names[index];
	}
	return name;
}

/* Return current input source mode for Atari joystick port */
int SDL_INPUT_GetPortMode(int port) {
	if (port >= 0 && port < MAX_JOYSTICKS)
		return joy_port_mode[port];
	return JOY_MODE_NONE;
}

/* Return current input source parameter for Atari joystick port
   (host joystick index, parallel port index, or 0 for keyboard) */
int SDL_INPUT_GetPortParam(int port) {
	if (port >= 0 && port < MAX_JOYSTICKS)
		return joy_port_param[port];
	return 0;
}

/* Assign input source to an Atari joystick port.
   Updates joy_port_mode/param and records host joystick name for
   persistent name-based matching across reboots. */
void SDL_INPUT_SetPortMode(int port, int mode, int param) {
	if (port >= 0 && port < MAX_JOYSTICKS) {
		joy_port_mode[port] = mode;
		joy_port_param[port] = param;
		if (mode == JOY_MODE_HOST_JOY || mode == JOY_MODE_PADDLE) {
			const char *name = SDL_INPUT_GetHostJoystickName(param);
			if (name) {
				Util_strlcpy(joy_port_name[port], name, sizeof(joy_port_name[port]));
				joy_port_has_name[port] = 1;
			}
		} else {
			joy_port_name[port][0] = '\0';
			joy_port_has_name[port] = 0;
			joy_port_slot[port] = 0;
		}
		apply_port_mapping();
	}
}

void SDL_INPUT_SetPortPaddleConfig(int port, int potA, int potB, int btnA, int btnB) {
	if (port >= 0 && port < MAX_JOYSTICKS) {
		paddle_pot_axis[port][0] = potA;
		paddle_pot_axis[port][1] = potB;
		paddle_fire_btn[port][0] = btnA;
		paddle_fire_btn[port][1] = btnB;
	}
}

void SDL_INPUT_GetPortPaddleConfig(int port, int *potA, int *potB, int *btnA, int *btnB) {
	if (potA) *potA = paddle_pot_axis[port][0];
	if (potB) *potB = paddle_pot_axis[port][1];
	if (btnA) *btnA = paddle_fire_btn[port][0];
	if (btnB) *btnB = paddle_fire_btn[port][1];
}

/* For getting sdl key map out of the config...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
static int was_config_initialized = FALSE;
int SDL_INPUT_ReadConfig(char *option, char *parameters)
{
	if (was_config_initialized == FALSE) {
		reset_real_js_configs();
		was_config_initialized=TRUE;
	}

	if (strcmp(option, KEY_SDL"JOY_PORT_0_MODE") == 0) {
		joy_port_mode[0] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_0_NAME") == 0) {
		Util_strlcpy(joy_port_name[0], parameters, sizeof(joy_port_name[0]));
		joy_port_has_name[0] = 1;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_0_SLOT") == 0) {
		joy_port_slot[0] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_1_MODE") == 0) {
		joy_port_mode[1] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_1_NAME") == 0) {
		Util_strlcpy(joy_port_name[1], parameters, sizeof(joy_port_name[1]));
		joy_port_has_name[1] = 1;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_1_SLOT") == 0) {
		joy_port_slot[1] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_2_MODE") == 0) {
		joy_port_mode[2] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_2_NAME") == 0) {
		Util_strlcpy(joy_port_name[2], parameters, sizeof(joy_port_name[2]));
		joy_port_has_name[2] = 1;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_2_SLOT") == 0) {
		joy_port_slot[2] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_3_MODE") == 0) {
		joy_port_mode[3] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_3_NAME") == 0) {
		Util_strlcpy(joy_port_name[3], parameters, sizeof(joy_port_name[3]));
		joy_port_has_name[3] = 1;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_3_SLOT") == 0) {
		joy_port_slot[3] = Util_sscandec(parameters);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_0_PADDLE_AXES") == 0) {
		sscanf(parameters, "%d,%d", &paddle_pot_axis[0][0], &paddle_pot_axis[0][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_0_PADDLE_BUTTONS") == 0) {
		sscanf(parameters, "%d,%d", &paddle_fire_btn[0][0], &paddle_fire_btn[0][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_1_PADDLE_AXES") == 0) {
		sscanf(parameters, "%d,%d", &paddle_pot_axis[1][0], &paddle_pot_axis[1][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_1_PADDLE_BUTTONS") == 0) {
		sscanf(parameters, "%d,%d", &paddle_fire_btn[1][0], &paddle_fire_btn[1][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_2_PADDLE_AXES") == 0) {
		sscanf(parameters, "%d,%d", &paddle_pot_axis[2][0], &paddle_pot_axis[2][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_2_PADDLE_BUTTONS") == 0) {
		sscanf(parameters, "%d,%d", &paddle_fire_btn[2][0], &paddle_fire_btn[2][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_3_PADDLE_AXES") == 0) {
		sscanf(parameters, "%d,%d", &paddle_pot_axis[3][0], &paddle_pot_axis[3][1]);
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_PORT_3_PADDLE_BUTTONS") == 0) {
		sscanf(parameters, "%d,%d", &paddle_fire_btn[3][0], &paddle_fire_btn[3][1]);
		return TRUE;
	}
	/* backward compat: old enable keys map to port mode */
	else if (strcmp(option, KEY_SDL"JOY_0_ENABLED") == 0) {
		joy_port_mode[0] = (parameters != NULL && parameters[0] != '0') ? JOY_MODE_KBD0 : JOY_MODE_NONE;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_1_ENABLED") == 0) {
		joy_port_mode[1] = (parameters != NULL && parameters[0] != '0') ? JOY_MODE_KBD1 : JOY_MODE_NONE;
		return TRUE;
	}
	else if (strcmp(option, KEY_SDL"JOY_0_LEFT") == 0)
		return SDLKeyBind(&KBD_STICK_0_LEFT, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_RIGHT") == 0)
		return SDLKeyBind(&KBD_STICK_0_RIGHT, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_DOWN") == 0)
		return SDLKeyBind(&KBD_STICK_0_DOWN, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_UP") == 0)
		return SDLKeyBind(&KBD_STICK_0_UP, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_TRIGGER") == 0)
		return SDLKeyBind(&KBD_TRIG_0, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_LEFT") == 0)
		return SDLKeyBind(&KBD_STICK_1_LEFT, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_RIGHT") == 0)
		return SDLKeyBind(&KBD_STICK_1_RIGHT, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_DOWN") == 0)
		return SDLKeyBind(&KBD_STICK_1_DOWN, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_UP") == 0)
		return SDLKeyBind(&KBD_STICK_1_UP, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_TRIGGER") == 0)
		return SDLKeyBind(&KBD_TRIG_1, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_USE_HAT") == 0)
		return set_real_js_use_hat(0,parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_USE_HAT") == 0)
		return set_real_js_use_hat(1,parameters);
	else if (strcmp(option, KEY_SDL"JOY_2_USE_HAT") == 0)
		return set_real_js_use_hat(2,parameters);
	else if (strcmp(option, KEY_SDL"JOY_3_USE_HAT") == 0)
		return set_real_js_use_hat(3,parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_AXES") == 0)
		return set_real_js_axes(0,parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_AXES") == 0)
		return set_real_js_axes(1,parameters);
	else if (strcmp(option, KEY_SDL"JOY_2_AXES") == 0)
		return set_real_js_axes(2,parameters);
	else if (strcmp(option, KEY_SDL"JOY_3_AXES") == 0)
		return set_real_js_axes(3,parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_DIAGONALS") == 0)
		return set_real_js_diagonals(0,parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_DIAGONALS") == 0)
		return set_real_js_diagonals(1,parameters);
	else if (strcmp(option, KEY_SDL"JOY_2_DIAGONALS") == 0)
		return set_real_js_diagonals(2,parameters);
	else if (strcmp(option, KEY_SDL"JOY_3_DIAGONALS") == 0)
		return set_real_js_diagonals(3,parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_BUTTON_ACTIONS") == 0)
		return set_real_js_actions(0, parameters);
	else if (strcmp(option, KEY_SDL"JOY_0_BUTTON_KEYS") == 0)
		return set_real_js_keys(0, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_BUTTON_ACTIONS") == 0)
		return set_real_js_actions(1, parameters);
	else if (strcmp(option, KEY_SDL"JOY_1_BUTTON_KEYS") == 0)
		return set_real_js_keys(1, parameters);
	else if (strcmp(option, KEY_SDL"JOY_2_BUTTON_ACTIONS") == 0)
		return set_real_js_actions(2, parameters);
	else if (strcmp(option, KEY_SDL"JOY_2_BUTTON_KEYS") == 0)
		return set_real_js_keys(2, parameters);
	else if (strcmp(option, KEY_SDL"JOY_3_BUTTON_ACTIONS") == 0)
		return set_real_js_actions(3, parameters);
	else if (strcmp(option, KEY_SDL"JOY_3_BUTTON_KEYS") == 0)
		return set_real_js_keys(3, parameters);
	else if (strcmp(option, KEY_SDL"UI_KEY") == 0)
		return SDLKeyBind(&KBD_UI, parameters);
	else if (strcmp(option, KEY_SDL"OPTION_KEY") == 0)
		return SDLKeyBind(&KBD_OPTION, parameters);
	else if (strcmp(option, KEY_SDL"SELECT_KEY") == 0)
		return SDLKeyBind(&KBD_SELECT, parameters);
	else if (strcmp(option, KEY_SDL"START_KEY") == 0)
		return SDLKeyBind(&KBD_START, parameters);
	else if (strcmp(option, KEY_SDL"RESET_KEY") == 0)
		return SDLKeyBind(&KBD_RESET, parameters);
	else if (strcmp(option, KEY_SDL"HELP_KEY") == 0)
		return SDLKeyBind(&KBD_HELP, parameters);
	else if (strcmp(option, KEY_SDL"BREAK_KEY") == 0)
		return SDLKeyBind(&KBD_BREAK, parameters);
	else if (strcmp(option, KEY_SDL"MON_KEY") == 0)
		return SDLKeyBind(&KBD_MON, parameters);
	else if (strcmp(option, KEY_SDL"EXIT_KEY") == 0)
		return SDLKeyBind(&KBD_EXIT, parameters);
	else if (strcmp(option, KEY_SDL"SSHOT_KEY") == 0)
		return SDLKeyBind(&KBD_SSHOT, parameters);
	else if (strcmp(option, KEY_SDL"TURBO_KEY") == 0)
		return SDLKeyBind(&KBD_TURBO, parameters);
	else
		return FALSE;
}

/* Save the keybindings and the keybindapp options to the config file...
   Authors: B.Schreiber, A.Martinez
   cleaned up by joy */
void SDL_INPUT_WriteConfig(FILE *fp)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++) {
		int s = joy_port_param[i] >= 0 && joy_port_param[i] < MAX_HOST_JOYSTICKS ? host_joy_slot[joy_port_param[i]] : -1;
		fprintf(fp, KEY_SDL"JOY_PORT_%d_MODE=%d\n", i, joy_port_mode[i]);
		fprintf(fp, KEY_SDL"JOY_PORT_%d_NAME=%s\n", i, joy_port_name[i]);
		fprintf(fp, KEY_SDL"JOY_PORT_%d_SLOT=%d\n", i, s >= 0 ? s : 0);
		fprintf(fp, KEY_SDL"JOY_PORT_%d_PADDLE_AXES=%d,%d\n", i, paddle_pot_axis[i][0], paddle_pot_axis[i][1]);
		fprintf(fp, KEY_SDL"JOY_PORT_%d_PADDLE_BUTTONS=%d,%d\n", i, paddle_fire_btn[i][0], paddle_fire_btn[i][1]);
		fprintf(fp, KEY_SDL"JOY_PORT_%d_SLOT=%d\n", i, joy_port_param[i]);
	}
	fprintf(fp, KEY_SDL"JOY_0_LEFT=%d\n", KBD_STICK_0_LEFT);
	fprintf(fp, KEY_SDL"JOY_0_RIGHT=%d\n", KBD_STICK_0_RIGHT);
	fprintf(fp, KEY_SDL"JOY_0_UP=%d\n", KBD_STICK_0_UP);
	fprintf(fp, KEY_SDL"JOY_0_DOWN=%d\n", KBD_STICK_0_DOWN);
	fprintf(fp, KEY_SDL"JOY_0_TRIGGER=%d\n", KBD_TRIG_0);

	fprintf(fp, KEY_SDL"JOY_1_LEFT=%d\n", KBD_STICK_1_LEFT);
	fprintf(fp, KEY_SDL"JOY_1_RIGHT=%d\n", KBD_STICK_1_RIGHT);
	fprintf(fp, KEY_SDL"JOY_1_UP=%d\n", KBD_STICK_1_UP);
	fprintf(fp, KEY_SDL"JOY_1_DOWN=%d\n", KBD_STICK_1_DOWN);
	fprintf(fp, KEY_SDL"JOY_1_TRIGGER=%d\n", KBD_TRIG_1);

	fprintf(fp, KEY_SDL"UI_KEY=%d\n", KBD_UI);
	fprintf(fp, KEY_SDL"OPTION_KEY=%d\n", KBD_OPTION);
	fprintf(fp, KEY_SDL"SELECT_KEY=%d\n", KBD_SELECT);
	fprintf(fp, KEY_SDL"START_KEY=%d\n", KBD_START);
	fprintf(fp, KEY_SDL"RESET_KEY=%d\n", KBD_RESET);
	fprintf(fp, KEY_SDL"HELP_KEY=%d\n", KBD_HELP);
	fprintf(fp, KEY_SDL"BREAK_KEY=%d\n", KBD_BREAK);
	fprintf(fp, KEY_SDL"MON_KEY=%d\n", KBD_MON);
	fprintf(fp, KEY_SDL"EXIT_KEY=%d\n", KBD_EXIT);
	fprintf(fp, KEY_SDL"SSHOT_KEY=%d\n", KBD_SSHOT);
	fprintf(fp, KEY_SDL"TURBO_KEY=%d\n", KBD_TURBO);

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
#if SDL2
	typedef SDL_Keycode SDLKey;
#endif
	const char *key = "";
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
#if SDL2
		SDL_Delay(10);
#endif
	}
}

#if SDL2
static struct INPUT_joystick_button* JoyButtonPress(SDL_JoystickID id, int button) {
	if (button < 0 || button >= INPUT_JOYSTICK_MAX_BUTTONS) return NULL;

	for (int i = 0; i < MAX_JOYSTICKS; ++i) {
		struct stick_dev* joy = &stick_devs[i];
		if (joy->sdl_joy && SDL_JoystickInstanceID(joy->sdl_joy) == id) {
			return &joy->real_config.buttons[button];
		}
	}

	return NULL;
}
#endif

static int lastkey = SDLK_UNKNOWN, key_pressed = 0, key_control = 0;
static int lastuni = 0;
static int previous_caps_state = -1;

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
	int keyboad_event_found = FALSE;

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

#if !defined(SDL12_COMPAT_HEADERS) || SDL_VERSIONNUM >= 1270
	/* Very ugly fix for SDL CAPSLOCK brokenness.  This will let the user
	 * press CAPSLOCK and get a brief keypress on the Atari but it is not
	 * possible to emulate holding down CAPSLOCK for longer periods with
	 * the broken SDL*/
	if (lastkey == SDLK_CAPSLOCK) {
		lastkey = SDLK_UNKNOWN;
		key_pressed = 0;
 		lastuni = 0;
	}
#endif

	while (!keyboad_event_found && SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			keyboad_event_found = TRUE;

			/* If the KEYDOWN event we get here is OPTION, SELECT, or START, then ignore it here as it's supposed to be it's own independent (i.e., separate from the keyboard) subsystem. */
			if ((event.key.keysym.sym == KBD_OPTION)
				|| (event.key.keysym.sym == KBD_SELECT)
				|| (event.key.keysym.sym == KBD_START)
				|| (event.key.keysym.sym == SDLK_LSHIFT)
				|| (event.key.keysym.sym == SDLK_RSHIFT)
				|| (event.key.keysym.sym == SDLK_LCTRL)
				|| (event.key.keysym.sym == SDLK_RCTRL)
			        || (event.key.keysym.sym == SDLK_CAPSLOCK))
				break;

			lastkey = event.key.keysym.sym;
#if SDL2
 			lastuni = 0;
#else
 			lastuni = event.key.keysym.unicode;
#endif
			key_pressed = 1;
			break;
		case SDL_KEYUP:
			keyboad_event_found = TRUE;

			/* Need to ignore KEYUP as well... */
			if ((event.key.keysym.sym == KBD_OPTION)
				|| (event.key.keysym.sym == KBD_SELECT)
				|| (event.key.keysym.sym == KBD_START)
				|| (event.key.keysym.sym == SDLK_LSHIFT)
				|| (event.key.keysym.sym == SDLK_RSHIFT)
				|| (event.key.keysym.sym == SDLK_LCTRL)
				|| (event.key.keysym.sym == SDLK_RCTRL)
			        || (event.key.keysym.sym == SDLK_CAPSLOCK))
				break;

			lastkey = event.key.keysym.sym;
 			lastuni = 0; /* event.key.keysym.unicode is not defined for KEYUP */
			key_pressed = 0;
#if !defined(SDL12_COMPAT_HEADERS) || SDL_VERSIONNUM >= 1270
			/* ugly hack to fix broken SDL CAPSLOCK*/
			/* Because SDL is only sending Keydown and keyup for every change
			 * of state of the CAPSLOCK status, rather than the actual key.*/
			if(lastkey == SDLK_CAPSLOCK) {
				key_pressed = 1;
			}
#endif
			break;
#if SDL2
		case SDL_TEXTINPUT:
 			lastuni = *event.text.text;
			break;

		case SDL_JOYDEVICEADDED:
		case SDL_JOYDEVICEREMOVED:
		 	Init_SDL_Joysticks();
			break;

		case SDL_JOYBUTTONDOWN: {
			int button = event.jbutton.button;
			struct INPUT_joystick_button* b = JoyButtonPress(event.jbutton.which, button);
			if (UI_is_active) {
				if (button > SDL_CONTROLLER_BUTTON_INVALID && button <= SDL_CONTROLLER_BUTTON_MISC1) {
					return AKEY_CONTROLLER_BUTTON_FIRST + button;
				}
			}
			else if (b) {
				if (b->action == JoystickUiAction) {
					if (b->key >= 0) {
						UI_alt_function = b->key;
						return AKEY_UI;
					}
					UI_alt_function = -1;
					return b->key;
				}
				else if (b->action == JoystickAtariKey && b->key) {
					switch (b->key) {
					case AKEY_START:
						INPUT_key_consol &= ~INPUT_CONSOL_START;
						break;
					case AKEY_SELECT:
						INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
						break;
					case AKEY_OPTION:
						INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
						break;
					case AKEY_HELP:
						return AKEY_HELP ^ shiftctrl;
					}
					return b->key;
				}
				else if (b->action == JoystickKeyboard && b->key) {
					lastkey = b->key;
					lastuni = 0;
					key_pressed = 1;
				}
			}
		} break;

		case SDL_JOYBUTTONUP: {
			int button = event.jbutton.button;
			struct INPUT_joystick_button* b = JoyButtonPress(event.jbutton.which, button);
			if (!UI_is_active && b) {
				if (b->action == JoystickAtariKey && b->key) {
					switch (b->key) {
					case AKEY_START:
						INPUT_key_consol |= INPUT_CONSOL_START;
						break;
					case AKEY_SELECT:
						INPUT_key_consol |= INPUT_CONSOL_SELECT;
						break;
					case AKEY_OPTION:
						INPUT_key_consol |= INPUT_CONSOL_OPTION;
						break;
					case AKEY_HELP:
						return AKEY_NONE;
					}
					return AKEY_NONE;
				}
				else if (b->action == JoystickKeyboard && b->key) {
					lastkey = b->key;
					lastuni = 0;
					key_pressed = 0;
				}
			}
		} break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			case SDL_WINDOWEVENT_RESIZED:
				VIDEOMODE_SetWindowSize(event.window.data1, event.window.data2);
				break;
			case SDL_WINDOWEVENT_EXPOSED:
				PLATFORM_DisplayScreen();
				break;
			}
			break;
#else
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
#endif
		case SDL_QUIT:
			return AKEY_EXIT;
			break;
#if HAVE_WINDOWS_H && !SDL2
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

	if (!keyboad_event_found && !key_pressed) {
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
		SDL_consol_keys();
		return SDL_controller_kb();
#else
		return AKEY_NONE;
#endif
	}

	UI_alt_function = -1;
	if (get_key_state(kbhits, SDLK_LALT)) {
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
			case SDLK_v:
				UI_alt_function = UI_MENU_VIDEO_RECORDING;
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
#if SDL2
	/* TODO - there's no mouse grab function in SDL2 */
#else
				SDL_WM_GrabInput(grab_mouse ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
				key_pressed = 0;
				break;
			case SDLK_1:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->hue > COLOURS_HUE_MIN)
						Colours_setup->hue -= 0.02;
				} else {
					if (Colours_setup->hue < COLOURS_HUE_MAX)
						Colours_setup->hue += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_2:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->saturation > COLOURS_SATURATION_MIN)
						Colours_setup->saturation -= 0.02;
				} else {
					if (Colours_setup->saturation < COLOURS_SATURATION_MAX)
						Colours_setup->saturation += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_3:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->contrast > COLOURS_CONTRAST_MIN)
						Colours_setup->contrast -= 0.04;
				} else {
					if (Colours_setup->contrast < COLOURS_CONTRAST_MAX)
					Colours_setup->contrast += 0.04;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_4:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->brightness > COLOURS_BRIGHTNESS_MIN)
						Colours_setup->brightness -= 0.04;
				} else {
					if (Colours_setup->brightness < COLOURS_BRIGHTNESS_MAX)
						Colours_setup->brightness += 0.04;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_5:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->gamma > COLOURS_GAMMA_MIN)
						Colours_setup->gamma -= 0.02;
				} else {
					if (Colours_setup->gamma < COLOURS_GAMMA_MAX)
						Colours_setup->gamma += 0.02;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_6:
				if (get_key_state(kbhits, SDLK_LSHIFT)) {
					if (Colours_setup->color_delay > COLOURS_DELAY_MIN)
						Colours_setup->color_delay -= 0.4;
				} else {
					if (Colours_setup->color_delay < COLOURS_DELAY_MAX)
						Colours_setup->color_delay += 0.4;
				}
				Colours_Update();
				return AKEY_NONE;
			case SDLK_LEFTBRACKET:
				if (get_key_state(kbhits, SDLK_LSHIFT))
					SDL_VIDEO_SetScanlinesPercentage(SDL_VIDEO_scanlines_percentage - 1);
				else
					SDL_VIDEO_SetScanlinesPercentage(SDL_VIDEO_scanlines_percentage + 1);
				return AKEY_NONE;
			default:
				if(FILTER_NTSC_emu != NULL){
					switch(lastkey){
					case SDLK_7:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
							if (FILTER_NTSC_setup.sharpness > FILTER_NTSC_SHARPNESS_MIN)
								FILTER_NTSC_setup.sharpness -= 0.02;
						} else {
							if (FILTER_NTSC_setup.sharpness < FILTER_NTSC_SHARPNESS_MAX)
								FILTER_NTSC_setup.sharpness += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_8:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
							if (FILTER_NTSC_setup.resolution > FILTER_NTSC_RESOLUTION_MIN)
								FILTER_NTSC_setup.resolution -= 0.02;
						} else {
							if (FILTER_NTSC_setup.resolution < FILTER_NTSC_RESOLUTION_MAX)
								FILTER_NTSC_setup.resolution += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_9:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
							if (FILTER_NTSC_setup.artifacts > FILTER_NTSC_ARTIFACTS_MIN)
								FILTER_NTSC_setup.artifacts -= 0.02;
						} else {
							if (FILTER_NTSC_setup.artifacts < FILTER_NTSC_ARTIFACTS_MAX)
								FILTER_NTSC_setup.artifacts += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_0:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
							if (FILTER_NTSC_setup.fringing > FILTER_NTSC_FRINGING_MIN)
								FILTER_NTSC_setup.fringing -= 0.02;
						} else {
							if (FILTER_NTSC_setup.fringing < FILTER_NTSC_FRINGING_MAX)
								FILTER_NTSC_setup.fringing += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_MINUS:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
							if (FILTER_NTSC_setup.bleed > FILTER_NTSC_BLEED_MIN)
								FILTER_NTSC_setup.bleed -= 0.02;
						} else {
							if (FILTER_NTSC_setup.bleed < FILTER_NTSC_BLEED_MAX)
								FILTER_NTSC_setup.bleed += 0.02;
						}
						FILTER_NTSC_Update(FILTER_NTSC_emu);
						return AKEY_NONE;
					case SDLK_EQUALS:
						if (get_key_state(kbhits, SDLK_LSHIFT)) {
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
	if (get_key_state(kbhits, SDLK_LSHIFT) || get_key_state(kbhits, SDLK_RSHIFT)) {
		INPUT_key_shift = 1;
		shiftctrl ^= AKEY_SHFT;
	} else {
		INPUT_key_shift = 0;
	}

	/* CONTROL STATE */
	if (get_key_state(kbhits, SDLK_LCTRL) || get_key_state(kbhits, SDLK_RCTRL)) {
		key_control = 1;
		shiftctrl ^= AKEY_CTRL;
	} else {
		key_control = 0;
	}

	/*
	if (event.type == 2 || event.type == 3) {
		Log_print("E:%x S:%x C:%x K:%x U:%x M:%x",event.type,INPUT_key_shift,key_control,lastkey,event.key.keysym.unicode,event.key.keysym.mod);
	}
	*/

	BINLOAD_pause_loading = FALSE;

	/* OPTION / SELECT / START keys */
	INPUT_key_consol = INPUT_CONSOL_NONE;
	if (get_key_state(kbhits, KBD_OPTION))
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
	if (get_key_state(kbhits, KBD_SELECT))
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
	if (get_key_state(kbhits, KBD_START)) {
		INPUT_key_consol &= ~INPUT_CONSOL_START;
		/* Special case: 5200's START is mapped to console START */
		if (Atari800_machine_type == Atari800_MACHINE_5200 && !UI_is_active)
			return AKEY_5200_START;
	}

	/* CAPSLOCK Handling via Modifier State Sync */
	{
	  int current_caps_state = (SDL_GetModState() & KMOD_CAPS) != 0;
	  if (previous_caps_state == -1) {
	    previous_caps_state = current_caps_state;
	  }
	  else if (current_caps_state != previous_caps_state) {
	    previous_caps_state = current_caps_state;
	    return AKEY_CAPSTOGGLE | shiftctrl;
	  }
	}

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
	if (lastkey == KBD_UI) {
		key_pressed = 0;
		return AKEY_UI;
	}
	if (lastkey == KBD_MON) {
		UI_alt_function = UI_MENU_MONITOR;
	}
	if (lastkey == KBD_HELP) {
		return AKEY_HELP ^ shiftctrl;
	}
	if (lastkey == KBD_BREAK) {
		if (BINLOAD_wait_active) {
			BINLOAD_pause_loading = TRUE;
			return AKEY_NONE;
		}
		else
			return AKEY_BREAK;
	}
	if (lastkey == KBD_SSHOT) {
		key_pressed = 0;
		return INPUT_key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
	}
	if (lastkey == KBD_TURBO) {
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
	if (!UI_is_active && kbd_layout_active[0]) {
		if (lastkey == KBD_STICK_0_LEFT || lastkey == KBD_STICK_0_RIGHT ||
			lastkey == KBD_STICK_0_UP || lastkey == KBD_STICK_0_DOWN || lastkey == KBD_TRIG_0) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (!UI_is_active && kbd_layout_active[1]) {
		if (lastkey == KBD_STICK_1_LEFT || lastkey == KBD_STICK_1_RIGHT ||
			lastkey == KBD_STICK_1_UP || lastkey == KBD_STICK_1_DOWN || lastkey == KBD_TRIG_1) {
			key_pressed = 0;
			return AKEY_NONE;
		}
	}

	if (Atari800_machine_type == Atari800_MACHINE_5200 && !UI_is_active) {
		switch (lastuni) {
		case 'p':
			return AKEY_5200_PAUSE;
		case 'r':
			return AKEY_5200_RESET;
		case '0':
			return AKEY_5200_0;
		case '1':
			return AKEY_5200_1;
		case '2':
			return AKEY_5200_2;
		case '3':
			return AKEY_5200_3;
		case '4':
			return AKEY_5200_4;
		case '5':
			return AKEY_5200_5;
		case '6':
			return AKEY_5200_6;
		case '7':
			return AKEY_5200_7;
		case '8':
			return AKEY_5200_8;
		case '9':
			return AKEY_5200_9;
		case '#':
		case '=':
			return AKEY_5200_HASH;
		case '*':
			return AKEY_5200_ASTERISK;
		}
		return AKEY_NONE;
	}

	switch (lastkey) {
	case SDLK_BACKQUOTE:
		return AKEY_ATARI ^ shiftctrl;
#if !SDL2
		/* These are the "Windows" keys, but they don't work on Windows*/
	case SDLK_LSUPER:
		return AKEY_ATARI ^ shiftctrl;
	case SDLK_RSUPER:
		return AKEY_CAPSTOGGLE;
#endif
	case SDLK_END:
	case SDLK_PAGEDOWN:
		return AKEY_F2 | AKEY_SHFT;
	case SDLK_PAGEUP:
		return AKEY_F1 | AKEY_SHFT;
	case SDLK_HOME:
		return key_control ? AKEY_LESS|shiftctrl : AKEY_CLEAR;
	case SDLK_PAUSE:
	case SDLK_CAPSLOCK:
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
		if (get_key_state(kbhits, SDLK_LALT)) {
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
#if !SDL2
				kbhits[SDLK_LALT] = 0;
#endif
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
#if SDL2
#else
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
#endif
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
		if (lastuni == 0 && lastkey >= SDLK_a && lastkey <= SDLK_z)
			lastuni = lastkey - SDLK_a + 1;
		/* lastuni is set 0 only in SDL2 even when Ctrl key
		   is pressed along with another key but we know that
		   the control key is currently down so we set lastuni
		   to the ASCII equivalent for control code (1-26)
		   only for alpha keys so that they can be handled
		   properly below.
		   in SDL1.2, lastuni is set properly already, so it
		   will have a non-zero value.
		*/
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
		return AKEY_A|shiftctrl;
	case 'B':
		return AKEY_B|shiftctrl;
	case 'C':
		return AKEY_C|shiftctrl;
	case 'D':
		return AKEY_D|shiftctrl;
	case 'E':
		return AKEY_E|shiftctrl;
	case 'F':
		return AKEY_F|shiftctrl;
	case 'G':
		return AKEY_G|shiftctrl;
	case 'H':
		return AKEY_H|shiftctrl;
	case 'I':
		return AKEY_I|shiftctrl;
	case 'J':
		return AKEY_J|shiftctrl;
	case 'K':
		return AKEY_K|shiftctrl;
	case 'L':
		return AKEY_L|shiftctrl;
	case 'M':
		return AKEY_M|shiftctrl;
	case 'N':
		return AKEY_N|shiftctrl;
	case 'O':
		return AKEY_O|shiftctrl;
	case 'P':
		return AKEY_P|shiftctrl;
	case 'Q':
		return AKEY_Q|shiftctrl;
	case 'R':
		return AKEY_R|shiftctrl;
	case 'S':
		return AKEY_S|shiftctrl;
	case 'T':
		return AKEY_T|shiftctrl;
	case 'U':
		return AKEY_U|shiftctrl;
	case 'V':
		return AKEY_V|shiftctrl;
	case 'W':
		return AKEY_W|shiftctrl;
	case 'X':
		return AKEY_X|shiftctrl;
	case 'Y':
		return AKEY_Y|shiftctrl;
	case 'Z':
		return AKEY_Z|shiftctrl;
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
		return AKEY_a|shiftctrl;
	case 'b':
		return AKEY_b|shiftctrl;
	case 'c':
		return AKEY_c|shiftctrl;
	case 'd':
		return AKEY_d|shiftctrl;
	case 'e':
		return AKEY_e|shiftctrl;
	case 'f':
		return AKEY_f|shiftctrl;
	case 'g':
		return AKEY_g|shiftctrl;
	case 'h':
		return AKEY_h|shiftctrl;
	case 'i':
		return AKEY_i|shiftctrl;
	case 'j':
		return AKEY_j|shiftctrl;
	case 'k':
		return AKEY_k|shiftctrl;
	case 'l':
		return AKEY_l|shiftctrl;
	case 'm':
		return AKEY_m|shiftctrl;
	case 'n':
		return AKEY_n|shiftctrl;
	case 'o':
		return AKEY_o|shiftctrl;
	case 'p':
		return AKEY_p|shiftctrl;
	case 'q':
		return AKEY_q|shiftctrl;
	case 'r':
		return AKEY_r|shiftctrl;
	case 's':
		return AKEY_s|shiftctrl;
	case 't':
		return AKEY_t|shiftctrl;
	case 'u':
		return AKEY_u|shiftctrl;
	case 'v':
		return AKEY_v|shiftctrl;
	case 'w':
		return AKEY_w|shiftctrl;
	case 'x':
		return AKEY_x|shiftctrl;
	case 'y':
		return AKEY_y|shiftctrl;
	case 'z':
		return AKEY_z|shiftctrl;
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

static void Init_SDL_Joysticks(void)
{
	int i;

#if SDL2
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
#else
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
#endif
		Log_print("SDL_InitSubSystem FAILED: %s", SDL_GetError());
		Log_flushlog();
		exit(-1);
	}

	/* close previously opened joysticks */
	for (i = 0; i < n_host_joys; i++) {
		if (host_joys[i] != NULL)
			SDL_JoystickClose(host_joys[i]);
	}
	n_host_joys = 0;

	n_host_joys = SDL_NumJoysticks();
	if (n_host_joys > MAX_HOST_JOYSTICKS)
		n_host_joys = MAX_HOST_JOYSTICKS;
	for (i = 0; i < n_host_joys; i++) {
		host_joys[i] = SDL_JoystickOpen(i);
		if (host_joys[i] == NULL) {
			Log_print("Joystick %i not found", i);
		} else {
			Log_print("Joystick %i: %s", i,
#if SDL2
				SDL_JoystickName(host_joys[i]));
#else
				SDL_JoystickName(i));
#endif
		}
	}

	apply_port_mapping();

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
	osk_stick = NULL;
	for (i = 0; i < MAX_JOYSTICKS; i++) {
		if (stick_devs[i].sdl_joy != NULL && osk_stick == NULL) {
			osk_stick = &stick_devs[i];
			if (osk_stick->nbuttons > OSK_MAX_BUTTONS)
				osk_stick->nbuttons = OSK_MAX_BUTTONS;
		}
	}
#endif
}

int SDL_INPUT_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	int no_joystick = FALSE;
	int help_only = FALSE;

	for(i = 0; i < MAX_JOYSTICKS; i++) {
		stick_devs[i].kbd = NULL;
		stick_devs[i].fd_lpt = -1;
		stick_devs[i].sdl_joy = NULL;
		stick_devs[i].nbuttons = 0;
	}
	if (!was_config_initialized) {
		reset_real_js_configs();
	}

	for (i = j = 1; i < *argc; i++) {
#ifdef LPTJOY
		int i_a = (i + 1 < *argc);
#endif
		int a_m = FALSE;
		if (strcmp(argv[i], "-nojoystick") == 0) {
			no_joystick = TRUE;
			Log_print("no joystick");
		}
		else if (strcmp(argv[i], "-grabmouse") == 0) {
			grab_mouse = TRUE;
		}
		else if (strcmp(argv[i], "-joy0hat") == 0) {
			stick_devs[0].real_config.use_hat = TRUE;
		}
		else if (strcmp(argv[i], "-joy1hat") == 0) {
			stick_devs[1].real_config.use_hat = TRUE;
		}
		else if (strcmp(argv[i], "-joy2hat") == 0) {
			stick_devs[2].real_config.use_hat = TRUE;
		}
		else if (strcmp(argv[i], "-joy3hat") == 0) {
			stick_devs[3].real_config.use_hat = TRUE;
		}
#ifdef LPTJOY
		else if (strcmp(argv[i], "-joy0") == 0) {
			if (i_a && n_lpt_joys < MAX_LPT_JOYSTICKS) {
				Util_strlcpy(lpt_path[n_lpt_joys], argv[++i], FILENAME_MAX);
				lpt_fd[n_lpt_joys] = open(lpt_path[n_lpt_joys], O_RDONLY);
				if (lpt_fd[n_lpt_joys] == -1)
					perror(lpt_path[n_lpt_joys]);
				else
					n_lpt_joys++;
			}
			else a_m = TRUE;
		}
		else if (!strcmp(argv[i], "-joy1")) {
			if (i_a && n_lpt_joys < MAX_LPT_JOYSTICKS) {
				Util_strlcpy(lpt_path[n_lpt_joys], argv[++i], FILENAME_MAX);
				lpt_fd[n_lpt_joys] = open(lpt_path[n_lpt_joys], O_RDONLY);
				if (lpt_fd[n_lpt_joys] == -1)
					perror(lpt_path[n_lpt_joys]);
				else
					n_lpt_joys++;
			}
			else a_m = TRUE;
		}
#endif
		else if (strcmp(argv[i], "-kbdjoy0") == 0) {
			joy_port_mode[0] = JOY_MODE_KBD0;
		}
		else if (!strcmp(argv[i], "-kbdjoy1")) {
			joy_port_mode[1] = JOY_MODE_KBD1;
		}
		else if (strcmp(argv[i], "-no-kbdjoy0") == 0) {
			joy_port_mode[0] = JOY_MODE_NONE;
		}
		else if (!strcmp(argv[i], "-no-kbdjoy1")) {
			joy_port_mode[1] = JOY_MODE_NONE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-nojoystick      Disable joystick");
				Log_print("\t-joy0hat         Use hat of joystick 0");
				Log_print("\t-joy1hat         Use hat of joystick 1");
				Log_print("\t-joy2hat         Use hat of joystick 2");
				Log_print("\t-joy3hat         Use hat of joystick 3");
#ifdef LPTJOY
				Log_print("\t-joy0 <pathname> Select LPTjoy0 device");
				Log_print("\t-joy1 <pathname> Select LPTjoy1 device");
#endif
				Log_print("\t-kbdjoy0         Use keyboard as joystick port 0");
				Log_print("\t-kbdjoy1         Use keyboard as joystick port 1");
				Log_print("\t-no-kbdjoy0      Disable keyboard on joystick port 0");
				Log_print("\t-no-kbdjoy1      Disable keyboard on joystick port 1");
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

	if (INPUT_cx85) {
		if (joy_port_mode[0] == JOY_MODE_KBD0)
			joy_port_mode[0] = JOY_MODE_NONE;
	}

	if (!no_joystick) {
		Init_SDL_Joysticks();
		/* Compute slot for each host joystick */
		{
			int i, j;
		for (i = 0; i < n_host_joys; i++) {
			const char *ni = SDL_INPUT_GetHostJoystickName(i);
			int slot = 0, count = 0;
			for (j = 0; j < n_host_joys; j++) {
				const char *nj = SDL_INPUT_GetHostJoystickName(j);
				if (ni && nj && strcmp(ni, nj) == 0) {
					if (j < i) slot++;
					count++;
				}
			}
			host_joy_slot[i] = count > 1 ? slot : -1;
		}
		}
		/* Copy config slot to param for parallel ports */
		{
			int p;
			for (p = 0; p < MAX_JOYSTICKS; p++) {
				if (joy_port_mode[p] == JOY_MODE_PARALLEL)
					joy_port_param[p] = joy_port_slot[p];
			}
		}
		/* Resolve stored joystick names to current indices */
		{
			int p;
			for (p = 0; p < MAX_JOYSTICKS; p++) {
				if ((joy_port_mode[p] == JOY_MODE_HOST_JOY || joy_port_mode[p] == JOY_MODE_PADDLE) && joy_port_has_name[p] && joy_port_name[p][0]) {
					int found = -1;
					int slot_count = 0;
					int j;
					for (j = 0; j < n_host_joys; j++) {
						const char *jname = SDL_INPUT_GetHostJoystickName(j);
						if (jname && strcmp(jname, joy_port_name[p]) == 0) {
							if (slot_count == joy_port_slot[p]) {
								found = j;
								break;
							}
							slot_count++;
						}
					}
					if (found >= 0)
						joy_port_param[p] = found;
					else
						joy_port_mode[p] = JOY_MODE_NONE;
				}
			}
		}
		/* Apply smart defaults when no saved config was loaded:
		   0 host joysticks -> port 0 = Keyboard 1
		   1 host joystick  -> port 0 = Host0, port 1 = Keyboard 1
		   2+ host joysticks -> ports 0..N = Host0..N, no keyboard */
		if (joy_port_mode[0] == JOY_MODE_NONE) {
			if (n_host_joys > 0) {
				int i;
				for (i = 0; i < MAX_JOYSTICKS && i < n_host_joys; i++) {
					joy_port_mode[i] = JOY_MODE_HOST_JOY;
					joy_port_param[i] = i;
				}
				if (n_host_joys == 1 && MAX_JOYSTICKS > 1)
					joy_port_mode[1] = JOY_MODE_KBD0;
			} else
				joy_port_mode[0] = JOY_MODE_KBD0;
		}
	}

	apply_port_mapping();

#if SDL2
	kbhits = SDL_GetKeyboardState(NULL);
#else
	if(grab_mouse)
		SDL_WM_GrabInput(SDL_GRAB_ON);

	kbhits = SDL_GetKeyState(NULL);
#endif

	if (kbhits == NULL) {
		Log_print("SDL_GetKeyState() failed");
		Log_flushlog();
		return FALSE;
	}

	return TRUE;
}

void SDL_INPUT_Exit(void)
{
#if !SDL2
	SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
}

void SDL_INPUT_Restart(void)
{
	lastkey = SDLK_UNKNOWN;
	key_pressed = key_control = lastuni = 0;
#if !SDL2
	if(grab_mouse) SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
}

#if SDL2

int Atari_POT(int pot) {
	int val = 228;
	int joy = pot / 2;
	struct stick_dev* s = &stick_devs[joy];

	if (joy >= MAX_JOYSTICKS) {
		return val;
	}
	if (s->sdl_joy == NULL) {
		return val;
	}

	if (Atari800_machine_type == Atari800_MACHINE_5200)
		return val;
#ifdef EMULATE_PADDLE
	else if (joy_port_mode[joy] == JOY_MODE_PADDLE)
#endif
	{
		int axis = paddle_pot_axis[joy][pot & 1];
		val = SDL_JoystickGetAxis(s->sdl_joy, axis);
		val += 32768;
		val *= 255;
		val /= 65535;
		val = MAX - val;
		if (val < 1) val = 1;
	}
	return val;
}

static int joy_axes_zone_no_diagonals[] = {
	INPUT_STICK_FORWARD,
	INPUT_STICK_LEFT,
	INPUT_STICK_BACK,
	INPUT_STICK_RIGHT
};
static int joy_axes_zone_narrow_diagonals[] = {
	INPUT_STICK_FORWARD,
	INPUT_STICK_FORWARD & INPUT_STICK_LEFT,
	INPUT_STICK_LEFT,
	INPUT_STICK_LEFT,
	INPUT_STICK_LEFT & INPUT_STICK_BACK,
	INPUT_STICK_BACK,
	INPUT_STICK_BACK,
	INPUT_STICK_BACK & INPUT_STICK_RIGHT,
	INPUT_STICK_RIGHT,
	INPUT_STICK_RIGHT,
	INPUT_STICK_RIGHT & INPUT_STICK_FORWARD,
	INPUT_STICK_FORWARD
};

static int joy_axes_zone_wide_diagonals[] = {
	INPUT_STICK_FORWARD,
	INPUT_STICK_FORWARD & INPUT_STICK_LEFT,
	INPUT_STICK_LEFT,
	INPUT_STICK_LEFT & INPUT_STICK_BACK,
	INPUT_STICK_BACK,
	INPUT_STICK_BACK & INPUT_STICK_RIGHT,
	INPUT_STICK_RIGHT,
	INPUT_STICK_RIGHT & INPUT_STICK_FORWARD,
};

#endif /* SDL2 */

static int get_SDL_joystick_state(SDL_Joystick* joystick, SDL_INPUT_RealJSConfig_t* cfg)
{
#if SDL2
	int x = SDL_JoystickGetAxis(joystick, cfg->axes);
	int y = SDL_JoystickGetAxis(joystick, cfg->axes + 1);
	int stick = INPUT_STICK_CENTRE;

	// dead zone?
	if (abs(x) < minjoy && abs(y) < minjoy) return stick;

	// analog joy outside of the dead zone; determine the angle (make it 0 to 360):
	double angle = (atan2(x / 32768.0, y / 32768.0) + M_PI) * 180 / M_PI;

	if (cfg->diagonal_zones == JoystickNarrowDiagonalsZone) {
		// quantize the angle to find which angular zone the joystick is pointing to
		int q = (int)(angle / 30); // 12 zones
		stick &= joy_axes_zone_narrow_diagonals[q];
	}
	else if (cfg->diagonal_zones == JoystickWideDiagonalsZone) {
		// quantize the angle to find which angular zone the joystick is pointing to
		int q = (int)((angle + 22.5) / 45); // 8 zones
		stick &= joy_axes_zone_wide_diagonals[q % 8];
	}
	else { // do not report diagonal movement, only plain left/right/up/down
		// quantize the angle to find which direction the joystick is pointing
		int q = (int)((angle + 45) / 90); // 4 zones
		stick &= joy_axes_zone_no_diagonals[q % 4];
	}
	return stick;
#else
	int x = SDL_JoystickGetAxis(joystick, 0);
	int y = SDL_JoystickGetAxis(joystick, 1);
	int stick = INPUT_STICK_CENTRE;

	if (x > minjoy)
		stick &= INPUT_STICK_RIGHT;
	else if (x < -minjoy)
		stick &= INPUT_STICK_LEFT;
	if (y > minjoy)
		stick &= INPUT_STICK_BACK;
	else if (y < -minjoy)
		stick &= INPUT_STICK_FORWARD;
	return stick;
#endif
}

/* Read hat/D-pad direction as joystick direction bits.
   Uses SDL_JoystickGetHat() which works in both SDL1 and SDL2. */
static int get_SDL_joystick_hat_state(SDL_Joystick* joystick)
{
	Uint8 hat = SDL_JoystickGetHat(joystick, 0);
	int stick = INPUT_STICK_CENTRE;
	if (hat & SDL_HAT_LEFT)
		stick &= INPUT_STICK_LEFT;
	if (hat & SDL_HAT_RIGHT)
		stick &= INPUT_STICK_RIGHT;
	if (hat & SDL_HAT_UP)
		stick &= INPUT_STICK_FORWARD;
	if (hat & SDL_HAT_DOWN)
		stick &= INPUT_STICK_BACK;
	return stick;
}

#ifdef LPTJOY
static int get_LPT_joystick_state(int fd)
{
	int status, stick;

	ioctl(fd, LPGETSTATUS, &status);
	status ^= 0x78;
	stick = INPUT_STICK_CENTRE;
	if (status & 0x80)
		stick &= INPUT_STICK_LEFT;
	if (status & 0x40)
		stick &= INPUT_STICK_RIGHT;
	if (status & 0x20)
		stick &= INPUT_STICK_BACK;
	if (status & 0x10)
		stick &= INPUT_STICK_FORWARD;
	return stick;
}
#endif /* LPTJOY */

/* Read joystick directional state for one Atari port, combining
   keyboard, parallel port, and host joystick sources. */
static int single_stick_port(int num) {
	int port;
	struct stick_dev *s;

	port = INPUT_STICK_CENTRE;
	if (num >= MAX_JOYSTICKS) {
		return port;
	}
	s = &stick_devs[num];
#ifdef EMULATE_PADDLE
	if (joy_port_mode[num] == JOY_MODE_PADDLE) {
		if (s->sdl_joy != NULL) {
			if (SDL_JoystickGetButton(s->sdl_joy, paddle_fire_btn[num][0]))
				port &= ~0x04;
			if (SDL_JoystickGetButton(s->sdl_joy, paddle_fire_btn[num][1]))
				port &= ~0x08;
		}
		return port;
	}
#endif
	if (s->kbd != NULL) {
		int i;
		for (i = 0; i < 4; i++) {
			if (get_key_state(kbhits, s->kbd[i])) {
				port &= ~(1 << i);
			}
		}
	}
#ifdef LPTJOY
	if (s->fd_lpt != -1) {
		port &= get_LPT_joystick_state(s->fd_lpt);
	}
#endif
	if (s->sdl_joy != NULL) {
#if !SDL2
		SDL_JoystickUpdate();
#endif
		if (s->real_config.use_hat)
			port &= get_SDL_joystick_hat_state(s->sdl_joy);
		else
			port &= get_SDL_joystick_state(s->sdl_joy, &s->real_config);
	}
	return port;
}

int PLATFORM_PORT(int num)
{
#ifdef DONT_DISPLAY
	return 0xff;
#else
	return (single_stick_port(2 * num + 1) << 4) |
		(single_stick_port(2 * num) & 0x0f);
#endif
}

/* Read fire button state for one Atari port.
   SDL2: first checks buttons explicitly mapped as trigger, then
   falls back to any pressed button (SDL1-compatible behavior). */
int PLATFORM_TRIG(int num)
{
#ifdef DONT_DISPLAY
	return 1;
#else
	int trig;
	struct stick_dev *s;

	trig = 1;
	if (num >= MAX_JOYSTICKS) {
		return trig;
	}
	s = &stick_devs[num];
#ifdef EMULATE_PADDLE
	if (joy_port_mode[num] == JOY_MODE_PADDLE) {
		return trig;
	}
#endif
	if (s->kbd != NULL) {
		trig &= !get_key_state(kbhits, s->kbd[4]);
	}
#ifdef LPTJOY
	if (s->fd_lpt != -1) {
		int status;
		ioctl(s->fd_lpt, LPGETSTATUS, &status);
		if ((status & 8) > 0)
			trig = 0;
	}
#endif
	if (s->sdl_joy != NULL) {
#if SDL2
		int found_trigger = 0;
		for (int btn = 0; btn < INPUT_JOYSTICK_MAX_BUTTONS && btn < s->nbuttons; ++btn) {
			int down = SDL_JoystickGetButton(s->sdl_joy, btn);
			if (down && s->real_config.buttons[btn].action == JoystickUiAction && s->real_config.buttons[btn].key == AKEY_CONTROLLER_BUTTON_TRIGGER) {
				trig = 0;
				found_trigger = 1;
				break;
			}
		}
		if (!found_trigger) {
			for (int btn = 0; btn < s->nbuttons; btn++) {
				if (SDL_JoystickGetButton(s->sdl_joy, btn)) {
					trig = 0;
					break;
				}
			}
		}
#else
		int i;
		for (i = 0; i < s->nbuttons; i++) {
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
			if (s == osk_stick && i != OSK_BUTTON_TRIGGER) {
				continue;
			}
#endif
			if (SDL_JoystickGetButton(s->sdl_joy, i)) {
				trig = 0;
			}
		}
#endif
	}
	return trig;
#endif
}

#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD

#define REPEAT_DELAY 100  /* in ms */
#define REPEAT_INI_DELAY (5 * REPEAT_DELAY)

int UI_BASIC_in_kbui;

static int ui_leave_in_progress;   /* was 'b_ui_leave' */

static Uint8 osk_joystick_button(int button) {
	if (osk_stick == NULL || osk_stick->sdl_joy == NULL ||
	    button >= osk_stick->nbuttons) {
		return 0;
	}
	return SDL_JoystickGetButton(osk_stick->sdl_joy, button);
}

/*
 * do some basic keyboard emulation using the joystick controller
 */
static int SDL_controller_kb1(void)
{
	static int prev_up = FALSE, prev_down = FALSE, prev_trigger = FALSE,
		prev_keyb = FALSE, prev_left = FALSE, prev_right = FALSE,
		prev_leave = FALSE, prev_ui = FALSE;
	static int repdelay_timeout = REPEAT_DELAY;

	if (osk_stick == NULL) return(AKEY_NONE);  /* no controller present */

	SDL_JoystickUpdate();

	if (!UI_is_active && osk_joystick_button(OSK_BUTTON_UI)) {
		return(AKEY_UI);
	}
	if (!UI_is_active && osk_joystick_button(OSK_BUTTON_KEYB)) {
		return(AKEY_KEYB);
	}
	/* provide keyboard emulation to enter file name */
	if (UI_is_active && !UI_BASIC_in_kbui && osk_joystick_button(OSK_BUTTON_KEYB)) {
		int keycode;
		SDL_JoystickUpdate();
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
		int port;
		if (osk_stick->real_config.use_hat)
			port = get_SDL_joystick_hat_state(osk_stick->sdl_joy);
		else
			port = get_SDL_joystick_state(osk_stick->sdl_joy, &osk_stick->real_config);
		if (!(port & 1)) {
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

		if (!(port & 2)) {
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

		if (!(port & 4)) {
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

		if (!(port & 8)) {
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


		if (osk_joystick_button(OSK_BUTTON_TRIGGER)) {
			if (! prev_trigger) {
				prev_trigger = TRUE;
				return(AKEY_RETURN);
			}
		}
		else {
			prev_trigger = FALSE;
		}

		if (osk_joystick_button(OSK_BUTTON_LEAVE)) {
			if (! prev_leave) {
				prev_leave = TRUE;
				ui_leave_in_progress = TRUE;   /* OSK_BUTTON_LEAVE must be released again */
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_leave = FALSE;
		}

		if (osk_joystick_button(OSK_BUTTON_UI)) {
			if (! prev_ui && UI_BASIC_in_kbui) {
				prev_ui = TRUE;
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_ui = FALSE;
		}

		if (osk_joystick_button(OSK_BUTTON_KEYB)) {
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
	INPUT_key_consol = INPUT_CONSOL_NONE;

#if OSK_BUTTON_START != OSK_BUTTON_LEAVE
#error FIXME: make button assignments configurable
#endif
	if (Atari800_machine_type != Atari800_MACHINE_5200) {
		if (! (UI_is_active || UI_BASIC_in_kbui)) {
			if (osk_joystick_button(OSK_BUTTON_START)) {
				if (! ui_leave_in_progress)
					INPUT_key_consol &= ~INPUT_CONSOL_START;
				else
					INPUT_key_consol |= INPUT_CONSOL_START;
			}
			else {
				ui_leave_in_progress = FALSE;
				INPUT_key_consol |= INPUT_CONSOL_START;
			}

			if (osk_joystick_button(OSK_BUTTON_SELECT))
				INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
			else
				INPUT_key_consol |= INPUT_CONSOL_SELECT;

			if (osk_joystick_button(OSK_BUTTON_OPTION))
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
