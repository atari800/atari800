/*
 * androidinput.c - handle touch & keyboard events from android
 *
 * Copyright (C) 2010 Kostas Nakos
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
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

#include <string.h>
#include <pthread.h>

#include "input.h"
#include "akey.h"
#include "log.h"
#include "pokey.h"

#include "graphics.h"
#include "androidinput.h"
#include "keys.inc"

#define HIT_OPACITY 0.6f
#define POTLIMIT 228

#define KBD_MAXKEYS (1 << 4)
#define KBD_MASK    (KBD_MAXKEYS - 1)

struct touchstate
{
	int x;
	int y;
	int s;
};
enum
{
	PTRSTL = -1,
	PTRJOY = 0,
	PTRTRG,
	MAXPOINTERS
};
/* always: pointer 0 is joystick pointer, 1 is fire pointer */
static struct touchstate prevtc[MAXPOINTERS];
static int prevconptr;

int Android_Joyleft = TRUE;
float Android_Splitpct = 0.5f;
int Android_Split;

int Android_Paddle = FALSE;
SWORD Android_POTX = 0;
SWORD Android_POTY = 0;
int Android_PlanetaryDefense = FALSE;
UBYTE Android_ReversePddle = 0;

struct joy_overlay_state AndroidInput_JoyOvl;
struct consolekey_overlay_state AndroidInput_ConOvl;

UWORD Android_PortStatus;
UBYTE Android_TrigStatus;
static int Android_Keyboard[KBD_MAXKEYS];
static int key_head = 0, key_tail = 0;
static int Android_key_control;
static pthread_mutex_t key_mutex = PTHREAD_MUTEX_INITIALIZER;
static int key_last = AKEY_NONE;

static const int derot_lut[2][4] =
{
	{ KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN },	/* derot left */
	{ KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP }	/* derot right */
};
SWORD softjoymap[SOFTJOY_MAXKEYS + SOFTJOY_MAXACTIONS][2] =
{
	{ KEY_LEFT,  	INPUT_STICK_LEFT    },
	{ KEY_RIGHT, 	INPUT_STICK_RIGHT   },
	{ KEY_UP,    	INPUT_STICK_FORWARD },
	{ KEY_DOWN,  	INPUT_STICK_BACK    },
	{ '2',       	0                   },
	{ ACTION_NONE,	AKEY_NONE			},
	{ ACTION_NONE,	AKEY_NONE			},
	{ ACTION_NONE,	AKEY_NONE			}
};
int Android_SoftjoyEnable = TRUE;
int Android_DerotateKeys = 0;

int Android_TouchEvent(int x1, int y1, int s1, int x2, int y2, int s2)
{
	int joyptr;		/* will point to joystick touch of input set */
	int dx, dy, dx2, dy2;
	struct touchstate newtc[MAXPOINTERS];
	UBYTE newjoy, newtrig;
	struct joy_overlay_state *jovl;
	struct consolekey_overlay_state *covl;
	int conptr;		/* will point to stolen ptr, PTRSTL otherwise */
	int ret = 0;

	jovl = &AndroidInput_JoyOvl;
	covl = &AndroidInput_ConOvl;
	prevtc[PTRJOY].x = jovl->joyarea.l + ((jovl->joyarea.r - jovl->joyarea.l) >> 1);
	prevtc[PTRJOY].y = jovl->joyarea.t + ((jovl->joyarea.b - jovl->joyarea.t) >> 1);

	/* establish joy ptr & fire ptr for new input */
	/* note: looks complicated & uses boolean magick but gets rid of a labyrinth of ifs :-) */
	if (s2 < 0 /* only one touch event */
	    || (x1 >= Android_Split) ^ (x2 >= Android_Split)) {	  /* pointers on opposite sides */
		joyptr = (x1 < Android_Split) ^ Android_Joyleft;
	} else {							/* both pointers either on joystick or on fire side */
		int both_fire = (x1 >= Android_Split) ^ (!Android_Joyleft); /* both pointers on fire side */
		dx  = (x1 - prevtc[both_fire].x);			  /* figure out which is closer to previous */
		dx2 = (x2 - prevtc[both_fire].x);
		dy  = (y1 - prevtc[both_fire].y);
		dy2 = (y2 - prevtc[both_fire].y);
		joyptr = ((dx2*dx2 + dy2*dy2) > (dx*dx + dy*dy)) ^ !both_fire;
		s1 &= joyptr ^ (!both_fire);								/* unpress unrelated touch */
		s2 &= !(joyptr ^ (!both_fire));
	}
	if (joyptr) {
		newtc[PTRTRG].x = x1; newtc[PTRTRG].y = y1; newtc[PTRTRG].s = s1; 
		newtc[PTRJOY].x = x2; newtc[PTRJOY].y = y2; newtc[PTRJOY].s = s2; 
	} else {
		newtc[PTRJOY].x = x1; newtc[PTRJOY].y = y1; newtc[PTRJOY].s = s1; 
		newtc[PTRTRG].x = x2; newtc[PTRTRG].y = y2; newtc[PTRTRG].s = s2; 
	}

	if (newtc[PTRJOY].s > 0 || newtc[PTRTRG].s > 0)
		ret = 1;

	/* console keys */
	conptr = PTRSTL;
	covl->hitkey = CONK_NOKEY;
	if (covl->ovl_visible >= COVL_READY) {				/* first a quick bounding box check */
		if (newtc[PTRJOY].s > 0             &&
			newtc[PTRJOY].x >= covl->bbox.l &&
			newtc[PTRJOY].x <  covl->bbox.r &&
			newtc[PTRJOY].y >= covl->bbox.t &&
			newtc[PTRJOY].y <  covl->bbox.b)
					conptr = PTRJOY;				  /* implicit: mask fire by joy pointer */
		else if (newtc[PTRTRG].s > 0             &&
				 newtc[PTRTRG].x >= covl->bbox.l &&
				 newtc[PTRTRG].x <  covl->bbox.r &&
				 newtc[PTRTRG].y >= covl->bbox.t &&
				 newtc[PTRTRG].y <  covl->bbox.b)
					conptr = PTRTRG;
		if (conptr != PTRSTL) {	  /* if bb is exact on top & bottom => check only horiz/lly */
			int i;
			dy = covl->keycoo[1] - newtc[conptr].y;
			for (i = 0; i < CONK_VERT_MAX; i += 8) {
				float a = ((float) covl->keycoo[i + 6] - covl->keycoo[i    ]) /
					((float) covl->keycoo[i + 1] - covl->keycoo[i + 7]);
				dx = covl->keycoo[i] + a * dy;
				if (newtc[conptr].x < dx)	continue;					   /* off left edge */
				a = ((float) covl->keycoo[i + 4] - covl->keycoo[i + 2]) /
					((float) covl->keycoo[i + 3] - covl->keycoo[i + 5]);
				dx = covl->keycoo[i + 2] + a * dy;
				if (newtc[conptr].x > dx)	continue;					  /* off right edge */
				covl->hitkey = i / 8;										  /* hit inside */
				break;
			}
			if (covl->hitkey != CONK_NOKEY) {
				covl->opacity = COVL_MAX_OPACITY;
				covl->statecnt = COVL_HOLD_TIME << 1;
				covl->ovl_visible = COVL_READY;
				switch (covl->hitkey) {
				case CONK_START:
					INPUT_key_consol = INPUT_CONSOL_NONE ^ INPUT_CONSOL_START;
					break;
				case CONK_SELECT:
					INPUT_key_consol = INPUT_CONSOL_NONE ^ INPUT_CONSOL_SELECT;
					break;
				case CONK_OPTION:
					INPUT_key_consol = INPUT_CONSOL_NONE ^ INPUT_CONSOL_OPTION;
					break;
				case CONK_HELP:
					Keyboard_Enqueue(AKEY_HELP);
					break;
				default: /* CONK_NOKEY, CONK_RESET */
					;
				/* RESET is handled at the overlay update */
				}
			} else {
				conptr = PTRSTL;					   /* didn't hit - let others handle it */
			}
		}
		if (prevconptr != PTRSTL && conptr == PTRSTL) {				/* unpressed overlay key */
			if (Keyboard_Peek() == AKEY_HELP)
				Keyboard_Enqueue(AKEY_NONE);
			INPUT_key_consol = INPUT_CONSOL_NONE;
			covl->resetcnt = 0;
		}
	} else if (newtc[PTRJOY].s > 0 && newtc[PTRJOY].x > Android_ScreenW - covl->hotlen
								 && newtc[PTRJOY].y < covl->hotlen) {
		covl->ovl_visible = COVL_FADEIN;						  /* touched overlay hotspot */
		conptr = PTRJOY;
	} else if (newtc[PTRTRG].s > 0 && newtc[PTRTRG].x > Android_ScreenW - covl->hotlen
								 && newtc[PTRTRG].y < covl->hotlen) {
		covl->ovl_visible = COVL_FADEIN;
		conptr = PTRTRG;
	}
	if (conptr == PTRSTL) {
		if (newtc[PTRJOY].s > 0 && 
				( (prevtc[PTRJOY].s <= 0 && newtc[PTRJOY].y < covl->hotlen) ||	/* menu area */
				  prevconptr != PTRSTL) &&								   /* still held */
				!(covl->ovl_visible != COVL_HIDDEN &&
				  newtc[PTRJOY].x >= covl->bbox.l - COVL_SHADOW_OFF &&	 /* outside bbox */
				  newtc[PTRJOY].y <= covl->bbox.b + COVL_SHADOW_OFF) ) {
			conptr = PTRJOY;										/* touched menu area */
			ret = 2;
		} else if (newtc[PTRTRG].s > 0 &&
					( (prevtc[PTRTRG].s <= 0 && newtc[PTRTRG].y < covl->hotlen) ||
					  prevconptr != PTRSTL) &&
					!(covl->ovl_visible != COVL_HIDDEN &&
					  newtc[PTRTRG].x >= covl->bbox.l - COVL_SHADOW_OFF &&
					  newtc[PTRTRG].y <= covl->bbox.b + COVL_SHADOW_OFF) ) {
			conptr = PTRTRG;
			ret = 2;
		}
	}

	/* joystick */
	newjoy = INPUT_STICK_CENTRE;
	if (newtc[PTRJOY].s > 0 && conptr != PTRJOY) {
		if (!Android_Paddle) {
			dx2 = (jovl->joyarea.r - jovl->joyarea.l) >> 1;
			dy2 = (jovl->joyarea.b - jovl->joyarea.t) >> 1;
			dx  = dx2 - dx2 * jovl->deadarea;
			dy  = dy2 - dy2 * jovl->deadarea;
			dx2 = (jovl->joyarea.r - jovl->joyarea.l) * jovl->gracearea;
		}
		if (Android_Paddle) {
			float potx = ((float) (newtc[PTRJOY].x - jovl->joyarea.l)) /
			             ((float) (jovl->joyarea.r - jovl->joyarea.l));
			float poty = (float) newtc[PTRJOY].y / (float) Android_ScreenH;
			Android_POTX = POTLIMIT - (UBYTE) (potx * ((float) POTLIMIT) + 0.5f);
			Android_POTY = POTLIMIT - (UBYTE) (poty * ((float) POTLIMIT) + 0.5f);
			if (Android_ReversePddle & 1)
				Android_POTX = POTLIMIT - Android_POTX;
			if (Android_ReversePddle & 2)
				Android_POTY = POTLIMIT - Android_POTY;
			if (Android_POTX < 0)	Android_POTX = 0;
			if (Android_POTY < 0)	Android_POTY = 0;
			if (Android_POTX > POTLIMIT)	Android_POTX = POTLIMIT;
			if (Android_POTY > POTLIMIT)	Android_POTY = POTLIMIT;

			jovl->joystick.x = newtc[PTRJOY].x;
			jovl->joystick.y = newtc[PTRJOY].y;
			jovl->stickopacity = HIT_OPACITY;
			if (!jovl->anchor) {
				dy = (jovl->joyarea.b - jovl->joyarea.t) >> 1;
				if (newtc[PTRJOY].y - dy < 0)    newtc[PTRJOY].y -= newtc[PTRJOY].y - dy;
				if (newtc[PTRJOY].y + dy > Android_ScreenH)
					newtc[PTRJOY].y -= newtc[PTRJOY].y + dy - Android_ScreenH;
				jovl->joyarea.t = newtc[PTRJOY].y - dy;
				jovl->joyarea.b = newtc[PTRJOY].y + dy;
				jovl->areaopacitycur = jovl->areaopacityset;
				jovl->areaopacityfrm = 0;
			}
		} else if ( (newtc[PTRJOY].x >= jovl->joyarea.l - dx2 &&
					 newtc[PTRJOY].x <= jovl->joyarea.r + dx2 &&
					 newtc[PTRJOY].y >= jovl->joyarea.t - dx2 &&
					 newtc[PTRJOY].y <= jovl->joyarea.b + dx2) ||
					jovl->anchor ) {

			if (newtc[PTRJOY].x <= jovl->joyarea.l + dx) {
				newjoy &= INPUT_STICK_LEFT;
			} else if (newtc[PTRJOY].x >= jovl->joyarea.r - dx) {
				newjoy &= INPUT_STICK_RIGHT;
			}
			if (newtc[PTRJOY].y <= jovl->joyarea.t + dy) {
				newjoy &= INPUT_STICK_FORWARD;
			} else if (newtc[PTRJOY].y >= jovl->joyarea.b - dy) {
				newjoy &= INPUT_STICK_BACK;
			}

			if (!jovl->anchor) {
				if (newtc[PTRJOY].x > jovl->joyarea.r) {		/* grace area */
					dx = newtc[PTRJOY].x - jovl->joyarea.r;
					jovl->joyarea.l += dx;
					jovl->joyarea.r += dx;
				} else if (newtc[PTRJOY].x < jovl->joyarea.l) {
					dx = jovl->joyarea.l - newtc[PTRJOY].x;
					jovl->joyarea.r -= dx;
					jovl->joyarea.l -= dx;
				}
				if (newtc[PTRJOY].y > jovl->joyarea.b) {
					dy = newtc[PTRJOY].y - jovl->joyarea.b;
					jovl->joyarea.t += dy;
					jovl->joyarea.b += dy;
				} else if (newtc[PTRJOY].y < jovl->joyarea.t) {
					dy = jovl->joyarea.t - newtc[PTRJOY].y;
					jovl->joyarea.b -= dy;
					jovl->joyarea.t -= dy;
				}
			}

			jovl->joystick.x = newtc[PTRJOY].x;
			jovl->joystick.y = newtc[PTRJOY].y;
			jovl->stickopacity = HIT_OPACITY;
			jovl->areaopacitycur = jovl->areaopacityset;
			jovl->areaopacityfrm = 0;
		} else {
			if (prevtc[PTRJOY].s > 0) {			/* drag area along */
				if (newtc[PTRJOY].x > jovl->joyarea.r) {
					dx = newtc[PTRJOY].x - jovl->joyarea.r;
					jovl->joyarea.l += dx;
					jovl->joyarea.r += dx;
					newjoy &= INPUT_STICK_RIGHT;
				} else if (newtc[PTRJOY].x < jovl->joyarea.l) {
					dx = jovl->joyarea.l - newtc[PTRJOY].x;
					jovl->joyarea.r -= dx;
					jovl->joyarea.l -= dx;
					newjoy &= INPUT_STICK_LEFT;
				} else if (newtc[PTRJOY].x <= jovl->joyarea.l + dx) {
					newjoy &= INPUT_STICK_LEFT;
				} else if (newtc[PTRJOY].x >= jovl->joyarea.r - dx) {
					newjoy &= INPUT_STICK_RIGHT;
				}
				if (newtc[PTRJOY].y > jovl->joyarea.b) {
					dy = newtc[PTRJOY].y - jovl->joyarea.b;
					jovl->joyarea.t += dy;
					jovl->joyarea.b += dy;
					newjoy &= INPUT_STICK_BACK;
				} else if (newtc[PTRJOY].y < jovl->joyarea.t) {
					dy = jovl->joyarea.t - newtc[PTRJOY].y;
					jovl->joyarea.b -= dy;
					jovl->joyarea.t -= dy;
					newjoy &= INPUT_STICK_FORWARD;
				} else if (newtc[PTRJOY].y <= jovl->joyarea.t + dy) {
					newjoy &= INPUT_STICK_FORWARD;
				} else if (newtc[PTRJOY].y >= jovl->joyarea.b - dy) {
					newjoy &= INPUT_STICK_BACK;
				}

				jovl->joystick.x = newtc[PTRJOY].x;
				jovl->joystick.y = newtc[PTRJOY].y;
				jovl->stickopacity = HIT_OPACITY;
			} else {						/* recenter area */
				dx = (jovl->joyarea.r - jovl->joyarea.l) >> 1;
				dy = (jovl->joyarea.b - jovl->joyarea.t) >> 1;
				if (Android_Joyleft) {
					if (newtc[PTRJOY].x + dx > Android_Split)
						newtc[PTRJOY].x = Android_Split - dx;
				} else {
					if (newtc[PTRJOY].x - dx < Android_Split)
						newtc[PTRJOY].x = Android_Split + dx;
				}
				if (newtc[PTRJOY].x - dx < 0)    newtc[PTRJOY].x -= newtc[PTRJOY].x - dx;
				if (newtc[PTRJOY].y - dy < 0)    newtc[PTRJOY].y -= newtc[PTRJOY].y - dy;
				if (newtc[PTRJOY].y + dy > Android_ScreenH)
					newtc[PTRJOY].y -= newtc[PTRJOY].y + dy - Android_ScreenH;
				jovl->joyarea.l = newtc[PTRJOY].x - dx;
				jovl->joyarea.r = newtc[PTRJOY].x + dx;
				jovl->joyarea.t = newtc[PTRJOY].y - dy;
				jovl->joyarea.b = newtc[PTRJOY].y + dy;
			}
			jovl->areaopacitycur = jovl->areaopacityset;
			jovl->areaopacityfrm = 0;
		}
	}

	/* trigger */
	newtrig = 1;
	if ( (newtc[PTRTRG].s > 0 && conptr != PTRTRG) ||	/* normal trigger */
		 (newtc[PTRJOY].s > 0 && conptr != PTRJOY && Android_PlanetaryDefense) ) {
		newtrig = 0;
		jovl->fire.x = newtc[PTRTRG].x;
		jovl->fire.y = newtc[PTRTRG].y;
		jovl->fireopacity = HIT_OPACITY;
	}

	/* thread unsafe => "no" problem */
	if (!Android_Paddle){
		Android_PortStatus = 0xFFF0 | newjoy;
		Android_TrigStatus = 0xE | newtrig;
	} else {
		POKEY_POT_input[INPUT_mouse_port << 1] = Android_POTX;
		POKEY_POT_input[(INPUT_mouse_port << 1) + 1] = Android_POTY;
		INPUT_mouse_buttons = !newtrig;
	}

	memcpy(prevtc, newtc, sizeof(struct touchstate) * MAXPOINTERS);
	prevconptr = conptr;

	return ret;
}

void Android_KeyEvent(int k, int s)
{
	int i, shft;

	if (Android_SoftjoyEnable) {
		for (i = 0; i < 4; i++)
			if (softjoymap[i][0] == k) {
				if (s)
					Android_PortStatus &= 0xFFF0 | softjoymap[i][1];
				else
					Android_PortStatus |= ~softjoymap[i][1];
				return;
			}
		if (softjoymap[SOFTJOY_FIRE][0] == k) {
			Android_TrigStatus = (Android_TrigStatus & (~(s != 0))) | (s == 0);
			return;
		}
		for (i = SOFTJOY_ACTIONBASE; i < SOFTJOY_MAXKEYS + SOFTJOY_MAXACTIONS; i++)
			if (softjoymap[i][0] == k && softjoymap[i][1] != AKEY_NONE) {
				k = softjoymap[i][1];
				break;
			}
	}

	if (Android_DerotateKeys && k <= KEY_UP && k >= KEY_RIGHT)
		k = derot_lut[Android_DerotateKeys - 1][KEY_UP - k];

	switch (k) {
	case KEY_SHIFT:
		INPUT_key_shift = (s) ? AKEY_SHFT : 0;
		break;
	case KEY_CONTROL:
		Android_key_control = (s) ? AKEY_CTRL : 0;
		break;
	case KEY_FIRE:
		Android_TrigStatus = (Android_TrigStatus & (~(s != 0))) | (s == 0);
		break;
	default:
		if (k >= STATIC_MAXKEYS)
			Log_print("Unmappable key %d", k);
		else {
			if (k == '+' || k == '<' || k == '>' || k == '*')
				shft = 0;
			else
				shft = INPUT_key_shift;
			Keyboard_Enqueue( (s) ? (skeyxlat[k] | Android_key_control | shft) : AKEY_NONE );
		}
	}
}

void Input_Initialize(void)
{
	int i;

	memset(prevtc, 0, sizeof(prevtc));
	prevconptr = PTRSTL;

	memset(&AndroidInput_JoyOvl, 0, sizeof(struct joy_overlay_state));
	AndroidInput_JoyOvl.ovl_visible = 1;
	AndroidInput_JoyOvl.areaopacitycur = AndroidInput_JoyOvl.areaopacityset = 0.25f;
	AndroidInput_JoyOvl.deadarea = 0.3f;
	AndroidInput_JoyOvl.gracearea = 0.3f;
	AndroidInput_JoyOvl.joyarea.t = AndroidInput_JoyOvl.joyarea.l = 10;
	AndroidInput_JoyOvl.joyarea.b = AndroidInput_JoyOvl.joyarea.r = 74;
	AndroidInput_JoyOvl.anchor = 0;

	memset(&AndroidInput_ConOvl, 0, sizeof(struct consolekey_overlay_state));
	AndroidInput_ConOvl.hitkey = CONK_NOKEY;
	AndroidInput_ConOvl.opacity = COVL_MAX_OPACITY;
	AndroidInput_ConOvl.ovl_visible = COVL_READY;
	AndroidInput_ConOvl.statecnt = COVL_HOLD_TIME;

	Android_PortStatus = 0xFFFF;
	Android_TrigStatus = 0xF;

	for (i = 0; i < KBD_MAXKEYS; Android_Keyboard[i] = AKEY_NONE, i++);
	INPUT_key_consol    = INPUT_CONSOL_NONE;
	INPUT_key_shift     = FALSE;
	Android_key_control = 0;
}

void Joy_Reposition(void)
{
	int dx = 0, dy = 0;

	if (Android_ScreenW == 0)	return; /* we're going to get called again @ initgraphics() */
	if (Android_Joyleft) {
		if (AndroidInput_JoyOvl.joyarea.r > Android_Split)
			dx = -(AndroidInput_JoyOvl.joyarea.r - Android_Split);
	} else {
		if (AndroidInput_JoyOvl.joyarea.l < Android_Split)
			dx = Android_Split - AndroidInput_JoyOvl.joyarea.l;
	}
	if (AndroidInput_JoyOvl.joyarea.l < 0)
		dx = -AndroidInput_JoyOvl.joyarea.l;
	else if (AndroidInput_JoyOvl.joyarea.r > Android_ScreenW)
		dx = -(AndroidInput_JoyOvl.joyarea.r - Android_ScreenW);
	if (AndroidInput_JoyOvl.joyarea.t < 0)
		dy = -AndroidInput_JoyOvl.joyarea.t;
	else if (AndroidInput_JoyOvl.joyarea.b > Android_ScreenH)
		dy = -(AndroidInput_JoyOvl.joyarea.b - Android_ScreenH);

	AndroidInput_JoyOvl.joyarea.l += dx;
	AndroidInput_JoyOvl.joyarea.r += dx;
	AndroidInput_JoyOvl.joyarea.t += dy;
	AndroidInput_JoyOvl.joyarea.b += dy;
}

void Android_SplitCalc(void)
{
	if (Android_Joyleft)
		Android_Split = Android_Splitpct * Android_ScreenW;
	else
		Android_Split = (1.0f - Android_Splitpct) * Android_ScreenW;
}

void Keyboard_Enqueue(int key)
{
	pthread_mutex_lock(&key_mutex);

	if (((key_head + 1) & KBD_MASK) == key_tail)
		key_head = key_tail;		/* on overflow, discard previous keys */
	Android_Keyboard[key_head++] = key;
	key_head &= KBD_MASK;

	pthread_mutex_unlock(&key_mutex);
}

int Keyboard_Dequeue(void)
{
	pthread_mutex_lock(&key_mutex);

	if (key_head != key_tail) {
		key_last = Android_Keyboard[key_tail++];
		key_tail &= KBD_MASK;
	}

	pthread_mutex_unlock(&key_mutex);

	return key_last;
}

int Keyboard_Peek(void)
{
	int tmp_key;

	tmp_key = key_last;
	if (key_head != key_tail)
		tmp_key = Android_Keyboard[key_tail];
	return tmp_key;
}
