/*
 * SEGA Dreamcast support using KallistiOS (http://cadcdev.sourceforge.net)
 * (c) 2002-2006,2008 Christian Groessler (chris@groessler.org)
 */

#include <stdarg.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <kos.h>
#include <sys/types.h>
#include "atari.h"
#include "config.h"
#include "sio.h"
#include "binload.h"
#include "input.h"
#include "akey.h"
#include "colours.h"
#include "ui.h"
#include "time.h"
#include "screen.h"
#include <dc/g2bus.h>
#include <arm/aica_cmd_iface.h>
#include <dc/sound/sound.h>
#include "statesav.h"
#include "sound.h"
#define _TYPEDEF_H   /* to prevent pokeysnd.h to create uint32 type #defines */
#include "pokeysnd.h"
#include "version.h"

#define AKEY_KEYB -120

#define REPEAT_DELAY 5000
#define REPEAT_INI_DELAY (5 * REPEAT_DELAY)
/*#define DEBUG*/
#ifndef DIRTYRECT
#error need DIRTYRECT
#endif

void PLATFORM_DisplayScreen(void);
int stat(const char *, struct stat *);
static void calc_palette(void);
static int check_tray_open(void);
static void autostart(void);
static void controller_update(void);
/*static void dc_atari_sync(void);*/
static void dc_sound_init(void);
static void dc_snd_stream_stop(void);

static unsigned char *atari_screen_backup;
static unsigned char *atari_screen_backup2;
static uint8 mcont[4];
static cont_cond_t mcond[4];
static int num_cont;   /* # of controllers found */
static int su_first_call = TRUE;
static int b_ui_leave = FALSE;
static int in_kbui = FALSE;
static int open_tray = FALSE;
static int tray_closed = TRUE;
#ifdef ASSEMBLER_SCREENUPDATE
int vid_mode_width;
#endif
#ifndef ASSEMBLER_SCREENUPDATE
static
#endif
       uint16 *screen_vram;
int emulate_paddles = FALSE;
int glob_snd_ena = TRUE;
int db_mode = FALSE;
int screen_tv_mode;    /* mode of connected tv set */

int x_ovr = FALSE, y_ovr = FALSE, b_ovr = FALSE;
int ovr_inject_key = AKEY_NONE;
int x_key = AKEY_NONE;
int y_key = AKEY_NONE;
int b_key = AKEY_NONE;

static vid_mode_t mymode_pal = {
	DM_320x240,
	320, 240,
	VID_PIXELDOUBLE|VID_LINEDOUBLE|VID_PAL,
	CT_ANY,
	PM_RGB565,
	313, 857,
	0xA4, 0x35, /*0x2d,*/
	0x15, 312,
	141, 843,
	24, 313*2,
	0, 1,
	{ 0, 0, 0, 0 }
};
static vid_mode_t mymode_ntsc = {
	DM_320x240,
	320, 240,			      /* width, height in pixels */
	VID_PIXELDOUBLE|VID_LINEDOUBLE,	      /* flags */
	CT_ANY,				      /* cable type */
	PM_RGB565,			      /* pixel mode */
	262, 857,			      /* scanlines/frame, clocks/scanline */
	0xA4, 0x18,			      /* bitmap window position x, y */
	0x15, 261,			      /* scanline interrupt positions */
	141, 843,			      /* border x start + stop */
	24, 263*2,			      /* border y start + stop */
	0, 1,
	{ 0, 0, 0, 0 }
};

void update_vidmode(void)
{
	vid_mode_t mymode;

	/* we need to copy it since vid_set_mode_ex() modifies it */
	switch(screen_tv_mode) {
	case TV_NTSC:
		memcpy(&mymode, &mymode_ntsc, sizeof(vid_mode_t));
		break;
	case TV_PAL:
	default:
		memcpy(&mymode, &mymode_pal, sizeof(vid_mode_t));
		break;
	}
	mymode.cable_type = vid_check_cable();
	vid_set_mode_ex(&mymode);

	screen_vram = (uint16*)0xA5000000;
	/* point to current line of atari screen in display memory */
	screen_vram += (vid_mode->height - Screen_HEIGHT) / 2 * vid_mode->width;
	/* adjust left column to screen resolution */
	screen_vram += (vid_mode->width - 320) / 2 - (Screen_WIDTH - 320) / 2 ;

#ifdef ASSEMBLER_SCREENUPDATE
        vid_mode_width = vid_mode->width; /* for the assembler routines... */
#endif
}

#ifndef ASSEMBLER_SCREENUPDATE
static
#endif
        uint16 mypal[256];

static void calc_palette(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		mypal[i] = ((Colours_GetR(i) >> 3) << 11) |
			((Colours_GetG(i) >> 2) << 5) |
			(Colours_GetB(i) >> 3);
	}
}

#ifndef ASSEMBLER_SCREENUPDATE
static
#endif
void vbl_wait(void)
{
	static int last_cntr = 0;

	while (maple_state.vbl_cntr == last_cntr) ;
	last_cntr = maple_state.vbl_cntr;
}

#define START_VAL ((Screen_WIDTH - 320) / 2)  /* shortcut, used in screen update routines */

#ifndef ASSEMBLER_SCREENUPDATE
static
#endif
UBYTE old_sd[2][Screen_HEIGHT * Screen_WIDTH / 8];

#ifdef ASSEMBLER_SCREENUPDATE
extern void PLATFORM_DisplayScreen_doubleb(UBYTE *screen);
#else
static void PLATFORM_DisplayScreen_doubleb(UBYTE *screen)
{
	static unsigned int sbase = 0;		/* screen base of current frame */
	unsigned int x, m, j;
	uint16 *vram;
	UBYTE *osd, *nsd;	/* old screen-dirty, new screen-dirty */

#ifdef SPEED_CHECK
	vid_border_color(0, 0, 0);
#endif
	if (sbase) {
		sbase = 0;
                vram = screen_vram;
		osd = old_sd[0];
		nsd = old_sd[1];
	}
	else {
		sbase = 1024 * 768 * 4;
                vram = screen_vram + sbase / 2;  /* "/ 2" since screen_vram is (uint16 *) */
		osd = old_sd[1];
		nsd = old_sd[0];
	}

	for (m=START_VAL/8, x=START_VAL; m < Screen_HEIGHT * Screen_WIDTH / 8; m++) {
		nsd[m] = Screen_dirty[m];  /* remember for other page */
		if (Screen_dirty[m] || osd[m]) {
			/* Draw eight pixels (x,y), (x+1,y),...,(x+7,y) here */
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			Screen_dirty[m] = 0;
		}
		x += 8;
		if (x >= 320 + (Screen_WIDTH - 320) / 2) { /* end of visible part of line */
			vram += vid_mode->width;
			screen += Screen_WIDTH;
			m += (Screen_WIDTH - 320) / 8;
			x = START_VAL;
		}
	}

#ifdef SPEED_CHECK
	vid_border_color(127, 127, 127);
#endif
	vbl_wait();
	vid_set_start(sbase);
#ifdef SPEED_CHECK
	vid_border_color(255, 255, 255);
#endif
}
#endif /* not defined ASSEMBLER_SCREENUPDATE */

#ifdef ASSEMBLER_SCREENUPDATE
extern void PLATFORM_DisplayScreen_singleb(UBYTE *screen);
#else
static void PLATFORM_DisplayScreen_singleb(UBYTE *screen)
{
	unsigned int x, m, j;
	uint16 *vram = screen_vram;

#ifdef SPEED_CHECK
	vid_border_color(0, 0, 0);
#endif
	for (m=START_VAL/8, x=START_VAL; m < Screen_HEIGHT * Screen_WIDTH / 8; m++) {
		if (Screen_dirty[m]) {
			/* Draw eight pixels (x,y), (x+1,y),...,(x+7,y) here */
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			Screen_dirty[m] = 0;
		}
		x += 8;
		if (x >= 320 + (Screen_WIDTH - 320) / 2) { /* end of visible part of line */
			vram += vid_mode->width;
			screen += Screen_WIDTH;
			m += (Screen_WIDTH - 320) / 8;
			x = START_VAL;
		}
	}
#ifdef SPEED_CHECK
	vid_border_color(127, 127, 127);
#endif
	vbl_wait();
#ifdef SPEED_CHECK
	vid_border_color(255, 255, 255);
#endif
}
#endif /* not defined ASSEMBLER_SCREENUPDATE */

static void (*screen_updater)(UBYTE *screen);

void PLATFORM_DisplayScreen(void)
{
	screen_updater((UBYTE *)atari_screen);
}

void update_screen_updater(void)
{
	if (db_mode) {
		screen_updater = PLATFORM_DisplayScreen_doubleb;
	}
	else {
		screen_updater = PLATFORM_DisplayScreen_singleb;
	}
}

void PLATFORM_Initialise(int *argc, char *argv[])
{
	static int fc = TRUE;

	if (! fc) return; else fc = FALSE;

	screen_tv_mode = Atari800_tv_mode;

#ifndef DEBUG
	/* Bother us with output only if something died */
	dbglog_set_level(DBG_DEAD);
#endif

	/* Set the video mode */
	update_vidmode();
	calc_palette();
}

int PLATFORM_Exit(int run_monitor)
{
	arch_reboot();
	return(0);  /* not reached */
}

/*
 * update num_cont and mcont[]
 */
static void dc_controller_init(void)
{
	int p, u;

	num_cont = 0;
	for (p = 0; p < MAPLE_PORT_COUNT; p++) {
		for (u = 0; u < MAPLE_UNIT_COUNT; u++) {
			if (maple_device_func(p, u) & MAPLE_FUNC_CONTROLLER) {
				mcont[num_cont++] = maple_addr(p, u);
			}
		}
	}
	return;
}

/*
 * update the in-core status of the controller buttons/settings
 */
static void controller_update(void)
{
	int i;

	dc_controller_init();  /* update dis-/reconnections */
	for (i = 0; i < num_cont; i++) {
		if (cont_get_cond(mcont[i], &mcond[i])) {
#ifdef DEBUG
			printf("controller_update: error getting controller status\n");
#endif
		}
	}
}

static int consol_keys(void)
{
	cont_cond_t *cond = &mcond[0];

	if (! (cond->buttons & CONT_START)) {
		if (UI_is_active)
			arch_reboot();
		else {
			if (! (cond->buttons & CONT_X))
				return(AKEY_COLDSTART);
			else
				return(AKEY_WARMSTART);
		}
	}

	if (Atari800_machine_type != Atari800_MACHINE_5200) {
		if (b_ovr && !UI_is_active && !b_ui_leave) {
			/* !UI_is_active and !b_ui_leave because B is also the esc key
			   (with rtrig) to leave the ui */
			if (b_key != AKEY_NONE && !(cond->buttons & CONT_B))
				ovr_inject_key = b_key;
		}
		else {
			if (! (cond->buttons & CONT_B)) {
				if (! b_ui_leave) {
					INPUT_key_consol &= ~INPUT_CONSOL_START;
				}
				else {
					INPUT_key_consol |= INPUT_CONSOL_START;
				}
			}
			else {
				b_ui_leave = FALSE;
				INPUT_key_consol |= INPUT_CONSOL_START;
			}
		}

		if (y_ovr) {
			if (y_key != AKEY_NONE && !(cond->buttons & CONT_Y))
				ovr_inject_key = y_key;
		}
		else {
			if (! (cond->buttons & CONT_Y)) {
				INPUT_key_consol &= ~INPUT_CONSOL_SELECT;
			}
			else {
				INPUT_key_consol |= INPUT_CONSOL_SELECT;
			}
		}

		if (x_ovr) {
			if (x_key != AKEY_NONE && !(cond->buttons & CONT_X))
				ovr_inject_key = x_key;
		}
		else {
			if (! (cond->buttons & CONT_X)) {
				INPUT_key_consol &= ~INPUT_CONSOL_OPTION;
			}
			else {
				INPUT_key_consol |= INPUT_CONSOL_OPTION;
			}
		}
	}
	else {
		/* @@@ *_ovr TODO @@@ */

		if (! (cond->buttons & CONT_X)) {  /* 2nd action button */
			INPUT_key_shift = 1;
		}
		else {
			INPUT_key_shift = 0;
		}
		if (! UI_is_active) {
			if (! (cond->buttons & CONT_B)) {  /* testtest */
				if (! b_ui_leave) {
					return(AKEY_4);	  /* ??? at least on the dc kb "4" starts some games?? */
				}
			}
			else {
				b_ui_leave = FALSE;
			}
			if (! (cond->buttons & CONT_Y)) {  /* testtest */
				return(AKEY_5200_START);
			}
		}
	}
	return(AKEY_NONE);
}

int get_emkey(UBYTE *title)
{
	int keycode;

	controller_update();
	in_kbui = TRUE;
	memcpy(atari_screen_backup, atari_screen, Screen_HEIGHT * Screen_WIDTH);
	keycode = UI_BASIC_OnScreenKeyboard(title, -1);
	memcpy(atari_screen, atari_screen_backup, Screen_HEIGHT * Screen_WIDTH);
	Screen_EntireDirty();
	PLATFORM_DisplayScreen();
	in_kbui = FALSE;
	return keycode;
}

/*
 * do some basic keyboard emulation for the controller,
 * so that the emulator menu can be used without keyboard
 * (basically for selecting bin files/disk images)
 */
static int controller_kb(void)
{
	static int prev_up = FALSE, prev_down = FALSE, prev_a = FALSE,
		prev_r = FALSE, prev_left = FALSE, prev_right = FALSE,
		prev_b = FALSE, prev_l = FALSE;
	static int repdelay = REPEAT_DELAY;
	cont_cond_t *cond = &mcond[0];

	if (! num_cont) return(AKEY_NONE);  /* no controller present */

	repdelay--;
	if (repdelay < 0) repdelay = REPEAT_DELAY;

	if (!UI_is_active && (cond->ltrig > 250 || !(cond->buttons & CONT_Z))) {
		return(AKEY_UI);
	}
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
	if (!UI_is_active && (cond->rtrig > 250 || !(cond->buttons & CONT_C))) {
		controller_update();
		return(AKEY_KEYB);
	}
	/* provide keyboard emulation to enter file name */
	if (UI_is_active && !in_kbui && (cond->rtrig > 250 || !(cond->buttons & CONT_C))) {
		int keycode;
		controller_update();
		in_kbui = TRUE;
		memcpy(atari_screen_backup, atari_screen, Screen_HEIGHT * Screen_WIDTH);
		keycode = UI_BASIC_OnScreenKeyboard(NULL, -1);
		memcpy(atari_screen, atari_screen_backup, Screen_HEIGHT * Screen_WIDTH);
		Screen_EntireDirty();
		PLATFORM_DisplayScreen();
		in_kbui = FALSE;
		return keycode;
	}
#endif

	if (UI_is_active) {
		if (! (cond->buttons & CONT_DPAD_UP)) {
			prev_down = FALSE;
			if (! prev_up) {
				repdelay = REPEAT_INI_DELAY;
				prev_up = 1;
				return(AKEY_UP);
			}
			else {
				if (! repdelay) {
					return(AKEY_UP);
				}
			}
		}
		else {
			prev_up = FALSE;
		}

		if (! (cond->buttons & CONT_DPAD_DOWN)) {
			prev_up = FALSE;
			if (! prev_down) {
				repdelay = REPEAT_INI_DELAY;
				prev_down = TRUE;
				return(AKEY_DOWN);
			}
			else {
				if (! repdelay) {
					return(AKEY_DOWN);
				}
			}
		}
		else {
			prev_down = FALSE;
		}

		if (! (cond->buttons & CONT_DPAD_LEFT)) {
			prev_right = FALSE;
			if (! prev_left) {
				repdelay = REPEAT_INI_DELAY;
				prev_left = TRUE;
				return(AKEY_LEFT);
			}
			else {
				if (! repdelay) {
					return(AKEY_LEFT);
				}
			}
		}
		else {
			prev_left = FALSE;
		}

		if (! (cond->buttons & CONT_DPAD_RIGHT)) {
			prev_left = FALSE;
			if (! prev_right) {
				repdelay = REPEAT_INI_DELAY;
				prev_right = TRUE;
				return(AKEY_RIGHT);
			}
			else {
				if (! repdelay) {
					return(AKEY_RIGHT);
				}
			}
		}
		else {
			prev_right = FALSE;
		}

		if (! (cond->buttons & CONT_A)) {
			if (! prev_a) {
				prev_a = TRUE;
				return(AKEY_RETURN);
			}
		}
		else {
			prev_a = FALSE;
		}

		if (! (cond->buttons & CONT_B)) {
			if (! prev_b) {
				prev_b = TRUE;
				b_ui_leave = TRUE;   /* B must be released again */
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_b = FALSE;
		}

		if (cond->ltrig > 250 || !(cond->buttons & CONT_Z)) {
			if (! prev_l && in_kbui) {
				prev_l = TRUE;
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_l = FALSE;
		}

		if (cond->rtrig > 250 || !(cond->buttons & CONT_C)) {
			if (! prev_r) {
				prev_r = TRUE;
				return(AKEY_ESCAPE);
			}
		}
		else {
			prev_r = FALSE;
		}
	}
	return(AKEY_NONE);
}

int PLATFORM_Keyboard(void)
{
	static int old_open_tray = FALSE;
	int open_tray;
	int keycode;
	int i;

	/* let's do this here... */
	open_tray = check_tray_open();
	if (open_tray != old_open_tray) {
		old_open_tray = open_tray;
		if (open_tray == TRUE) {
#ifdef DEBUG
			printf("XXX TRAY OPEN!\n");
#endif
			tray_closed = FALSE;
			return(AKEY_UI);
		}
		else {
#ifdef DEBUG
			printf("XXX TRAY CLOSE!\n");
#endif
			tray_closed = TRUE;
			cdrom_init();
			iso_reset();
			chdir("/");
			for (i=0; i<UI_n_atari_files_dir; i++)
				strcpy(UI_atari_files_dir[i], "/");
		}
	}

	if (UI_is_active) controller_update();

	if (num_cont && (keycode = consol_keys()) != AKEY_NONE) return(keycode);

	if (ovr_inject_key != AKEY_NONE) {
		keycode = ovr_inject_key;
		ovr_inject_key = AKEY_NONE;
		return(keycode);
	}

	keycode = controller_kb();
	if (keycode != AKEY_NONE) return(keycode);

	keycode = kbd_get_key();
	if (keycode == -1) return(AKEY_NONE);

#ifdef DEBUG
	printf("DC key: %04X (%c)\n", keycode, keycode > 32 && keycode < 127 ? keycode : '.');
#endif

	switch (keycode) {

	/* OPTION / SELECT / START keys */
	/*INPUT_key_consol = INPUT_CONSOL_NONE;  -- already set in consol_keys... */
	case 0x3b00:
		INPUT_key_consol &= (~INPUT_CONSOL_OPTION);
		return(AKEY_NONE);
	case 0x3c00:
		INPUT_key_consol &= (~INPUT_CONSOL_SELECT);
		return(AKEY_NONE);
	case 0x3d00:
		INPUT_key_consol &= (~INPUT_CONSOL_START);
		return(AKEY_NONE);

	case 0x1b:  /* ESC */
		keycode = AKEY_ESCAPE;
		break;
	case 9:  /* TAB */
		keycode = AKEY_TAB;
		break;
	case '`':
		keycode = AKEY_CAPSTOGGLE;
		break;
	case '!':
		keycode = AKEY_EXCLAMATION;
		break;
	case '"':
		keycode = AKEY_DBLQUOTE;
		break;
	case '#':
		keycode = AKEY_HASH;
		break;
	case '$':
		keycode = AKEY_DOLLAR;
		break;
	case '%':
		keycode = AKEY_PERCENT;
		break;
	case '&':
		keycode = AKEY_AMPERSAND;
		break;
	case '\'':
		keycode = AKEY_QUOTE;
		break;
	case '@':
		keycode = AKEY_AT;
		break;
	case '(':
		keycode = AKEY_PARENLEFT;
		break;
	case ')':
		keycode = AKEY_PARENRIGHT;
		break;
	case '[':
		keycode = AKEY_BRACKETLEFT;
		break;
	case ']':
		keycode = AKEY_BRACKETRIGHT;
		break;
	case '<':
		keycode = AKEY_LESS;
		break;
	case '>':
		keycode = AKEY_GREATER;
		break;
	case '=':
		keycode = AKEY_EQUAL;
		break;
	case '?':
		keycode = AKEY_QUESTION;
		break;
	case '-':
		keycode = AKEY_MINUS;
		break;
	case '+':
		keycode = AKEY_PLUS;
		break;
	case '*':
		keycode = AKEY_ASTERISK;
		break;
	case '/':
		keycode = AKEY_SLASH;
		break;
	case ':':
		keycode = AKEY_COLON;
		break;
	case ';':
		keycode = AKEY_SEMICOLON;
		break;
	case ',':
		keycode = AKEY_COMMA;
		break;
	case '.':
		keycode = AKEY_FULLSTOP;
		break;
	case '_':
		keycode = AKEY_UNDERSCORE;
		break;
	case '^':
		keycode = AKEY_CIRCUMFLEX;
		break;
	case '\\':
		keycode = AKEY_BACKSLASH;
		break;
	case '|':
		keycode = AKEY_BAR;
		break;
	case ' ':
		keycode = AKEY_SPACE;
		break;
	case '0':
		keycode = AKEY_0;
		break;
	case '1':
		keycode = AKEY_1;
		break;
	case '2':
		keycode = AKEY_2;
		break;
	case '3':
		keycode = AKEY_3;
		break;
	case '4':
		keycode = AKEY_4;
		break;
	case '5':
		keycode = AKEY_5;
		break;
	case '6':
		keycode = AKEY_6;
		break;
	case '7':
		keycode = AKEY_7;
		break;
	case '8':
		keycode = AKEY_8;
		break;
	case '9':
		keycode = AKEY_9;
		break;
	case 'a':
		keycode = AKEY_a;
		break;
	case 'b':
		keycode = AKEY_b;
		break;
	case 'c':
		keycode = AKEY_c;
		break;
	case 'd':
		keycode = AKEY_d;
		break;
	case 'e':
		keycode = AKEY_e;
		break;
	case 'f':
		keycode = AKEY_f;
		break;
	case 'g':
		keycode = AKEY_g;
		break;
	case 'h':
		keycode = AKEY_h;
		break;
	case 'i':
		keycode = AKEY_i;
		break;
	case 'j':
		keycode = AKEY_j;
		break;
	case 'k':
		keycode = AKEY_k;
		break;
	case 'l':
		keycode = AKEY_l;
		break;
	case 'm':
		keycode = AKEY_m;
		break;
	case 'n':
		keycode = AKEY_n;
		break;
	case 'o':
		keycode = AKEY_o;
		break;
	case 'p':
		keycode = AKEY_p;
		break;
	case 'q':
		keycode = AKEY_q;
		break;
	case 'r':
		keycode = AKEY_r;
		break;
	case 's':
		keycode = AKEY_s;
		break;
	case 't':
		keycode = AKEY_t;
		break;
	case 'u':
		keycode = AKEY_u;
		break;
	case 'v':
		keycode = AKEY_v;
		break;
	case 'w':
		keycode = AKEY_w;
		break;
	case 'x':
		keycode = AKEY_x;
		break;
	case 'y':
		keycode = AKEY_y;
		break;
	case 'z':
		keycode = AKEY_z;
		break;
	case 'A':
		keycode = AKEY_A;
		break;
	case 'B':
		keycode = AKEY_B;
		break;
	case 'C':
		keycode = AKEY_C;
		break;
	case 'D':
		keycode = AKEY_D;
		break;
	case 'E':
		keycode = AKEY_E;
		break;
	case 'F':
		keycode = AKEY_F;
		break;
	case 'G':
		keycode = AKEY_G;
		break;
	case 'H':
		keycode = AKEY_H;
		break;
	case 'I':
		keycode = AKEY_I;
		break;
	case 'J':
		keycode = AKEY_J;
		break;
	case 'K':
		keycode = AKEY_K;
		break;
	case 'L':
		keycode = AKEY_L;
		break;
	case 'M':
		keycode = AKEY_M;
		break;
	case 'N':
		keycode = AKEY_N;
		break;
	case 'O':
		keycode = AKEY_O;
		break;
	case 'P':
		keycode = AKEY_P;
		break;
	case 'Q':
		keycode = AKEY_Q;
		break;
	case 'R':
		keycode = AKEY_R;
		break;
	case 'S':
		keycode = AKEY_S;
		break;
	case 'T':
		keycode = AKEY_T;
		break;
	case 'U':
		keycode = AKEY_U;
		break;
	case 'V':
		keycode = AKEY_V;
		break;
	case 'W':
		keycode = AKEY_W;
		break;
	case 'X':
		keycode = AKEY_X;
		break;
	case 'Y':
		keycode = AKEY_Y;
		break;
	case 'Z':
		keycode = AKEY_Z;
		break;

	case 0x3a00:  /* F1 */
		keycode = AKEY_UI;
		break;

	case 0x3e00:  /* F5 */
		keycode = AKEY_COLDSTART;
		break;

	case 0x4500:  /* F12 */
		arch_reboot();

	/* cursor keys */
	case 0x5200:
		keycode = AKEY_UP;
		break;
	case 0x5100:
		keycode = AKEY_DOWN;
		break;
	case 0x5000:
		keycode = AKEY_LEFT;
		break;
	case 0x4f00:
		keycode = AKEY_RIGHT;
		break;

	case 0x6500:  /* S3 */
		keycode = AKEY_ATARI;
		break;

	case 0x4a00:  /* Home */
		keycode = AKEY_CLEAR;
		break;
	case 0x4d00:  /* End */
		keycode = AKEY_HELP;
		break;
	case 0x4800:  /* Break/Pause */
		keycode = AKEY_BREAK;
		break;

	case 0x4c00:  /* Del */
		if (INPUT_key_shift)
			keycode = AKEY_DELETE_LINE;
		else
			keycode |= AKEY_DELETE_CHAR;
		break;
	case 0x4900:  /* Ins */
		if (INPUT_key_shift)
			keycode = AKEY_INSERT_LINE;
		else
			keycode |= AKEY_INSERT_CHAR;
		break;

	case 127:
	case 8:
		keycode = AKEY_BACKSPACE;
		break;
	case 13:
	case 10:
		keycode = AKEY_RETURN;
		break;
	default:
		keycode = AKEY_NONE;
		break;
	}
	return keycode;
}

int PLATFORM_PORT(int num)
{
	cont_cond_t *cond;
	int retval = 0xff;
	int cix = 0;

	if (num)
		cix = 2;   /* js #2 and #3 */

	if (num_cont < 1 + cix) return(retval);	 /* no controller present */

	cond = &mcond[cix];
	if (! emulate_paddles) {
		if (! (cond->buttons & CONT_DPAD_UP)) {
			retval &= 0xfe;
		}
		if (! (cond->buttons & CONT_DPAD_DOWN)) {
			retval &= 0xfd;
		}
		if (! (cond->buttons & CONT_DPAD_LEFT)) {
			retval &= 0xfb;
		}
		if (! (cond->buttons & CONT_DPAD_RIGHT)) {
			retval &= 0xf7;
		}
		/* if joypad not used, try the joystick instead */
		if (retval == 0xff) {
			if (cond->joyx > 127 + 64) {  /* right */
				retval &= 0xf7;
			}
			if (cond->joyx < 127 - 64) {  /* left */
				retval &= 0xfb;
			}
			if (cond->joyy > 127 + 64) {  /* down */
				retval &= 0xfd;
			}
			if (cond->joyy < 127 - 64) {  /* up */
				retval &= 0xfe;
			}
		}
		if (num_cont > 1 + cix) {
			cond = &mcond[cix+1];
			if (! (cond->buttons & CONT_DPAD_UP)) {
				retval &= 0xef;
			}
			if (! (cond->buttons & CONT_DPAD_DOWN)) {
				retval &= 0xdf;
			}
			if (! (cond->buttons & CONT_DPAD_LEFT)) {
				retval &= 0xbf;
			}
			if (! (cond->buttons & CONT_DPAD_RIGHT)) {
				retval &= 0x7f;
			}
			/* if joypad not used, try the joystick instead */
			if ((retval & 0xf0) == 0xf0) {
				if (cond->joyx > 127 + 64) {  /* right */
					retval &= 0x7f;
				}
				if (cond->joyx < 127 - 64) {  /* left */
					retval &= 0xbf;
				}
				if (cond->joyy > 127 + 64) {  /* down */
					retval &= 0xdf;
				}
				if (cond->joyy < 127 - 64) {  /* up */
					retval &= 0xef;
				}
			}
		}
	}
	else {	/* emulate paddles */
		/* 1st paddle trigger */
		if (! (cond->buttons & CONT_A)) {
			retval &= (cix > 1) ? 0xbf : 0xfb;
		}
		if (num_cont > 1 + cix) {
			cond = &mcond[cix+1];

			/* 2nd paddle trigger */
			if (! (cond->buttons & CONT_A)) {
				retval &= (cix > 1) ? 0x7f : 0xf7;
			}
		}
	}

	return(retval);
}

int Atari_POT(int num)
{
	int val;
	cont_cond_t *cond;

	if (Atari800_machine_type != Atari800_MACHINE_5200) {
		if (emulate_paddles) {
			if (num + 1 > num_cont) return(228);

			cond = &mcond[num];
			val = cond->joyx;
			val = val * 228 / 255;
			if (val > 227) return(1);
			return(228 - val);
		}
		else {
			return(228);
		}
	}
	else {	/* 5200 version:
		 *
		 * num / 2: which controller
		 * num & 1 == 0: x axis
		 * num & 1 == 1: y axis
		 */

		if (num / 2 + 1 > num_cont) return(INPUT_joy_5200_center);

		cond = &mcond[num / 2];
		val = (num & 1) ? cond->joyy : cond->joyx;

		/* normalize into 5200 range */
		if (val == 127) return(INPUT_joy_5200_center);
		if (val < 127) {
			/*val -= INPUT_joy_5200_min;*/
			val = val * (INPUT_joy_5200_center - INPUT_joy_5200_min) / 127;
			return(val + INPUT_joy_5200_min);
		}
		else {
			val = val * INPUT_joy_5200_max / 255;
			if (val < INPUT_joy_5200_center)
				val = INPUT_joy_5200_center;
			return(val);
		}
	}
}

int PLATFORM_TRIG(int num)
{
	cont_cond_t *cond;

	if (num + 1 > num_cont) return(1);  /* no controller present */

	cond = &mcond[num];
	if (! (cond->buttons & CONT_A)) {
		return(0);
	}

	return(1);
}

#if 0
/*
 * fill up unused (no entry in atari800.cfg) UI_saved_files_dir
 * entries with the available VMUs
 */
static void setup_saved_files_dirs(void)
{
	int p, u;

	/* loop over VMUs */
	for (p=0; p<MAPLE_PORT_COUNT; p++) {
		for (u=0; u<MAPLE_UNIT_COUNT; u++) {
			if (maple_device_func(p, u) & MAPLE_FUNC_MEMCARD) {
				if (UI_n_saved_files_dir < MAX_DIRECTORIES) {
					sprintf(UI_saved_files_dir[UI_n_saved_files_dir], "/vmu/%c%c",
						'a' + p, '0' + u);
					UI_n_saved_files_dir++;
				}
				else
					return;
			}
		}
	}
}
#endif

#ifdef HZ_TEST
static void cls(int xres, int yres)
{
	int x, y;
	for(x = 0; x < xres; x++)
		for(y = 0; y < yres; y++)
			vram_s[xres*y + x] = 0;
}

static void wait_a(void)
{
	cont_cond_t cond;

	do {
		if (cont_get_cond(mcont[0], &cond)) {
			return;
		}

	} while (cond.buttons & CONT_A);
}

void do_hz_test(void)
{
	uint32 s, ms, s2, ms2, z;
	char buffer[80];

	cls(320,240);
	bfont_draw_str(vram_s+64*320+5, 320, 0, "chk dsply hz.. 500 vbls");
	timer_ms_gettime(&s, &ms);
	for (z=0; z<500; z++) {
		vbl_wait();
	}
	timer_ms_gettime(&s2, &ms2);
	sprintf(buffer, "start time: %lu.%03lu\n", s, ms);
	bfont_draw_str(vram_s+89*320+5, 320, 0, buffer);
	sprintf(buffer, "end   time: %lu.%03lu\n", s2, ms2);
	bfont_draw_str(vram_s+114*320+5, 320, 0, buffer);
	sprintf(buffer, "diff(ms) = %ld\n", s2*1000 + ms2 - s * 1000 - ms);
	bfont_draw_str(vram_s+139*320+5, 320, 0, buffer);
	sprintf(buffer, "hz = %f\n", 500.0 / ((s2*1000 + ms2 - s * 1000 - ms) / 1000.0));
	bfont_draw_str(vram_s+164*320+5, 320, 0, buffer);
	wait_a();
	cls(320,240);
}
#endif /* ifdef HZ_TEST */

KOS_INIT_FLAGS(INIT_IRQ | INIT_THD_PREEMPT);
#ifdef ROMDISK
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#endif

void dc_init_serial(void)
{
	dbgio_disable();
	scif_set_parameters(9600, 1);
	scif_init();
	scif_set_irq_usage(1);
}

void dc_set_baud(int baud)
{
	scif_set_parameters(baud, 1);
	scif_init();
	scif_set_irq_usage(1);
#ifdef DEBUG
	printf("setting baud rate: %d\n", baud);
#endif
}

int dc_write_serial(unsigned char byte)
{
	if (scif_write_buffer(&byte, 1, 0) == 1)
		return 1;
	return 0;
}

int dc_read_serial(unsigned char *byte)
{
	int c = scif_read();
	if (c == -1) return 0;
	*byte = c;
	return 1;
}

int main(int argc, char **argv)
{
	printf("Atari800DC main() starting\n");  /* workaound for fopen-before-printf kos bug */

	/* initialize screen updater */
	update_screen_updater();

	/* initialize Atari800 core */
	Atari800_Initialise(&argc, argv);

	/* initialize dc controllers for the first time */
	dc_controller_init();

	/* initialize sound */
	dc_sound_init();

	/*
	ser_console_init();
	dbgio_init();
	*/

#ifdef DEBUG
	printf("\nFor Tobias & Dominik\n");
	printf("--------------------\n");
#endif

#ifdef HZ_TEST
	do_hz_test();
#endif

#if 0
	gdb_init();
	asm("mov #7,r12");
	asm("trapa #0x20");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	//gdb_breakpoint();
#endif

	atari_screen_backup = malloc(Screen_HEIGHT * Screen_WIDTH);
	atari_screen_backup2 = malloc(Screen_HEIGHT * Screen_WIDTH);
	/*setup_saved_files_dirs();*/

#ifdef DEBUG
	{
		int i;
		printf("UI_n_saved_files_dir: %d\n", UI_n_saved_files_dir);
		for (i=0; i<UI_n_saved_files_dir; i++) {
			if (UI_saved_files_dir[i])
				printf("%d: %s\n", i, UI_saved_files_dir[i]);
		}
	}
#endif

	chdir("/");   /* initialize cwd in dc_chdir.c */
	autostart();

	/* main loop */
	while(TRUE)
	{
		int keycode;

		keycode = PLATFORM_Keyboard();

		switch (keycode) {
#ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD
		case AKEY_KEYB:
			Sound_Pause();
			in_kbui = TRUE;
			{
				int temp = UI_is_active;
				UI_is_active = TRUE;
				INPUT_key_code = UI_BASIC_OnScreenKeyboard(NULL, Atari800_machine_type);
				UI_is_active = temp;
			}
			in_kbui = FALSE;
			Sound_Continue();
			controller_update();
			switch (INPUT_key_code) {
			case AKEY_OPTION:
				INPUT_key_consol &= (~INPUT_CONSOL_OPTION);
				INPUT_key_code = AKEY_NONE;
				break;
			case AKEY_SELECT:
				INPUT_key_consol &= (~INPUT_CONSOL_SELECT);
				INPUT_key_code = AKEY_NONE;
				break;
			case AKEY_START:
				INPUT_key_consol &= (~INPUT_CONSOL_START);
				INPUT_key_code = AKEY_NONE;
				break;
			default:
				break;
			}
			break;
#endif /* #ifdef USE_UI_BASIC_ONSCREEN_KEYBOARD */
		default:
			INPUT_key_code = keycode;
			break;
		}

		Atari800_Frame();
		PLATFORM_DisplayScreen();
		controller_update();  /* get new values from the controllers */
	}
}

/*
 * autostart: check for autorun.xxx files on the
 * CD root directory and start the first one found
 */
static void autostart(void)
{
	struct stat sb;

	if (! stat("/cd/autorun.com", &sb)) {
		if (S_ISREG(sb.st_mode)) {
			if (BIN_loader("/cd/autorun.com")) {
				Atari800_Coldstart();
				return;
			}
		}
	}
	if (! stat("/cd/autorun.exe", &sb)) {
		if (S_ISREG(sb.st_mode)) {
			if (BIN_loader("/cd/autorun.exe")) {
				Atari800_Coldstart();
				return;
			}
		}
	}
	if (! stat("/cd/autorun.atr", &sb)) {
		if (S_ISREG(sb.st_mode)) {
			SIO_Mount(1, "/cd/autorun.atr", TRUE);
			return;
		}
	}
}

#if 0  /* not stable, KOS crashes sometimes with files on the ramdisk */
static char mytmpnam[] = "/ram/tmpf";
char *tmpnam(char *space)
{
	static int inc = 0;
	char b[16];
	if (space) {
		strcpy(space, mytmpnam);
		sprintf(b, ".%d\n", inc++);
		strcat(space, b);
		return(space);
	}
	else {
		return(mytmpnam);
	}
}
char *_tmpnam_r (struct reent *r, char *s)
{
	return tmpnam(s);
}
#endif

/* parts taken from KOS' kernel/libc/koslib/opendir.c */
static int odc;
DIR *opendir(const char *name)
{
	file_t handle;
	DIR *newd;
#ifdef DEBUG
	printf("opendir...(%s)\n", name);
#endif
	if (open_tray) return(NULL);
	/*if (tray_closed)*/ tray_closed = FALSE;

	if (! strcmp(name, "/cd")) {
		/* hack */
		cdrom_init();
		iso_reset();
	}
	if (strcmp(name, "/")
	    && strcmp(name, "/cd") && strcmp(name, "/pc") && strcmp(name, "/ram")
	    && strcmp(name, "/vmu") && strcmp(name, "/pty") && strcmp(name, "/rd")
	    && strncmp(name, "/cd/", 4) && strncmp(name, "/pc/", 4) && strncmp(name, "/ram/", 5)
	    && strncmp(name, "/vmu/", 5) && strncmp(name, "/pty/", 5) && strncmp(name, "/rd/", 4)) {
#ifdef DEBUG
		printf("opendir: punt!\n");
#endif
		return(NULL);
	}
	handle = fs_open(name, O_DIR | O_RDONLY);
	if (handle < 0) return(NULL);

	newd = malloc(sizeof(DIR));
	if (!newd) {
		errno = ENOMEM;
		return(NULL);
	}

	newd->fd = handle;
	memset(&newd->d_ent, 0, sizeof(struct dirent));

	if (strcmp(name, "/") && strcmp(name, "/pc")	/* no ".." in the root directory */
	    && strncmp(name, "/pc/", 4))		/* and in /pc and subdirectories */
		odc = 1;                                /* (/pc provides ".." by itself) */
	return(newd);
}

static struct dirent myd;

/* parts taken from KOS' kernel/libc/koslib/readdir.c */
struct dirent *readdir(DIR *dir)
{
	dirent_t *d;

	if (open_tray) return(NULL);
	if (tray_closed) {
		tray_closed = FALSE;
		return(NULL);
	}
	if (odc) {
		odc = 0;
		memset(&myd, 0, sizeof(struct dirent));
		strcpy(myd.d_name, "..");
		myd.d_type = 4;	// DT_DIR
		return(&myd);
	}

	if (!dir) {
		errno = EBADF;
		return(NULL);
	}
	d = fs_readdir(dir->fd);
	if (!d) return(NULL);

	dir->d_ent.d_ino = 0;
	dir->d_ent.d_off = 0;
	dir->d_ent.d_reclen = 0;
	if (d->size < 0)
		dir->d_ent.d_type = 4;	// DT_DIR
	else
		dir->d_ent.d_type = 8;	// DT_REG
	strncpy(dir->d_ent.d_name, d->name, 255);

	return(&dir->d_ent);
}

/* our super-duper stat */
/* only check, whether the S_IFDIR bit has to be set */
int stat(const char *path, struct stat *sb)
{
	file_t handle;

	memset(sb, 0, sizeof(struct stat));  /* preinitialize result */

	if (open_tray) return(-1);
	if (tray_closed) tray_closed = FALSE;

	/* special ".." case */
	if (strlen(path) > 3 &&
	    ! strcmp(path + strlen(path) - 3, "/..")) {
		sb->st_mode = _IFDIR;
		return(0);  /* success */
	}

	/* check if dir */
	if ((handle = fs_open(path, O_DIR | O_RDONLY)) != -1) {	/* is dir */
		sb->st_mode = _IFDIR;
	}
	else if ((handle = fs_open(path, O_RDONLY)) != -1) {	/* is file */
		sb->st_mode = _IFREG;
	}
	else {	/* is not here */
#ifdef DEBUG
		printf("stat: error with '%s'\n", path);
#endif
		return(-1);
	}
	fs_close(handle);
	return(0);  /* success */
}

#if 0
static void dc_atari_sync(void)
{
	static unsigned long long nextclock = 0;
	unsigned long long curclock;
	uint32 s, ms;

	do {
		timer_ms_gettime(&s, &ms);
		curclock = ((unsigned long long)s << 32) + ms;
	} while (curclock < nextclock);

	nextclock = curclock + CLK_TCK / ((Atari800_tv_mode == TV_PAL ? 50 : 60));
}
#endif

static int check_tray_open(void)
{
	int status, disk_type, retval;

	retval = cdrom_get_status(&status, &disk_type);
	if (retval == ERR_OK) {
		if ((status & 15) == 6) return(TRUE);
		return(FALSE);
	}
	else {
		return(FALSE); /* error case: assume tray not open */
	}
}


void DCStateSave(void)
{
	unsigned int i, f = 0;

	StateSav_SaveINT(&screen_tv_mode, 1);
	StateSav_SaveINT(&Atari800_tv_mode, 1);
	StateSav_SaveINT(&x_ovr, 1);
	StateSav_SaveINT(&x_key, 1);
	StateSav_SaveINT(&y_ovr, 1);
	StateSav_SaveINT(&y_key, 1);
	StateSav_SaveINT(&b_ovr, 1);
	StateSav_SaveINT(&b_key, 1);
	StateSav_SaveINT(&emulate_paddles, 1);
	StateSav_SaveINT(&glob_snd_ena, 1);
	StateSav_SaveINT(&db_mode, 1);
	StateSav_SaveINT(&INPUT_joy_autofire[0], 1);
	for (i=0; i<16; i++) StateSav_SaveINT(&f, 1);  /* future stuff */

#ifdef DEBUG
        printf("DCStateSave: Atari800_tv_mode = %d, screen_tv_mode = %d, db_mode = %d\n",
               Atari800_tv_mode, screen_tv_mode, db_mode);
#endif
}

void DCStateRead(void)
{
	StateSav_ReadINT(&screen_tv_mode, 1);
	StateSav_ReadINT(&Atari800_tv_mode, 1);
	StateSav_ReadINT(&x_ovr, 1);
	StateSav_ReadINT(&x_key, 1);
	StateSav_ReadINT(&y_ovr, 1);
	StateSav_ReadINT(&y_key, 1);
	StateSav_ReadINT(&b_ovr, 1);
	StateSav_ReadINT(&b_key, 1);
	StateSav_ReadINT(&emulate_paddles, 1);
	StateSav_ReadINT(&glob_snd_ena, 1);
	StateSav_ReadINT(&db_mode, 1);
	StateSav_ReadINT(&INPUT_joy_autofire[0], 1);
#ifdef DEBUG
        printf("DCStateRead: Atari800_tv_mode = %d, screen_tv_mode = %d, db_mode = %d\n",
               Atari800_tv_mode, screen_tv_mode, db_mode);
#endif

	/* sanity check */
	if (Atari800_tv_mode != screen_tv_mode) db_mode = FALSE;

	update_vidmode();
	update_screen_updater();
}

void AboutAtariDC(void)
{
	UI_driver->fInfoScreen("About AtariDC",
			       "AtariDC v" A800DCVERASC " ("__DATE__")\0"
			       "(c) 2002-2006 Christian Groessler\0"
			       "http://www.groessler.org/a800dc\0"
			       "\0"
			       "Please report all problems\0"
			       "to chris@groessler.org\0"
			       "\0"
			       "This port is based on\0"
			       Atari800_TITLE "\0"
			       "http://atari800.atari.org\0"
			       "\0"
			       "It uses the KallistiOS library\0"
			       "http://cadcdev.sourceforge.net\0"
			       "\0"
                               "Dedicated to Tobias & Dominik\0\n");
}


static void ovr2str(int keycode, char *keystr)
{
	sprintf(keystr, "\"%c\" ($%02X)", UI_BASIC_key_to_ascii[keycode], keycode);
}

/* "Button configuration" submenu of "Controller Configuration" */
void ButtonConfiguration(void)
{
	char keystr[3][10];
	int option = 0;
	static UI_tMenuItem menu_array[] = {
		UI_MENU_ACTION(0, "Override X key: "),
		UI_MENU_ACTION(1, "Override Y key: "),
		UI_MENU_ACTION(2, "Override B key: "),
		UI_MENU_END
	};


	do {
		if (x_ovr) {
			ovr2str(x_key, keystr[0]);
			menu_array[0].suffix = keystr[0];
		}
		else {
			menu_array[0].suffix = "OFF      ";
		}

		if (y_ovr) {
			ovr2str(y_key, keystr[1]);
			menu_array[1].suffix = keystr[1];
		}
		else {
			menu_array[1].suffix = "OFF      ";
		}

		if (b_ovr) {
			ovr2str(b_key, keystr[2]);
			menu_array[2].suffix = keystr[2];
		}
		else {
			menu_array[2].suffix = "OFF      ";
		}

		option = UI_driver->fSelect(NULL, TRUE, option, menu_array, NULL);

		switch(option) {
		case 0:
			if (x_ovr) x_ovr = FALSE;
			else {
				x_key = get_emkey("Select X button definition");
				if (x_key != AKEY_NONE) {
					x_ovr = TRUE;
				}
				else {
					x_ovr = FALSE;
				}
			}
			break;
		case 1:
			if (y_ovr) y_ovr = FALSE;
			else {
				y_key = get_emkey("Select Y button definition");
				if (y_key != AKEY_NONE) {
					y_ovr = TRUE;
				}
				else {
					y_ovr = FALSE;
				}
			}
			break;
		case 2:
			if (b_ovr) b_ovr = FALSE;
			else {
				b_key = get_emkey("Select B button definition");
				if (b_key != AKEY_NONE) {
					b_ovr = TRUE;
				}
				else {
					b_ovr = FALSE;
				}
			}
			break;
		}
	} while (option >= 0);
}


static int sound_enabled = TRUE;	/* sound: on or off */

#define DSPRATE 22050
#define FRAGSIZE       12		/* 1<<FRAGSIZE is size of sound buffer */

void dc_sound_init(void)
{
	snd_init();
#ifdef STEREO
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, DSPRATE, 2, 0);
#else
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, DSPRATE, 1, 0);
#endif
}

void Sound_Pause(void)
{
	if (sound_enabled) {
		sound_enabled = FALSE;
		dc_snd_stream_stop();
	}
}

void Sound_Continue(void)
{
	if (! sound_enabled && glob_snd_ena) {
		sound_enabled = TRUE;
		su_first_call = TRUE;
	}
}

void Sound_Exit(void)
{
#ifdef DEBUG
	printf("Sound_Exit called\n");
#endif
}

/* taken from KOS' snd_stream.c */
#define SPU_RAM_BASE		0xa0800000

#define FRAG_SIZE    0x80      /* size of one fragment */
#define FRAG_NUM     16	       /* max. # of fragments in the buffer */
#define FRAG_BUFSZ   (FRAG_NUM * FRAG_SIZE)

static unsigned char fragbuf[FRAG_SIZE]; /* scratch buffer to generate sound data for a fragment */
static uint32 spu_ram_sch1 = 0;

/* ripped from KOS */
static void dc_snd_stream_stop(void)
{
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

	/* Stop stream */
	/* Channel 0 */
	cmd->cmd = AICA_CMD_CHAN;
	cmd->timestamp = 0;
	cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
	cmd->cmd_id = 0;
	chan->cmd = AICA_CH_CMD_STOP;
	snd_sh4_to_aica(tmp, cmd->size);
}

/* ripped from KOS */
static inline unsigned int aica_getpos(int chan)
{
	return(g2_read_32(SPU_RAM_BASE + AICA_CHANNEL(chan) + offsetof(aica_channel_t, pos)));
}

/* some parts taken from KOS' snd_stream.c */
void Sound_Update(void)
{
	static int last_frag;
	int cur_pos, cur_frag, fill_frag, first_frag_to_fill, last_frag_to_fill;

	if (! sound_enabled) return;

	if (su_first_call) {
		AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

		su_first_call = FALSE;

		if (! spu_ram_sch1)
			spu_ram_sch1 = snd_mem_malloc(FRAG_BUFSZ);

		/* prefill buffers */
		POKEYSND_Process(fragbuf, FRAG_SIZE);
		spu_memload(spu_ram_sch1, fragbuf, FRAG_SIZE);

		last_frag = 0;

#ifdef STEREO
#error STEREO not implemented!
#endif
		/* start streaming */
		/* use channel 0 */
		cmd->cmd = AICA_CMD_CHAN;
		cmd->timestamp = 0;
		cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
		cmd->cmd_id = 0;
		chan->cmd = AICA_CH_CMD_START;
		chan->base = spu_ram_sch1;
		chan->type = AICA_SM_8BIT;
		chan->length = FRAG_BUFSZ;
		chan->loop = 1;
		chan->loopstart = 0;
		chan->loopend = FRAG_BUFSZ - 1;
		chan->freq = DSPRATE;
		chan->vol = 240;
		chan->pan = 127;
		snd_sh4_to_aica(tmp, cmd->size);
	}

	/* get current playing position */
	cur_pos = aica_getpos(0);
	cur_frag = cur_pos / FRAG_SIZE;

	/* calc # of new fragments needed to fill sound buffer */
	first_frag_to_fill = last_frag + 1;
	first_frag_to_fill %= FRAG_NUM;
	last_frag_to_fill = cur_frag;

	fill_frag = first_frag_to_fill;
	while (fill_frag != last_frag_to_fill) {
		POKEYSND_Process(fragbuf, FRAG_SIZE);
		spu_memload(spu_ram_sch1 + FRAG_SIZE * fill_frag, fragbuf, FRAG_SIZE);
		last_frag = fill_frag;
		fill_frag++;
		fill_frag %= FRAG_NUM;
	}
#if 0  /* some arm debug stuff */
	printf("cur_pos was %x (%x) (%x)\n", cur_pos,
	       g2_read_32(SPU_RAM_BASE + 0x1f800),
	       g2_read_32(SPU_RAM_BASE + 0x1f804));
#endif
}
