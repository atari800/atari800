/*
 * main.c - Atari Falcon specific port code
 *
 * Copyright (c) 1997-1998 Petr Stehlik and Karel Rous
 * Copyright (c) 1998-2017 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gemx.h>
#include <mint/cookie.h>
#include <mint/falcon.h>
#include <mint/osbind.h>
#include <mint/sysvars.h>

#include "atari.h"
#include "binload.h"
#include "colours.h"
#include "ui.h"
#include "input.h"
#include "akey.h"
#include "screen.h"
#include "platform.h"
#include "pokeysnd.h"
#include "monitor.h"
#include "sound.h"
#include "log.h"
#include "util.h"

#include "falcon/xcb.h"		/* for NOVA screensaver */

/* -------------------------------------------------------------------------- */

#ifdef SCREENSAVER

#include "falcon/jclkcook.h"

static int Clocky_SS(int on)
{
	long adr;
	JCLKSTRUCT *jclk;
	int oldval;

/*	CHECK_CLOCKY_STRUCT; */

	if (Getcookie(CLOCKY_IDENT_NUM, &adr) == C_NOTFOUND)
		return 0;

	jclk = (JCLKSTRUCT *) adr;

	if (jclk->name != CLOCKY_IDENT) {
		return 0;
	}

	if ((jclk->version / 0x100) != (CLOCKY_VERSION / 0x100)) {
		return 0;
	}

	oldval = jclk->switches.par.SaverOn;
	jclk->switches.par.SaverOn = on;	/* turn the Clocky's screen saver on/off */

	return oldval;
}

static int Clocky_SSval;		/* original value */
static int NOVA_SSval;
#endif	/* SCREENSAVER */

/* -------------------------------------------------------------------------- */

typedef enum {
	UNKNOWN,
	TT030,
	F030,
	Milan
} Video_HW;
static Video_HW video_hw = UNKNOWN;
static int bitplanes = TRUE;	/* Atari 256 colour mode uses 8 bit planes */
static int vga;
static int vga50;
static int sv;

static short gl_app_id = -1;
static short gl_vdi_handle;
static XCB *NOVA_xcb;
static int NOVA_double_size;
static int HOST_WIDTH, HOST_HEIGHT, HOST_PLANES;
#define EMUL_WIDTH	(NOVA_double_size ? 2*336 : 336)
#define EMUL_HEIGHT	(NOVA_double_size ? 2*240 : 240)
#undef QUAD
#define CENTER_X	((vramw - EMUL_WIDTH) / 2)
#define CENTER_Y	((vramh - EMUL_HEIGHT) / 2)
#define CENTER		(CENTER_X + CENTER_Y * vramw)

#ifdef SHOW_DISK_LED
static int LED_timeout = 0;
#endif

/* -------------------------------------------------------------------------- */

static int trig0;
static int stick0;
static int joyswap;

/* parameters for c2p_uni */
UWORD screenw, screenh, vramw, vramh;
UBYTE *odkud, *kam;
UBYTE *oldscreen = NULL;	/* pointer to previous screen if double buffering is turned on */

/* parameters for DisplayScreen */
static int skip_N_frames;
static int delta_screen;

extern void init_kb(void);
extern void rem_kb(void);
extern char key_buf[128];
extern UBYTE joy0, joy1, buttons;
_KEYTAB *key_tab;
#define	KEYBUF_SIZE	256
unsigned char keybuf[KEYBUF_SIZE];
int kbhead = 0;

static UBYTE *Original_Phys_base;
static UWORD original_videl_settings[26];
static UWORD mode336x240_vga[26]= {
	0x0133, 0x0001, 0x3b00, 0x0150, 0x00f0, 0x0008, 0x0002, 0x0010, \
	0x0000, 0x00a8, 0x0186, 0x0005, 0x00c6, 0x0095, 0x000d, 0x0292, 0x0083, \
	0x0096, 0x0000, 0x0000, 0x0419, 0x03ff, 0x003f, 0x003f, 0x03ff, 0x0415
};
static UWORD mode336x240_vga50[26]= {
	0x0133, 0x0001, 0x3b00, 0x0150, 0x00f0, 0x0008, 0x0002, 0x0010, \
	0x0000, 0x00a8, 0x0186, 0x0005, 0x00c6, 0x008d, 0x0015, 0x0292, 0x0083, \
	0x0097, 0x0000, 0x0000, 0x04eb, 0x0465, 0x00a5, 0x00a5, 0x0465, 0x04e7
};
static UWORD mode336x240_rgb[26]= {
	0x0133, 0x0001, 0x3b00, 0x0150, 0x00f0, 0x0008, 0x0002, 0x0010, \
	0x0000, 0x00a8, 0x0185, 0x0000, 0x00c7, 0x00a0, 0x001f, 0x02b2, 0x0091, \
	0x00ab, 0x0000, 0x0000, 0x0271, 0x0265, 0x002f, 0x0059, 0x0239, 0x026b
};

static int reprogram_VIDEL;
static int new_videl_mode_valid;
static UBYTE *new_videoram_unaligned;
static UBYTE *Screen_atari_unaligned;
static UBYTE *Screen_atari_b_unaligned;

extern void rplanes(void);
extern void rplanes_delta(void);
extern void load_r(void);
extern void save_r(void);
extern UWORD *p_str_p;
extern void store_palette();
extern void restore_palette();
extern void set_falcon_palette();

static ULONG f030coltable[256];
static short int coltable[256][3], coltable_backup[256][3];

/* -------------------------------------------------------------------------- */

static ULONG old_frclock;
#define SCREEN_BUFFERS	2
static UBYTE* new_videobases[SCREEN_BUFFERS];
#define new_videobase new_videobases[0]	/* work address (pointer to logical buffer) */

static void SwapScreen()
{
	ULONG new_frclock;
	int i;
	UBYTE* tmp_videobase;
	size_t offset;

	/* always wait at least one VBL before swapping the screen */
	for (new_frclock = (ULONG)get_sysvar((void*)_frclock);
		 new_frclock == old_frclock;
		 new_frclock = (ULONG)get_sysvar((void*)_frclock));
	old_frclock = new_frclock;

	if (delta_screen || SCREEN_BUFFERS < 2) {
		/* wait for VBL only */
		return;
	}

	/* set new physical address */
	offset = sv ? (Screen_WIDTH - 336) / 2 : 0;	/* in pixels */
	VsetScreen(SCR_NOCHANGE, new_videobase + offset, SCR_NOCHANGE, SCR_NOCHANGE);

	/* cycle screens */
	tmp_videobase = new_videobases[0];
	for (i = 0; i < SCREEN_BUFFERS-1; ++i) {
		new_videobases[i] = new_videobases[i+1];
	}
	new_videobases[SCREEN_BUFFERS-1] = tmp_videobase;

	if (sv && !UI_is_active) {
		Screen_atari = (ULONG*)new_videobase;
	}
}

static void* NewLogbase()
{
	if (reprogram_VIDEL)
		return new_videobase;
	else
		return Logbase();
}

/* -------------------------------------------------------------------------- */

static void set_colors(int new)
{
	int i;

	if (reprogram_VIDEL) {
		if (new) {
			p_str_p = (UWORD*) f030coltable;
			Supexec(set_falcon_palette);
		} else {
			Supexec(restore_palette);
		}
	} else {
		for(i=0; i<256; i++)
			vs_color(gl_vdi_handle, i, new ? coltable[i] : coltable_backup[i]);
	}
}

static void save_original_colors(void)
{
	int i;

	if (reprogram_VIDEL) {
		Supexec(store_palette);
	} else {
		for(i=0; i<256; i++)
			vq_color(gl_vdi_handle, i, 1, coltable_backup[i]);
	}
}

static void set_new_colors(void) { set_colors(1); }

static void restore_original_colors(void) { set_colors(0); }

/* -------------------------------------------------------------------------- */

static int setup_done;
static void SetupEmulatedEnvironment(void)
{
	if (reprogram_VIDEL) {
		size_t offset;

		/* save original VIDEL settings */
		p_str_p = original_videl_settings;
		Supexec(save_r);

		/* set new video resolution by direct VIDEL programming */
		offset = sv ? (Screen_WIDTH - 336) / 2 : 0;	/* in pixels */
		(void)VsetScreen(SCR_NOCHANGE, new_videobases[delta_screen ? 0 : SCREEN_BUFFERS-1] + offset, SCR_NOCHANGE, SCR_NOCHANGE);
		p_str_p = vga ? (vga50 ? mode336x240_vga50 : mode336x240_vga) : mode336x240_rgb;
		Supexec(load_r);
		new_videl_mode_valid = 1;
	}

	set_new_colors();	/* setup new color palette */

	Supexec(init_kb);	/* our keyboard routine */

	Bconout(4, 0x14);	/* joystick init */

	setup_done = TRUE;
}

static void ShutdownEmulatedEnvironment(void)
{
	if (!setup_done)
		return;

	if (new_videl_mode_valid) {
		/* restore original VIDEL mode */
		p_str_p = original_videl_settings;
		Supexec(load_r);
		new_videl_mode_valid = 0;
		(void)VsetScreen(SCR_NOCHANGE, Original_Phys_base, SCR_NOCHANGE, SCR_NOCHANGE);
	}

	restore_original_colors();

	Supexec(rem_kb);	/* original keyboard routine */

	Bconout(4, 8);		/* joystick disable */

	setup_done = FALSE;
}

/* -------------------------------------------------------------------------- */

static void PrintError(const char* msg)
{
	if (gl_app_id != -1) {
		char buf[255+1];
		sprintf(buf, "[1][%s][ OK ]", msg);
		form_alert(1, buf);
	} else {
		Log_print(msg);
		Log_print("Press enter to exit.");
		getchar();
	}
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	long video_hardware;
	int force_videl = FALSE;	/* force Atari800 to switch VIDEL into new resolution by direct HW programming */

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */

		if (strcmp(argv[i], "-interlace") == 0) {
			if (i_a)
				skip_N_frames = Util_sscandec(argv[++i]);
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-joyswap") == 0)
			joyswap = TRUE;
		else if (strcmp(argv[i], "-videl") == 0)
			force_videl = TRUE;
		else if (strcmp(argv[i], "-double") == 0)
			NOVA_double_size = TRUE;
		else if (strcmp(argv[i], "-delta") == 0)
			delta_screen = TRUE;
		else if (strcmp(argv[i], "-vga50") == 0)
			vga50 = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-interlace x  Generate Falcon screen only every X frame\n");
				Log_print("\t-joyswap      Exchange joysticks\n");
				Log_print("\t-videl        Direct VIDEL programming (Falcon only); enables double buffering\n");
				Log_print("\t-double       Double width/height (ignored for planar modes and -videl)\n");
				Log_print("\t-delta        Delta screen output (ignored for -videl with SuperVidel; disables double buffering)\n");
				Log_print("\t-vga50        Use VGA/50 Hz for -videl\n");
			}

			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}

	*argc = j;

	/* recalculate color tables */
	for (i = 0; i < 256; i++) {
		UBYTE r = Colours_GetR(i);
		UBYTE g = Colours_GetG(i);
		UBYTE b = Colours_GetB(i);

		/* Native:  RRRRRRrrGGGGGGgg--------BBBBBBbb */
		f030coltable[i] = (r << 24) | (g << 16) | b;
		if (sv) {
			coltable[i][0] = r * 1000 / 255;
			coltable[i][1] = g * 1000 / 255;
			coltable[i][2] = b * 1000 / 255;
		} else {
			/* not sure what's going on here, the VDI sets wrong colours if
			 * code above is used
			 */
			coltable[i][0] = (r/4) * 1000 / 63;
			coltable[i][1] = (g/4) * 1000 / 63;
			coltable[i][2] = (b/4) * 1000 / 63;
		}
	}

	/* check for VIDEL hardware */
	if (Getcookie(C__VDO, &video_hardware) == C_NOTFOUND)
		video_hardware = 0;

	switch(video_hardware >> 16) {
		case 2:
			video_hw = TT030;
			break;
		case 3:
			video_hw = F030;
			break;
		case 4:
			video_hw = Milan;
			bitplanes = FALSE;
			break;
		default:
			video_hw = UNKNOWN;
			bitplanes = FALSE;
	}

	vga = (video_hw == F030) ? VgetMonitor() == MON_VGA : TRUE;

	/* check for the SuperVidel */
	if (Getcookie(C_SupV, NULL) == C_FOUND && vga)
		sv = TRUE;

	if (!force_videl || !sv) {
		if ((Screen_atari_unaligned = (UBYTE*)Mxalloc(Screen_HEIGHT*Screen_WIDTH+15, MX_PREFTTRAM)) == NULL) {
			PrintError("Error allocating Screen_atari.");
			return FALSE;
		}
		Screen_atari = (ULONG*)(((ULONG)Screen_atari_unaligned + 15) & ~15);
		memset(Screen_atari, 0, Screen_HEIGHT * Screen_WIDTH);
	}

#ifdef BITPL_SCR
	if (delta_screen && !sv) {
		if ((Screen_atari_b_unaligned = (UBYTE*)Mxalloc(Screen_HEIGHT*Screen_WIDTH+15, MX_PREFTTRAM)) == NULL) {
			PrintError("Error allocating Screen_atari.");
			return FALSE;
		}
		Screen_atari_b = (ULONG*)(((ULONG)Screen_atari_b_unaligned + 15) & ~15);
		memset(Screen_atari_b, 0, Screen_HEIGHT * Screen_WIDTH);

		Screen_atari1 = Screen_atari;
		Screen_atari2 = Screen_atari_b;
	}
#endif

	if (force_videl && video_hw == F030) {	/* we may switch VIDEL directly */
		int buffers = (delta_screen && !sv) ? 1 : SCREEN_BUFFERS;
		vramw = screenw = sv ? Screen_WIDTH : 336;
		vramh = screenh = Screen_HEIGHT;

		if ((new_videoram_unaligned = (UBYTE*)Mxalloc(buffers*vramw*vramh+15, MX_STRAM)) == NULL) {
			PrintError("Error allocating video memory.");
			return FALSE;
		}

		new_videobases[0] = (UBYTE*)(((ULONG)new_videoram_unaligned + 15) & ~15);
		for (i = 1; i < buffers; ++i) {
			new_videobases[i] = new_videobases[i-1] + vramw*vramh;
		}
		memset(new_videobases[0], 0, buffers*vramw*vramh);	/* all buffers */

		if (sv) {
			delta_screen = FALSE;

			/* use SuperVidel's memory instead of ST RAM */
			for (i = 0; i < buffers; ++i) {
				new_videobases[i] = (UBYTE*)((ULONG)new_videobases[i] | 0xA0000000);
			}
			/* use direct rendering */
			Screen_atari = (ULONG*)new_videobase;
			if (vga50) {
				/* use the new chunky mode */
				mode336x240_vga50[7] |= 0x1000;
				/* use virtual offset of 384 - 336 = 48 bytes = 24 words */
				mode336x240_vga50[8] = (Screen_WIDTH - 336) / 2;
			} else {
				/* use the new chunky mode */
				mode336x240_vga[7] |= 0x1000;
				/* use virtual offset of 384 - 336 = 48 bytes = 24 words */
				mode336x240_vga[8] = (Screen_WIDTH - 336) / 2;
			}
			bitplanes = FALSE;
		} else {
			bitplanes = TRUE;
		}

		/* create new graphics mode {336|384}x240 in 256 colors */
		reprogram_VIDEL = TRUE;
	} else {
		short work_in[16], work_out[272];

		/* GEM init */
		gl_app_id = appl_init();

		if (gl_app_id == -1 && aes_global[0] != 0) {
			PrintError("Failed to initialise the AES.");
			return FALSE;
		}

		if (gl_app_id == -1) {
			appl_exit();
		}

		memset(work_in, 0, sizeof(work_in));
		work_in[0] = 1;	/* current resolution */
		for(i = 1; i < 10; work_in[i++] = 1);
		work_in[10] = 2;

		if (gl_app_id != -1) {
			short wcell, hcell, wbox, hbox;
			gl_vdi_handle = graf_handle(&wcell, &hcell, &wbox, &hbox);
			v_opnvwk(work_in, &gl_vdi_handle, work_out);
		} else {
			v_opnwk(work_in, &gl_vdi_handle, work_out);
		}

		if (gl_vdi_handle == 0) {
			PrintError("Failed to open VDI workstation.");
			return FALSE;
		}

		/* get current screen size and color depth */
		HOST_WIDTH = work_out[0] + 1;
		HOST_HEIGHT = work_out[1] + 1;

		vq_extnd(gl_vdi_handle, 1, work_out);
		HOST_PLANES = work_out[4];

		if (Getcookie(C_EdDI, NULL) == C_FOUND) {
			vq_scrninfo(gl_vdi_handle, work_out);
			bitplanes = (work_out[0] != 2);
		} else {
			/* guess some common setups */
			if (Getcookie(C_NOVA, (long int*)&NOVA_xcb) == C_FOUND)
				bitplanes = FALSE;
			else if (Getcookie(C_fVDI, NULL) == C_FOUND)
				bitplanes = FALSE;
			else if (sv && (VsetMode(VM_INQUIRE) & BPS8C) == BPS8C)
				bitplanes = FALSE;
		}

		if (HOST_PLANES == 8 && HOST_WIDTH >= 320 && HOST_HEIGHT >= Screen_HEIGHT) {
			/* current resolution is OK */
			vramw = HOST_WIDTH;
			vramh = HOST_HEIGHT;

			if (vramw >= 336)
				screenw = 336;
			else
				screenw = 320;
			screenh = Screen_HEIGHT;

			sv = FALSE;
		} else {
			/* we may also try to switch into proper resolution using XBios call and then
			   reinitialize VDI - we've been told it would work OK */
			if (video_hw == F030)
				PrintError("Atari800 needs 320x240|or higher res. in 256 colors.|Or use the -videl switch.");
			else
				PrintError("Atari800 needs 320x240|or higher res. in 256 colors.");
			return FALSE;
		}

		if (gl_app_id != -1) {
			graf_mouse(M_OFF, NULL);
			/* lock GEM */
			wind_update(BEG_UPDATE);
		}
		v_clrwk(gl_vdi_handle);		/* clear whole screen */
	}

	save_original_colors();

#ifdef SCREENSAVER
	Clocky_SSval = Clocky_SS(0);	/* turn off Clocky's screen saver */
	if (NOVA_xcb) {
		NOVA_SSval = NOVA_xcb->blnk_time;
		NOVA_xcb->blnk_time = 0;
	}
#endif

	Original_Phys_base = Physbase();

	key_tab = Keytbl(-1, -1, -1);

#ifdef SOUND
	if (!Sound_Initialise(argc, argv))
		return FALSE;
#endif

	SetupEmulatedEnvironment();

	return TRUE;
}

/* -------------------------------------------------------------------------- */

int PLATFORM_Exit(int run_monitor)
{
	ShutdownEmulatedEnvironment();

#ifdef BUFFERED_LOG
	Log_flushlog();
#endif

	if (run_monitor) {
		if (MONITOR_Run()) {
			SetupEmulatedEnvironment();

			return 1;			/* go back to emulation */
		}
	}

	if (new_videoram_unaligned)
		Mfree(new_videoram_unaligned);
	if (Screen_atari_unaligned)
		Mfree(Screen_atari_unaligned);
#ifdef BITPL_SCR
	if (Screen_atari_b_unaligned)
		Mfree(Screen_atari_b_unaligned);
#endif

	if (gl_app_id != -1) {
		/* unlock GEM */
		wind_update(END_UPDATE);
		form_dial(FMD_FINISH, 0, 0, 0, 0, 0, 0, HOST_WIDTH, HOST_HEIGHT);	/* redraw screen */
	}
	if (!reprogram_VIDEL && gl_vdi_handle != 0) {
		if (gl_app_id != -1)
			v_clsvwk(gl_vdi_handle);
		else
			v_clswk(gl_vdi_handle);
	}
	if (gl_app_id != -1) {
		graf_mouse(M_ON, NULL);
		/* GEM exit */
		appl_exit();
	}

#ifdef SCREENSAVER
	Clocky_SS(Clocky_SSval);
	if (NOVA_xcb)
		NOVA_xcb->blnk_time = NOVA_SSval;
#endif

	return 0;
}

/* -------------------------------------------------------------------------- */

#define DoubleSizeIt(in, out1, out2) \
	__asm__ (				\
		"\n\tmove.l	%2,%0"	\
		"\n\tmove.l	%0,%1"	\
		"\n\tclr.w	%0"		\
		"\n\teor.l	%0,%1"	\
							\
		"\n\tmove.l	%0,d2"	\
		"\n\tmove.l	%1,d3"	\
							\
		"\n\tswap	%1"		\
		"\n\tswap	d2"		\
							\
		"\n\tor.l	d2,%0"	\
		"\n\tor.l	d3,%1"	\
							\
		"\n\tror.w	#8,%0"	\
		"\n\tror.w	#8,%1"	\
							\
		"\n\tror.l	#8,%0"	\
		"\n\tror.l	#8,%1"	\
		: "=d" (out1), "=d" (out2) \
		: "g" (in) 			\
		: "cc", "d2", "d3"	\
	);

void PLATFORM_DisplayScreen(void)
{
	static int i = 0;
	static ULONG Screen_ui[Screen_HEIGHT*Screen_WIDTH/sizeof(ULONG)];

#ifdef SHOW_DISK_LED
	if (LED_timeout)
		if (--LED_timeout == 0)
			Atari_Set_LED(0);
#endif

	if (i < skip_N_frames) {
		i++;
		return;
	}
	i = 0;

	if (!sv) {
		/* not direct rendering */
		UBYTE *screen = (UBYTE *) Screen_atari;
		odkud = screen;
		kam = NewLogbase();
#ifdef BITPL_SCR
		oldscreen = (UBYTE *) Screen_atari_b;

		if (delta_screen) {
			if (!UI_is_active) {
				/* switch between screens to enable delta output */
				if (Screen_atari == Screen_atari1) {
					Screen_atari = Screen_atari2;
					Screen_atari_b = Screen_atari1;
				} else {
					Screen_atari = Screen_atari1;
					Screen_atari_b = Screen_atari2;
				}
			} else {
				/* when UI_is_active == true maintain Screen_atari's copy so
				 * when UI_is_active == false the difference will be noted
				 */
				memcpy (Screen_atari_b, Screen_atari, Screen_HEIGHT*Screen_WIDTH);
			}
		}
#endif

		if (bitplanes) {
#ifdef BITPL_SCR
			if (delta_screen && !UI_is_active)
				rplanes_delta();
			else
#endif
				rplanes();
		}
		else {
			UBYTE *ptr_from = screen + 24;
			UBYTE *ptr_mirror = oldscreen + 24;
			UBYTE *ptr_dest = kam + CENTER;
			int j;

			for (j = 0; j < Screen_HEIGHT; j++) {
				short cycles;
				ULONG *long_ptr_from = (ULONG*)ptr_from;
				ULONG *long_ptr_mirror = (ULONG*)ptr_mirror;
				ULONG *long_ptr_dest = (ULONG*)ptr_dest;

				if (NOVA_double_size) {
					cycles = 83;
#ifdef BITPL_SCR
					if (delta_screen && !UI_is_active) {
						do {
							ULONG data = *long_ptr_from++;
							if (data == *long_ptr_mirror++)
								long_ptr_dest+=2;
							else {
								ULONG data1, data2;
								DoubleSizeIt(data, data1, data2);
								*(long_ptr_dest + vramw / 4) = data1;
								*long_ptr_dest++ = data1;
								*(long_ptr_dest + vramw / 4) = data2;
								*long_ptr_dest++ = data2;
							}
						} while (cycles--);
					}
					else
#endif
					{
						do {
							ULONG data1, data2;
							DoubleSizeIt(*long_ptr_from++, data1, data2);

							*(long_ptr_dest + vramw / 4) = data1;
							*long_ptr_dest++ = data1;
							*(long_ptr_dest + vramw / 4) = data2;
							*long_ptr_dest++ = data2;
						} while (cycles--);
					}
					ptr_dest += vramw;
				}
				else {
					cycles = 20;
#ifdef BITPL_SCR
					if (delta_screen && !UI_is_active) {
						do {
							ULONG data;
#define	CHECK_AND_WRITE										\
							data = *long_ptr_from++;		\
							if (data == *long_ptr_mirror++)	\
								long_ptr_dest++;			\
							else							\
								*long_ptr_dest++ = data;

								CHECK_AND_WRITE
								CHECK_AND_WRITE
								CHECK_AND_WRITE
								CHECK_AND_WRITE
						} while (cycles--);
					}
					else
#endif
					{
						do {
							*long_ptr_dest++ = *long_ptr_from++;
							*long_ptr_dest++ = *long_ptr_from++;
							*long_ptr_dest++ = *long_ptr_from++;
							*long_ptr_dest++ = *long_ptr_from++;
						} while (cycles--);
					}
				}

				ptr_from += Screen_WIDTH;
				ptr_mirror += Screen_WIDTH;
				ptr_dest += vramw;
			}
		}
	} else {
		/* direct rendering */
		if (UI_is_active) {
			if (Screen_ui != Screen_atari) {
				memcpy(Screen_ui, Screen_atari, sizeof(Screen_ui));
				Screen_atari = Screen_ui;
			} else {
				memcpy(new_videobase, Screen_ui, sizeof(Screen_ui));
			}
		} else {
			if (Screen_ui == Screen_atari) {
				Screen_atari = (ULONG*)new_videobase;
				memcpy(Screen_atari, Screen_ui, sizeof(Screen_ui));
			}
		}
	}

	if (reprogram_VIDEL) {
		SwapScreen();
	}
}

#ifdef SHOW_DISK_LED
void Atari_Set_LED(int how)
{
	if (how) {
		Offgibit(~0x02);
		LED_timeout = 8;
	}
	else {
		Ongibit(0x02);
		LED_timeout = 0;
	}
}
#endif

/* -------------------------------------------------------------------------- */

int PLATFORM_Keyboard(void)
{
	UBYTE shift_key, control_key;
	int scancode, keycode;
	int i;

	trig0 = 1;
	stick0 = INPUT_STICK_CENTRE;

	shift_key = (key_buf[0x2a] || key_buf[0x36]);
	control_key = key_buf[0x1d];

	if (!shift_key && !control_key) {
		if (key_buf[0x70])
			trig0 = 0;
		if (key_buf[0x6d] || key_buf[0x6e] || key_buf[0x6f])
			stick0 -= (INPUT_STICK_CENTRE - INPUT_STICK_BACK);
		if (key_buf[0x6f] || key_buf[0x6c] || key_buf[0x69])
			stick0 -= (INPUT_STICK_CENTRE - INPUT_STICK_RIGHT);
		if (key_buf[0x6d] || key_buf[0x6a] || key_buf[0x67])
			stick0 -= (INPUT_STICK_CENTRE - INPUT_STICK_LEFT);
		if (key_buf[0x67] || key_buf[0x68] || key_buf[0x69])
			stick0 -= (INPUT_STICK_CENTRE - INPUT_STICK_FORWARD);
	}

	scancode = 0;

	if (stick0 == INPUT_STICK_CENTRE && trig0 == 1) {
		for (i = 1; i <= 0x72; i++)	{	/* search for pressed key */
			if (key_buf[i]) {
				if (i == 0x1d || i == 0x2a || i == 0x36		/* Shift, Control skip */
					|| i == 0x3c || i == 0x3d || i == 0x3e)	/* F2, F3, F4 skip */
					continue;
				scancode = i;
				break;
			}
		}
	}

#define SCANCODE_TAB	0x0f
#define SCANCODE_CONTROL	0x1d
#define SCANCODE_LSHIFT		0x2a
#define SCANCODE_RSHIFT		0x36
#define SCANCODE_ALT		0x38

#define SCANCODE_R			0x13
#define SCANCODE_Y			0x2c
#define SCANCODE_O			0x18
#define SCANCODE_A			0x1e
#define SCANCODE_S			0x1f
#define SCANCODE_D			0x20
#define SCANCODE_L			0x26
#define SCANCODE_C			0x2e
#define SCANCODE_T			0x14

	UI_alt_function = -1;		/* no alt function */
	if (key_buf[0x38]) {		/* left Alt key is pressed */
		if (scancode == SCANCODE_R)
			UI_alt_function = UI_MENU_RUN;		/* ALT+R .. Run file */
		else if (scancode == SCANCODE_Y)
			UI_alt_function = UI_MENU_SYSTEM;		/* ALT+Y .. Select system */
		else if (scancode == SCANCODE_O)
			UI_alt_function = UI_MENU_SOUND;		/* ALT+O .. mono/stereo sound */
		else if (scancode == SCANCODE_A)
			UI_alt_function = UI_MENU_ABOUT;		/* ALT+A .. About */
		else if (scancode == SCANCODE_S)
			UI_alt_function = UI_MENU_SAVESTATE;	/* ALT+S .. Save state */
		else if (scancode == SCANCODE_D)
			UI_alt_function = UI_MENU_DISK;		/* ALT+D .. Disk management */
		else if (scancode == SCANCODE_L)
			UI_alt_function = UI_MENU_LOADSTATE;	/* ALT+L .. Load state */
		else if (scancode == SCANCODE_C)
			UI_alt_function = UI_MENU_CARTRIDGE;	/* ALT+C .. Cartridge management */
		else if (scancode == SCANCODE_T)
			UI_alt_function = UI_MENU_CASSETTE;	/* ALT+T .. Tape management */
	}
	if (UI_alt_function != -1)
		return AKEY_UI;

	BINLOAD_pause_loading = FALSE;

	if (key_buf[0x3c])	/* F2 */
		INPUT_key_consol &= ~INPUT_CONSOL_OPTION;	/* OPTION key ON */
	else
		INPUT_key_consol |= INPUT_CONSOL_OPTION;	/* OPTION key OFF */
	if (key_buf[0x3d])	/* F3 */
		INPUT_key_consol &= ~INPUT_CONSOL_SELECT;	/* SELECT key ON */
	else
		INPUT_key_consol |= INPUT_CONSOL_SELECT;	/* SELECT key OFF */
	if (key_buf[0x3e])	/* F4 */
		INPUT_key_consol &= ~INPUT_CONSOL_START;	/* START key ON */
	else
		INPUT_key_consol |= INPUT_CONSOL_START;	/* START key OFF */

	if (scancode) {
		/* read ASCII code of pressed key */
		if (shift_key)
			keycode = *(UBYTE *) (key_tab->shift + scancode);
		else
			keycode = *(UBYTE *) (key_tab->unshift + scancode);

		if (control_key)
			keycode -= 64;

		switch (keycode) {
		case 0x01:
			keycode = AKEY_CTRL_a;
			break;
		case 0x02:
			keycode = AKEY_CTRL_b;
			break;
		case 0x03:
			keycode = AKEY_CTRL_c;
			break;
		case 0x04:
			keycode = AKEY_CTRL_d;
			break;
		case 0x05:
			keycode = AKEY_CTRL_e;
			break;
		case 0x06:
			keycode = AKEY_CTRL_f;
			break;
		case 0x07:
			keycode = AKEY_CTRL_g;
			break;
		case 0x08:
			if (scancode == 0x0e)
				keycode = AKEY_BACKSPACE;
			else
				keycode = AKEY_CTRL_h;
			break;
		case 0x09:
			if (scancode == 0x0f) {
				if (shift_key)
					keycode = AKEY_SETTAB;
				else if (control_key)
					keycode = AKEY_CLRTAB;
				else
					keycode = AKEY_TAB;
			}
			else
				keycode = AKEY_CTRL_i;
			break;
		case 0x0a:
			keycode = AKEY_CTRL_j;
			break;
		case 0x0b:
			keycode = AKEY_CTRL_k;
			break;
		case 0x0c:
			keycode = AKEY_CTRL_l;
			break;
		case 0x0d:
			if (scancode == 0x1c || scancode == 0x72)
				keycode = AKEY_RETURN;
			else
				keycode = AKEY_CTRL_m;
			break;
		case 0x0e:
			keycode = AKEY_CTRL_n;
			break;
		case 0x0f:
			keycode = AKEY_CTRL_o;
			break;
		case 0x10:
			keycode = AKEY_CTRL_p;
			break;
		case 0x11:
			keycode = AKEY_CTRL_q;
			break;
		case 0x12:
			keycode = AKEY_CTRL_r;
			break;
		case 0x13:
			keycode = AKEY_CTRL_s;
			break;
		case 0x14:
			keycode = AKEY_CTRL_t;
			break;
		case 0x15:
			keycode = AKEY_CTRL_u;
			break;
		case 0x16:
			keycode = AKEY_CTRL_v;
			break;
		case 0x17:
			keycode = AKEY_CTRL_w;
			break;
		case 0x18:
			keycode = AKEY_CTRL_x;
			break;
		case 0x19:
			keycode = AKEY_CTRL_y;
			break;
		case 0x1a:
			keycode = AKEY_CTRL_z;
			break;
		case ' ':
			keycode = AKEY_SPACE;
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
		case 0x1b:
			keycode = AKEY_ESCAPE;
			break;
		case 0x00:
			switch (scancode) {
			case 0x3b:			/* F1 */
			case 0x61:			/* Undo */
				keycode = AKEY_UI;
				break;
			case 0x62:			/* Help */
				keycode = AKEY_HELP;
				break;
			case 0x3f:			/* F5 */
				keycode = shift_key ? AKEY_COLDSTART : AKEY_WARMSTART;
				break;
			case 0x40:			/* F6 */
				keycode = AKEY_HELP;
				break;
			case 0x41:			/* F7 */
				if (BINLOAD_wait_active) {
					BINLOAD_pause_loading = TRUE;
					keycode = AKEY_NONE;
				}
				else
					keycode = AKEY_BREAK;
				break;
			case 0x42:			/* F8 */
				keycode = PLATFORM_Exit(1) ? AKEY_NONE : AKEY_EXIT;	/* invoke monitor */
				break;
			case 0x43:			/* F9 */
				keycode = AKEY_EXIT;
				break;
			case 0x44:			/* F10*/
				keycode = shift_key ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
				break;
			case 0x50:
				keycode = AKEY_DOWN;
				break;
			case 0x4b:
				keycode = AKEY_LEFT;
				break;
			case 0x4d:
				keycode = AKEY_RIGHT;
				break;
			case 0x48:
				keycode = AKEY_UP;
				break;
			default:
				keycode = AKEY_NONE;
				break;
			}
			break;
		default:
			keycode = AKEY_NONE;
			break;
		}
	}
	else {
		INPUT_key_shift = shift_key;
		keycode = AKEY_NONE;
	}

	return keycode;
}

/* -------------------------------------------------------------------------- */

int PLATFORM_PORT(int num)
{
	if (num == 0) {
		if (stick0 == INPUT_STICK_CENTRE && trig0 == 1)
			return (((~joy1 << 4) & 0xf0) | ((~joy0) & 0x0f));
		else {
			if (joyswap)
				return ((stick0 << 4) | ((~joy0) & 0x0f));
			else
				return (((~joy0 << 4) & 0xf0) | stick0);
		}
	}
	else
		return 0xff;
}

/* -------------------------------------------------------------------------- */

int PLATFORM_TRIG(int num)
{
	switch (num) {
	case 0:
		return (joy0 > 0x0f) ? 0 : joyswap ? 1 : trig0;
	case 1:
		return (joy1 > 0x0f) ? 0 : joyswap ? trig0 : 1;
	case 2:
	case 3:
	default:
		return 1;
	}
}

/* -------------------------------------------------------------------------- */

void PLATFORM_Sleep(double s)
{
	static const double one_tick = 1.0 / CLOCKS_PER_SEC;
	if (s >= one_tick) {
		const ULONG final_tick = (ULONG)(clock() + s*CLOCKS_PER_SEC + 0.5);
		while (clock() < final_tick);
	}
}

/* -------------------------------------------------------------------------- */

double PLATFORM_Time(void)
{
	return clock() * (1.0 / CLOCKS_PER_SEC);
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
#ifdef SOUND
	/* set default desired sound */
	Sound_desired.freq = 24585;
	Sound_desired.sample_size = 1;
	Sound_desired.channels = 2;
	Sound_desired.buffer_ms = 40;
#endif

	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return EXIT_FAILURE;

	/* main loop */
	for (;;) {
		INPUT_key_code = PLATFORM_Keyboard();
		Atari800_Frame();
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
	}
}
