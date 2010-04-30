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

#define MOUSE_CENTER_X  100
#define MOUSE_CENTER_Y  100
#define WM_MOUSEHWHEEL 0x020E

#include "config.h"
#include <windows.h>
#include <windowsx.h>
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
#include "ui_basic.h"
#include "log.h"
#include "config.h"
#include "main_menu.h"

#include "main.h"
#include "screen_win32.h"
#include "atari_win32.h"
#include "keyboard.h"
#include "joystick.h"
}

char *myname = "Atari800";
HWND hWndMain = NULL;
HINSTANCE myInstance = NULL;
FILE *stdout_stream;

// menus
HMENU hMainMenu = NULL;
HMENU hFileMenu = NULL;
HMENU hManageMenu = NULL;
HMENU hSystemMenu = NULL;
HMENU hDisplayMenu = NULL;
HMENU hHelpMenu = NULL;

static int bActive = 0;			/* activity indicator */
static bool quit_ok = TRUE;
static keycommand_t keycommand = {AKEY_NONE, -1};
bool help_only = FALSE;
BOOL useconsole = FALSE;

#if 1
void exit(int code)
{
	if (useconsole) 
	{
		FreeConsole();
    }		
	else
	{
	    fclose(stdout_stream);
	}

	MSG msg;
	PostMessage(hWndMain, WM_CLOSE, 0, 0);
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	ExitProcess(code);
}
#endif

// Used where necessary to keep UI 
// responsive to system events 
void DoEvents()
{
	MSG msg;
	msg.message = WM_NULL; 
	
	PLATFORM_DisplayScreen();
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
	{
		if (msg.message == WM_QUIT) 
		{
			PostQuitMessage(10);
			Atari800_Exit(FALSE);
			exit(0);
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

// Used by PLATFORM_Keyboard to retrieve a menu command
void GetCommandKey(keycommand_t *kc)
{
	if (keycommand.keystroke != AKEY_NONE) {
		kc->keystroke = keycommand.keystroke;
		kc->function = keycommand.function;
		keycommand.keystroke = AKEY_NONE;
		keycommand.function = -1;
	}
	else {
		kc->keystroke = AKEY_NONE;
		kc->function = -1;
	}
}
		
void SuppressNextQuitMessage()
{	
	quit_ok = FALSE;
}

void OnSize(HWND hWnd, UINT state, int cx, int cy)
{
	refreshframe(); // handled in screen_win32.c
}

void OnActivateApp(HWND hWnd, BOOL fActivate, DWORD dwThreadId)
{
	bActive = fActivate;
	if (bActive) {
		kbreacquire();
#ifdef SOUND
		Sound_Continue();
#endif
	}
}

HWND OnSysChar(HWND hWnd, TCHAR ch, int cRepeat)
{
	/* empty function suppresses beep on alt-key shortcuts */
	return(0); 
}

void OnClose(HWND hWnd)
{
	groff();
#ifdef SOUND
	Sound_Exit();
#endif
	uninitjoystick();
	uninitinput();
}

void OnDestroy(HWND hWnd)
{
	if (quit_ok)
		PostQuitMessage(10);
	quit_ok = TRUE;
}

void OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id) {	
		// File Menu
		case ID_RUN_ATARI_PROGRAM:
			if (UI_current_function != UI_MENU_RUN) {
				keycommand.keystroke = AKEY_UI;
				keycommand.function = UI_MENU_RUN;
			}
			break;
		case ID_RESET:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_RESETW;
			break;
		case ID_REBOOT:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_RESETC;
			break;
		case ID_CONFIGURATION:
			keycommand.keystroke = AKEY_UI;
			break;
		case ID_SAVE_CONFIG:
			if (UI_is_active) {
				keycommand.keystroke = AKEY32_MENU_SAVE_CONFIG;
			}
			else {
				keycommand.keystroke = AKEY_UI;
				keycommand.function = UI_MENU_SAVE_CONFIG;	
			}
			break;
		case ID_BACK:
			keycommand.keystroke = AKEY_ESCAPE;
			keycommand.function = -1;
			break;
		case ID_EXIT:
			keycommand.keystroke = AKEY_EXIT;
			keycommand.function = -1;
			break;
		
		// Manage Menu
		case ID_DISK_MANAGEMENT:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_DISK;
			break;
		case ID_CART_MANAGEMENT:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_CARTRIDGE;
			break;
		case ID_TAPE_MANAGEMENT:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_CASSETTE;
			break;
		case ID_EMULATOR_BIOS:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_SETTINGS;
			break;
			
		// System Menu
		case ID_NTSC:
			//CheckMenuItem(hSystemMenu, ID_NTSC, MF_CHECKED);
			//CheckMenuItem(hSystemMenu, ID_PAL, MF_UNCHECKED);
			Atari800_SetTVMode(Atari800_TV_NTSC);
			Atari800_InitialiseMachine();
			break;
		case ID_PAL:
			//CheckMenuItem(hSystemMenu, ID_PAL, MF_CHECKED);
			//CheckMenuItem(hSystemMenu, ID_NTSC, MF_UNCHECKED);
			Atari800_SetTVMode(Atari800_TV_PAL);
			Atari800_InitialiseMachine();
			break;
		case ID_SELECT_MACHINE:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_SYSTEM;	
			break;
#ifdef SOUND
		case ID_SOUND:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_SOUND;
			break;
#endif
		case ID_CONTROLLERS:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_CONTROLLER;
			break;
			
		// Display Menu
		case ID_FULLSCREEN:
			togglewindowstate();
			break;
		case ID_WINDOW_SIZE_UP:
			changewindowsize(STEPUP, 50);
			break;
		case ID_WINDOW_SIZE_DOWN:
			changewindowsize(STEPDOWN, 50);
			break;
		case ID_ATARI_DISPLAY:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_DISPLAY;
			break;
		case ID_WINDOWS_DISPLAY:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_WINDOWS;
			break;
			
		// Help Menu
		case ID_ABOUT_ATARI800:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_ABOUT;
			break;
		case ID_FUNCTION_KEY_HELP:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_FUNCT_KEY_HELP;
			break;
		case ID_HOT_KEY_HELP:
			keycommand.keystroke = AKEY_UI;
			keycommand.function = UI_MENU_HOT_KEY_HELP;
			break;
	}
}

// set the PAL/NTSC mode - called in atari.c
void SetTVModeMenuItem(int mode)
{
	if (mode == Atari800_TV_NTSC) {
		CheckMenuItem(hSystemMenu, ID_NTSC, MF_CHECKED);
		CheckMenuItem(hSystemMenu, ID_PAL, MF_UNCHECKED);
	} 
	else {
		CheckMenuItem(hSystemMenu, ID_PAL, MF_CHECKED);
		CheckMenuItem(hSystemMenu, ID_NTSC, MF_UNCHECKED);
	}
}
	
extern "C" LRESULT CALLBACK Atari_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int nx, ny;

	switch (message) {
	
		HANDLE_MSG(hWnd, WM_SIZE, OnSize);
		HANDLE_MSG(hWnd, WM_ACTIVATEAPP, OnActivateApp);
		HANDLE_MSG(hWnd, WM_SYSCHAR, OnSysChar);
		HANDLE_MSG(hWnd, WM_CLOSE, OnClose);
		HANDLE_MSG(hWnd, WM_DESTROY, OnDestroy);
		HANDLE_MSG(hWnd, WM_COMMAND, OnCommand);
		
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			INPUT_mouse_buttons = ((wParam & MK_LBUTTON) ? 1 : 0)
								| ((wParam & MK_RBUTTON) ? 2 : 0)
								| ((wParam & MK_MBUTTON) ? 4 : 0);

			// handle mouse clicks in the config UI
			if (UI_is_active) {
				switch (INPUT_mouse_buttons) {
					case 1:
						getnativecoords(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &nx, &ny);
						SetMouseIndex(nx, ny);
						keycommand.keystroke = AKEY32_UI_MOUSE_CLICK;
						break;
					case 2: 
						keycommand.keystroke = AKEY_ESCAPE;
						keycommand.function = -1;
						break;
					case 4:
						keycommand.keystroke = AKEY_RETURN;
						keycommand.function = -1;
						break;			
				}
			}			
			break;
		case WM_LBUTTONDBLCLK:
			if (UI_is_active && UI_mouse_click.y != -1) {
				keycommand.keystroke = AKEY_RETURN;
				keycommand.function = -1;
			}
			break;
		case WM_MOUSEWHEEL:
			if (UI_is_active) {				
				if (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA > 0) 
					keycommand.keystroke = AKEY_UP;
				else if (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA < 0) 
					keycommand.keystroke = AKEY_DOWN;
			}
			break;
		case WM_MOUSEHWHEEL:
			if (UI_is_active) {				
				if (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA > 0) 
					keycommand.keystroke = AKEY_RIGHT;
				else if (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA < 0) 
					keycommand.keystroke = AKEY_LEFT;
			}
			break;
		case WM_SETCURSOR:
			if (GetRenderMode() == DIRECTDRAW) {
				SetCursor(NULL);
				return TRUE;
			}
			break;
		case WM_ENTERMENULOOP:
			Sound_Pause();
			break;
		case WM_EXITMENULOOP:
			if (!UI_is_active) 
				Sound_Continue();
			break;		
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

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
	// if -console switch is set.
	//*********************************************
	int i,j;

	for (i = j = 1; i < argc; i++) {
		if (strcmp(argv[i], "-console") == 0) {
			useconsole = TRUE;
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
	else // not using console
	{
	    // console is supressed, so stream console output to a file
		stdout_stream = freopen("atari800.txt", "w", stdout);
		
		if (stdout_stream == NULL)
		  fprintf(stdout, "Error opening atari800.txt\n");
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
		Log_print("\t-console         Show the Atari800 console window");
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

		// support mouse device modes 
		// only supported in fullscreen modes for now
		if (GetScreenMode() == FULLSCREEN)
		{
			GetCursorPos(&mouse);
			INPUT_mouse_delta_x = mouse.x - MOUSE_CENTER_X;
			INPUT_mouse_delta_y = mouse.y - MOUSE_CENTER_Y;
			if (INPUT_mouse_delta_x | INPUT_mouse_delta_y)
				SetCursorPos(MOUSE_CENTER_X, MOUSE_CENTER_Y);
		}
			
		Atari800_Frame();
		Process_Hotkeys();  
		if (Atari800_display_screen)
			PLATFORM_DisplayScreen();
	}

	return msg.wParam;
}
