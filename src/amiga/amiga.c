/*
 * amiga.c - Amiga specific port code
 *
 * Copyright (c) 2000 Sebastian Bauer
 * Copyright (c) 2000-2005 Atari800 development team (see DOC/CREDITS)
 * Newest changes by Ventzislav Tzvetkov - http://drhirudo.hit.bg
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

#include "atari.h"
#include "ui.h"
#include "screen.h"
#include "input.h"

#undef FILENAME_SIZE
#undef UBYTE
#undef UWORD
#undef ULONG

#define __USE_INLINE__
#define DoMethod IDoMethod

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>

#include <devices/ahi.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <cybergraphx/cybergraphics.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>

#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/gadtools.h>
#include <proto/timer.h>
#include <proto/keymap.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <proto/lowlevel.h>

#include <proto/ahi.h>
#include <proto/cybergraphics.h>

#include "config.h"

#include "binload.h"
#include "colours.h"
#include "input.h"
#include "monitor.h"
#include "pokeysnd.h"
#include "screen.h"
#include "sio.h"
#include "statesav.h"

#include "amiga.h"
#include "support.h"
#include "afile.h"
#include "cfg.h"
#include "akey.h"

/******************************/

int PLATFORM_Exit (int run_monitor);

/******************************/

#include "Atari800_rev.h"


/* joystick emulation
   keys are loaded from config file
   Here the defaults if there is no keymap in the config file... */

/* a runtime switch for the kbd_joy_X_enabled vars is in the UI */
int PLATFORM_kbd_joy_0_enabled = TRUE;	/* enabled by default, doesn't hurt */
int PLATFORM_kbd_joy_1_enabled = FALSE;	/* disabled, would steal normal keys */

/* real joysticks */

static int fd_joystick0 = -1;
static int fd_joystick1 = -1;

#define minjoy 10000			/* real joystick tolerancy */


static char __attribute__((used)) verstag[] = VERSTAG;

/******************************/

#define PORT_SOURCE_UNASSIGNED  0
#define PORT_SOURCE_GAMEPORT    1
#define PORT_SOURCE_NUMERIC     2
#define PORT_SOURCE_CURSOR      3

static char *port_source_text[] =
{
	"UNASSIGNED","GAMEPORT","NUMERICPAD","CURSOR",NULL
};

/******************************/

#define DISPLAY_WINDOW				0
#define DISPLAY_SCALABLEWINDOW		1
#define DISPLAY_CUSTOMSCREEN		2

static char *display_text[] =
{
	"WINDOW","SCALBALEWINDOW","CUSTOMSCREEN",NULL
};

/*******************************/

struct timezone;

double fps;

/******************************/

#define ICONIFY_GID 1

/*******************************/

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *LayersBase;
struct Library *AslBase;
struct Device *TimerBase;
struct Library *AHIBase;
struct Library *GadToolsBase;
struct Library *CyberGfxBase;
struct Library *KeymapBase;
struct Library *UtilityBase;
struct Library *WorkbenchBase;
struct Library *IconBase;
struct Library *LowLevelBase;

struct IntuitionIFace *IIntuition;
struct GraphicsIFace *IGraphics;
struct LayersIFace *ILayers;
struct TimerIFace *ITimer;
struct AslIFace *IAsl;
struct GadToolsIFace *IGadTools;
struct CyberGfxIFace *ICyberGfx;
struct KeymapIFace *IKeymap;
struct UtilityIFace *IUtility;
struct WorkbenchIFace *IWorkbench;
struct IconIFace *IIcon;
struct LowLevelIFace *ILowLevel;

/* Timer */
static struct MsgPort *timer_msg_port;
static struct TimeRequest *timer_request;
static BOOL timer_error;

/* Sound */
static struct MsgPort *ahi_msg_port;
static struct AHIRequest *ahi_request;
static struct AHIRequest *ahi_soundreq[2];
static BOOL ahi_current;

static UWORD ahi_fps;                  /* frames per second */
static UWORD ahi_rupf;                 /* real updates per frame */
static UBYTE *ahi_streambuf[2];
static ULONG ahi_streamfreq = 48000;   /* playing frequence */
static ULONG ahi_streamlen;

/* App Window */
static struct MsgPort *app_port; /* initialized within Atari_Initialise() */
static struct AppWindow *app_window; /* initialized during SetupDisplay() */

/* Emulation */
static int menu_consol;
static int keyboard_consol;
static int trig[2];
static int stick[2];

/* Settings */
static LONG DisplayType;
static LONG UseBestID = TRUE;
static LONG SoundEnabled = TRUE;
static LONG ShowFPS;
static ULONG DisplayID = INVALID_ID;
static ULONG PortSource[2];

/* Emulator GUI */
static LONG ScreenIsCustom;
static struct Screen *ScreenMain;
static ULONG ScreenDepth;
static APTR VisualInfoMain;
static struct Window *WindowMain = NULL;
static struct Menu *MenuMain;
static Object *IconifyImage;
static Object *IconifyGadget;
static BOOL SizeVerify; /* SizeVerify State (don't draw anything) */

static UBYTE pentable[256];
static BOOL pensallocated;
static UBYTE *tempscreendata;
static struct RastPort lineRastPort;
static struct BitMap *lineBitMap;

struct FileRequester *DiskFileReq;
struct FileRequester *CartFileReq;
struct FileRequester *StateFileReq;
struct FileRequester *BinFileReq;

static int PaddlePos;

char atari_disk_dirs[UI_MAX_DIRECTORIES][FILENAME_MAX];
char atari_rom_dir[FILENAME_MAX];
char atari_h1_dir[FILENAME_MAX];
char atari_h2_dir[FILENAME_MAX];
char atari_h3_dir[FILENAME_MAX];
char atari_h4_dir[FILENAME_MAX];
char atari_exe_dir[FILENAME_MAX];
char atari_state_dir[FILENAME_MAX];
int disk_directories;

/**************************************************************************
 Returns the Programm's Diskobject
**************************************************************************/
struct DiskObject *GetProgramObject(void)
{
	struct DiskObject *obj;
	extern char program_name[]; /* from startup.c */
	BPTR odir;

	odir = CurrentDir(GetProgramDir());

	obj = GetIconTags(program_name,
						ICONGETA_FailIfUnavailable, FALSE,
						ICONGETA_GenerateImageMasks, FALSE,
						TAG_DONE);

	if(!obj)
		obj = GetIconTags(NULL,
						ICONGETA_GetDefaultType, WBTOOL,
						TAG_DONE);

	CurrentDir(odir);

	return obj;
}


/**************************************************************************
 Load any file. Returns TRUE on success.
**************************************************************************/
int Atari_LoadAnyFile(STRPTR name)
{
	return AFILE_OpenFile(name, TRUE, 1, FALSE) != AFILE_ERROR;
}

/**************************************************************************
 This is the menu when the emulator is running
**************************************************************************/
static struct NewMenu MenuEntries[] =
{
	{NM_TITLE, "Project", NULL, 0, 0L, (APTR)MEN_PROJECT},
	{NM_ITEM, "About...", "?", 0, 0L, (APTR)MEN_PROJECT_ABOUT},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Boot Disk...", "B", 0, 0L, (APTR)MEN_SYSTEM_BOOT},
	{NM_ITEM, "Insert Disk", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID},
	{NM_SUB, "Drive 1...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID1},
	{NM_SUB, "Drive 2...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID2},
	{NM_SUB, "Drive 3...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID3},
	{NM_SUB, "Drive 4...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID4},
	{NM_SUB, "Drive 5...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID5},
	{NM_SUB, "Drive 6...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID6},
	{NM_SUB, "Drive 7...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID7},
	{NM_SUB, "Drive 8...", NULL, 0, 0L, (APTR)MEN_SYSTEM_ID8},
	{NM_ITEM, "Eject Disk", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED},
	{NM_SUB, "Drive 1", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED1},
	{NM_SUB, "Drive 2", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED2},
	{NM_SUB, "Drive 3", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED3},
	{NM_SUB, "Drive 4", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED4},
	{NM_SUB, "Drive 5", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED5},
	{NM_SUB, "Drive 6", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED6},
	{NM_SUB, "Drive 7", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED7},
	{NM_SUB, "Drive 8", NULL, 0, 0L, (APTR)MEN_SYSTEM_ED8},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Load State...", "L", 0, 0L, (APTR)MEN_PROJECT_LOADSTATE},
	{NM_ITEM, "Save State...", "S", 0, 0L, (APTR)MEN_PROJECT_SAVESTATE},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Load BIN...", NULL, 0, 0L, (APTR)MEN_PROJECT_LOADBIN},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Internal User Interface", "I", 0, 0L, (APTR)MEN_SYSTEM_UI},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Iconify", NULL, 0, 0L, (APTR)MEN_PROJECT_ICONIFY},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Quit...", "Q", 0, 0L, (APTR)MEN_PROJECT_QUIT},
	{NM_TITLE, "Console", NULL, 0, 0L, (APTR)MEN_CONSOLE},
	{NM_ITEM, "Option", "F2", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_OPTION},
	{NM_ITEM, "Select", "F3", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_SELECT},
	{NM_ITEM, "Start", "F4", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_START},
	{NM_ITEM, "Help", "Help", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_HELP},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Break", "F7", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_BREAK},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Reset", "F5", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_RESET},
	{NM_ITEM, "Coldstart", "Shift F5", NM_COMMANDSTRING, 0L, (APTR)MEN_CONSOLE_COLDSTART},
	{NM_TITLE, "Settings", NULL, 0, 0L, (APTR)MEN_SETTINGS},
	{NM_ITEM, "Show Framerate?", NULL, MENUTOGGLE|CHECKIT, 0L, (APTR)MEN_SETTINGS_FRAMERATE},
	{NM_ITEM, NM_BARLABEL, NULL, 0, 0L, NULL},
	{NM_ITEM, "Display in", NULL, 0, 0L, NULL},
	{NM_SUB, "Window?", NULL, CHECKIT, 2+4, (APTR)MEN_SETTINGS_WINDOW},
	{NM_SUB, "Scalable Window?", NULL, CHECKIT, 1+4, (APTR)MEN_SETTINGS_SCALABLEWINDOW},
	{NM_SUB, "Custom Screen?", NULL, CHECKIT, 1+2, (APTR)MEN_SETTINGS_CUSTOMSCREEN},
	{NM_ITEM, "Atari Gameport 0", NULL, 0, 0L, NULL},
	{NM_SUB, "Gameport 0", NULL, CHECKIT, 2+4+8, (APTR)MEN_SETTINGS_PORT0_GAMEPORT},
	{NM_SUB, "Numeric Pad", NULL, CHECKIT, 1+4+8, (APTR)MEN_SETTINGS_PORT0_NUMERICPAD},
	{NM_SUB, "Cursor Keys", NULL, CHECKIT, 1+2+8, (APTR)MEN_SETTINGS_PORT0_CURSORKEYS},
	{NM_SUB, "Unassigned", NULL, CHECKIT, 1+2+4, (APTR)MEN_SETTINGS_PORT0_UNASSIGNED},
	{NM_ITEM, "Atari Gameport 1", NULL, 0, 0L, NULL},
	{NM_SUB, "Gameport 1", NULL, CHECKIT, 2+4+8, (APTR)MEN_SETTINGS_PORT1_GAMEPORT},
	{NM_SUB, "Numeric Pad", NULL, CHECKIT, 1+4+8, (APTR)MEN_SETTINGS_PORT1_NUMERICPAD},
	{NM_SUB, "Cursor Keys", NULL, CHECKIT, 1+2+8, (APTR)MEN_SETTINGS_PORT1_CURSORKEYS},
	{NM_SUB, "Unassigned", NULL, CHECKIT, 1+2+4, (APTR)MEN_SETTINGS_PORT1_UNASSIGNED},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Save Settings", NULL, 0, 0L, (APTR)MEN_SETTINGS_SAVE},
	{NM_END, NULL, NULL, 0, 0L, NULL}
};

/**************************************************************************
 Close all previously opened libraries
**************************************************************************/
VOID CloseLibraries(void)
{
	if (LowLevelBase) CloseLibraryInterface(LowLevelBase, ILowLevel);
	if (IconBase) CloseLibraryInterface(IconBase, IIcon);
	if (WorkbenchBase) CloseLibraryInterface(WorkbenchBase, IWorkbench);
	if (CyberGfxBase) CloseLibraryInterface(CyberGfxBase, ICyberGfx);
	if (KeymapBase) CloseLibraryInterface(KeymapBase,IKeymap);
	if (GadToolsBase) CloseLibraryInterface(GadToolsBase,IGadTools);
	if (AslBase) CloseLibraryInterface(AslBase,IAsl);
	if (LayersBase) CloseLibraryInterface(LayersBase,ILayers);
	if (GfxBase) CloseLibraryInterface(GfxBase,IGraphics);
	if (UtilityBase) CloseLibraryInterface(UtilityBase, IUtility);
	if (IntuitionBase) CloseLibraryInterface(IntuitionBase,IIntuition);
}

/**************************************************************************
 Open all necessary and optional libraries. Returns TRUE if succeeded
**************************************************************************/
BOOL OpenLibraries(void)
{
	if ((IntuitionBase = OpenLibraryInterface ("intuition.library", 39, &IIntuition)))
	{
		if ((UtilityBase = OpenLibraryInterface ("utility.library", 50, &IUtility)))
		{
			if ((GfxBase = OpenLibraryInterface ("graphics.library", 39, &IGraphics)))
			{
				if ((LayersBase = OpenLibraryInterface ("layers.library", 39, &ILayers)))
				{
					if ((AslBase = OpenLibraryInterface ("asl.library", 39, &IAsl)))
					{
						if ((GadToolsBase = OpenLibraryInterface("gadtools.library",39, &IGadTools)))
						{
							if ((KeymapBase = OpenLibraryInterface("keymap.library",36, &IKeymap)))
							{
								if ((CyberGfxBase = OpenLibraryInterface("cybergraphics.library",41, &ICyberGfx)))
								{

									if ((WorkbenchBase = OpenLibraryInterface("workbench.library",36, &IWorkbench)))
									{
										if ((IconBase = OpenLibraryInterface("icon.library",50,&IIcon)))
										{
											if ((LowLevelBase = OpenLibraryInterface("lowlevel.library",44,&ILowLevel)))
											return TRUE;
										}		
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return FALSE;
}

/**************************************************************************
 Free sound stuff
**************************************************************************/
VOID FreeSound(void)
{
	if(ahi_soundreq[0])
	{
		if(!CheckIO((struct IORequest*)ahi_soundreq[0]))
		{
			AbortIO((struct IORequest*)ahi_soundreq[0]);
			WaitIO((struct IORequest*)ahi_soundreq[0]);
		}
	}

	if(ahi_soundreq[1])
	{
		if(!CheckIO((struct IORequest*)ahi_soundreq[1]))
		{
			AbortIO((struct IORequest*)ahi_soundreq[1]);
			WaitIO((struct IORequest*)ahi_soundreq[1]);
		}
	}

	if(ahi_request )
	{
		if(AHIBase)
		{
			if(!CheckIO((struct IORequest*)ahi_request))
			{
				AbortIO((struct IORequest*)ahi_request);
				WaitIO((struct IORequest*)ahi_request);
			}
			CloseDevice((struct IORequest*)ahi_request);
		}
		DeleteIORequest((struct IORequest*)ahi_request);
	}

	if(ahi_msg_port) DeleteMsgPort(ahi_msg_port);

	if(ahi_streambuf[0]) FreeVec(ahi_streambuf[0]);
	if(ahi_streambuf[1]) FreeVec(ahi_streambuf[1]);
}

/**************************************************************************
 Allocate Soundstuff
**************************************************************************/
BOOL SetupSound(void)
{
	if(Atari800_tv_mode== Atari800_TV_PAL)
	{
		ahi_fps = 50;
		ahi_rupf = 312;
	}	else
	{
		ahi_fps = 60;
		ahi_rupf = 262;
	}

	if(!SoundEnabled) return TRUE;

	if((ahi_msg_port = CreateMsgPort()))
	{
		if((ahi_request = (struct AHIRequest *) CreateIORequest(
           ahi_msg_port, sizeof(struct AHIRequest))))
		{
			ahi_request->ahir_Version = 4;  /* Open at least version 4. */

			if (!OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *) ahi_request, 0))
			{
				AHIBase = (struct Library *) ahi_request->ahir_Std.io_Device;


#ifdef STEREO_SOUND
				ahi_streamlen = (ahi_streamfreq/ahi_fps) * 2;
				POKEYSND_Init(POKEYSND_FREQ_17_EXACT, ahi_streamfreq, 2, 0);
#else
				ahi_streamlen = ahi_streamfreq/ahi_fps;
				POKEYSND_Init(POKEYSND_FREQ_17_EXACT, ahi_streamfreq, 1, 0);
#endif
				if((ahi_streambuf[0] = AllocVec(ahi_streamlen*3, MEMF_PUBLIC|MEMF_CLEAR)))
				{
					if((ahi_streambuf[1] = AllocVec(ahi_streamlen*3, MEMF_PUBLIC|MEMF_CLEAR)))
					{
						ahi_request->ahir_Std.io_Message.mn_Node.ln_Pri = -50;
						ahi_request->ahir_Std.io_Command  = CMD_WRITE;
						ahi_request->ahir_Std.io_Data     = NULL;
						ahi_request->ahir_Std.io_Length   = ahi_streamlen;
						ahi_request->ahir_Std.io_Offset   = 0;
						ahi_request->ahir_Frequency       = ahi_streamfreq;

#ifdef STEREO_SOUND
						ahi_request->ahir_Type            = AHIST_S8S;
#else
						ahi_request->ahir_Type            = AHIST_M8S;
#endif
						ahi_request->ahir_Volume          = 0x10000;          /* Full volume */
						ahi_request->ahir_Position        = 0x8000;           /* Centered */
						ahi_request->ahir_Link            = NULL;

						if((ahi_soundreq[0] = (struct AHIRequest*)AllocVec(sizeof(struct AHIRequest),MEMF_PUBLIC)))
						{
							if((ahi_soundreq[1] = (struct AHIRequest*)AllocVec(sizeof(struct AHIRequest),MEMF_PUBLIC)))
							{
								return TRUE;
							}
							FreeVec(ahi_soundreq[0]);
							ahi_soundreq[0] = NULL;
						}
						FreeVec(ahi_streambuf[1]);
						ahi_streambuf[1] = NULL;
					}
					FreeVec(ahi_streambuf[0]);
					ahi_streambuf[0] = NULL;
				}
				CloseDevice((struct IORequest*)ahi_request);
			}
			DeleteIORequest((struct IORequest*)ahi_request);
			ahi_request = NULL;
		}
		DeleteMsgPort(ahi_msg_port);
		ahi_msg_port = NULL;
	}
	SoundEnabled = FALSE;
	return FALSE;
}

/**************************************************************************
 Free everything which is assoicated with the Atari Screen while
 Emulating
**************************************************************************/
VOID FreeDisplay(void)
{
	int i;

	if (MenuMain)
	{
		if (WindowMain) ClearMenuStrip(WindowMain);
		FreeMenus(MenuMain);
		MenuMain = NULL;
	}

	if (WindowMain)
	{
		if (app_window)
		{
			RemoveAppWindow(app_window);
		}

		CloseWindow( WindowMain );
		WindowMain = NULL;
	}

	if (IconifyImage)
	{
		DisposeObject(IconifyImage);
		IconifyImage = NULL;
	}

	if (IconifyGadget)
	{
		DisposeObject(IconifyGadget);
		IconifyGadget = NULL;
	}

	if (VisualInfoMain)
	{
		FreeVisualInfo( VisualInfoMain );
		VisualInfoMain = NULL;
	}

	if (lineBitMap)
	{
		FreeBitMap(lineBitMap);
		lineBitMap = NULL;
	}

	if (tempscreendata)
	{
		FreeVec(tempscreendata);
		tempscreendata=NULL;
	}

	if (ScreenMain)
	{
		if (ScreenIsCustom) CloseScreen(ScreenMain);
		else
		{
			if (pensallocated)
			{
				for(i=0;i<256;i++) ReleasePen(ScreenMain->ViewPort.ColorMap,pentable[i]);
				pensallocated = 0;
			}
			UnlockPubScreen(NULL,ScreenMain);
		}
		ScreenMain = NULL;
	}
}

/**************************************************************************
 Ensures that the scaledscreendata is big enough
**************************************************************************/
LONG EnsureScaledDisplay(LONG Width)
{
	ULONG depth;

	if (lineBitMap)
	{
		if (GetBitMapAttr(lineBitMap, BMA_WIDTH) >= Width)
			return 1;
		FreeBitMap(lineBitMap);
	}

	depth = GetBitMapAttr(ScreenMain->RastPort.BitMap, BMA_DEPTH);
	lineBitMap = AllocBitMap(Width,1,depth,BMF_MINPLANES|BMF_DISPLAYABLE,ScreenMain->RastPort.BitMap);
	InitRastPort(&lineRastPort);
	lineRastPort.BitMap = lineBitMap;
	return !!lineBitMap;
}

/**************************************************************************
 Allocate everything which is assoicated with the Atari Screen while
 Emulating
**************************************************************************/
LONG SetupDisplay(void)
{
	UWORD ScreenWidth = 0, ScreenHeight = 0;
	struct MenuItem *mi;
	int i;

	STATIC WORD ScreenPens[NUMDRIPENS+1];

	ScreenPens[DETAILPEN] = 0;
	ScreenPens[BLOCKPEN] = 15;
	ScreenPens[TEXTPEN] = 0;
	ScreenPens[SHINEPEN] = 15;
	ScreenPens[SHADOWPEN] = 0;
	ScreenPens[FILLPEN] = 120;
	ScreenPens[FILLTEXTPEN] = 0;
	ScreenPens[BACKGROUNDPEN] = 9;
	ScreenPens[HIGHLIGHTTEXTPEN] = 15;
	ScreenPens[BARDETAILPEN] = 0;
	ScreenPens[BARBLOCKPEN] = 15;
	ScreenPens[BARTRIMPEN] = 0;
	ScreenPens[NUMDRIPENS] = -1;

#if DRI_VERSION > 2
	ScreenPens[MENUBACKGROUNDPEN] = 13;
	ScreenPens[SELECTPEN] = 120;
#endif

	if (DisplayType == DISPLAY_CUSTOMSCREEN)
	{
		ULONG ScreenDisplayID;
		static ULONG colors32[3*256+2];

		ScreenIsCustom = TRUE;
		ScreenWidth = Screen_WIDTH - 64;
		ScreenHeight = Screen_HEIGHT;
		ScreenDepth = 8;

		ScreenDisplayID = DisplayID;
		if (UseBestID) ScreenDisplayID = GetBestID(ScreenWidth,ScreenHeight,ScreenDepth);

		colors32[0] = 256 << 16;
		for (i=0;i<256;i++)
		{
			int rgb = Colours_table[i];
			int red,green,blue;

			red = rgb & 0x00ff0000;
			green = rgb & 0x0000ff00;
			blue = rgb & 0x000000ff;

			colors32[1+i*3] = red | (red << 8) | (red << 16) | (red << 24);
			colors32[2+i*3] = green | (green << 8) | (green << 16) | (green << 24);
			colors32[3+i*3] = blue | (blue << 8) | (blue << 16) | (blue << 24);
		}

		ScreenMain = OpenScreenTags( NULL,
									SA_Left, 0,
									SA_Top, 0,
									SA_Width, ScreenWidth,
									SA_Height, ScreenHeight,
									SA_Depth, ScreenDepth,
									SA_Pens, ScreenPens,
									SA_Quiet, TRUE,
									SA_Type, CUSTOMSCREEN,
									SA_AutoScroll, TRUE,
									SA_DisplayID, ScreenDisplayID,
									SA_Colors32, colors32,
									SA_Exclusive, TRUE,
									SA_OffScreenDragging, TRUE,
									TAG_DONE);
	}	else
	{
		if ((ScreenMain = LockPubScreen(NULL)))
		{
			ScreenIsCustom = FALSE;
			ScreenWidth = Screen_WIDTH - 56;
			ScreenHeight = Screen_HEIGHT;
			ScreenDepth = GetBitMapAttr(ScreenMain->RastPort.BitMap,BMA_DEPTH);

			if (ScreenDepth <= 8 || !CyberGfxBase)
			{
				int i;

				for(i=0;i<256;i++)
				{
					ULONG rgb = Colours_table[i];
					ULONG red = (rgb & 0x00ff0000) >> 16;
					ULONG green = (rgb & 0x0000ff00) >> 8;
					ULONG blue = (rgb & 0x000000ff);

					red |= (red<<24)|(red<<16)|(red<<8);
					green |= (green<<24)|(green<<16)|(green<<8);
					blue |= (blue<<24)|(blue<<16)|(blue<<8);

					pentable[i] = ObtainBestPenA(ScreenMain->ViewPort.ColorMap,red,green,blue,NULL);
				}
				pensallocated = 1;
			}

			/* Iconfiy Gadget is optional */
			if (IntuitionBase->lib_Version >= 50)
			{
				struct DrawInfo *dri;

				if ((dri = GetScreenDrawInfo(ScreenMain)))
				{
				    if ((IconifyImage = NewObject( NULL, "sysiclass",
							SYSIA_DrawInfo, dri,
							SYSIA_Which, ICONIFYIMAGE,
							TAG_DONE )))
					{
						IconifyGadget = NewObject( NULL, "buttongclass",
						        GA_Image, IconifyImage,
								GA_Titlebar, TRUE,     /* Implies GA_TopBorder, TRUE */
								GA_RelRight, 0,        /* Just to set GFLG_RELRIGHT */
								GA_ID, ICONIFY_GID,
								GA_RelVerify, TRUE,
								TAG_DONE );
					}

					FreeScreenDrawInfo(ScreenMain,dri);
				}
			}


			if (!(tempscreendata = (UBYTE*)AllocVec(Screen_WIDTH*(Screen_HEIGHT+16),MEMF_CLEAR)))
			{
				UnlockPubScreen(NULL,ScreenMain);
				ScreenMain = NULL;
			}
		}
	}

	if (ScreenMain)
	{
		if ((VisualInfoMain = GetVisualInfoA( ScreenMain, NULL )))
		{
			if ((MenuMain = CreateMenus(MenuEntries, GTMN_NewLookMenus, TRUE, TAG_DONE)))
			{
				LayoutMenus( MenuMain, VisualInfoMain, GTMN_NewLookMenus, TRUE, TAG_DONE);

				if ((WindowMain = OpenWindowTags( NULL,
					WA_Activate, TRUE,
					WA_NewLookMenus, TRUE,
					WA_MenuHelp, TRUE,
					WA_InnerWidth, ScreenWidth,
					WA_InnerHeight, ScreenHeight,
					WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW |
									IDCMP_RAWKEY | IDCMP_NEWSIZE | IDCMP_SIZEVERIFY | IDCMP_GADGETUP,
					WA_ReportMouse, TRUE,
					WA_CustomScreen, ScreenMain,
					WA_Borderless, ScreenIsCustom,
					WA_CloseGadget, !ScreenIsCustom,
					WA_DragBar, !ScreenIsCustom,
					WA_DepthGadget, !ScreenIsCustom,
					ScreenIsCustom?TAG_IGNORE:WA_Title, "Atari 800",
					ScreenIsCustom?TAG_IGNORE:WA_ScreenTitle, VERS,
					WA_SizeGadget, DisplayType == DISPLAY_SCALABLEWINDOW,
					WA_SmartRefresh, TRUE,
					DisplayType == DISPLAY_SCALABLEWINDOW?WA_SizeBBottom:TAG_IGNORE, TRUE,
					DisplayType == DISPLAY_SCALABLEWINDOW?WA_MaxWidth:TAG_IGNORE,-1,
					DisplayType == DISPLAY_SCALABLEWINDOW?WA_MaxHeight:TAG_IGNORE,-1,
					TAG_DONE)))
				{
					if (IconifyGadget)
					{
						AddGList( WindowMain, (struct Gadget*)IconifyGadget, (UWORD)0, 1, NULL );
						RefreshGList( (struct Gadget*)IconifyGadget, WindowMain, NULL, 1 );
					}

					if ((mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_FRAMERATE)))
						mi->Flags |= ShowFPS?CHECKED:0;

					switch (DisplayType)
					{
						case	DISPLAY_SCALABLEWINDOW: mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_SCALABLEWINDOW); break;
						case	DISPLAY_CUSTOMSCREEN: mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_CUSTOMSCREEN); break;
						default: mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_WINDOW); break;
					}
					if (mi) mi->Flags |= CHECKED;

					switch (PortSource[0])
					{
						case PORT_SOURCE_GAMEPORT:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT0_GAMEPORT);break;
						case PORT_SOURCE_NUMERIC:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT0_NUMERICPAD);break;
						case PORT_SOURCE_CURSOR:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT0_CURSORKEYS);break;
						default: mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT0_UNASSIGNED);break;
					}
					if (mi) mi->Flags |= CHECKED;

					switch (PortSource[1])
					{
						case PORT_SOURCE_GAMEPORT:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT1_GAMEPORT);break;
						case PORT_SOURCE_NUMERIC:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT1_NUMERICPAD);break;
						case PORT_SOURCE_CURSOR:mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT1_CURSORKEYS);break;
						default: mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_PORT1_UNASSIGNED);break;
					}
					if (mi) mi->Flags |= CHECKED;

					mi = FindUserData(MenuMain, (APTR)MEN_SETTINGS_PORT0_GAMEPORT);
					if (mi)
					{

						if (SetJoyPortAttrs(0,SJA_Type, SJA_TYPE_GAMECTLR, NULL )) mi->Flags |= ITEMENABLED;
						else mi->Flags &= ~ITEMENABLED;
					}

					mi = FindUserData(MenuMain, (APTR)MEN_SETTINGS_PORT1_GAMEPORT);
					if (mi)
					{
						if (SetJoyPortAttrs(1,SJA_Type, SJA_TYPE_GAMECTLR, NULL )) mi->Flags |= ITEMENABLED;
						else mi->Flags &= ~ITEMENABLED;
					}

					SetMenuStrip(WindowMain, MenuMain);

					/* An AppWindow is not required */
					app_window = AddAppWindowA(1 /* id */, 0, WindowMain, app_port, NULL);;

					return 1;
				}
			}
		}
	}
	FreeDisplay();
	return 0;
}

/**************************************************************************
 Free timer stuff
**************************************************************************/
VOID FreeTimer(void)
{
	if (timer_request)
	{
		if (ITimer) DropInterface((struct Interface*)ITimer);
		if (!timer_error) CloseDevice((struct IORequest *)timer_request);

		DeleteIORequest((struct IORequest*)timer_request);
		timer_request = NULL;
	}
	if (timer_msg_port)
	{
		DeleteMsgPort(timer_msg_port);
		timer_msg_port = NULL;
	}
}

/**************************************************************************
 Setup Timer
**************************************************************************/
BOOL SetupTimer(void)
{
	if((timer_msg_port = CreateMsgPort()))
	{
		timer_request = (struct TimeRequest*)CreateIORequest( timer_msg_port, sizeof( struct TimeRequest ));
		if( timer_request )
		{
			timer_error = OpenDevice( TIMERNAME, UNIT_MICROHZ, (struct IORequest*)timer_request, 0);
			if( !timer_error )
			{
				TimerBase = timer_request->Request.io_Device;
				if ((ITimer = (struct TimerIFace*)GetInterface((struct Library*)TimerBase,"main",1,NULL)))
					return TRUE;
			}
		}
	}
	return FALSE;
}


/**************************************************************************
 Iconify the atari emu by closing the display and placing an icon
 on the wb.
**************************************************************************/
VOID Iconify(void)
{
	struct DiskObject *dobj = GetProgramObject();
	struct AppIcon *app_icon;
	extern char program_name[];

	if (!dobj) return;

	if ((app_icon = AddAppIcon(2,0L,program_name,app_port,ZERO,dobj,
								TAG_DONE)))
	{
		int ready = 0;

		FreeDisplay();

		while (!ready)
		{
			struct AppMessage *amsg;
			WaitPort(app_port);

			while ((amsg = (struct AppMessage *)GetMsg(app_port)))
			{
				if (amsg->am_Type == AMTYPE_APPICON && amsg->am_ID == 2)
					ready = 1;
				ReplyMsg((struct Message*)amsg);
			}
		}

		RemoveAppIcon(app_icon);

		if (!SetupDisplay())
			PLATFORM_Exit(0);
	}

	FreeDiskObject(dobj);
}

/**************************************************************************
 Handle the menu
**************************************************************************/
int HandleMenu(UWORD code)
{
	int keycode = -1;

	while( code != MENUNULL )
	{
		struct MenuItem *mi = ItemAddress( MenuMain, code );
		if( mi )
		{
			LONG udata = (LONG)GTMENUITEM_USERDATA( mi );
			switch(udata)
			{
				case	MEN_PROJECT_ABOUT:
						{
							struct EasyStruct easy;
							easy.es_StructSize = sizeof(struct EasyStruct);
							easy.es_Flags = 0;
							easy.es_Title = "Atari800";
							easy.es_TextFormat = "Atari800 version %ld.%ld\n\nBased upon " Atari800_TITLE "\nCopyright (C) 2000-2009\nAtari800 development team\nAmiga port done by Sebastian Bauer\nand Ventzislav Tzvetkov";
							easy.es_GadgetFormat = "Ok";

							EasyRequest( WindowMain, &easy, NULL, VERSION, REVISION);
						}
						break;

				case	MEN_PROJECT_LOADSTATE:
							if(AslRequestTags(StateFileReq,
											ASLFR_DoSaveMode, FALSE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(StateFileReq->fr_Drawer, ACCESS_READ);
								if(cd)
								{
									BPTR odir = CurrentDir(cd);
									StateSav_ReadAtariState(StateFileReq->fr_File,"rb");
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_SAVESTATE:
							if(AslRequestTags(StateFileReq,
											ASLFR_DoSaveMode, TRUE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(StateFileReq->fr_Drawer, ACCESS_READ);
								if (cd)
								{
									BPTR odir = CurrentDir(cd);
									STRPTR fileName = AddSuffix(StateFileReq->fr_File,".sav");
									if(fileName)
									{
										StateSav_SaveAtariState(fileName,"wb",TRUE);
										FreeVec(fileName);
									}
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_LOADBIN:
							if(AslRequestTags(BinFileReq,
											ASLFR_DoSaveMode, FALSE,
											ASLFR_Screen, ScreenMain,
											TAG_DONE))
							{
								BPTR cd = Lock(BinFileReq->fr_Drawer, ACCESS_READ);
								if(cd)
								{
									BPTR odir = CurrentDir(cd);
									BINLOAD_Loader(BinFileReq->fr_File);
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

				case	MEN_PROJECT_ICONIFY: keycode = -2; break;
				case	MEN_PROJECT_QUIT: keycode = AKEY_EXIT; break;

				case	MEN_SYSTEM_BOOT:
							if(InsertDisk (1))
							{
								menu_consol = 7;
								keycode = AKEY_COLDSTART;
							}
							break;

				case	MEN_SYSTEM_ID1: InsertDisk(1); break;
				case	MEN_SYSTEM_ID2: InsertDisk(2); break;
				case	MEN_SYSTEM_ID3: InsertDisk(3); break;
				case	MEN_SYSTEM_ID4: InsertDisk(4); break;
				case	MEN_SYSTEM_ID5: InsertDisk(5); break;
				case	MEN_SYSTEM_ID6: InsertDisk(6); break;
				case	MEN_SYSTEM_ID7: InsertDisk(7); break;
				case	MEN_SYSTEM_ID8: InsertDisk(8); break;
				case	MEN_SYSTEM_ED1: SIO_Dismount(1); break;
				case	MEN_SYSTEM_ED2: SIO_Dismount(2); break;
				case	MEN_SYSTEM_ED3: SIO_Dismount(3); break;
				case	MEN_SYSTEM_ED4: SIO_Dismount(4); break;
				case	MEN_SYSTEM_ED5: SIO_Dismount(5); break;
				case	MEN_SYSTEM_ED6: SIO_Dismount(6); break;
				case	MEN_SYSTEM_ED7: SIO_Dismount(7); break;
				case	MEN_SYSTEM_ED8: SIO_Dismount(8); break;

				case	MEN_SYSTEM_UI:
						keycode = AKEY_UI;
						break;

				case	MEN_CONSOLE_RESET:
							Atari800_Warmstart();
							menu_consol = 7;
							break;

				case	MEN_CONSOLE_OPTION:
							menu_consol &= 0x03;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_SELECT:
							menu_consol &= 0x05;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_START:
							menu_consol &= 0x06;
							keycode = AKEY_NONE;
							break;

				case	MEN_CONSOLE_HELP:
							keycode = AKEY_HELP;
							break;

				case	MEN_CONSOLE_BREAK:
							keycode = AKEY_BREAK;
							break;

				case	MEN_CONSOLE_COLDSTART:
							Atari800_Coldstart();
							menu_consol = 7;
							break;

				case	MEN_SETTINGS_FRAMERATE:
						ShowFPS = !!(mi->Flags & CHECKED);
						break;

				case	MEN_SETTINGS_PORT0_GAMEPORT: PortSource[0] = PORT_SOURCE_GAMEPORT; break;
				case	MEN_SETTINGS_PORT0_NUMERICPAD: PortSource[0] = PORT_SOURCE_NUMERIC; break;
				case	MEN_SETTINGS_PORT0_CURSORKEYS: PortSource[0] = PORT_SOURCE_CURSOR; break;
				case	MEN_SETTINGS_PORT0_UNASSIGNED: PortSource[0] = PORT_SOURCE_UNASSIGNED; break;

				case	MEN_SETTINGS_PORT1_GAMEPORT: PortSource[1] = PORT_SOURCE_GAMEPORT; break;
				case	MEN_SETTINGS_PORT1_NUMERICPAD: PortSource[1] = PORT_SOURCE_NUMERIC; break;
				case	MEN_SETTINGS_PORT1_CURSORKEYS: PortSource[1] = PORT_SOURCE_CURSOR; break;
				case	MEN_SETTINGS_PORT1_UNASSIGNED: PortSource[1] = PORT_SOURCE_UNASSIGNED; break;

				case	MEN_SETTINGS_WINDOW: DisplayType = DISPLAY_WINDOW; break;
				case	MEN_SETTINGS_SCALABLEWINDOW: DisplayType = DISPLAY_SCALABLEWINDOW; break;
				case	MEN_SETTINGS_CUSTOMSCREEN: DisplayType = DISPLAY_CUSTOMSCREEN; break;

				case	MEN_SETTINGS_SAVE:
						CFG_WriteConfig();
						break;
			}

/*	if (key_buf[0x3c])	// F2
		consol &= 0x03;	// OPTION key ON
	else
		consol |= 0x04;	// OPTION key OFF
	if (key_buf[0x3d])	// F3
		consol &= 0x05;	// SELECT key ON
	else
		consol |= 0x02;	// SELECT key OFF
	if (key_buf[0x3e])	// F4
		consol &= 0x06;	// START key ON
	else
		consol |= 0x01;	// START key OFF
*/

			code = mi->NextSelect;
		}	else code = MENUNULL;
	}

	return keycode;
}

/**************************************************************************
 Handle the Vanillakeys
**************************************************************************/
int HandleVanillakey(int code)
{
	int keycode;

	switch (code)
	{
		case	0x01: keycode = AKEY_CTRL_a; break;
		case	0x02: keycode = AKEY_CTRL_b; break;
		case	0x03: keycode = AKEY_CTRL_c; break;
		case	0x04: keycode = AKEY_CTRL_d; break;
		case	0x05: keycode = AKEY_CTRL_E; break;
		case	0x06: keycode = AKEY_CTRL_F; break;
		case	0x07: keycode = AKEY_CTRL_G; break;

		/*
							case 0x08 :
								keycode = AKEY_CTRL_H;
								break;
		*/
		/* TAB - see case '\t' :
							case 0x09 :
								keycode = AKEY_CTRL_I;
								break;
		*/
		case	0x0a: keycode = AKEY_CTRL_J; break;
		case	0x0b: keycode = AKEY_CTRL_K; break;
		case	0x0c: keycode = AKEY_CTRL_L; break;

		/*
							case 0x0d :
								keycode = AKEY_CTRL_M;
								break;
		*/
		case 0x0e: keycode = AKEY_CTRL_N; break;
		case 0x0f: keycode = AKEY_CTRL_O; break;
		case 0x10: keycode = AKEY_CTRL_P; break;
		case 0x11: keycode = AKEY_CTRL_Q; break;
		case 0x12: keycode = AKEY_CTRL_R; break;
		case 0x13: keycode = AKEY_CTRL_S; break;
		case 0x14: keycode = AKEY_CTRL_T; break;
		case 0x15: keycode = AKEY_CTRL_U; break;
		case 0x16: keycode = AKEY_CTRL_V; break;
		case 0x17: keycode = AKEY_CTRL_W; break;
		case 0x18: keycode = AKEY_CTRL_X; break;
		case 0x19: keycode = AKEY_CTRL_Y; break;
		case 0x1a: keycode = AKEY_CTRL_Z; break;
		case 8:    keycode = AKEY_BACKSPACE; break;
		case 13:   keycode = AKEY_RETURN; break;
		case 0x1b: keycode = AKEY_ESCAPE; break;
		case '0':  keycode = AKEY_0; break;
		case '1':  keycode = AKEY_1; break;
		case '2':  keycode = AKEY_2; break;
		case '3':  keycode = AKEY_3; break;
		case '4':  keycode = AKEY_4; break;
		case '5':  keycode = AKEY_5; break;
		case '6':  keycode = AKEY_6; break;
		case '7':  keycode = AKEY_7; break;
		case '8':  keycode = AKEY_8; break;
		case '9':  keycode = AKEY_9; break;
		case 'A': case	'a': keycode = AKEY_a; break;
		case 'B' : case 'b' : keycode = AKEY_b; break;
		case 'C' : case 'c' : keycode = AKEY_c; break;
		case 'D' : case 'd' : keycode = AKEY_d; break;
		case 'E' : case 'e' : keycode = AKEY_e; break;
		case 'F' : case 'f' : keycode = AKEY_f; break;
		case 'G' : case 'g' : keycode = AKEY_g; break;
		case 'H' : case 'h' : keycode = AKEY_h; break;
		case 'I' : case 'i' : keycode = AKEY_i; break;
		case 'J' : case 'j' : keycode = AKEY_j; break;
		case 'K' : case 'k' : keycode = AKEY_k; break;
		case 'L' : case 'l' : keycode = AKEY_l; break;
		case 'M' : case 'm' : keycode = AKEY_m; break;
		case 'N' : case 'n' : keycode = AKEY_n; break;
		case 'O' : case 'o' : keycode = AKEY_o; break;
		case 'P' : case 'p' : keycode = AKEY_p; break;
		case 'Q' : case 'q' : keycode = AKEY_q; break;
		case 'R' : case 'r' : keycode = AKEY_r; break;
		case 'S' : case 's' : keycode = AKEY_s; break;
		case 'T' : case 't' : keycode = AKEY_t; break;
		case 'U' : case 'u' : keycode = AKEY_u; break;
		case 'V' : case 'v' : keycode = AKEY_v; break;
		case 'W' : case 'w' : keycode = AKEY_w; break;
		case 'X' : case 'x' : keycode = AKEY_x; break;
		case 'Y' : case 'y' : keycode = AKEY_y; break;
		case 'Z' : case 'z' : keycode = AKEY_z; break;
		case ' ' : keycode = AKEY_SPACE; break;
		case '\t' : keycode = AKEY_TAB; break;
		case '!' : keycode = AKEY_EXCLAMATION; break;
		case '"' : keycode = AKEY_DBLQUOTE; break;
		case '#' : keycode = AKEY_HASH; break;
		case '$' : keycode = AKEY_DOLLAR; break;
		case '%' : keycode = AKEY_PERCENT; break;
		case '&' : keycode = AKEY_AMPERSAND; break;
		case '\'' : keycode = AKEY_QUOTE; break;
		case '@' : keycode = AKEY_AT; break;
		case '(' : keycode = AKEY_PARENLEFT; break;
		case ')' : keycode = AKEY_PARENRIGHT; break;
		case '<' : keycode = AKEY_LESS; break;
		case '>' : keycode = AKEY_GREATER; break;
		case '=' : keycode = AKEY_EQUAL; break;
		case '?' : keycode = AKEY_QUESTION; break;
		case '-' : keycode = AKEY_MINUS; break;
		case '+' : keycode = AKEY_PLUS; break;
		case '*' : keycode = AKEY_ASTERISK; break;
		case '/' : keycode = AKEY_SLASH; break;
		case ':' : keycode = AKEY_COLON; break;
		case ';' : keycode = AKEY_SEMICOLON; break;
		case ',' : keycode = AKEY_COMMA; break;
		case '.' : keycode = AKEY_FULLSTOP; break;
		case '_' : keycode = AKEY_UNDERSCORE; break;
		case '[' : keycode = AKEY_BRACKETLEFT; break;
		case ']' : keycode = AKEY_BRACKETRIGHT; 	break;
		case '^' : keycode = AKEY_CIRCUMFLEX; break;
 		case '\\' : keycode = AKEY_BACKSLASH; break;
 		case '|' : keycode = AKEY_BAR; break;
		default : keycode = AKEY_NONE; break;
	}

	return keycode;
}

/**************************************************************************
 Handle the Rawkey events
**************************************************************************/
int HandleRawkey( UWORD code, UWORD qual, APTR iaddress )
{
	int keycode = -1;

	if ((qual & IEQUALIFIER_NUMERICPAD) && (PortSource[0] == PORT_SOURCE_NUMERIC || PortSource[1] == PORT_SOURCE_NUMERIC))
	{
		/* TODO: This needs to be improved when pressing multiple keys */
		static int matrix[8];

		int i, before_pressed = 0, pressed = 0;
		int local_stick = -1;

		before_pressed = 0;
		for (i=0;i<8;i++)
			if (matrix[i]) before_pressed = 1;

		/* key pressed */
		if (!(code & 0x80))
		{
			switch (code)
			{
				case	29: matrix[0] = 1; local_stick = 9; break;
				case	30: matrix[1] = 1; local_stick = 13; break;
				case	31: matrix[2] = 1; local_stick = 5; break;
				case	45: matrix[3] = 1; local_stick = 11; break;
				case	47: matrix[4] = 1; local_stick = 7; break;
				case	61: matrix[5] = 1; local_stick = 10; break;
				case	62: matrix[6] = 1; local_stick = 14; break;
				case	63: matrix[7] = 1; local_stick = 6; break;

				case	46:	/* 5 in the middle */
						if (PortSource[0] == PORT_SOURCE_NUMERIC) trig[0] = 0;
						if (PortSource[1] == PORT_SOURCE_NUMERIC) trig[1] = 0;
						return -1;
			}
		} else
		{
			switch (code&0x7f)
			{
				case	29: matrix[0] = 0; break;
				case	30: matrix[1] = 0; break;
				case	31: matrix[2] = 0; break;
				case	45: matrix[3] = 0; break;
				case	47: matrix[4] = 0; break;
				case	61: matrix[5] = 0; break;
				case	62: matrix[6] = 0; break;
				case	63: matrix[7] = 0; break;
				case	46:	/* 5 in the middle */
						if (PortSource[0] == PORT_SOURCE_NUMERIC) trig[0] = 1;
						if (PortSource[1] == PORT_SOURCE_NUMERIC) trig[1] = 1;
						return -1;
			}
		}

		pressed = 0;
		for (i=0;i<8;i++)
			if (matrix[i]) pressed = 1;

		if (local_stick != -1 || (before_pressed && !pressed))
		{
			if (!pressed) local_stick = 15;
			if (PortSource[0] == PORT_SOURCE_NUMERIC) stick[0] = local_stick;
			if (PortSource[1] == PORT_SOURCE_NUMERIC) stick[1] = local_stick;
			return -1;
		}
	}

	switch (code)
	{
		case	0x51: keyboard_consol &= 0x03; keycode = AKEY_NONE; break; /* F2 pressed */
		case	0xd1: keyboard_consol |= 0x04; keycode = AKEY_NONE; break; /* F2 released */
		case	0x52: keyboard_consol &= 0x05; keycode = AKEY_NONE; break; /* F3 pressed */
		case	0xd2: keyboard_consol |= 0x02; keycode = AKEY_NONE; break; /* F3 released */
		case	0x53: keyboard_consol &= 0x06; keycode = AKEY_NONE; break; /* F4 pressed */
		case	0xd3: keyboard_consol |= 0x01; keycode = AKEY_NONE; break; /* F4 released */
		case	0x56: keycode = AKEY_BREAK; break; /* F7 */
		case	0x59: keycode = AKEY_NONE; break;
		case	0x5f: keycode = AKEY_HELP; 	break;

		case	0x54:	/* F5 */
					if (qual & (IEQUALIFIER_RSHIFT | IEQUALIFIER_RSHIFT)) keycode = AKEY_COLDSTART;
					else keycode = AKEY_WARMSTART;
					keyboard_consol = 7;
					break;

		case	CURSORLEFT: /* Cursor Left */
				if (qual & IEQUALIFIER_CONTROL ||
				   (PortSource[0] != PORT_SOURCE_CURSOR && PortSource[1] != PORT_SOURCE_CURSOR))
				{
					keycode = AKEY_LEFT;
				} else
				{
					if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] &= ~4;
					if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] &= ~4;
				}
		 		break;
		case	CURSORLEFT|0x80:
				if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] |= 4;
				if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] |= 4;
		 		break;

		case	CURSORRIGHT: /* Cursor Right */
				if (qual & IEQUALIFIER_CONTROL ||
				   (PortSource[0] != PORT_SOURCE_CURSOR && PortSource[1] != PORT_SOURCE_CURSOR))
				{
					keycode = AKEY_RIGHT;
				} else
				{
					if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] &= ~8;
					if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] &= ~8;
				}
		 		break;
		case	CURSORRIGHT|0x80:
				if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] |= 8;
				if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] |= 8;
		 		break;

		case	CURSORUP: /* Cursor Up */
				if (qual & IEQUALIFIER_CONTROL ||
				   (PortSource[0] != PORT_SOURCE_CURSOR && PortSource[1] != PORT_SOURCE_CURSOR))
				{
					keycode = AKEY_UP;
				} else
				{
					if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] &= ~1;
					if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] &= ~1;
				}
		 		break;
		case	CURSORUP|0x80:
				if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] |= 1;
				if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] |= 1;
		 		break;

		case	CURSORDOWN: /* Cursor Down */
				if (qual & IEQUALIFIER_CONTROL ||
				   (PortSource[0] != PORT_SOURCE_CURSOR && PortSource[1] != PORT_SOURCE_CURSOR))
				{
					keycode = AKEY_DOWN;
				} else
				{
					if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] &= ~2;
					if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] &= ~2;
				}
		 		break;
		case	CURSORDOWN|0x80:
				if (PortSource[0] == PORT_SOURCE_CURSOR) stick[0] |= 2;
				if (PortSource[1] == PORT_SOURCE_CURSOR) stick[1] |= 2;
		 		break;

		case	101: /* R ALT */
				if (PortSource[0] == PORT_SOURCE_CURSOR) trig[0] = 0;
				if (PortSource[1] == PORT_SOURCE_CURSOR) trig[1] = 0;
				break;

		case	101|0x80: /* R ALT - released */
				if (PortSource[0] == PORT_SOURCE_CURSOR) trig[0] = 1;
				if (PortSource[1] == PORT_SOURCE_CURSOR) trig[1] = 1;
				break;

		default:
					{
						struct InputEvent ev;
						char key;

						ev.ie_Class = IECLASS_RAWKEY;
						ev.ie_SubClass = 0;
						ev.ie_Code = code;
						ev.ie_Qualifier = qual;
						ev.ie_EventAddress = (APTR) *((ULONG *) iaddress);
						ev.ie_NextEvent = NULL;

						if (MapRawKey( &ev, &key, 1, NULL ))
						{
							keycode = HandleVanillakey(key);
						}
					}
					break;
	}
	return keycode;
}

/**************************************************************************
 Play the sound. Called by the emulator.
**************************************************************************/
void Sound_Update(void)
{
	static int sent[2] = {0,0};

	if (SoundEnabled)
	{
		if (sent[ahi_current])
			WaitIO(((struct IORequest*)ahi_soundreq[ahi_current]));

		POKEYSND_Process(ahi_streambuf[ahi_current], ahi_streamlen);

		*ahi_soundreq[ahi_current] = *ahi_request;
		ahi_soundreq[ahi_current]->ahir_Std.io_Data = ahi_streambuf[ahi_current];
		ahi_soundreq[ahi_current]->ahir_Std.io_Length = ahi_streamlen;
		ahi_soundreq[ahi_current]->ahir_Std.io_Offset = 0;

		if (ahi_current && sent[0])
			ahi_soundreq[1]->ahir_Link = ahi_soundreq[0];
		else if (!ahi_current && sent[1])
			ahi_soundreq[0]->ahir_Link = ahi_soundreq[1];

		SendIO((struct IORequest*)ahi_soundreq[ahi_current]);
		sent[ahi_current] = 1;
		ahi_current = !ahi_current;
	}
}


/**************************************************************************
 Initialize the Atari Prefs to sensible values
**************************************************************************/
void Atari_ConfigInit(void)
{
	int i;

	strcpy(CFG_osa_filename, "PROGDIR:ROMs/atariosa.rom");
	strcpy(CFG_osb_filename, "PROGDIR:ROMs/atariosb.rom");
	strcpy(CFG_xlxe_filename, "PROGDIR:ROMs/atarixl.rom");
	strcpy(CFG_basic_filename, "PROGDIR:ROMs/ataribas.rom");
	CFG_5200_filename[0] = '\0';

	for (i=0;i<UI_MAX_DIRECTORIES;i++)
		strcpy(atari_disk_dirs[i], "PROGDIR:Disks");

	disk_directories = 1;
	strcpy(atari_rom_dir,"PROGDIR:Cartriges");
	atari_h1_dir[0] = '\0';
	atari_h2_dir[0] = '\0';
	atari_h3_dir[0] = '\0';
	atari_h4_dir[0] = '\0';
	strcpy(atari_exe_dir,"PROGDIR:EXEs");
	strcpy(atari_state_dir,"PROGDIR:States");
}

/**************************************************************************
 Set additional Amiga config parameters
**************************************************************************/
int PLATFORM_Configure(char* option, char *parameters)
{
	int i;

	if(!strcmp(option,"AMIGA_GFX_DISPLAYID")) sscanf(parameters, "%lx",&DisplayID);
	else if(!strcmp(option, "AMIGA_GFX_DISPLAYTYPE"))
	{
		for (i=0;display_text[i];i++)
			if (!strcmp(display_text[i],parameters)) DisplayType = i;
	}
	else if(!strcmp(option,"AMIGA_CONTROLLER0_SOURCE"))
	{
		for (i=0;port_source_text[i];i++)
			if (!strcmp(port_source_text[i],parameters)) PortSource[0] = i;
	}
	else if(!strcmp(option,"AMIGA_CONTROLLER1_SOURCE"))
	{
		for (i=0;port_source_text[i];i++)
			if (!strcmp(port_source_text[i],parameters)) PortSource[1] = i;
	}
	else if(!strcmp(option,"AMIGA_SOUND"))
	{
		if(!strcmp(parameters,"AHI")) SoundEnabled=TRUE;
		else SoundEnabled=FALSE;
	}	else return FALSE;
	return TRUE;
}

/**************************************************************************
 Save additional Amiga config parameters
**************************************************************************/
void PLATFORM_ConfigSave(FILE *fp)
{
	int i;

	fprintf(fp,"AMIGA_GFX_DISPLAYID=0x%lx\n",DisplayID);
	fprintf(fp,"AMIGA_GFX_DISPLAYTYPE=%s\n",display_text[DisplayType]);

	fputs("AMIGA_SOUND=",fp);
	if(SoundEnabled) fputs("AHI\n",fp);
	else fputs("NO\n",fp);

	for (i=0;i<2;i++)
	{
		fprintf(fp,"AMIGA_CONTROLLER%d_SOURCE=%s\n",i,port_source_text[PortSource[i]]);
	}
}


/**************************************************************************
 Exit the emulator
**************************************************************************/
int PLATFORM_Exit (int run_monitor)
{
	if (run_monitor)
	{
		if (MONITOR_Run ())
		{
			return TRUE;
		}
	}

	FreeDisplay();
	if (app_port) DeleteMsgPort(app_port);
	FreeTimer();
	FreeSound();
	SetJoyPortAttrs(0,SJA_Type, SJA_TYPE_AUTOSENSE, NULL);

	if (DiskFileReq) FreeAslRequest(DiskFileReq);
	if (StateFileReq) FreeAslRequest(StateFileReq);
	if (BinFileReq) FreeAslRequest(BinFileReq);

	CloseLibraries();
	exit(0);
}

/**************************************************************************
 Initialize the Amiga specific part of the Atari800 Emulator.
**************************************************************************/
void PLATFORM_Initialise (int *argc, unsigned char **argv)
{
	PaddlePos = 228;

	if (OpenLibraries())
	{
		if((DiskFileReq = AllocAslRequestTags( ASL_FileRequest,
											ASLFR_DoPatterns, TRUE,
											ASLFR_InitialPattern,"#?.(atr|xfd)",
											ASLFR_InitialDrawer,atari_disk_dirs[0],
											TAG_DONE )))
		{
			if((StateFileReq = AllocAslRequestTags( ASL_FileRequest,
										ASLFR_DoPatterns, TRUE,
										ASLFR_InitialPattern,"#?.sav",
										ASLFR_InitialDrawer,atari_state_dir,
										TAG_DONE )))
			{
				if((BinFileReq = AllocAslRequestTags( ASL_FileRequest,
										ASLFR_DoPatterns, TRUE,
										ASLFR_InitialPattern,"#?.(bin|xex)",
										ASLFR_InitialDrawer,atari_exe_dir,
										TAG_DONE )))
				{
					SetupSound();

					if (SetupTimer())
					{
						if ((app_port = CreateMsgPort()))
						{
							if (SetupDisplay())
							{
								trig[0] = trig[1] = 1;
								stick[0] = stick[1] = 15;
								menu_consol = 7;
								keyboard_consol = 7;
								return;
							}
						}
					}
				}
			}
		}
	}
	PLATFORM_Exit(0);
}

/**************************************************************************
 Convert src to dest with Colours_table
**************************************************************************/
static void ScreenData28bit(UBYTE *src, UBYTE *dest, UBYTE *pentable, ULONG width, ULONG height)
{
	int x,y;

	for (y=0;y<height;y++)
	{
		for (x=0;x<width;x++)
		{
			*dest++ = pentable[*src++];
		}
	}
}

/**************************************************************************

**************************************************************************/
static void Scale8Bit(UBYTE *src, LONG srcwidth, LONG srcheight, LONG srcmod,
					  struct RastPort *destrp, LONG destx, LONG desty, LONG destwidth, LONG destheight)
{
	int x;
	int dx;
	int i;
	int y;
	int w, h;
	int count;
	int dy;
	UBYTE *ss;
	ULONG quad;
	ULONG *dest32;
	static UBYTE lineBuffer[4096];

	if ((srcwidth % 4) || (srcwidth > destwidth) || srcheight > destheight || destwidth > (sizeof(lineBuffer)/sizeof(lineBuffer[0])))
		return;

	w = (srcwidth) << 16;
	h = (srcheight) << 16;
	dx = w / destwidth;
	dy = h / destheight;
	ss = src;
	y = (0) << 16;
	i = destheight;

	while (i > 0)
	{
		int lines, j;

		ss = src + srcmod * (y >> 16);
		dest32 = (ULONG*)lineBuffer;

		count = (destwidth + 3) /  4;
		x = 0 << 16;
		while (count > 0)
		{
			quad = (ss[x >> 16] << 24);
			x = x + dx;
			quad += (ss[x >> 16] << 16);
			x = x + dx;
			quad += (ss[x >> 16] << 8);
			x = x + dx;
			quad += (ss[x >> 16] << 0);
			x = x + dx;

			*dest32++ = quad;
			count--;
		}

		lines = (0x1ffff - (y & 0xffff))/dy;

#if 1
		WriteLUTPixelArray(lineBuffer, 0, 0, destwidth, WindowMain->RPort, Colours_table, destx, desty + destheight-i, destwidth, 1, CTABFMT_XRGB8);
		for (j=1;j<lines;j++)
		{
			ClipBlit(WindowMain->RPort, destx, desty + destheight - i, WindowMain->RPort, destx, desty + destheight - i + j, destwidth, 1, 0xc0);
		}

#else
		if (lines == 1)
		{
			WriteLUTPixelArray(lineBuffer, 0, 0, destwidth, WindowMain->RPort, Colours_table, destx, desty + destheight-i, destwidth, 1, CTABFMT_XRGB8);
		} else
		{
			int j;

			WriteLUTPixelArray(lineBuffer, 0, 0, destwidth, &lineRastPort, Colours_table, 0, 0, destwidth, 1, CTABFMT_XRGB8);
			for (j=0;j<lines;j++)
			{
				BltBitMapRastPort(lineBitMap, 0, 0, WindowMain->RPort, destx, desty + destheight - i + j, destwidth, 1, 0xc0);
			}
		}
#endif

		y = y + dy * lines;
		i -= lines;
	}
}

/**************************************************************************
 Do the graphical output.
**************************************************************************/
void PLATFORM_DisplayScreen(void)
{
	UBYTE *screen = (UBYTE *) Screen_atari;
	static char fpsbuf[32];
	int fgpen = 15,bgpen = 0;

	if (DisplayType == DISPLAY_CUSTOMSCREEN)
	{
		WriteChunkyPixels(WindowMain->RPort, 0, 0, Screen_WIDTH - 1 - 64, Screen_HEIGHT - 1,
						  screen + 32, Screen_WIDTH);
	} else
	{
		LONG atariWidth = Screen_WIDTH - 56;
		LONG atariHeight = Screen_HEIGHT;
		LONG atariX = 28;
		LONG atariY = 0;
		LONG offx = WindowMain->BorderLeft;
		LONG offy = WindowMain->BorderTop;

		if (InnerWidth(WindowMain) != atariWidth || InnerHeight(WindowMain) != atariHeight)
		{
			if (pensallocated)
			{
				ScreenData28bit(screen, tempscreendata, pentable, Screen_WIDTH, Screen_HEIGHT);
				ScalePixelArray( tempscreendata + atariX, atariWidth, atariHeight, Screen_WIDTH, WindowMain->RPort, offx, offy,
										InnerWidth(WindowMain), InnerHeight(WindowMain), RECTFMT_LUT8);
				fgpen = pentable[fgpen];
				bgpen = pentable[bgpen];
			} else
			{
				if (EnsureScaledDisplay(InnerWidth(WindowMain)))
				{
					Scale8Bit(screen + atariX, atariWidth, atariHeight, Screen_WIDTH,
					  WindowMain->RPort, offx, offy, InnerWidth(WindowMain), InnerHeight(WindowMain));
				}
			}
		}	else
		{
			if (pensallocated)
			{
				ScreenData28bit(screen, tempscreendata, pentable, Screen_WIDTH, Screen_HEIGHT);
				WriteChunkyPixels(WindowMain->RPort, offx, offy, offx + atariWidth - 1, offy + atariHeight - 1,
							  tempscreendata + atariX, Screen_WIDTH);

				fgpen = pentable[fgpen];
				bgpen = pentable[bgpen];
			} else
			{
				WriteLUTPixelArray(screen, atariX, atariY, Screen_WIDTH, WindowMain->RPort, Colours_table, offx, offy, atariWidth, atariHeight, CTABFMT_XRGB8);
			}
		}
	}

	if (ShowFPS)
	{
		LONG len;

		extern double fps;
		double fpsv;
		double fpsn = modf(fps, &fpsv);

		snprintf(fpsbuf,sizeof(fpsbuf),"%d.%d/%d (%d%%)",(int)fpsv,(int)(fpsn*10),ahi_fps,(int)(100*fps/ahi_fps));

		SetABPenDrMd(WindowMain->RPort,fgpen,bgpen,JAM2);
		len = TextLength(WindowMain->RPort,fpsbuf,strlen(fpsbuf));
		Move(WindowMain->RPort, WindowMain->Width - 4 - len - WindowMain->BorderRight, 4 + WindowMain->RPort->TxBaseline + WindowMain->BorderTop);
		Text(WindowMain->RPort, fpsbuf,strlen(fpsbuf));
	}

}

/**************************************************************************
 Handle Keyboard
**************************************************************************/
int PLATFORM_Keyboard (void)
{
	static int old_keycode = -1;

	int keycode = -1;
	int iconify = 0;

	struct IntuiMessage *imsg;

	while ((imsg = (struct IntuiMessage*) GetMsg (WindowMain->UserPort)))
	{
		ULONG cl = imsg->Class;
		UWORD code = imsg->Code;
		UWORD qual = imsg->Qualifier;
		APTR iaddress = imsg->IAddress;
//		WORD mx = imsg->MouseX;
		/*WORD my = imsg->MouseY;*/

		if (cl == IDCMP_MENUVERIFY)
		{
			ReplyMsg((struct Message*)imsg);
			continue;
		}

		if (cl == IDCMP_SIZEVERIFY)
		{
			SizeVerify = TRUE;
			ReplyMsg((struct Message*)imsg);
			continue;
		}

		ReplyMsg((struct Message*)imsg);

		switch (cl)
		{
			case	IDCMP_NEWSIZE: SizeVerify = FALSE; break;
			case	IDCMP_RAWKEY: keycode = HandleRawkey(code,qual,iaddress); break;
			case	IDCMP_CLOSEWINDOW: keycode = AKEY_EXIT; break;
			case	IDCMP_MENUPICK:
					keycode = HandleMenu(code);
					if (keycode == -2)
					{
						iconify = 1;
						keycode = -1;
					}
					break;
			case	IDCMP_GADGETUP: iconify = 1; break;

			case	IDCMP_MOUSEBUTTONS :
#if 0
					if (Controller == controller_Paddle)
					{
						switch (code)
						{
							case	SELECTDOWN : stick0 = 251; break;
							case	SELECTUP : stick0 = 255; break;
							default: break;
						}
					}
#endif
					break;

			case	IDCMP_MOUSEMOVE :
#if 0
					if (Controller == controller_Paddle)
					{
						if (mx > 57)
						{
							if (mx < 287) PaddlePos = 228 - (mx - 58);
							else PaddlePos = 0;
						}
						else
						{
							PaddlePos = 228;
						}
					}
#endif
					break;

			default: break;
		}

		old_keycode = keycode;
	}

	if (keycode == -1) keycode = old_keycode;

	if (menu_consol != 7)
	{
		INPUT_key_consol = menu_consol;
		menu_consol = 7;
	} else INPUT_key_consol = keyboard_consol;

	if (app_window)
	{
		/* handle application messages (e.g. icons drops on window */
		struct AppMessage *amsg;

		while ((amsg = (struct AppMessage *)GetMsg(app_port)))
		{
			if (amsg->am_Type == AMTYPE_APPWINDOW && amsg->am_ID == 1 && amsg->am_NumArgs > 0)
			{
				BPTR olddir;

				olddir = CurrentDir(amsg->am_ArgList[0].wa_Lock);

				if (Atari_LoadAnyFile(amsg->am_ArgList[0].wa_Name))
				{
					ActivateWindow(WindowMain);
				}
				CurrentDir(olddir);
			}
			ReplyMsg((struct Message*)amsg);
		}
	}

	if (iconify)
	{
		Iconify();
		keycode = -1;
	}

	return keycode;
}

/**************************************************************************
 Handle Joystick Port (direction)
**************************************************************************/
int PLATFORM_PORT (ULONG num)
{

    ULONG JoyStickState;

    if (PortSource[num] == PORT_SOURCE_GAMEPORT)
	{
	JoyStickState = ReadJoyPort(num);//==0? 1 : 0); // Swap Joystick Ports

	if (JoyStickState & JPF_JOY_LEFT) {
		if (JoyStickState & JPF_JOY_UP) return INPUT_STICK_UL;
			else if (JoyStickState & JPF_JOY_DOWN) return INPUT_STICK_LL;
		else return INPUT_STICK_LEFT;}

	if (JoyStickState & JPF_JOY_RIGHT) {
		if (JoyStickState & JPF_JOY_UP) return INPUT_STICK_UR;
			else if (JoyStickState & JPF_JOY_DOWN) return INPUT_STICK_LR;
		else return INPUT_STICK_RIGHT;}

	if (JoyStickState & JPF_JOY_UP)   return INPUT_STICK_FORWARD;
	if (JoyStickState & JPF_JOY_DOWN) return INPUT_STICK_BACK;
	}
    return stick[num];

}



/**************************************************************************
 Handle Joystick Triggers
**************************************************************************/


static void get_platform_TRIG(UBYTE *t0, UBYTE *t1)
{

	ULONG JoyStickState;

	int trig0, trig1;
	trig0 = trig1 = 1;

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
	else {JoyStickState = ReadJoyPort(0);
		if (JoyStickState != 0) {
		trig0 = 1;

        if (JoyStickState & JPF_BUTTON_RED) trig0 = 0;
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
	else {
	JoyStickState = ReadJoyPort(1);
		if (JoyStickState != 0) {
		trig1 = 1;

    if (JoyStickState & JPF_BUTTON_RED) trig1 = 0;
		}
		*t1 = trig1;
	}
}



int PLATFORM_TRIG(int num)
{

#ifndef DONT_DISPLAY
	UBYTE a, b;
	get_platform_TRIG(&a, &b);
	switch (num) { // Swapped ports on the Amiga 
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

/**************************************************************************
 ...
**************************************************************************/
void Sound_Pause(void)
{
	if (SoundEnabled)
	{
	SoundEnabled=0;
	}
}

/**************************************************************************
 ...
**************************************************************************/
void Sound_Continue(void)
{
	if (!SoundEnabled)
	{
	SoundEnabled=1;
	}
}

/**************************************************************************
 Insert a disk into the drive
**************************************************************************/
LONG InsertDisk( LONG Drive )
{
	char Filename[256];
	int Success = FALSE;

	if (AslRequestTags( DiskFileReq,
					ASLFR_Screen, ScreenMain,
					TAG_DONE))
	{
		SIO_Dismount( Drive );

		strcpy( Filename, DiskFileReq->fr_Drawer );
		if (AddPart( Filename, DiskFileReq->fr_File,255) != DOSFALSE )
		{
			if (SIO_Mount (Drive, Filename, 0)) /* last parameter is read only */
			{
				Success = TRUE;
			}
		}
	}

	return Success;
}

/**************************************************************************
 Insert a ROM (not improved yet)
**************************************************************************/
LONG InsertROM(LONG CartType)
{
	struct FileRequester *FileRequester = NULL;
	char Filename[256];
	int Success = FALSE;

	if ((FileRequester = (struct FileRequester*) AllocAslRequestTags (ASL_FileRequest, TAG_DONE)))
	{
		if (AslRequestTags (FileRequester,
			ASLFR_Screen, ScreenMain,
			TAG_DONE))
		{
			if (FileRequester->fr_Drawer) Strlcpy(Filename, FileRequester->fr_Drawer, sizeof(Filename));
			else Filename[0] = 0;
			AddPart(Filename, FileRequester->fr_File, sizeof(Filename));

			if (CartType == 0)
			{
/*				Remove_ROM ();
				if (Insert_8K_ROM(Filename))
				{
					Atari800_Coldstart ();
				}*/
			}
			else if (CartType == 1)
			{
/*				Remove_ROM ();
				if (Insert_16K_ROM(Filename))
				{
					Atari800_Coldstart ();
				}*/
			}
			else if (CartType == 2)
			{
/*				Remove_ROM ();
				if (Insert_OSS_ROM(Filename))
				{
					Atari800_Coldstart ();
				}*/
			}
		}
		else
		{
			/*
			 * Cancelled
			 */
		}
	}
	else
	{
		printf ("Unable to create requester\n");
	}

	return Success;
}

char program_name[256];

/**************************************************************************
 main
**************************************************************************/
int main(int argc, char **argv)
{
	struct WBStartup *wbs = NULL;

	if (!argc)
	{
		/* started from workbench */
		if ((wbs = (struct WBStartup*)argv))
		{
			strncpy(program_name, wbs->sm_ArgList->wa_Name, sizeof(program_name)-1);
			program_name[sizeof(program_name)-1]= 0;
		}
	} else
	{
		/* started from shell */
		strncpy(program_name,argv[0],sizeof(program_name)-1);
		program_name[sizeof(program_name)-1]=0;
	}

	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 20;

	if (wbs && wbs->sm_NumArgs > 1)
	{
		BPTR odir = CurrentDir(wbs->sm_ArgList[1].wa_Lock);
		Atari_LoadAnyFile(wbs->sm_ArgList[1].wa_Name);
		CurrentDir(odir);
	}

	for (;;)
	{
		LONG LastDisplayType = DisplayType;

		INPUT_key_code = PLATFORM_Keyboard();

		if (LastDisplayType != DisplayType)
		{
			FreeDisplay();
			if (!(SetupDisplay()))
				break;
		}

		Atari800_Frame();

		if (Atari800_display_screen && !SizeVerify)
			PLATFORM_DisplayScreen();
	}

	PLATFORM_Exit(0);
	return 0;
}
