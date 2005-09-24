/*
 * atari_ps2.c - Sony PlayStation 2 port code
 *
 * Copyright (c) 2005 Troy Ayers and Piotr Fusik
 * Copyright (c) 2005 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include <tamtypes.h>
#include <loadfile.h>
#include <fileio.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <kernel.h>
#include <debug.h>
#include <libkbd.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>

#include "atari.h"
#include "colours.h"
#include "input.h"
#include "log.h"
#include "monitor.h"
#include "screen.h"
#include "ui.h"

extern unsigned char usbd[];
extern unsigned int size_usbd;

extern unsigned char ps2kbd[];
extern unsigned int size_ps2kbd;

static GSGLOBAL *gsGlobal = NULL;

static int clut[256] __attribute__((aligned(16)));

#define PAD_PORT 0
#define PAD_SLOT 0

static char padBuf[256] __attribute__((aligned(64)));

void loadModules(void)
{
	int ret;
	init_scr();

	ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
	if (ret < 0) {
		Aprint("Sio2man loading failed: %d", ret);
		SleepThread();
	}

//	Aprint("mcman");
	SifLoadModule("rom0:MCMAN", 0, NULL);

//	Aprint("mcserv");
	SifLoadModule("rom0:MCSERV", 0, NULL);

//	Aprint("padman");
	ret = SifLoadModule("rom0:PADMAN", 0, NULL);
	if (ret < 0) {
		Aprint("Padman loading failed: %d", ret);
		SleepThread();
	}

//	mcInit(MC_TYPE_MC);

//	cdinit(1);
	SifInitRpc(0);

#if 1
	SifExecModuleBuffer(usbd, size_usbd, 0, NULL, &ret);
	SifExecModuleBuffer(ps2kbd, size_ps2kbd, 0, NULL, &ret);
#else
	SifLoadModule("mc0:/ATARI/usbd.irx", 0, 0);
	SifLoadModule("mc0:/ATARI/ps2kbd.irx", 0, 0);
#endif

	if (PS2KbdInit() == 0) {
		Aprint("Failed to Init Keyboard.");
	}
//	PS2KbdSetReadmode(PS2KBD_READMODE_RAW);
	PS2KbdSetReadmode(PS2KBD_READMODE_NORMAL);
//	Aprint("keyboard read mode is %d", PS2KBD_READMODE_NORMAL);
}

void Atari_Initialise(int *argc, char *argv[])
{
	// Swap Red and Blue components
	int i;
	for (i = 0; i < 256; i++) {
		int c = colortable[i];
		clut[i] = (c >> 16) + (c & 0xff00) + ((c & 0xff) << 16);
	}
	// Clear debug from screen
	init_scr();
	// Initialize graphics
	gsGlobal = gsKit_init_global(GS_MODE_NTSC);
	dmaKit_init(D_CTRL_RELE_ON,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
	            D_CTRL_STD_OFF, D_CTRL_RCYC_8);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	gsGlobal->PSM = GS_PSM_CT32;
	gsGlobal->Test->ZTE = 0;
	gsKit_init_screen(gsGlobal);
	// Init joypad
	padInit(0);
	padPortOpen(PAD_PORT, PAD_SLOT, padBuf);
}

int Atari_Exit(int run_monitor)
{
	// TODO: shutdown graphics mode
	Aflushlog();
#if 0
	if (run_monitor && monitor()) {
		// TODO: reinitialize graphics mode
		return TRUE;
	}
#endif
	return FALSE;
}

void Atari_DisplayScreen(UBYTE *screen)
{
	GSTEXTURE tex;
	tex.Width = ATARI_WIDTH;
	tex.Height = ATARI_HEIGHT;
	tex.PSM = GS_PSM_T8;
	tex.Mem = screen;
	tex.Clut = clut;
	tex.Vram = 0x200000;
	tex.VramClut = 0x280000;
	tex.Filter = GS_FILTER_LINEAR;
	// TODO: upload clut just once
	gsKit_texture_upload(gsGlobal, &tex);
	gsKit_prim_sprite_texture(gsGlobal, &tex, 0, 0, 32, 0, 640, 480, 32 + 320, 240, 0, 0x80808080);
	// TODO: flip without vsync
	gsKit_sync_flip(gsGlobal);
}

static int PadButtons(void)
{
	struct padButtonStatus buttons;
	for (;;) {
		int ret = padGetState(PAD_PORT, PAD_SLOT);
		if (ret == PAD_STATE_STABLE || ret == PAD_STATE_FINDCTP1)
			break;
		if (ret == PAD_STATE_DISCONN)
			return 0;
	}
	padRead(PAD_PORT, PAD_SLOT, &buttons);
	return ~buttons.btns;
}

int Atari_Keyboard(void)
{
	int new_pad = PadButtons();
	key_consol = CONSOL_NONE;

	if (ui_is_active) {
		if (new_pad & PAD_CROSS)
			return AKEY_RETURN;
		if (new_pad & PAD_CIRCLE)
			return AKEY_ESCAPE;
		if (new_pad & PAD_LEFT)
			return AKEY_LEFT;
		if (new_pad & PAD_RIGHT)
			return AKEY_RIGHT;
		if (new_pad & PAD_UP)
			return AKEY_UP;
		if (new_pad & PAD_DOWN)
			return AKEY_DOWN;
	}
	else {
		if (new_pad & PAD_TRIANGLE)
			return AKEY_UI;
	}
	if (new_pad & PAD_L1)
		return AKEY_COLDSTART;
	if (machine_type == MACHINE_5200) {
		if (new_pad & PAD_START)
			return AKEY_5200_START;
	}
	else {
		if (new_pad & PAD_START)
			key_consol ^= CONSOL_START;
		if (new_pad & PAD_SELECT)
			key_consol ^= CONSOL_SELECT;
		if (new_pad & PAD_CROSS)
			return AKEY_HELP;
	}
	if (machine_type != MACHINE_5200 || ui_is_active) {
		char key;
		while (PS2KbdRead(&key) != 0) {
			if (key == PS2KBD_ESCAPE_KEY)
				PS2KbdRead(&key);
			switch (key) {
			case EOF:
				Atari800_Exit(FALSE);
				exit(0);
				break;
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
			case ' ':
				return AKEY_SPACE;
			case '`':
				return AKEY_CAPSTOGGLE;
			case '!':
				return AKEY_EXCLAMATION;
			case '"':
				return AKEY_DBLQUOTE;
			case '#':
				return AKEY_HASH;
			case '$':
				return AKEY_DOLLAR;
			case '%':
				return AKEY_PERCENT;
			case '&':
				return AKEY_AMPERSAND;
			case '\'':
				return AKEY_QUOTE;
			case '@':
				return AKEY_AT;
			case '(':
				return AKEY_PARENLEFT;
			case ')':
				return AKEY_PARENRIGHT;
			case '[':
				return AKEY_BRACKETLEFT;
			case ']':
				return AKEY_BRACKETRIGHT;
			case '<':
				return AKEY_LESS;
			case '>':
				return AKEY_GREATER;
			case '=':
				return AKEY_EQUAL;
			case '?':
				return AKEY_QUESTION;
			case '-':
				return AKEY_MINUS;
			case '+':
				return AKEY_PLUS;
			case '*':
				return AKEY_ASTERISK;
			case '/':
				return AKEY_SLASH;
			case ':':
				return AKEY_COLON;
			case ';':
				return AKEY_SEMICOLON;
			case ',':
				return AKEY_COMMA;
			case '.':
				return AKEY_FULLSTOP;
			case '_':
				return AKEY_UNDERSCORE;
			case '^':
				return AKEY_CIRCUMFLEX;
			case '\\':
				return AKEY_BACKSLASH;
			case '|':
				return AKEY_BAR;
			case '\n':
				return AKEY_RETURN;
			case '\t':
				return AKEY_TAB;
			case '\b':
				return AKEY_BACKSPACE;
			// TODO: control keys, arrows
			default:
				break;
			}
		}
	}
	return AKEY_NONE;
}

int Atari_PORT(int num)
{
	int ret = 0xff;
	if (num == 0) {
		int pad = PadButtons();
		if (pad & PAD_LEFT)
			ret &= 0xf0 | STICK_LEFT;
		if (pad & PAD_RIGHT)
			ret &= 0xf0 | STICK_RIGHT;
		if (pad & PAD_UP)
			ret &= 0xf0 | STICK_FORWARD;
		if (pad & PAD_DOWN)
			ret &= 0xf0 | STICK_BACK;
	}
	return ret;
}

int Atari_TRIG(int num)
{
	if (num == 0 && PadButtons() & PAD_SQUARE)
		return 0;
	return 1;
}

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while (TRUE) {
		key_code = Atari_Keyboard();
		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen((UBYTE *) atari_screen);
	}
}
