#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <time.h>
#include "winatari.h"
#else
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#endif

#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#endif

#ifdef DJGPP
int timesync = 1;
#endif

#define FALSE   0
#define TRUE    1

#ifdef AT_USE_ALLEGRO
#include <allegro.h>
static int i_love_bill = TRUE;	/* Perry, why this? */
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "antic.h"
#include "gtia.h"
#include "pia.h"
#include "pokey.h"
#include "supercart.h"
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
#ifdef SOUND
#include "sound.h"
#endif

#ifdef CRASH_MENU
extern int crash_code;
extern UWORD crash_address;
extern UWORD crash_afterCIM;
#endif

void Sound_Pause(void);	/* cross-platform sound.h would be better :) */
void Sound_Continue(void);

ULONG *atari_screen = NULL;
#ifdef BITPL_SCR
ULONG *atari_screen_b = NULL;
ULONG *atari_screen1 = NULL;
ULONG *atari_screen2 = NULL;
#endif

int tv_mode = TV_PAL;
Machine machine = Atari;
int mach_xlxe = FALSE;
int verbose = FALSE;
double fps;
int nframes;
ULONG lastspeed;				/* measure time between two Antic runs */

#ifdef WIN32		/* AUGH clean this crap up REL */
#include "ddraw.h"
#include "registry.h"
extern unsigned char *screenbuff;
extern void (*Atari_PlaySound)( void );
extern HWND	hWnd;
extern void GetJoystickInput( void );
extern void Start_Atari_Timer( void );
extern void Screen_Paused( UBYTE *screen );
extern unsigned long ulMiscStates;
extern void CheckBootFailure( void );
int			ulAtariState = ATARI_UNINITIALIZED | ATARI_PAUSED;
int		test_val;
int		FindCartType( char *rom_filename );
char	current_rom[ _MAX_PATH ];
int pil_on = FALSE;
#else	/* not Win32 */
int pil_on = FALSE;
#endif

int	os = 2;

extern UBYTE PORTA;
extern UBYTE PORTB;

extern int xe_bank;
extern int selftest_enabled;
extern int Ram256;
extern int rom_inserted;

UBYTE *cart_image = NULL;		/* For cartridge memory */
int cart_type = NO_CART;

double deltatime;
int draw_display=1;		/* Draw actualy generated screen */

int Atari800_Exit(int run_monitor);
void Atari800_Hardware(void);

int load_cart(char *filename, int type);

static char *rom_filename = NULL;

void sigint_handler(int num)
{
	int restart;
	int diskno;

	restart = Atari800_Exit(TRUE);
	if (restart) {
		signal(SIGINT, sigint_handler);
		return;
	}
	for (diskno = 1; diskno < 8; diskno++)
		SIO_Dismount(diskno);

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
		selftest_enabled = TRUE;	/* necessary for init RAM */
		PORTB = 0x00;			/* necessary for init RAM */
		PIA_PutByte(_PORTB, 0xff);	/* turn on operating system */
	}
	else
		PORTB = 0xff;
	Poke(0x244, 1);
	ANTIC_Reset();
	CPU_Reset();

	if (hold_option)
		next_console_value = 0x03;	/* Hold Option During Reboot */
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

#ifdef WIN32
	/* The Win32 version doesn't use configuration files, it reads values in from the Registry */
	int update_registry = FALSE;
	Ram256 = 0;

	if( argc > 1 )
		update_registry = TRUE;
	ulAtariState &= ~ATARI_UNINITIALIZED;
	ulAtariState |= ATARI_INITIALIZING | ATARI_PAUSED;
	rom_inserted = FALSE;
	Aprint("Start log");
	test_val = 0;
#else
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

#endif /* !Win32 */

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

	switch (default_tv_mode) {
	case 1:
		tv_mode = TV_PAL;
		break;
	case 2:
		tv_mode = TV_NTSC;
		break;
	default:
		tv_mode = TV_PAL;
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
			hold_option = TRUE;
		else if (strcmp(argv[i], "-basic") == 0)
			hold_option = FALSE;
		else if (strcmp(argv[i], "-nopatch") == 0)
			enable_sio_patch = FALSE;
		else if (strcmp(argv[i], "-nopatchall") == 0)
			enable_rom_patches = enable_sio_patch = FALSE;
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
			cart_type = CARTRIDGE;
		}
		else if (strcmp(argv[i], "-rom") == 0) {
			rom_filename = argv[++i];
			cart_type = NORMAL8_CART;
		}
		else if (strcmp(argv[i], "-rom16") == 0) {
			rom_filename = argv[++i];
			cart_type = NORMAL16_CART;
		}
		else if (strcmp(argv[i], "-ags32") == 0) {
			rom_filename = argv[++i];
			cart_type = AGS32_CART;
		}
		else if (strcmp(argv[i], "-oss") == 0) {
			rom_filename = argv[++i];
			cart_type = OSS_SUPERCART;
		}
		else if (strcmp(argv[i], "-db") == 0) {		/* db 16/32 superduper cart */
			rom_filename = argv[++i];
			cart_type = DB_SUPERCART;
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
#ifndef WIN32
			Aprint("\t-configure    Update Configuration File");
			Aprint("\t-config fnm   Specify Alternate Configuration File");
#endif
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
			Aprint("\t-rom fnm      Install standard 8K Cartridge");
			Aprint("\t-rom16 fnm    Install standard 16K Cartridge");
			Aprint("\t-oss fnm      Install OSS Super Cartridge");
			Aprint("\t-db fnm       Install DB's 16/32K Cartridge (not for normal use)");
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
#ifndef WIN32
			Aprint("\nPress Return/Enter to continue...");
			getchar();
			Aprint("\r                                 \n");
#endif
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
		default_tv_mode = 1;
	}
	else
	{
		deltatime = (1.0 / 60.0);
		default_tv_mode = 2;
	}

	Palette_Initialise(&argc, argv);
	Device_Initialise(&argc, argv);
	SIO_Initialise (&argc, argv);
	Atari_Initialise(&argc, argv);	/* Platform Specific Initialisation */

	if(hold_option)
		next_console_value = 0x03; /* Hold Option During Reboot */


	if (!atari_screen) {
#ifdef WIN32
		atari_screen = (ULONG *)screenbuff;
#else
		atari_screen = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
#ifdef BITPL_SCR
		atari_screen_b = (ULONG *) malloc(ATARI_HEIGHT * ATARI_WIDTH);
		atari_screen1 = atari_screen;
		atari_screen2 = atari_screen_b;
#endif
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
		Aprint("Usage: %s [-rom filename] [-oss filename] [diskfile1...diskfile8]", argv[0]);
		Aprint("\t-help         Extended Help");
		Atari800_Exit(FALSE);
		return FALSE;
	}
	/*
	 * Install CTRL-C Handler
	 */
#ifndef WIN32
	signal(SIGINT, sigint_handler);
#endif
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
#ifdef WIN32
		if( !Ram256 )
		{
		if( ulMiscStates & ATARI_RAMBO_MODE )
			Ram256 = 1;
		else
			Ram256 = 2;
		}
#endif
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

#ifdef WIN32
	if (!status || (ulAtariState & ATARI_UNINITIALIZED) )
    {
		Aprint("Failed loading specified Atari OS ROM (or basic), check filename\nunder the Atari/Hardware and Atari/Cartridges menu.");
		ulAtariState |= ATARI_LOAD_FAILED | ATARI_PAUSED | ATARI_LOAD_WARNING;

		CheckBootFailure( );
		
		if( update_registry )
			WriteAtari800Registry( NULL );
		return FALSE;
	}
#else
	if (!status) {
		Aprint("Operating System not available - using Emulated OS");
		status = Initialise_EmuOS();
	}
#endif /* WIN32 */

/*
 * ================================
 * Install requested ROM cartridges
 * ================================
 */
#ifdef WIN32
	if( current_rom[0] )
	{
		rom_filename = current_rom;
		if( !strcmp( current_rom, "None" ) )
			rom_filename = NULL;
		if( !strcmp( atari_basic_filename, current_rom ) )
		{
			if( hold_option )
				rom_filename = NULL;
			else
				cart_type = NORMAL8_CART;
		}
	}
#endif /* WIN32 */
	if (rom_filename) {
#ifdef WIN32
	  if( !cart_type || cart_type == NO_CART )
	  {
		  cart_type = FindCartType( rom_filename );
	  }
#endif /* WIN32 */
		switch (cart_type) {
		case CARTRIDGE:
			status = Insert_Cartridge(rom_filename);
			break;
		case OSS_SUPERCART:
			status = Insert_OSS_ROM(rom_filename);
			break;
		case DB_SUPERCART:
			status = Insert_DB_ROM(rom_filename);
			break;
		case NORMAL8_CART:
			status = Insert_8K_ROM(rom_filename);
			break;
		case NORMAL16_CART:
			status = Insert_16K_ROM(rom_filename);
			break;
		case AGS32_CART:
			status = Insert_32K_5200ROM(rom_filename);
			break;
		}

		if (status) {
			rom_inserted = TRUE;
		}
		else {
			rom_inserted = FALSE;
		}
	}
	else {
		rom_inserted = FALSE;
	}
/*
 * ======================================
 * Reset CPU and start hardware emulation
 * ======================================
 */

	if (run_direct != NULL)
		BIN_loader(run_direct);

#ifdef WIN32
	if( update_registry )
		WriteAtari800Registry( NULL );
	Start_Atari_Timer();
	ulAtariState = ATARI_HW_READY | ATARI_PAUSED;
#else
	Atari800_Hardware();
	Aprint("Fatal error: Atari800_Hardware() returned");
	Atari800_Exit(FALSE);
#endif /* WIN32 */
	return 0;
}

#ifdef WIN32
int FindCartType( char *rom_filename )
{
	int		file;
	int		length = 0, result = -1;

	file =  _open( rom_filename, _O_RDONLY | _O_BINARY, 0 );
	if( file == -1 )
		return NO_CART;

	length = _filelength( file );
	_close( file );

	cart_type = NO_CART;
	if( length < 8193 )
		cart_type = NORMAL8_CART;
	else if( length < 16385 )
		cart_type = NORMAL16_CART;
	else if( length < 32769 )
		cart_type = AGS32_CART;

	return cart_type;
}
#endif /* WIN32 */

int Atari800_Exit(int run_monitor)
{
	if (verbose) {
		Aprint("Current Frames per Secound = %f", fps);
	}
	return Atari_Exit(run_monitor);
}

UBYTE Atari800_GetByte(UWORD addr)
{
	UBYTE byte = 0xff;
	switch (addr & 0xff00) {
	case 0x4f00:
		bounty_bob1(addr);
		byte = 0;
		break;
	case 0x5f00:
		bounty_bob2(addr);
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
		byte = SuperCart_GetByte(addr);
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
		bounty_bob1(addr);
		break;
	case 0x5f00:
		bounty_bob2(addr);
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
		SuperCart_PutByte(addr, byte);
		break;
	default:
		break;
	}
}

#ifdef SNAILMETER
void ShowRealSpeed(ULONG * atari_screen, int refresh_rate)
{
	UWORD *ptr, *ptr2;
	int i = (clock() - lastspeed) * (tv_mode == TV_PAL ? 50 : 60) / CLK_TCK / refresh_rate;
	lastspeed = clock();

	if (i > ATARI_WIDTH / 4)
		return;

	ptr = (UWORD *) atari_screen;
	ptr += (ATARI_WIDTH / 2) * (ATARI_HEIGHT - 2) + ATARI_WIDTH / 4;	/* begin in middle of line */
	ptr2 = ptr + ATARI_WIDTH / 2;

	while (--i > 0) {			/* number of blocks = times slower */
		int j = i << 1;
		ptr[j] = ptr2[j] = 0x0707;
		j++;
		ptr[j] = ptr2[j] = 0;
	}
}
#endif

void atari_sleep_ms(ULONG miliseconds)
{
#ifdef POSIX
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = miliseconds * 1E6UL;
	nanosleep(&t, NULL);	/* POSIX  */
#else
	usleep(miliseconds * 1000);
#endif
}

void Atari800_Hardware(void)
{
#ifndef WIN32
	static struct timeval tp;
	static struct timezone tzp;
	static double lasttime;
#ifdef USE_CLOCK
	static ULONG nextclock = 0;	/* put here a non-zero value to enable speed regulator */
#endif
#ifdef SNAILMETER
	static long emu_too_fast = 0;	/* automatically enable/disable snailmeter */
#endif

	nframes = 0;
	gettimeofday(&tp, &tzp);
	lasttime = tp.tv_sec + (tp.tv_usec / 1000000.0);
	fps = 0.0;

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
			/* unmount disk drives so the temporary ones are deleted */
			{
				int diskno;
				for (diskno = 1; diskno < 8; diskno++)
					SIO_Dismount(diskno);
			}
			exit(1);
		case AKEY_BREAK:
			IRQST &= ~0x80;
			if (IRQEN & 0x80) {
				GenerateIRQ();
			}
			break;
		case AKEY_UI:
			Sound_Pause();
			ui((UBYTE *)atari_screen);
			Sound_Continue();
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
			last_key = -1;
			break;
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
			if (emu_too_fast == 0)
				ShowRealSpeed(atari_screen, refresh_rate);
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
			Atari_DisplayScreen((UBYTE *) atari_screen);
#endif	/* VERY_SLOW */
		}
#endif	/* !SVGA_SPEEDUP */
#else	/* !BASIC */
		for (ypos = 0; ypos < max_ypos; ypos++) {
			GO(LINE_C);
			xpos -= LINE_C - DMAR;
		}
#ifdef SOUND
       	Sound_Update();
#endif
#endif	/* !BASIC */

		nframes++;

#ifndef	DONT_SYNC_WITH_HOST
/* if too fast host computer, slow the emulation down to 100% of original speed */

#ifdef USE_CLOCK
		/* on Atari Falcon CLK_TCK = 200 (i.e. 5 ms granularity) */
		/* on DOS (DJGPP) CLK_TCK = 91 (not too precise, but should work anyway)*/
		if (nextclock) {
			ULONG curclock;

#ifdef SNAILMETER
			emu_too_fast = -1;
#endif
			do {
				curclock = clock();
#ifdef SNAILMETER
				emu_too_fast++;
#endif
			} while (curclock < nextclock);

			nextclock = curclock + (CLK_TCK / (tv_mode == TV_PAL ? 50 : 60));
		}
#else	/* USE_CLOCK */
#ifndef DJGPP
		if (deltatime > 0.0) {
			double curtime;

#ifdef linux
		gettimeofday(&tp, NULL);
		curtime = (lasttime + deltatime)
			- tp.tv_sec - (tp.tv_usec / 1000000.0);
		if( curtime>0 )
		{	tp.tv_sec=  (int)(curtime);
			tp.tv_usec= (int)((curtime-tp.tv_sec)*1000000);
/* printf("delta=%f sec=%d usec=%d\n",curtime,tp.tv_sec,tp.tv_usec); */
			select(1,NULL,NULL,NULL,&tp);
		}
		gettimeofday(&tp, NULL);
		curtime = tp.tv_sec + (tp.tv_usec / 1000000.0);
#else	/* linux */

#ifdef SNAILMETER
			emu_too_fast = -1;
#endif
			do {
				gettimeofday(&tp, &tzp);
				curtime = tp.tv_sec + (tp.tv_usec / 1000000.0);
#ifdef SNAILMETER
				emu_too_fast++;
#endif
			} while (curtime < (lasttime + deltatime));
#endif	/* linux */
			
			fps = 1.0 / (curtime - lasttime);
			lasttime = curtime;
		}
#else	/* !DJGPP */	/* for dos, count ticks and use the ticks_per_second global variable */
		/* Use the high speed clock tick function uclock() */
		if (timesync) {
			if (deltatime > 0.0) {
				static uclock_t lasttime = 0;
				uclock_t curtime, uclockd;
				unsigned long uclockd_hi, uclockd_lo;
				unsigned long uclocks_per_int;

				uclocks_per_int = (deltatime * (double) UCLOCKS_PER_SEC) + 0.5;
				/* printf( "ticks_per_int %d, %.8X\n", uclocks_per_int, (unsigned long) lasttime ); */

#ifdef SNAILMETER
				emu_too_fast = -1;
#endif
				do {
					curtime = uclock();
					if (curtime > lasttime) {
						uclockd = curtime - lasttime;
					}
					else {
						uclockd = ((uclock_t) ~ 0 - lasttime) + curtime + (uclock_t) 1;
					}
					uclockd_lo = (unsigned long) uclockd;
					uclockd_hi = (unsigned long) (uclockd >> 32);
#ifdef SNAILMETER
					emu_too_fast++;
#endif
				} while ((uclockd_hi == 0) && (uclocks_per_int > uclockd_lo));

				lasttime = curtime;
			}
		}
#endif	/* !DJGPP */
#endif	/* USE_CLOCK */
#endif	/* !DONT_SYNC_WITH_HOST */
	}
#endif	/* !WIN32 */
}

#ifdef WIN32
void WinAtari800_Hardware (void)
{
	Atari_Keyboard ();
	GetJoystickInput( );
	ANTIC_RunDisplayList();			/* generate screen */
	Update_LED();
	if (++test_val==refresh_rate)
	{
		Atari_DisplayScreen ((UBYTE*)atari_screen);
		test_val = 0;
	}
	Atari_PlaySound();

	if( ulAtariState & ATARI_PAUSED )
		Screen_Paused( (UBYTE *)atari_screen );
}
#endif /* Win32 */

#ifndef WIN32
int zlib_capable(void)
{
#ifdef ZLIB_CAPABLE
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
#endif

void MainStateSave( void )
{
	UBYTE	temp;

	/* Possibly some compilers would handle an enumerated type differently,
	   so convert these into unsigned bytes and save them out that way */
	if( tv_mode == TV_PAL )
		temp = 0;
	else
		temp = 1;
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
