/* $Id$ */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "antic.h"
#include "gtia.h"
#include "pia.h"
#include "pokey.h"
#include "cartridge.h"
#include "devices.h"
#include "sio.h"
#include "monitor.h"
#include "platform.h"
#include "prompts.h"
#include "rt-config.h"
#include "ui.h"
#include "ataripcx.h"		/* for Save_PCX() */
#include "log.h"
#include "statesav.h"
#include "diskled.h"
#include "colours.h"
#include "binload.h"
#include "rtime.h"
#include "cassette.h"
#ifdef SOUND
#include "sound.h"
#endif

#ifdef CRASH_MENU
extern int crash_code;
extern UWORD crash_address;
extern UWORD crash_afterCIM;
#endif

ULONG *atari_screen = NULL;
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif

int tv_mode = TV_PAL;
Machine machine = Atari;
int mach_xlxe = FALSE;
int Ram256 = 0;
int verbose = FALSE;
double fps;
int nframes;
static double frametime = 0.1;	/* measure time between two Antic runs */
static int emu_too_fast = 0;

int pil_on = FALSE;

int	os = 2;

double deltatime;
int draw_display=1;		/* Draw actualy generated screen */

void Atari800_Hardware(void);

static char *rom_filename = NULL;

void sigint_handler(int num)
{
	int restart;

	restart = Atari800_Exit(TRUE);
	if (restart) {
		signal(SIGINT, sigint_handler);
		return;
	}

	exit(0);
}

/* static int Reset_Disabled = FALSE; - why is this defined here? (PS) */

void Warmstart(void)
{
	PORTA = 0x00;
/* After reset must by actived rom os */
	if (mach_xlxe) {
		PIA_PutByte(_PORTB, 0xff);	/* turn on operating system */
	}
	else
		PORTB = 0xff;

	ANTIC_Reset();
	CPU_Reset();
}

void Coldstart(void)
{
	PORTA = 0x00;
	if (mach_xlxe) {
#if 0	/* to make "-cart" working */
		selftest_enabled = TRUE;	/* necessary for init RAM */
		PORTB = 0x00;			/* necessary for init RAM */
#endif
		PIA_PutByte(_PORTB, 0xff);	/* turn on operating system */
	}
	else
		PORTB = 0xff;
	CART_Start();
	Poke(0x244, 1);
	ANTIC_Reset();
	CPU_Reset();

	consol_index = 2;
	consol_table[2] = 0x0f;
	if (disable_basic)
		consol_table[2] &= ~4;	/* hold Option during reboot */
	if (hold_start)
		consol_table[2] &= ~1;	/* hold Start during reboot */
	consol_table[1] = consol_table[2];
}

int Initialise_AtariXE(void)
{
	int status;
	mach_xlxe = TRUE;
	status = Initialise_AtariXL();
	machine = AtariXE;
	xe_bank = 0;
	return status;
}

int Initialise_Atari320XE(void)
{
	int status;
	mach_xlxe = TRUE;
	status = Initialise_AtariXE();
	machine = Atari320XE;
	return status;
}

#ifdef linux
#ifdef REALTIME
#include <sched.h>
#endif /* REALTIME */
#endif /* linux */

int main(int argc, char **argv)
{
	int status;
	int error = FALSE;
	int diskno = 1;
	int i, j;
	char *run_direct=NULL;
#ifdef linux
#ifdef REALTIME
	struct sched_param sp;
#endif /* REALTIME */
#endif /* linux */

	char *rtconfig_filename = NULL;
	int config = FALSE;

#ifdef linux
#ifdef REALTIME
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	sched_setscheduler(getpid(),SCHED_RR, &sp);
#endif /* REALTIME */
#endif /* linux */

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-configure") == 0)
			config = TRUE;
		else if (strcmp(argv[i], "-config") == 0)
			rtconfig_filename = argv[++i];
		else if (strcmp(argv[i], "-v") == 0) {
			Aprint("%s", ATARI_TITLE);
			return 0;
		}
		else if (strcmp(argv[i], "-verbose") == 0)
			verbose = TRUE;
		else
			argv[j++] = argv[i];
	}

	argc = j;

	if (!RtConfigLoad(rtconfig_filename))
		config = TRUE;

	if (config) {

#ifndef DONT_USE_RTCONFIGUPDATE
		RtConfigUpdate();
#endif /* !DONT_USE_RTCONFIGUPDATE */

		RtConfigSave();
	}

	switch (default_system) {
	case 1:
		machine = Atari;
		os = 1;
		break;
	case 2:
		machine = Atari;
		os = 2;
		break;
	case 3:
		machine = AtariXL;
		break;
	case 4:
		machine = AtariXE;
		break;
	case 5:
		machine = Atari320XE;
		break;
	case 6:
		machine = Atari5200;
		break;
	default:
		machine = AtariXL;
		break;
	}

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-atari") == 0)
			machine = Atari;
		else if (strcmp(argv[i], "-xl") == 0)
			machine = AtariXL;
		else if (strcmp(argv[i], "-xe") == 0)
			machine = AtariXE;
		else if (strcmp(argv[i], "-320xe") == 0) {
			machine = Atari320XE;
			if (!Ram256)
				Ram256 = 2;	/* default COMPY SHOP */
		}
		else if (strcmp(argv[i], "-rambo") == 0) {
			machine = Atari320XE;
			Ram256 = 1;
		}
		else if (strcmp(argv[i], "-5200") == 0)
			machine = Atari5200;
		else if (strcmp(argv[i], "-nobasic") == 0)
			disable_basic = TRUE;
		else if (strcmp(argv[i], "-basic") == 0)
			disable_basic = FALSE;
		else if (strcmp(argv[i], "-nopatch") == 0)
			enable_sio_patch = FALSE;
		else if (strcmp(argv[i], "-nopatchall") == 0)
			enable_sio_patch = enable_h_patch = enable_p_patch = FALSE;
		else if (strcmp(argv[i], "-pal") == 0)
			tv_mode = TV_PAL;
		else if (strcmp(argv[i], "-ntsc") == 0)
			tv_mode = TV_NTSC;
		else if (strcmp(argv[i], "-osa_rom") == 0)
			strcpy(atari_osa_filename, argv[++i]);
		else if (strcmp(argv[i], "-osb_rom") == 0)
			strcpy(atari_osb_filename, argv[++i]);
		else if (strcmp(argv[i], "-xlxe_rom") == 0)
			strcpy(atari_xlxe_filename, argv[++i]);
		else if (strcmp(argv[i], "-5200_rom") == 0)
			strcpy(atari_5200_filename, argv[++i]);
		else if (strcmp(argv[i], "-basic_rom") == 0)
			strcpy(atari_basic_filename, argv[++i]);
		else if (strcmp(argv[i], "-cart") == 0) {
			rom_filename = argv[++i];
		}
		else if (strcmp(argv[i], "-run") == 0) {
			run_direct = argv[++i];
		}
		else if (strcmp(argv[i], "-refresh") == 0) {
			sscanf(argv[++i], "%d", &refresh_rate);
			if (refresh_rate < 1)
				refresh_rate = 1;
		}
		else if (strcmp(argv[i], "-palette") == 0)
			read_palette(argv[++i]);
		else if (strcmp(argv[i], "-help") == 0) {
			Aprint("\t-configure    Update Configuration File");
			Aprint("\t-config fnm   Specify Alternate Configuration File");
			Aprint("\t-atari        Standard Atari 800 mode");
			Aprint("\t-xl           Atari 800XL mode");
			Aprint("\t-xe           Atari 130XE mode");
			Aprint("\t-320xe        Atari 320XE mode (COMPY SHOP)");
			Aprint("\t-rambo        Atari 320XE mode (RAMBO)");
			Aprint("\t-nobasic      Turn off Atari BASIC ROM");
			Aprint("\t-basic        Turn on Atari BASIC ROM");
			Aprint("\t-5200         Atari 5200 Games System");
			Aprint("\t-pal          Enable PAL TV mode");
			Aprint("\t-ntsc         Enable NTSC TV mode");
			Aprint("\t-cart fnm     Install cartridge (raw or CART format)");
			Aprint("\t-run fnm      Run file directly");
			Aprint("\t-refresh num  Specify screen refresh rate");
			Aprint("\t-nopatch      Don't patch SIO routine in OS");
			Aprint("\t-nopatchall   Don't patch OS at all, H: device won't work");
			Aprint("\t-palette fnm  Use external palette");
			Aprint("\t-a            Use A OS");
			Aprint("\t-b            Use B OS");
			Aprint("\t-c            Enable RAM between 0xc000 and 0xd000");
			Aprint("\t-v            Show version/release number");
			argv[j++] = argv[i];
			Aprint("\nPress Return/Enter to continue...");
			getchar();
			Aprint("\r                                 \n");
		}
		else if (strcmp(argv[i], "-a") == 0)
			os = 1;
		else if (strcmp(argv[i], "-b") == 0)
			os = 2;
		else if (strcmp(argv[i], "-emuos") == 0) {
			machine = Atari;
			os = 4;
		}
		else if (strcmp(argv[i], "-c") == 0)
			enable_c000_ram = TRUE;
		else
			argv[j++] = argv[i];
	}

	argc = j;

	if (tv_mode == TV_PAL)
	{
		deltatime = (1.0 / 50.0);
	}
	else
	{
		deltatime = (1.0 / 60.0);
	}

	Palette_Initialise(&argc, argv);
	Device_Initialise(&argc, argv);
	RTIME_Initialise(&argc, argv);
	SIO_Initialise (&argc, argv);
	CASSETTE_Initialise(&argc, argv);
	Atari_Initialise(&argc, argv);	/* Platform Specific Initialisation */

	if (!atari_screen) {
		atari_screen = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
#ifdef BITPL_SCR
		atari_screen_b = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
		atari_screen1 = atari_screen;
		atari_screen2 = atari_screen_b;
#endif
	}
	/*
	 * Initialise basic 64K memory to zero.
	 */

	ClearRAM();

	/*
	 * Initialise Custom Chips
	 */

	ANTIC_Initialise(&argc, argv);
	GTIA_Initialise(&argc, argv);
	PIA_Initialise(&argc, argv);
	POKEY_Initialise(&argc, argv);

	/*
	 * Any parameters left on the command line must be disk images.
	 */

	for (i = 1; i < argc; i++) {
		if (!SIO_Mount(diskno++, argv[i], FALSE)) {
			Aprint("Disk File %s not found", argv[i]);
			error = TRUE;
		}
	}

	if (error) {
		Aprint("Usage: %s [options] [diskfile1...diskfile8]", argv[0]);
		Aprint("\t-help         Extended Help");
		Atari800_Exit(FALSE);
		return FALSE;
	}
	/*
	 * Install CTRL-C Handler
	 */
	signal(SIGINT, sigint_handler);
	/*
	 * Configure Atari System
	 */

	switch (machine) {
	case Atari:
		Ram256 = 0;
		if (os == 1)
			status = Initialise_AtariOSA();
		else if (os == 2)
			status = Initialise_AtariOSB();
		else
			status = Initialise_EmuOS();
		break;
	case AtariXL:
		Ram256 = 0;
		status = Initialise_AtariXL();
		break;
	case AtariXE:
		Ram256 = 0;
		status = Initialise_AtariXE();
		break;
	case Atari320XE:
		/* Ram256 is even now set */
		status = Initialise_Atari320XE();
		break;
	case Atari5200:
		Ram256 = 0;
		status = Initialise_Atari5200();
		break;
	default:
		Aprint("Fatal Error in atari.c");
		Atari800_Exit(FALSE);
		return FALSE;
	}

	if (!status) {
		Aprint("Operating System not available - using Emulated OS");
		status = Initialise_EmuOS();
	}

/*
 * ================================
 * Install requested ROM cartridges
 * ================================
 */
	if (rom_filename) {
		int r = CART_Insert(rom_filename);
		if (r < 0) {
			Aprint("Error inserting cartridge %s: %s", rom_filename,
			r == CART_CANT_OPEN ? "Can't open file" :
			r == CART_BAD_FORMAT ? "Bad format" :
			r == CART_BAD_CHECKSUM ? "Bad checksum" :
			"Unknown error");
		}
		if (r > 0) {
			ui_is_active = TRUE;
			cart_type = SelectCartType((UBYTE *) atari_screen, r);
			ui_is_active = FALSE;
			CART_Start();
		}
		if (cart_type != CART_NONE) {
			int for5200 = CART_IsFor5200(cart_type);
			if (for5200 && machine != Atari5200) {
				Ram256 = 0;
				Initialise_Atari5200();
			}
			else if (!for5200 && machine == Atari5200) {
				Ram256 = 0;
				Initialise_AtariXL();
			}
		}
	}
/*
 * ======================================
 * Reset CPU and start hardware emulation
 * ======================================
 */

	if (run_direct != NULL)
		BIN_loader(run_direct);

	Atari800_Hardware();
	Aprint("Fatal error: Atari800_Hardware() returned");
	Atari800_Exit(FALSE);
	return 0;
}

int Atari800_Exit(int run_monitor)
{
	int restart;
	if (verbose) {
		Aprint("Current Frames per Secound = %f", fps);
	}
	restart = Atari_Exit(run_monitor);
	if (!restart)
		SIO_Exit();	/* umount disks, so temporary files are deleted */
	return restart;
}

UBYTE Atari800_GetByte(UWORD addr)
{
	UBYTE byte = 0xff;
	switch (addr & 0xff00) {
	case 0x4f00:
		CART_BountyBob1(addr);
		byte = 0;
		break;
	case 0x5f00:
		CART_BountyBob2(addr);
		byte = 0;
		break;
	case 0xd000:				/* GTIA */
	case 0xc000:				/* GTIA - 5200 */
		byte = GTIA_GetByte(addr);
		break;
	case 0xd200:				/* POKEY */
	case 0xe800:				/* POKEY - 5200 */
	case 0xeb00:				/* POKEY - 5200 */
		byte = POKEY_GetByte(addr);
		break;
	case 0xd300:				/* PIA */
		byte = PIA_GetByte(addr);
		break;
	case 0xd400:				/* ANTIC */
		byte = ANTIC_GetByte(addr);
		break;
	case 0xd500:				/* RTIME-8 */
		byte = CART_GetByte(addr);
		break;
	default:
		break;
	}

	return byte;
}

void Atari800_PutByte(UWORD addr, UBYTE byte)
{
	switch (addr & 0xff00) {
	case 0x4f00:
		CART_BountyBob1(addr);
		break;
	case 0x5f00:
		CART_BountyBob2(addr);
		break;
	case 0xd000:				/* GTIA */
	case 0xc000:				/* GTIA - 5200 */
		GTIA_PutByte(addr, byte);
		break;
	case 0xd200:				/* POKEY */
	case 0xe800:				/* POKEY - 5200 AAA added other pokey space */
	case 0xeb00:				/* POKEY - 5200 */
		POKEY_PutByte(addr, byte);
		break;
	case 0xd300:				/* PIA */
		PIA_PutByte(addr, byte);
		break;
	case 0xd400:				/* ANTIC */
		ANTIC_PutByte(addr, byte);
		break;
	case 0xd500:				/* Super Cartridges */
		CART_PutByte(addr, byte);
		break;
	default:
		break;
	}
}

void Atari800_UpdatePatches(void)
{
	if (machine == Atari) {
		/* Restore unpatched OS */
		memcpy(memory + 0xd800, atari_os, 0x2800);
		/* Set patches */
		PatchOS();
	}
	else if (mach_xlxe && PORTB & 1) {
		/* Restore unpatched OS */
		memcpy(memory + 0xc000, atari_os, 0x1000);
		memcpy(memory + 0xd800, atari_os + 0x1800, 0x2800);
		/* Set patches */
		PatchOS();
	}
}

#ifdef SNAILMETER
static void ShowRealSpeed(ULONG * atari_screen)
{
  UBYTE *ptr;
  int i;
  int speed = (100.0 * deltatime / frametime + 0.5);

  if (speed > 200)
    speed = 200;

  ptr = (UBYTE *) atari_screen + 32 + ATARI_WIDTH * LED_lastline;

  for (i = 0; i < speed; i++)
    ptr[i] = 0xc8;
  for (; i < 100; i++)
    ptr[i] = 0x02;
  ptr[100] = 0x38;
}
#endif

static double Atari_time(void)
{
#ifdef WIN32
  return GetTickCount() * 1e-3;
#elif defined(DJGPP)
  return uclock() * (1.0 / UCLOCKS_PER_SEC);
#else
  struct timeval tp;

  gettimeofday(&tp, NULL);
  return tp.tv_sec + 1e-6 * tp.tv_usec;
#endif
}

static void Atari_sleep(double s)
{
  emu_too_fast = 0;
  if (s > 0)
  {
#ifdef linux
    struct timeval tp;

    tp.tv_sec = 0;
    tp.tv_usec = 1e6 * s;
    select(1,NULL,NULL,NULL,&tp);
#elif defined(WIN32)
    Sleep(s * 1e3);
#elif defined(DJGPP)
    double curtime = Atari_time();
    while ((curtime + s) > Atari_time());
#else
    usleep(s * 1e6);
#endif
    emu_too_fast = 1;
  }
}

void atari_sync(void)
{
#ifdef USE_CLOCK
	static ULONG nextclock = 0;	/* put here a non-zero value to enable speed regulator */
	/* on Atari Falcon CLK_TCK = 200 (i.e. 5 ms granularity) */
	/* on DOS (DJGPP) CLK_TCK = 91 (not too precise, but should work anyway)*/
	if (nextclock) {
		ULONG curclock;
		do {
			curclock = clock();
		} while (curclock < nextclock);

		nextclock = curclock + (CLK_TCK / (tv_mode == TV_PAL ? 50 : 60));
	}
#else /* USE_CLOCK */
	static double lasttime = 0, lastcurtime = 0;
	double curtime;

	if (deltatime > 0.0)
	{
	  curtime = Atari_time();
	  Atari_sleep(lasttime + deltatime - curtime);
	  curtime = Atari_time();

	  /* make average time */
	  frametime = (frametime * 4.0 + curtime - lastcurtime) * 0.2;
	  fps = 1.0 / frametime;
	  lastcurtime = curtime;

	  lasttime += deltatime;
	  if ((lasttime + deltatime) < curtime)
	    lasttime = curtime;
	}
#endif /* USE_CLOCK */
}

void Atari800_Hardware(void)
{
	nframes = 0;

	while (TRUE) {
#ifndef BASIC
		static int test_val = 0;
		static int last_key = -1;	/* no key is pressed */
		int keycode;

		draw_display=1;

		keycode = Atari_Keyboard();

		switch (keycode) {
		case AKEY_COLDSTART:
			Coldstart();
			break;
		case AKEY_WARMSTART:
			Warmstart();
			break;
		case AKEY_EXIT:
			Atari800_Exit(FALSE);
			exit(1);
		case AKEY_BREAK:
			IRQST &= ~0x80;
			if (IRQEN & 0x80) {
				GenerateIRQ();
			}
			break;
		case AKEY_UI:
#ifdef SOUND
			Sound_Pause();
#endif
			ui((UBYTE *)atari_screen);
#ifdef SOUND
			Sound_Continue();
#endif
			break;
		case AKEY_PIL:
			if (pil_on)
				DisablePILL();
			else
				EnablePILL();
			break;
		case AKEY_SCREENSHOT:
			Save_PCX((UBYTE *)atari_screen);
			break;
		case AKEY_SCREENSHOT_INTERLACE:
			Save_PCX_interlaced();
			break;
		case AKEY_NONE:
			if (press_space) {
				keycode = AKEY_SPACE;
				press_space = 0;
			}
			else {
				last_key = -1;
				break;
			}
		default:
			if (keycode == last_key)
				break;	
			last_key = keycode;
			KBCODE = keycode;
			IRQST &= ~0x40;
			if (IRQEN & 0x40) {
				GenerateIRQ();
			}
			break;
		}
#endif	/* !BASIC */

#ifdef SOUND
		Sound_Update();
#endif

		/*
		 * Generate Screen
		 */

#ifndef BASIC
#ifndef SVGA_SPEEDUP
		if (++test_val == refresh_rate) {
#endif
			ANTIC_RunDisplayList();			/* generate screen */
			Update_LED();
#ifdef SNAILMETER
			if (!emu_too_fast)
			  ShowRealSpeed(atari_screen);
#endif
#ifndef DONT_SYNC_WITH_HOST
			atari_sync(); /* here seems to be the best place to sync */
#endif
			Atari_DisplayScreen((UBYTE *) atari_screen);
#ifndef SVGA_SPEEDUP
			test_val = 0;
		}
		else {
#ifdef VERY_SLOW
			for (ypos = 0; ypos < max_ypos; ypos++) {
				GO(LINE_C);
				xpos -= LINE_C - DMAR;
			}
#else	/* VERY_SLOW */
			draw_display=0;
			ANTIC_RunDisplayList();
#ifndef DONT_SYNC_WITH_HOST
			atari_sync();
#endif
			Atari_DisplayScreen((UBYTE *) atari_screen);
#endif	/* VERY_SLOW */
		}
#endif	/* !SVGA_SPEEDUP */
#else	/* !BASIC */
		for (ypos = 0; ypos < max_ypos; ypos++) {
			GO(LINE_C);
			xpos -= LINE_C - DMAR;
		}
#ifndef	DONT_SYNC_WITH_HOST
		atari_sync();
#endif
#endif	/* !BASIC */

		nframes++;
	}
}

int zlib_capable(void)
{
#ifdef HAVE_LIBZ
	return TRUE;
#else
	return FALSE;
#endif
}

int prepend_tmpfile_path(char *buffer)
{
	if (buffer)
		*buffer = 0;
	return 0;
}

int ReadDisabledROMs(void)
{
	return FALSE;
}

void MainStateSave( void )
{
	UBYTE	temp;
	int default_tv_mode;	/* for compatibility with previous versions */

	/* Possibly some compilers would handle an enumerated type differently,
	   so convert these into unsigned bytes and save them out that way */
	if( tv_mode == TV_PAL ) {
		temp = 0;
		default_tv_mode = 1;
	}
	else {
		temp = 1;
		default_tv_mode = 2;
	}
	SaveUBYTE( &temp, 1 );

	if( machine == Atari )
		temp = 0;
	if( machine == AtariXL )
		temp = 1;
	if( machine == AtariXE )
		temp = 2;
	if( machine == Atari320XE )
		temp = 3;
	if( machine == Atari5200 )
		temp = 4;
	SaveUBYTE( &temp, 1 );

	SaveINT( &os, 1 );
	SaveINT( &pil_on, 1 );
	SaveINT( &default_tv_mode, 1 );
	SaveINT( &default_system, 1 );
}

void MainStateRead( void )
{
	UBYTE	temp;
	int default_tv_mode;	/* for compatibility with previous versions */

	ReadUBYTE( &temp, 1 );
	if( temp == 0 )
		tv_mode = TV_PAL;
	else
		tv_mode = TV_NTSC;

	mach_xlxe = FALSE;
	ReadUBYTE( &temp, 1 );
	switch( temp )
	{
		case	0:
			machine = Atari;
			break;

		case	1:
			machine = AtariXL;
			mach_xlxe = TRUE;
			break;

		case	2:
			machine = AtariXE;
			mach_xlxe = TRUE;
			break;

		case	3:
			machine = Atari320XE;
			mach_xlxe = TRUE;
			break;

		case	4:
			machine = Atari5200;
			break;

		default:
			machine = AtariXL;
			Aprint( "Warning: Bad machine type read in from state save, defaulting to XL" );
			break;
	}

	ReadINT( &os, 1 );
	ReadINT( &pil_on, 1 );
	ReadINT( &default_tv_mode, 1 );
	ReadINT( &default_system, 1 );
}

/*
$Log$
Revision 1.16  2001/09/16 11:22:56  fox
removed default_tv_mode

Revision 1.15  2001/09/09 08:34:13  fox
hold_option -> disable_basic

Revision 1.14  2001/08/16 23:24:25  fox
selecting cartridge type didn't worked in 5200 mode

Revision 1.13  2001/08/06 13:11:19  fox
hold_start support

Revision 1.12  2001/08/03 12:48:55  fox
cassette support

Revision 1.11  2001/07/25 12:58:25  fox
added SIO_Exit(), slight clean up

Revision 1.10  2001/07/20 20:14:14  fox
support for new rtime and cartridge modules

Revision 1.8  2001/04/15 09:14:33  knik
zlib_capable -> have_libz (autoconf compatibility)

Revision 1.7  2001/04/08 05:57:12  knik
sound calls update

Revision 1.6  2001/04/03 05:43:36  knik
reorganized sync code; new snailmeter

Revision 1.5  2001/03/18 06:34:58  knik
WIN32 conditionals removed

Revision 1.4  2001/03/18 06:24:04  knik
unused variable removed

*/
