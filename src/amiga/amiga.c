/*
 * amiga.c - Amiga specific port code
 *
 * Copyright (c) 2000 Sebastian Bauer
 * Copyright (c) 2000-2003 Atari800 development team (see DOC/CREDITS)
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
#include "rt-config.h"

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
#include <devices/gameport.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <workbench/workbench.h>

#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/gadtools.h>
#include <proto/timer.h>
#include <proto/keymap.h>
#include <proto/utility.h>

#include <proto/ahi.h>
#include <proto/cybergraphics.h>

#include "config.h"

#include "binload.h"
#include "colours.h"
#include "input.h"
#include "monitor.h"
#include "pokeysnd.h"
#include "sio.h"
#include "statesav.h"
#include "rt-config.h"

#include "amiga.h"
#include "support.h"


/******************************/

struct timezone;
/******************************/

extern int refresh_rate;

#define GAMEPORT0 0
#define GAMEPORT1 1

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

struct IntuitionIFace *IIntuition;
struct GraphicsIFace *IGraphics;
struct LayersIFace *ILayers;
struct TimerIFace *ITimer;
struct AslIFace *IAsl;
struct GadToolsIFace *IGadTools;
struct CyberGfxIFace *ICyberGfx;
struct KeymapIFace *IKeymap;
struct UtilityIFace *IUtility;

/* Gameport */
static struct InputEvent gameport_data;
static struct MsgPort *gameport_msg_port;
static struct IOStdReq *gameport_io_msg;
static BOOL gameport_error;

/* Timer */
static struct MsgPort *timer_msg_port;
static struct timerequest *timer_request;
static BOOL timer_error;

/* Sound */
static struct MsgPort *ahi_msg_port;
static struct AHIRequest *ahi_request;
static struct AHIRequest *ahi_soundreq[2];
static STRPTR ahi_soundpath;
static BPTR ahi_soundhandle;			/* FileHandle for Soundoutput */
static BOOL ahi_current;

static UWORD ahi_fps;                  /* frames per second */
static UWORD ahi_rupf;                 /* real updates per frame */
static BYTE *ahi_streambuf[2];
static ULONG ahi_streamfreq = 44100;   /* playing frequence */
static ULONG ahi_streamlen;

/* Emulation */
static int menu_consol;
static int keyboard_consol;
static int trig0;
static int stick0;

static LONG Overlay;
static LONG Scalable;
static LONG UseCustomScreen = TRUE;
static LONG UseBestID = TRUE;
static LONG SoundEnabled = TRUE;
static LONG ShowFPS;
static ULONG DisplayID = INVALID_ID;

/* Emulator GUI */
static LONG ScreenIsCustom;
static struct Screen *ScreenMain;
static ULONG ScreenDepth;
static APTR VisualInfoMain;
static struct Window *WindowMain = NULL;
static struct Menu *MenuMain;

static UBYTE pentable[256];
static BOOL pensallocated;
static UBYTE *tempscreendata;

struct FileRequester *DiskFileReq;
struct FileRequester *CartFileReq;
struct FileRequester *StateFileReq;
struct FileRequester *BinFileReq;

static int Controller;
static int PaddlePos;

#define controller_Null 0
#define controller_Joystick 1
#define controller_Paddle 2

/**************************************************************************
 usleep() function
**************************************************************************/
void usleep(unsigned long usec)
{
	timer_request->tr_node.io_Command = TR_ADDREQUEST;
	timer_request->tr_time.tv_secs = 0;
	timer_request->tr_time.tv_micro = usec;

	DoIO((struct IORequest*)timer_request);
}

/**************************************************************************
 gettimeofday() function emulation
**************************************************************************/
int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
	if (TimerBase)
		GetSysTime(tp);
	return 0;
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
	{NM_ITEM, "Internal User Interface", "F1", NM_COMMANDSTRING, 0L, (APTR)MEN_SYSTEM_UI},
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
	{NM_ITEM, "Use Custom Screen?", NULL, MENUTOGGLE|CHECKIT, 0L, (APTR)MEN_SETTINGS_CUSTOMSCREEN},
	{NM_ITEM, "Show Framerate?", NULL, MENUTOGGLE|CHECKIT, 0L, (APTR)MEN_SETTINGS_FRAMERATE},
	{NM_ITEM, NM_BARLABEL, NULL,	0, 0L, NULL},
	{NM_ITEM, "Save Settings", NULL, 0, 0L, (APTR)MEN_SETTINGS_SAVE},
	{NM_END, NULL, NULL, 0, 0L, NULL}
};

/**************************************************************************
 Close all previously opened libraries
**************************************************************************/
VOID CloseLibraries(void)
{
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
								CyberGfxBase = OpenLibraryInterface("cybergraphics.library",41, &ICyberGfx);

								return TRUE;
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
 Close gameport.device
**************************************************************************/
VOID FreeJoystick(void)
{
	if (gameport_io_msg)
	{
		if( !gameport_error)
		{
			BYTE type = GPCT_NOCONTROLLER;

			AbortIO((struct IORequest*)gameport_io_msg);
			WaitIO((struct IORequest*)gameport_io_msg);

			gameport_io_msg->io_Command = GPD_SETCTYPE;
			gameport_io_msg->io_Length = 1;
			gameport_io_msg->io_Data = (APTR) &type;
			DoIO ((struct IORequest*)gameport_io_msg);

			CloseDevice((struct IORequest*)gameport_io_msg);
		}

		DeleteIORequest((struct IORequest*)gameport_io_msg);
		gameport_io_msg = NULL;
	}
	if( gameport_msg_port )
	{
		DeleteMsgPort(gameport_msg_port);
		gameport_msg_port = NULL;
	}
}

/**************************************************************************
 Open gameport.device
**************************************************************************/
BOOL SetupJoystick(void)
{
	if((gameport_msg_port = CreateMsgPort()))
	{
		gameport_io_msg = (struct IOStdReq *) CreateIORequest (gameport_msg_port, sizeof(struct IOStdReq));
		if( gameport_io_msg )
		{
			gameport_error = OpenDevice ("gameport.device", GAMEPORT1, (struct IORequest*)gameport_io_msg, 0xFFFF);
			if(!gameport_error)
			{
				BYTE type = 0;
				struct GamePortTrigger gpt;

				gameport_io_msg->io_Command = GPD_ASKCTYPE;
				gameport_io_msg->io_Length = 1;
				gameport_io_msg->io_Data = (APTR) &type;

				DoIO ((struct IORequest*)gameport_io_msg);

				if (type == (BYTE)GPCT_ALLOCATED)
				{
					printf("gameport already in use\n");
					CloseDevice((struct IORequest*)gameport_io_msg);
					gameport_error = TRUE;
					FreeJoystick();
					return FALSE;
				}

				type = GPCT_ABSJOYSTICK;

				gameport_io_msg->io_Command = GPD_SETCTYPE;
				gameport_io_msg->io_Length = 1;
				gameport_io_msg->io_Data = &type;

				DoIO ((struct IORequest*)gameport_io_msg);

				if(gameport_io_msg->io_Error)
					printf ("Failed to set controller type\n");

				gpt.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
				gpt.gpt_Timeout = 0;
				gpt.gpt_XDelta = 1;
				gpt.gpt_YDelta = 1;

				gameport_io_msg->io_Command = GPD_SETTRIGGER;
				gameport_io_msg->io_Length = sizeof (gpt);
				gameport_io_msg->io_Data = (APTR) &gpt;

				DoIO ((struct IORequest*)gameport_io_msg);

				if(gameport_io_msg->io_Error)
					printf ("Failed to set controller trigger\n");

				gameport_io_msg->io_Command = GPD_READEVENT;
				gameport_io_msg->io_Length = sizeof (struct InputEvent);
				gameport_io_msg->io_Data = (APTR) &gameport_data;
				gameport_io_msg->io_Flags = 0;

				SendIO ((struct IORequest*)gameport_io_msg);
				return TRUE;
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
	if(tv_mode== TV_PAL)
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
				Pokey_sound_init(FREQ_17_EXACT, ahi_streamfreq, 2, 0);
#else
				ahi_streamlen = ahi_streamfreq/ahi_fps;
				Pokey_sound_init(FREQ_17_EXACT, ahi_streamfreq, 1, 0);
#endif
				if((ahi_streambuf[0] = (STRPTR)AllocVec(ahi_streamlen*3, MEMF_PUBLIC|MEMF_CLEAR)))
				{
					if((ahi_streambuf[1] = (STRPTR)AllocVec(ahi_streamlen*3, MEMF_PUBLIC|MEMF_CLEAR)))
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
 Closes RecordSoundfile handle
**************************************************************************/
VOID FreeSoundFile(void)
{
	if (ahi_soundhandle)
	{
		Close(ahi_soundhandle);
		ahi_soundhandle = ZERO;
	}
}

/**************************************************************************
 Opens the Recordsoundfile handle
**************************************************************************/
VOID SetupSoundFile(void)
{
	FreeSoundFile();

	if (ahi_soundpath)
	{
		ahi_soundhandle = Open(ahi_soundpath, MODE_NEWFILE);
	}
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
		timer_request = (struct timerequest*)CreateIORequest( timer_msg_port, sizeof( struct timerequest ));
		if( timer_request )
		{
			timer_error = OpenDevice( TIMERNAME, UNIT_MICROHZ, (struct IORequest*)timer_request, 0);
			if( !timer_error )
			{
				TimerBase = timer_request->tr_node.io_Device;
				if ((ITimer = (struct TimerIFace*)GetInterface((struct Library*)TimerBase,"main",1,NULL)))
					return TRUE;
			}
		}
	}
	return FALSE;
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
							easy.es_TextFormat = ATARI_TITLE "\n\nCopyright (C) 2000-2004\nAtari800 development team\nAmiga port done by Sebastian Bauer";
							easy.es_GadgetFormat = "Ok";
							
							EasyRequestArgs( WindowMain, &easy, NULL, NULL );
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
									ReadAtariState(StateFileReq->fr_File,"rb");
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
										SaveAtariState(fileName,"wb",TRUE);
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
									BIN_loader(BinFileReq->fr_File);
									CurrentDir(odir);
									UnLock(cd);
								}
							}
							break;

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
							keycode = AKEY_WARMSTART;
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
							keycode = AKEY_COLDSTART;
							menu_consol = 7;
							break;

				case	MEN_SETTINGS_CUSTOMSCREEN:
						UseCustomScreen = !!(mi->Flags & CHECKED);
						break;

				case	MEN_SETTINGS_FRAMERATE:
						ShowFPS = !!(mi->Flags & CHECKED);
						break;

				case	MEN_SETTINGS_SAVE:
						RtConfigSave();
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

	switch (code)
	{
		case	0x50: keycode = AKEY_UI; break; /* F1 */
		case	0x51: keyboard_consol &= 0x03; keycode = AKEY_NONE; break; /* F2 pressed */
		case	0xd1: keyboard_consol |= 0x04; keycode = AKEY_NONE; break; /* F2 released */
		case	0x52: keyboard_consol &= 0x05; keycode = AKEY_NONE; break; /* F3 pressed */
		case	0xd2: keyboard_consol |= 0x02; keycode = AKEY_NONE; break; /* F3 released */
		case	0x53: keyboard_consol &= 0x06; keycode = AKEY_NONE; break; /* F4 pressed */
		case	0xd3: keyboard_consol |= 0x01; keycode = AKEY_NONE; break; /* F4 released */

		case	0x54:	/* F5 */
					if (qual & (IEQUALIFIER_RSHIFT | IEQUALIFIER_RSHIFT)) keycode = AKEY_COLDSTART;
					else keycode = AKEY_WARMSTART;
					keyboard_consol = 7;
					break;

		case	0x56: keycode = AKEY_BREAK; break; /* F7 */
		case	0x59: keycode = AKEY_NONE; break;
		case	0x5f: keycode = AKEY_HELP; 	break;
		case	CURSORLEFT: keycode = AKEY_LEFT; break;
		case	CURSORUP: keycode = AKEY_UP; break;
 		case	CURSORRIGHT: keycode = AKEY_RIGHT; break;
		case	CURSORDOWN: keycode = AKEY_DOWN; break;

		default:
					{
						struct InputEvent ev;
						UBYTE key;

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
 Called by the emulator.
**************************************************************************/
BOOL IsSoundFileOpen(void)
{
	return FALSE;
}

/**************************************************************************
 Called by the emulator.
**************************************************************************/
void WriteToSoundFile(void)
{
}

/**************************************************************************
 Called by the emulator.
**************************************************************************/
void OpenSoundFile(void)
{
}

/**************************************************************************
 Called by the emulator.
**************************************************************************/
void CloseSoundFile(void)
{
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

		Pokey_process(ahi_streambuf[ahi_current], ahi_streamlen);
	
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
	
		if (ahi_soundhandle)
		{
			Write(ahi_soundhandle, ahi_streambuf[ahi_current], ahi_streamlen);
		}
	
		ahi_current = !ahi_current;
	}
}


/**************************************************************************
 Initialize the Atari Prefs to sensible values
**************************************************************************/
void Atari_ConfigInit(void)
{
	int i;

	strcpy(atari_osa_filename, "PROGDIR:atariosa.rom");
	strcpy(atari_osb_filename, "PROGDIR:atariosb.rom");
	strcpy(atari_xlxe_filename, "PROGDIR:atarixl.rom");
	strcpy(atari_basic_filename, "PROGDIR:ataribas.rom");
	atari_5200_filename[0] = '\0';

	for (i=0;i<MAX_DIRECTORIES;i++)
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
int Atari_Configure(char* option, char *parameters)
{
	if(!strcmp(option,"AMIGA_GFX_USEBESTMODE")) sscanf(parameters,"%d",&UseBestID);
	else if(!strcmp(option,"AMIGA_GFX_USEOVERLAY")) sscanf(parameters,"%d",&Overlay);
	else if(!strcmp(option,"AMIGA_GFX_USECUSTOMSCREEN")) sscanf(parameters,"%d",&UseCustomScreen);
	else if(!strcmp(option,"AMIGA_GFX_SCALABLE")) sscanf(parameters, "%d",&Scalable);
	else if(!strcmp(option,"AMIGA_GFX_DISPLAYID")) sscanf(parameters, "%x",&DisplayID);
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
void Atari_ConfigSave(FILE *fp)
{
	fprintf(fp,"AMIGA_GFX_DISPLAYID=0x%x\n",DisplayID);
	fprintf(fp,"AMIGA_GFX_USEBESTMODE=%d\n",UseBestID);
	fprintf(fp,"AMIGA_GFX_USECUSTOMSCREEN=%d\n",UseCustomScreen);
	fprintf(fp,"AMIGA_GFX_USEOVERLAY=%d\n",Overlay);
	fprintf(fp,"AMIGA_GFX_SCALABLE=%d\n",Scalable);

	fputs("AMIGA_SOUND=",fp);
	if(SoundEnabled) fputs("AHI\n",fp);
	else fputs("NO\n",fp);
}


/**************************************************************************
 Exit the emulator
**************************************************************************/
int Atari_Exit (int run_monitor)
{
	if (run_monitor)
	{
		if (monitor ())
		{
			return TRUE;
		}
	}

	FreeDisplay();
	FreeTimer();
	FreeSoundFile();
	FreeSound();
	FreeJoystick();

	if (DiskFileReq) FreeAslRequest(DiskFileReq);
	if (StateFileReq) FreeAslRequest(StateFileReq);
	if (BinFileReq) FreeAslRequest(BinFileReq);

	CloseLibraries();
	exit(0);
}

/**************************************************************************
 Initialize the Amiga specific part of the Atari800 Emulator.
**************************************************************************/
void Atari_Initialise (int *argc, unsigned char **argv)
{
	Controller = controller_Joystick;
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
										ASLFR_InitialPattern,"#?.(bin|exe)",
										ASLFR_InitialDrawer,atari_exe_dir,
										TAG_DONE )))
				{
					SetupJoystick();
					SetupSound();

					if (SetupTimer())
					{
						if (SetupDisplay())
						{
							trig0 = 1;
							stick0 = 15;
							menu_consol = 7;
							keyboard_consol = 7;
						}
					}
				}
			}
		}
	}
}

/**************************************************************************
 Convert src to dest with colortable
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
 Do the graphical output (also call the sound play routine)
**************************************************************************/
void Atari_DisplayScreen(UBYTE *screen)
{
	static char fpsbuf[32];
	int fgpen = 15,bgpen = 0;

	if (UseCustomScreen)
	{
		WriteChunkyPixels(WindowMain->RPort, 0, 0, ATARI_WIDTH - 1 - 64, ATARI_HEIGHT - 1,
						  screen + 32, ATARI_WIDTH);
	} else
	{
		LONG offx = WindowMain->BorderLeft;
		LONG offy = WindowMain->BorderTop;

		if (Scalable && (InnerWidth(WindowMain)!=ATARI_WIDTH || InnerHeight(WindowMain) != ATARI_HEIGHT))
		{
			ScreenData28bit(screen, tempscreendata, pentable, ATARI_WIDTH, ATARI_HEIGHT);

			ScalePixelArray( tempscreendata, ATARI_WIDTH, ATARI_HEIGHT, ATARI_WIDTH, WindowMain->RPort, offx, offy,
										InnerWidth(WindowMain), InnerHeight(WindowMain), RECTFMT_LUT8);
		}	else
		{
			if (pensallocated)
			{
				ScreenData28bit(screen, tempscreendata, pentable, ATARI_WIDTH, ATARI_HEIGHT);
				WriteChunkyPixels(WindowMain->RPort, offx, offy, offx + ATARI_WIDTH - 1 - 56, offy + ATARI_HEIGHT - 1,
							  tempscreendata + 28, ATARI_WIDTH);
			} else
			{
				WriteLUTPixelArray(screen, 28, 0, ATARI_WIDTH, WindowMain->RPort, colortable, offx, offy, ATARI_WIDTH-56, ATARI_HEIGHT, CTABFMT_XRGB8);
			}
		}
		fgpen = pentable[fgpen];
		bgpen = pentable[bgpen];
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
int Atari_Keyboard (void)
{
	static int old_keycode = -1;

	int keycode = -1;

	struct IntuiMessage *imsg;

	while ((imsg = (struct IntuiMessage*) GetMsg (WindowMain->UserPort)))
	{
		ULONG cl = imsg->Class;
		UWORD code = imsg->Code;
		UWORD qual = imsg->Qualifier;
		APTR iaddress = imsg->IAddress;
		WORD mx = imsg->MouseX;
		/*WORD my = imsg->MouseY;*/

		if (cl == IDCMP_MENUVERIFY)
		{
			ReplyMsg((struct Message*)imsg);
			continue;
		}

		ReplyMsg((struct Message*)imsg);

		switch (cl)
		{
			case	IDCMP_RAWKEY: keycode = HandleRawkey(code,qual,iaddress); break;
			case	IDCMP_CLOSEWINDOW: keycode = AKEY_EXIT; break;
			case	IDCMP_MENUPICK: keycode = HandleMenu(code); break;

			case	IDCMP_MOUSEBUTTONS :
					if (Controller == controller_Paddle)
					{
						switch (code)
						{
							case	SELECTDOWN : stick0 = 251; break;
							case	SELECTUP : stick0 = 255; break;
							default: break;
						}
					}
					break;

			case	IDCMP_MOUSEMOVE :
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
					break;

			default: break;
		}
		
		old_keycode = keycode;
	}

	if (keycode == -1) keycode = old_keycode;

	if (menu_consol != 7)
	{
		key_consol = menu_consol;
		menu_consol = 7;
	} else key_consol = keyboard_consol;

	return keycode;
}

/**************************************************************************
 Handle the joysting
**************************************************************************/
static void Atari_Joystick(void)
{
  static int old_stick = 0xf;

  if (GetMsg (gameport_msg_port))
  {
    WORD x = gameport_data.ie_X;
    WORD y = gameport_data.ie_Y;
    UWORD code = gameport_data.ie_Code;
    int stick=0;

    switch(x)
    {
      case  -1:
            switch (y)
            {
              case  -1: stick = 10; break;
              case   0: stick = 11; break;
              case   1: stick = 9; break;
            }
            break;

      case  0:
            switch (y)
            {
              case  -1: stick = 14; break;
              case   0: stick = 15; break;
              case   1: stick = 13; break;
            }
            break;

      case  1:
            switch (y)
            {
              case	-1: stick = 6; break;
              case	 0: stick = 7; break;
              case	 1: stick = 5; break;
            }
            break;
    }
    if (stick != 0) old_stick = stick;

    if(code == IECODE_LBUTTON) trig0 = 0;
    else if (code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) trig0 = 1;

    SendIO ((struct IORequest*)gameport_io_msg);
  }

  stick0 = old_stick;
}

/**************************************************************************
 Handle Joystick Port (direction)
**************************************************************************/
int Atari_PORT (int num)
{
	if (num == 0)
	{
		if (Controller == controller_Joystick)
		{
			Atari_Joystick ();
			return 0xf0 | stick0;
		}
		else
		{
			return stick0;
		}
	}
	else
	{
		return 0xff;
	}
}

/**************************************************************************
 Handle Joystick Triggers
**************************************************************************/
int Atari_TRIG (int num)
{
	if (num == 0)
	{
		Atari_Joystick ();
		return trig0;
	}
	else
		return 1;
}

/**************************************************************************
 ...
**************************************************************************/
int Atari_POT (int num)
{
	return PaddlePos;
}

/**************************************************************************
 ...
**************************************************************************/
int Atari_PEN(int vertical)
{
	return vertical?0xff:0;
}

/**************************************************************************
 ...
**************************************************************************/
void Sound_Pause(void)
{
	if (SoundEnabled)
	{
	}
}

/**************************************************************************
 ...
**************************************************************************/
void Sound_Continue(void)
{
	if (SoundEnabled)
	{
	}
}

/**************************************************************************
 Display a question requester. Returns TRUE for a positive
 answer otherwise FALSE
**************************************************************************/
LONG DisplayYesNoWindow(void)
{
	struct EasyStruct easy;
	easy.es_StructSize = sizeof(struct EasyStruct);
	easy.es_Flags = 0;
	easy.es_Title = "Warning";
	easy.es_TextFormat = "Are you sure?";
	easy.es_GadgetFormat = "OK|Cancel";

	return EasyRequestArgs( WindowMain, &easy, NULL, NULL );
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

		strcpy( Filename, DiskFileReq->rf_Dir );
		if (AddPart( Filename, DiskFileReq->rf_File,255) != DOSFALSE )
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
			if (FileRequester->rf_Dir) Strlcpy(Filename, FileRequester->rf_Dir, sizeof(Filename));
			else Filename[0] = 0;
			AddPart(Filename, FileRequester->rf_File, sizeof(Filename));

			if (CartType == 0)
			{
/*				Remove_ROM ();
				if (Insert_8K_ROM(Filename))
				{
					Coldstart ();
				}*/
			}
			else if (CartType == 1)
			{
/*				Remove_ROM ();
				if (Insert_16K_ROM(Filename))
				{
					Coldstart ();
				}*/
			}
			else if (CartType == 2)
			{
/*				Remove_ROM ();
				if (Insert_OSS_ROM(Filename))
				{
					Coldstart ();
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
		CloseWindow( WindowMain );
		WindowMain = NULL;
	}

	if (VisualInfoMain)
	{
		FreeVisualInfo( VisualInfoMain );
		VisualInfoMain = NULL;
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
 Allocate everything which is assoicated with the Atari Screen while
 Emulating
**************************************************************************/
LONG SetupDisplay(void)
{
	UWORD ScreenWidth, ScreenHeight;
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

	if (UseCustomScreen)
	{
		ULONG ScreenDisplayID;
		static ULONG colors32[3*256+1];

		ScreenIsCustom = TRUE;
		ScreenWidth = ATARI_WIDTH - 64;
		ScreenHeight = ATARI_HEIGHT;
		ScreenDepth = 8;

		ScreenDisplayID = DisplayID;
		if (UseBestID) ScreenDisplayID = GetBestID(ScreenWidth,ScreenHeight,ScreenDepth);

		colors32[0] = 256 << 16;
		for (i=0;i<256;i++)
		{
			int rgb = colortable[i];
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
			ScreenWidth = ATARI_WIDTH - 56;
			ScreenHeight = ATARI_HEIGHT;
			ScreenDepth = GetBitMapAttr(ScreenMain->RastPort.BitMap,BMA_DEPTH);

			if (ScreenDepth <= 8 || !CyberGfxBase)
			{
				int i;

				for(i=0;i<256;i++)
				{
					ULONG rgb = colortable[i];
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

			if (!(tempscreendata = (UBYTE*)AllocVec(ATARI_WIDTH*(ATARI_HEIGHT+16),MEMF_CLEAR)))
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
									IDCMP_RAWKEY,
					WA_ReportMouse, TRUE,
					WA_CustomScreen, ScreenMain,
					WA_Borderless, UseCustomScreen,
					WA_CloseGadget, !UseCustomScreen,
					WA_DragBar, !UseCustomScreen,
					WA_DepthGadget, !UseCustomScreen,
					UseCustomScreen?TAG_IGNORE:WA_Title, "Atari 800",
					UseCustomScreen?TAG_IGNORE:WA_ScreenTitle, ATARI_TITLE,
					WA_SizeGadget, Scalable,
					Scalable?WA_SizeBBottom:TAG_IGNORE, TRUE,
					Scalable?WA_MaxWidth:TAG_IGNORE,-1,
					Scalable?WA_MaxHeight:TAG_IGNORE,-1,
					TAG_DONE)))
				{
					if ((mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_CUSTOMSCREEN)))
						mi->Flags |= UseCustomScreen?CHECKED:0;

					if ((mi = FindUserData(MenuMain,(APTR)MEN_SETTINGS_FRAMERATE)))
						mi->Flags |= ShowFPS?CHECKED:0;
					
					SetMenuStrip(WindowMain, MenuMain);
					return 1;
				}
			}
		}
	}
	FreeDisplay();
	return 0;
}

/**************************************************************************
 Iconify the emulator
**************************************************************************/
VOID Iconify(void)
{
	struct Window *iconifyWnd;

	FreeDisplay();

	iconifyWnd = OpenWindowTags( NULL,
									WA_InnerHeight,-2,
									WA_InnerWidth, 130,
									WA_CloseGadget, TRUE,
									WA_DragBar, TRUE,
									WA_DepthGadget, TRUE,
									WA_IDCMP, IDCMP_CLOSEWINDOW,
									WA_Title, "Atari800 Emulator",
									TAG_DONE );

	if( iconifyWnd )
	{
		WaitPort( iconifyWnd->UserPort );
		CloseWindow( iconifyWnd );
	}

	if (!SetupDisplay())
		Atari_Exit(0);
}

/**************************************************************************
 main
**************************************************************************/
int main(int argc, char **argv)
{
	int keycode;

	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 20;

	while (1)
	{
		keycode = Atari_Keyboard();

		if (keycode == AKEY_EXIT) break;

		if (ScreenIsCustom != UseCustomScreen)
		{
			FreeDisplay();
			if (!(SetupDisplay()))
				break;
		}

		switch (keycode)
		{
				case AKEY_UI:
					Sound_Pause();
					ui((UBYTE *)atari_screen);
					Sound_Continue();
					break;

				case AKEY_COLDSTART:
					Coldstart();
					break;

				case AKEY_WARMSTART:
					Warmstart();
					break;

		}
		key_code = keycode;

		Atari800_Frame(EMULATE_FULL);

		atari_sync(); /* here seems to be the best place to sync */

		Atari_DisplayScreen((UBYTE *) atari_screen);
	}

	Atari_Exit(0);
	return 0;
}
