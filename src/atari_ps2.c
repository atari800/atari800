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

//int PS2KbdCAPS=0;
int PS2KbdSHIFT=0;
int PS2KbdCONTROL=0;


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
	PS2KbdSetReadmode(PS2KBD_READMODE_RAW);
//	PS2KbdSetReadmode(PS2KBD_READMODE_NORMAL);
//	Aprint("keyboard read mode is %d", PS2KBD_READMODE_NORMAL);
}

void Atari_Initialise(int *argc, char *argv[])
{
	// Swap Red and Blue components
	int i;
	for (i = 0; i < 256; i++) {
		int c = colortable[i];
//		clut[i] = (c >> 16) + (c & 0xff00) + ((c & 0xff) << 16);
	clut[(i ^ i * 2) & 16 ? i ^ 24 : i] = (c >> 16) + (c & 0xff00) + ((c & 0xff) << 16);
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
//zzz temp exit procedure
//Hard coded to go back to ulaunch
	fioExit();
	SifExitRpc();
	LoadExecPS2("mc0:/BOOT/BOOT.ELF", 0, NULL);
//zzz end
	return FALSE;
}

void Atari_DisplayScreen(void)
{
	GSTEXTURE tex;
	tex.Width = ATARI_WIDTH;
	tex.Height = ATARI_HEIGHT;
	tex.PSM = GS_PSM_T8;
	tex.Mem = (UBYTE *) atari_screen;
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
	PS2KbdRawKey key;
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
		if (new_pad & PAD_L1)
		    return AKEY_COLDSTART;
		if (new_pad & PAD_R1)
			return AKEY_WARMSTART;
	}
	//PAD_CROSS is used for Atari_TRIG().
	if (new_pad & PAD_TRIANGLE)
		return AKEY_UI;
	if (new_pad & PAD_SQUARE)
		return AKEY_SPACE;
	if (new_pad & PAD_CIRCLE)
		return AKEY_RETURN;
	if (new_pad & PAD_L1)
		return AKEY_COLDSTART;
	if (new_pad & PAD_R1)
		return AKEY_WARMSTART;
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

	while (PS2KbdReadRaw(&key) != 0) {
		if (key.state == PS2KBD_RAWKEY_DOWN) {
			switch (key.key) {
			case EOF:
				Atari800_Exit(FALSE);
				exit(0);
			    break;
			case 0x28:
				return AKEY_RETURN;
			case 0x29:
			    return AKEY_ESCAPE;
			case 0x2A:
				return AKEY_BACKSPACE;
			case 0x2B:
				return AKEY_TAB;
			case 0x2C:
				return AKEY_SPACE;
			case 0x46:
				return AKEY_SCREENSHOT;
			case 0x4F:
			    return AKEY_RIGHT;
			case 0x50:
			    return AKEY_LEFT;
			case 0x51:
			    return AKEY_DOWN;
			case 0x52:
			    return AKEY_UP;
			case 0x58:
			    return AKEY_RETURN;

			case 0xE0:
				PS2KbdCONTROL = 1;
				return AKEY_NONE;
			case 0xE4:
				PS2KbdCONTROL = 1;
				return AKEY_NONE;
			case 0xE1:
				PS2KbdSHIFT = 1;
				return AKEY_NONE;
			case 0xE5:
				PS2KbdSHIFT = 1;
				return AKEY_NONE;
			//todo: map the following keys
			//control <>: (suits)
			//akey_clear
			//AKEY_BREAK
			//control-1 = pause??(is this break?)
			//control-2 = bell (clear?)
			//control-3 = error - 136? (break?)
			//akey_caret && circumflex difference???
			//TODO:enable keyboard repeat when shift nor control pressed
			default:
				break;
			}
		}

		if ((key.state == PS2KBD_RAWKEY_DOWN) && (!PS2KbdSHIFT)) {
			switch (key.key) {
			case 0x1E:
				return AKEY_1;
			case 0X1F:
				return AKEY_2;
			case 0x20:
				return AKEY_3;
			case 0x21:
				return AKEY_4;
			case 0x22:
				return AKEY_5;
			case 0x23:
				return AKEY_6;
			case 0x24:
				return AKEY_7;
			case 0x25:
				return AKEY_8;
			case 0x26:
				return AKEY_9;
			case 0x27:
				return AKEY_0;
			case 0x2D:
				return AKEY_MINUS;
			case 0x2E:
				return AKEY_EQUAL;
			case 0x2F:
				return AKEY_BRACKETLEFT;
			case 0x30:
				return AKEY_BRACKETRIGHT;
			case 0x31:
				return AKEY_BACKSLASH;
			case 0x33:
				return AKEY_SEMICOLON;
			case 0x34:
				return AKEY_QUOTE;
			case 0x35:
				return AKEY_ATARI;
			case 0x36:
				return AKEY_COMMA;
			case 0x37:
				return AKEY_FULLSTOP;
			case 0x38:
				return AKEY_SLASH;
			case 0x3A:
				return AKEY_UI;
			case 0x3E:
				return AKEY_WARMSTART;
			case 0x41:
				return AKEY_EXIT;
			case 0x43:
				return AKEY_SCREENSHOT;
			default:
				break;
			}
		}
		if ((key.state == PS2KBD_RAWKEY_DOWN) && PS2KbdSHIFT && !PS2KbdCONTROL) {
			switch (key.key) {
			case 0x4:
				return AKEY_A;
			case 0x5:
				return AKEY_B;
			case 0x6:
				return AKEY_C;
			case 0x7:
				return AKEY_D;
			case 0x8:
				return AKEY_E;
			case 0x9:
				return AKEY_F;
			case 0xA:
				return AKEY_G;
			case 0xB:
				return AKEY_H;
			case 0xC:
				return AKEY_I;
			case 0xD:
				return AKEY_J;
			case 0xE:
				return AKEY_K;
			case 0xF:
				return AKEY_L;
			case 0x10:
				return AKEY_M;
			case 0x11:
				return AKEY_N;
			case 0x12:
				return AKEY_O;
			case 0x13:
				return AKEY_P;
			case 0x14:
				return AKEY_Q;
			case 0x15:
				return AKEY_R;
			case 0x16:
				return AKEY_S;
			case 0x17:
				return AKEY_T;
			case 0x18:
				return AKEY_U;
			case 0x19:
				return AKEY_V;
			case 0x1A:
				return AKEY_W;
			case 0x1B:
				return AKEY_X;
			case 0x1C:
				return AKEY_Y;
			case 0x1D:
				return AKEY_Z;
			case 0x1E:
				return AKEY_EXCLAMATION;
			case 0X1F:
				return AKEY_AT;
			case 0x20:
				return AKEY_HASH;
			case 0x21:
				return AKEY_DOLLAR;
			case 0x22:
				return AKEY_PERCENT;
			case 0x23:
//				return AKEY_CIRCUMFLEX;
				return AKEY_CARET;
			case 0x24:
				return AKEY_AMPERSAND;
			case 0x25:
				return AKEY_ASTERISK;
			case 0x26:
				return AKEY_PARENLEFT;
			case 0x27:
				return AKEY_PARENRIGHT;
			case 0x2B:
				return AKEY_SETTAB;
			case 0x2D:
				return AKEY_UNDERSCORE;
			case 0x2E:
				return AKEY_PLUS;
			case 0x31:
				return AKEY_BAR;
			case 0x33:
				return AKEY_COLON;
			case 0x34:
				return AKEY_DBLQUOTE;
			case 0x36:
				return AKEY_LESS;
			case 0x37:
				return AKEY_GREATER;
			case 0x38:
				return AKEY_QUESTION;
			case 0x3E:
				return AKEY_COLDSTART;
			case 0x49:
				return AKEY_INSERT_LINE;
			case 0x4C:
				return AKEY_DELETE_LINE;
			default:
				break;
			}
		}
		if ((key.state == PS2KBD_RAWKEY_DOWN) && !PS2KbdSHIFT && !PS2KbdCONTROL) {
			switch (key.key) {
			case 0x4:
				return AKEY_a;
			case 0x5:
				return AKEY_b;
			case 0x6:
				return AKEY_c;
			case 0x7:
				return AKEY_d;
			case 0x8:
				return AKEY_e;
			case 0x9:
				return AKEY_f;
			case 0xA:
				return AKEY_g;
			case 0xB:
				return AKEY_h;
			case 0xC:
				return AKEY_i;
			case 0xD:
				return AKEY_j;
			case 0xE:
				return AKEY_k;
			case 0xF:
				return AKEY_l;
			case 0x10:
				return AKEY_m;
			case 0x11:
				return AKEY_n;
			case 0x12:
				return AKEY_o;
			case 0x13:
				return AKEY_p;
			case 0x14:
				return AKEY_q;
			case 0x15:
				return AKEY_r;
			case 0x16:
				return AKEY_s;
			case 0x17:
				return AKEY_t;
			case 0x18:
				return AKEY_u;
			case 0x19:
				return AKEY_v;
			case 0x1A:
				return AKEY_w;
			case 0x1B:
				return AKEY_x;
			case 0x1C:
				return AKEY_y;
			case 0x1D:
				return AKEY_z;
			case 0x49:
				return AKEY_INSERT_CHAR;
			case 0x4C:
				return AKEY_DELETE_CHAR;
			default:
				break;
			}
		}
		if ((key.state == PS2KBD_RAWKEY_DOWN) && (PS2KbdCONTROL)) {
			switch(key.key) {
			case 0x4:
				return AKEY_CTRL_a;
			case 0x5:
				return AKEY_CTRL_b;
			case 0x6:
				return AKEY_CTRL_c;
			case 0x7:
				return AKEY_CTRL_d;
			case 0x8:
				return AKEY_CTRL_e;
			case 0x9:
				return AKEY_CTRL_f;
			case 0xA:
				return AKEY_CTRL_g;
			case 0xB:
				return AKEY_CTRL_h;
			case 0xC:
				return AKEY_CTRL_i;
			case 0xD:
				return AKEY_CTRL_j;
			case 0xE:
				return AKEY_CTRL_k;
			case 0xF:
				return AKEY_CTRL_l;
			case 0x10:
				return AKEY_CTRL_m;
			case 0x11:
				return AKEY_CTRL_n;
			case 0x12:
				return AKEY_CTRL_o;
			case 0x13:
				return AKEY_CTRL_p;
			case 0x14:
				return AKEY_CTRL_q;
			case 0x15:
				return AKEY_CTRL_r;
			case 0x16:
				return AKEY_CTRL_s;
			case 0x17:
				return AKEY_CTRL_t;
			case 0x18:
				return AKEY_CTRL_u;
			case 0x19:
				return AKEY_CTRL_v;
			case 0x1A:
				return AKEY_CTRL_w;
			case 0x1B:
				return AKEY_CTRL_x;
			case 0x1C:
				return AKEY_CTRL_y;
			case 0x1D:
				return AKEY_CTRL_z;
			case 0x2B:
				return AKEY_CLRTAB;
			default:
				break;
			}
		}
		if (key.state == PS2KBD_RAWKEY_UP) {
			switch (key.key) {
			case 0x39:
			//todo: Turn off caps lock LED.
			return AKEY_CAPSTOGGLE;
			case 0xE0:
				PS2KbdCONTROL = 0;
				return AKEY_NONE;
			case 0xE4:
				PS2KbdCONTROL = 0;
				return AKEY_NONE;
			case 0xE1:
				PS2KbdSHIFT = 0;
				return AKEY_NONE;
			case 0xE5:
				PS2KbdSHIFT = 0;
				return AKEY_NONE;
			default:
				break;
			}
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
	if (num == 0 && PadButtons() & PAD_CROSS)
		return 0;
	return 1;
}

int main(int argc, char **argv)
{
	loadModules();
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
