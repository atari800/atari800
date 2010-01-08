/*
 * main.cpp - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2010 Atari800 development team (see DOC/CREDITS)
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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>

extern "C" {
#include "atari.h"
#include "input.h"
#include "platform.h"
#include "screen.h"
#include "sound.h"
#include "ui.h"
#include "log.h"

#include "main.h"
#include "screen_win32.h"
#include "atari_win32.h"
#include "keyboard.h"
#include "joystick.h"
}

char *myname = "Atari800";
HWND hWndMain;
HMENU hMainMenu;
HINSTANCE myInstance;

static int bActive = 0;			/* activity indicator */
static bool ui_active = FALSE;
static bool quit_ok = TRUE;
bool help_only = FALSE;
bool useconsole = TRUE;

#if 1
void exit(int code)
{
	if (useconsole) 
		FreeConsole();	

	MSG msg;
	PostMessage(hWndMain, WM_CLOSE, 0, 0);
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	ExitProcess(code);
}
#endif

void SuppressNextQuitMessage()
{	
	quit_ok = FALSE;
}

extern "C" LRESULT CALLBACK Atari_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_SYSCHAR:    
		return(0);  /* suppresses beep on alt-key shortcuts */
		break;
	case WM_SYSKEYUP:
		//message=NULL;
	    break;
	case WM_ACTIVATEAPP:
		bActive = wParam;
		if (bActive) {
			kbreacquire();
#ifdef SOUND
			Sound_Continue();
#endif
		}
		break; 
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		INPUT_mouse_buttons = ((wParam & MK_LBUTTON) ? 1 : 0)
		              | ((wParam & MK_RBUTTON) ? 2 : 0)
		              | ((wParam & MK_MBUTTON) ? 4 : 0);
		break;
	case WM_SETCURSOR:
		if (GetRenderMode() == DIRECTDRAW) {
			SetCursor(NULL);
			return TRUE;
		}
		break;
	case WM_CREATE:
		break;
	case WM_CLOSE:
		groff();
#ifdef SOUND
		Sound_Exit();
#endif
		uninitjoystick();
		uninitinput();
		break;
	case WM_DESTROY:
		if (quit_ok)
			PostQuitMessage(10);
		quit_ok = TRUE;
		break;
	case WM_SIZE:
		refreshframe(); // handled in screen_win32.c
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#define MOUSE_CENTER_X  100
#define MOUSE_CENTER_Y  100

int WINAPI WinMain(HINSTANCE hinstance,
		   HINSTANCE hprevinstance,
		   LPSTR lpcmdline,
		   int nshowcmd)
{
	//***********************************************
	// Convert WinMain() style command line arguments
	// to main() style arguments (argc, *argv[])
	// This maintains compatibility with the former
	// main() entry point's parameters.
	//***********************************************

	char **argv = NULL;
   	int argc = 1;

	char *app_path = new char[MAX_PATH];
   	strcpy(app_path, GetCommandLine());
   	if (app_path[0] == '\"')
   	{
		app_path = (app_path+1);
		char *lastdit = strchr(app_path, '\"');
		*lastdit = '\x0';
   	}

   	if ( *lpcmdline != '\x0' )
   	{
		char *cmdlinecopy = new char[strlen(lpcmdline)+1];
		strcpy(cmdlinecopy, lpcmdline);

		char *c = cmdlinecopy;
		while(c)
		{
			++argc;
			c = strchr((c+1),' ');
		}

		argv = new char*[argc];
		argv[0] = app_path;

		if(argc > 1)
		{
			argv[1] = cmdlinecopy;
			char *c = strchr(cmdlinecopy, ' ');
			int n = 2;
			while(c)
			{
					*c = '\x0';
					argv [n] = (c+1);
					++n;
					c = strchr((c+1), ' ');
			}
		}
   	}
   	else
   	{
		argv = new char *[1];
		argv[0] = app_path;
   	}
	
	//*********************************************
	// Process commandline and activate console
	// as long as -noconsole is not set.
	//*********************************************
	int i,j;

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-noconsole") == 0) {
			useconsole = FALSE;
		}
		else if (strcmp(argv[i], "-help") == 0) {	
			help_only = TRUE;
		}
	}

	if (useconsole || help_only) {
		AllocConsole(); 				// start a console window
		freopen("CONIN$","rb",stdin);   // reopen stdin handle as console window input
		freopen("CONOUT$","wb",stdout); // reopen stout handle as console window output
		freopen("CONOUT$","wb",stderr); // reopen stderr handle as console window output
	}
	
	//*********************************************
	// Begin main processing
	//*********************************************
	
	MSG msg;
	POINT mouse;
	
	Win32_Init();
	myInstance = GetModuleHandle(NULL);
	
	if (help_only) {
	    /* initialize the Atari800 for help only */
		Atari800_Initialise(&argc, argv);
		Log_print("\t-noconsole       Do not show the Atari800 console window");
		Log_print("\n");
		system("PAUSE");
		return 0;
	}
	
	/* initialise Atari800 core for use */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	msg.message = WM_NULL;

	/* main loop */
	for (;;) {

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
			break;

		if (!bActive)
			continue;

		INPUT_key_code = PLATFORM_Keyboard();

		GetCursorPos(&mouse);
		INPUT_mouse_delta_x = mouse.x - MOUSE_CENTER_X;
		INPUT_mouse_delta_y = mouse.y - MOUSE_CENTER_Y;
		if (GetRenderMode() == DIRECTDRAW && (INPUT_mouse_delta_x | INPUT_mouse_delta_y))
			SetCursorPos(MOUSE_CENTER_X, MOUSE_CENTER_Y);

		Atari800_Frame();
		Atari800_Frame32();  // win32 specific ops
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
	}

	return msg.wParam;
}
