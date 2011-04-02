/*
 * screen_win32.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2006 Atari800 development team (see DOC/CREDITS)
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

#define DIRECTDRAW_VERSION   0x0500
#define DIRECTINPUT_VERSION  0x0500
#define WIN32_LEAN_AND_MEAN
#define DW_WINDOWSTYLE WS_TILEDWINDOW | WS_SIZEBOX | WS_DLGFRAME

#define DISP_CHANGE_NOMODE -6

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "atari.h"
#include "colours.h"
#include "log.h"
#include "util.h"
#include "keyboard.h"
#include "screen.h"
#include "sound.h"
#include "main_menu.h"
#include "ui.h"

#include "screen_win32.h"
#include "main.h"
#include "render_gdi.h"
#include "render_gdiplus.h"
#include "render_directdraw.h"
#include "render_direct3d.h"

static RECT currentwindowrect = {0,0,0,0};
static WINDOWPLACEMENT currentwindowstate;
static HCURSOR cursor;
static BOOL refreshframeparams = TRUE;  
static int scaledWidth = 0;
static int scaledHeight = 0;
static int bltgfx = 0;
static int scrwidth = SCREENWIDTH;
static int scrheight = SCREENHEIGHT;

RENDERMODE rendermode = GDI;
DISPLAYMODE displaymode = GDI_NEARESTNEIGHBOR;
FSRESOLUTION fsresolution = DESKTOP;
SCREENMODE screenmode = WINDOW;
ASPECTMODE scalingmethod = NORMAL;
ASPECTRATIO aspectmode = AUTO;
FRAMEPARAMS frameparams;
CROP crop;
OFFSET offset;
BOOL hidecursor = FALSE;
BOOL lockaspect = FALSE;
BOOL usecustomfsresolution = FALSE;
BOOL showmenu = TRUE;
int cropamount = 0;
int windowscale = 200;
int currentscale = 200;
int fullscreenWidth = 0;
int fullscreenHeight = 0;
int origScreenWidth = 0;
int origScreenHeight = 0;
int origScreenDepth = 0; 

void groff(void)
{
	switch (rendermode)
	{
		case GDI:
			DestroyWindow(hWndMain);
		case DIRECTDRAW:
			shutdowndirectdraw();
			break;
		case GDIPLUS:
			DestroyWindow(hWndMain);	
			shutdowngdiplus();
			break;
		case DIRECT3D:
			DestroyWindow(hWndMain); 
			shutdowndirect3d();
			break;			
	}
}

void refreshframe()
{
	refreshframeparams = TRUE;
}

void setcursor()
{
	if (screenmode == WINDOW)
		while (ShowCursor(TRUE) < 0) {}
	else if (hidecursor)
		while (ShowCursor(FALSE) > -1) {}
	else if (UI_is_active)		
		while (ShowCursor(TRUE) < 0) {}
	else 
		while (ShowCursor(FALSE) > -1) {}
}

RENDERMODE GetRenderMode() {
	return rendermode;
}

DISPLAYMODE GetDisplayMode() {
	return displaymode;
}

SCREENMODE GetScreenMode() {
	return screenmode;
}

void GetDisplayModeName(char *name) {

	switch (displaymode) {
		case GDI_NEARESTNEIGHBOR:
			sprintf(name, "%s", "GDI");
			break;
		case GDIPLUS_NEARESTNEIGHBOR:
			sprintf(name, "%s", "GDI+");
			break;
		case GDIPLUS_BILINEAR:
			sprintf(name, "%s", "GDI+/Bilinear");
			break;
		case GDIPLUS_HQBICUBIC:
			sprintf(name, "%s", "GDI+/Bicubic(HQ)");
			break;
		case DIRECT3D_NEARESTNEIGHBOR:
			sprintf(name, "%s", "Direct3D");
			break;
		case DIRECT3D_BILINEAR:
			sprintf(name, "%s", "Direct3D/Bilinear");
			break;
		case GDIPLUS_HQBILINEAR:
			sprintf(name, "%s", "GDI+/Bilinear(HQ)");
			break;
		case GDIPLUS_BICUBIC:
			sprintf(name, "%s", "GDI+/Bicubic");
			break;
	}
}

DISPLAYMODE GetActiveDisplayMode() {
	
	DISPLAYMODE retval;
	
	if (rendermode == GDI && frameparams.filter == NEARESTNEIGHBOR)
		retval = GDI_NEARESTNEIGHBOR;
	else if (rendermode == GDIPLUS && frameparams.filter == NEARESTNEIGHBOR)
		retval = GDIPLUS_NEARESTNEIGHBOR;
	else if (rendermode == GDIPLUS && frameparams.filter == BILINEAR)
		retval = GDIPLUS_BILINEAR;
	else if (rendermode == GDIPLUS && frameparams.filter == HQBICUBIC)
		retval = GDIPLUS_HQBICUBIC;
	else if (rendermode == DIRECT3D && frameparams.filter == NEARESTNEIGHBOR)
		retval = DIRECT3D_NEARESTNEIGHBOR;
	else if (rendermode == DIRECT3D && frameparams.filter == BILINEAR)
		retval = DIRECT3D_BILINEAR;
	else if (rendermode == GDIPLUS && frameparams.filter == HQBILINEAR)
		retval = GDIPLUS_HQBILINEAR;
	else if (rendermode == GDIPLUS && frameparams.filter == BICUBIC)
		retval = GDIPLUS_BICUBIC;
		
	return retval;
}

void SetDisplayMode(DISPLAYMODE dm) {
	
	displaymode = dm;
	
	switch (dm) {
		case GDI_NEARESTNEIGHBOR:
			rendermode = GDI;
			frameparams.filter = NEARESTNEIGHBOR;
			break;
		case GDIPLUS_NEARESTNEIGHBOR:
			rendermode = GDIPLUS;
			frameparams.filter = NEARESTNEIGHBOR;
			break;
		case GDIPLUS_BILINEAR:
			rendermode = GDIPLUS;
			frameparams.filter = BILINEAR;
			break;
		case GDIPLUS_HQBICUBIC:
			rendermode = GDIPLUS;
			frameparams.filter = HQBICUBIC;
			break;
		case DIRECT3D_NEARESTNEIGHBOR:
			rendermode = DIRECT3D;
			frameparams.filter = NEARESTNEIGHBOR;
			break;
		case DIRECT3D_BILINEAR:
			rendermode = DIRECT3D;
			frameparams.filter = BILINEAR;
			break;
		case GDIPLUS_HQBILINEAR:
			rendermode = GDIPLUS;
			frameparams.filter = HQBILINEAR;
			break;
		case GDIPLUS_BICUBIC:
			rendermode = GDIPLUS;
			frameparams.filter = BICUBIC;
			break;			
	}
}

static BOOL initwin(void)
{
	LONG retval;
	RECT windowRect = {0,0,0,0};
	int xpos, ypos;
	static DEVMODE dmDevMode;
	char msgbuffer[200];
	WNDCLASS wc;

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc = Atari_WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = myInstance;
	wc.hIcon = LoadIcon(myInstance, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = myname;
	wc.lpszClassName = myname;
	
	RegisterClass(&wc);
	setmenu();

	// save the original screen resolution settings
	if (!origScreenWidth && !origScreenHeight)
	{
		// Get current display settings
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dmDevMode);
		origScreenWidth = dmDevMode.dmPelsWidth;
		origScreenHeight = dmDevMode.dmPelsHeight;
		origScreenDepth = dmDevMode.dmBitsPerPel;
	}

	// display to a window
	if (rendermode != DIRECTDRAW && screenmode == WINDOW) 
	{
		// If the screen resolution has changed (due, perhaps, to switching into 
		// fullscreen mode), return to the original screen resolution.
		retval = ChangeDisplaySettings(NULL, 0);
		
		// compute a properly scaled client rect for our new window
		getscaledrect(&windowRect);
		scaledWidth = windowRect.right - windowRect.left;
		scaledHeight = windowRect.bottom - windowRect.top;

		// get the coordinates necessary to center the rectangle
		getcenteredcoords(windowRect, &xpos, &ypos);
		
		hWndMain = CreateWindowEx(0, myname, myname, DW_WINDOWSTYLE, xpos, ypos,
			                  scaledWidth, scaledHeight, NULL, hMainMenu, myInstance, NULL );
	}
	else  // display as fullscreen
	{
		// User has specified a custom fullscreen screen resolution.
		// Scan all supported graphics mode for the one that best
		// matches the requested resolution.
		if (usecustomfsresolution) {		
			int  i, nModeExists;			

			for (i=0; ;i++) {
				nModeExists = EnumDisplaySettings(NULL, i, &dmDevMode);
				
				if (!nModeExists) {
					// we've reached the end of the list
					retval = DISP_CHANGE_NOMODE;
					break;
				}
				else {
					// look for a graphics mode that matches our requested resolution
					// and has a pixel depth equal to the original screen mode.
				    if (dmDevMode.dmPelsWidth == fullscreenWidth &&
					    dmDevMode.dmPelsHeight == fullscreenHeight &&
					    dmDevMode.dmBitsPerPel == origScreenDepth)	{
						
						// Change the resolution now.
						// Using CDS_FULLSCREEN means that this resolution is temporary, so Windows 
						// rather than us, will take care of resetting this back to the default when we
						// exit the application.
						retval = ChangeDisplaySettings(&dmDevMode, CDS_FULLSCREEN);
						break;
					}
				}
			}
		}
		else  // no custom screen resolution was specified.
		{
			// Check current resolution and change it back to the default
			// if it is different from the current fullscreen settings.
			EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dmDevMode);
			if (dmDevMode.dmPelsWidth != fullscreenWidth ||
				dmDevMode.dmPelsHeight != fullscreenHeight ||
				dmDevMode.dmBitsPerPel != origScreenDepth)	{
				retval = ChangeDisplaySettings(NULL, 0);
				fullscreenWidth = dmDevMode.dmPelsWidth;
				fullscreenHeight = dmDevMode.dmPelsHeight;
			}
			else 
				retval = DISP_CHANGE_SUCCESSFUL;
		}

		// create an empty fullscreen window
		hWndMain = CreateWindowEx(
			WS_EX_TOPMOST, myname, myname, WS_POPUP, 0, 0,
			GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
			NULL, NULL, myInstance, NULL);
		
		// If we were unsuccessful in changing the resolution, report this to the user,
		// Then attempt to recover by returning to window mode.
		if (retval != DISP_CHANGE_SUCCESSFUL) {
			sprintf(msgbuffer, "Error %d.\n\nCould not change resolution\nto requested size: %dx%d.\n\nReturning to window mode.\nPlease check your settings.",
					retval, fullscreenWidth, fullscreenHeight);
			MessageBox(hWndMain, msgbuffer, myname, MB_OK);
			
			// Restore screen to window state
			screenmode = WINDOW;
			retval = ChangeDisplaySettings(NULL, 0);
			
			getcenteredcoords(windowRect, &xpos, &ypos);
			hWndMain = CreateWindowEx(0, myname, myname, DW_WINDOWSTYLE, xpos, ypos,
			                  scaledWidth, scaledHeight, NULL, hMainMenu, myInstance, NULL );							  
		}

		setcursor();
	}
	
	if (!hWndMain)
		return 1;
		
	return 0;
}

// initialize menu the first time for a given window
void initmenu(void)
{
	hMainMenu = CreateMenu();
	hFileMenu = CreateMenu();
	hManageMenu = CreateMenu();
	hSystemMenu = CreateMenu();
	hDisplayMenu = CreateMenu();
	hHelpMenu = CreateMenu();
	SetMenu(hWndMain, hMainMenu);
		
	InsertMenu(hMainMenu, ID_FILE, MF_POPUP, (UINT)hFileMenu, "File");
	AppendMenu(hFileMenu, MF_STRING, ID_RUN_ATARI_PROGRAM, "&Run Atari Program...\tAlt+R");
	AppendMenu(hFileMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hFileMenu, MF_STRING, ID_RESET, "Reset (Warm Start)\tF5");
	AppendMenu(hFileMenu, MF_STRING, ID_REBOOT, "Reboot (Cold Start)\tShift+F5");
	AppendMenu(hFileMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hFileMenu, MF_STRING, ID_CONFIGURATION, "Configuration...\tF1");
	AppendMenu(hFileMenu, MF_STRING, ID_SAVE_CONFIG, "Save Configuration");
	AppendMenu(hFileMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hFileMenu, MF_STRING, ID_BACK, "Back\tEsc");
	AppendMenu(hFileMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hFileMenu, MF_STRING, ID_EXIT, "Exit\tF9");
	
	InsertMenu(hMainMenu, ID_MANAGE, MF_POPUP, (UINT)hManageMenu, "Manage");	
	AppendMenu(hManageMenu, MF_STRING, ID_DISK_MANAGEMENT, "&Disks...\tAlt+D");
	AppendMenu(hManageMenu, MF_STRING, ID_CART_MANAGEMENT, "&Cartridges...\tAlt+C");
	AppendMenu(hManageMenu, MF_STRING, ID_TAPE_MANAGEMENT, "Tapes...");
	AppendMenu(hManageMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hManageMenu, MF_STRING, ID_EMULATOR_BIOS, "Emulator/BIOS...");
	
	InsertMenu(hMainMenu, ID_SYSTEM, MF_POPUP, (UINT)hSystemMenu, "System");	
	AppendMenu(hSystemMenu, MF_STRING, ID_NTSC, "NTSC");
	AppendMenu(hSystemMenu, MF_STRING, ID_PAL, "PAL");
	AppendMenu(hSystemMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hSystemMenu, MF_STRING, ID_SELECT_MACHINE, "Select Machine...\tAlt+Y");
	AppendMenu(hSystemMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hSystemMenu, MF_STRING, ID_SOUND, "Sound...\tAlt+O");
	AppendMenu(hSystemMenu, MF_STRING, ID_CONTROLLERS, "Controllers...");	
	if (Atari800_tv_mode == Atari800_TV_NTSC) {
		CheckMenuItem(hSystemMenu, ID_NTSC, MF_CHECKED);
		CheckMenuItem(hSystemMenu, ID_PAL, MF_UNCHECKED);
	}
	else {
		CheckMenuItem(hSystemMenu, ID_NTSC, MF_UNCHECKED);
		CheckMenuItem(hSystemMenu, ID_PAL, MF_CHECKED);
	}
	
	InsertMenu(hMainMenu, ID_DISPLAY, MF_POPUP, (UINT)hDisplayMenu, "Display");	
	AppendMenu(hDisplayMenu, MF_STRING, ID_FULLSCREEN, "Toggle Fullscreen\tAlt+Enter");
	AppendMenu(hDisplayMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hDisplayMenu, MF_STRING, ID_WINDOW_SIZE_UP, "Increase Window Size\tAlt+PgUp");
	AppendMenu(hDisplayMenu, MF_STRING, ID_WINDOW_SIZE_DOWN, "Decrease Window Size\tAlt+PgDn");
	AppendMenu(hDisplayMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hDisplayMenu, MF_STRING, ID_ATARI_DISPLAY, "General Display Settings...");
	AppendMenu(hDisplayMenu, MF_STRING, ID_WINDOWS_DISPLAY, "Windows Display Options...");
	
	InsertMenu(hMainMenu, ID_HELP, MF_POPUP, (UINT)hHelpMenu, "Help");	
	AppendMenu(hHelpMenu, MF_STRING, ID_FUNCTION_KEY_HELP, "Function Key List...");
	AppendMenu(hHelpMenu, MF_STRING, ID_HOT_KEY_HELP, "Hot Key List...");	
	AppendMenu(hHelpMenu, MF_SEPARATOR, 0, "");
	AppendMenu(hHelpMenu, MF_STRING, ID_ABOUT_ATARI800, "About Atari800...\tAlt+A");
}

// restore a previously initialized menu to the window
void restoremenu(void)
{
	// menus are never shown in fullscreen mode
	if (screenmode == FULLSCREEN)
		return;
		
	LoadMenu((HINSTANCE)hWndMain, (LPCSTR)hMainMenu);
	SetMenu(hWndMain, hMainMenu);
}

// remove menu from the window
void destroymenu(void)
{
	SetMenu(hWndMain, NULL);
	DestroyMenu((HMENU)hWndMain);
}

// set the menu based on the current state
void setmenu(void)
{	
	if (showmenu && !hMainMenu)
		initmenu();
	else if (showmenu)
		restoremenu();
	else if (!showmenu)
		destroymenu();
		
	changewindowsize(RESET, 0);
}

// switch menu on or off
void togglemenustate(void)
{	
	showmenu = !showmenu;
	setmenu();
}

// helper function that creates a scaled rectangle with the exact client area that we want.
void getscaledrect(RECT *rect)
{
	// This function is intended for windowed mode only
	if (screenmode == FULLSCREEN)
		return;
		
	// Set base window size based on selected aspect mode
	if (aspectmode == WIDE || aspectmode == AUTO) {
	
		// Enforce strict 7:5 aspect dimensions for the window
		// if the aspect ratio is locked.
		if (lockaspect) {
		    scrwidth = SCREENWIDTH;
			scrheight = SCREENHEIGHT;
		}
		else  
		{
			// Otherwise allow dimensions of the window to dynamically 
			// vary based on the crop.horizontal and crop.vertical values.
			scrwidth = SCREENWIDTH - crop.horizontal * 2;
			scrheight = SCREENHEIGHT - crop.vertical * 2;
		}
	}
	else {  // a cropped mode was selected
	    if (lockaspect) {		
			// Enforce strict 4:3 aspect dimensions for the window
			scrwidth = CROPPEDWIDTH;
			scrheight = SCREENHEIGHT;
		}
		else {
			// Allow dimensions of the window to dynamically vary
			// based on the crop.horizontal and crop.vertical values.
			scrwidth = CROPPEDWIDTH - crop.horizontal * 2;
			scrheight = SCREENHEIGHT - crop.vertical * 2;
		}
	}
	
	// now scale the window dimensions based on the windowscale 
	scaledWidth = (int)(scrwidth * windowscale / 100.0f);
	scaledHeight = (int)(scrheight * windowscale / 100.0f);

	// prevent the scaled size from exceeding the screen resolution
	if (scaledWidth > GetSystemMetrics(SM_CXSCREEN) || scaledHeight > GetSystemMetrics(SM_CYSCREEN))
	{
		scaledWidth = GetSystemMetrics(SM_CXSCREEN);
		scaledHeight = GetSystemMetrics(SM_CYSCREEN);
	}				
	
	// set a preliminary scaled rectangle
	// to the calculated size...
	rect->right = scaledWidth;
	rect->bottom = scaledHeight;

	// To avoid screen artifacts in windowed mode, especially when using scanlines, it is important
	// that the client area of the window is of the exact scaled dimensions.  CreateWindow/MoveWindow by itself
	// does not do this, therefore, we need the AdjustWindowRectEx() method to help enforce the dimensions.
	AdjustWindowRectEx(rect, DW_WINDOWSTYLE, showmenu, 0);
}

static void cmdlineFail(char msg[])
{
	char txt[256];
	sprintf(txt, "There was an invalid or missing argument\nfor commandline parameter: %s\nThe emulator may not run as expected.", msg);
	MessageBox(NULL, txt, "Commandline Error", MB_OK | MB_ICONWARNING);
}

// switches to the next scanline mode
void changescanlinemode()
{
	frameparams.scanlinemode++;
	if (frameparams.scanlinemode == HIGH + 1)
		frameparams.scanlinemode = NONE;
   
	// Force rewrite frame parameters
	refreshframe(); 

	return; 
}

// Change the 3D tilt effect to the next mode
// This only has an effect in Direct3D mode
void changetiltlevel()
{
	frameparams.tiltlevel++;
	if (frameparams.tiltlevel > TILTLEVEL3)
		frameparams.tiltlevel = TILTLEVEL0;
}

// Turns the 3D screensaver on or off.
// This only has an effect in Direct3D mode
void togglescreensaver()
{
	if (rendermode == DIRECT3D)
	{
		frameparams.screensaver = !frameparams.screensaver;
		frameparams.d3dRefresh = TRUE;
	}
}

// toggle between fullscreen and windowed modes
void togglewindowstate()
{
	int showCmd = SW_RESTORE;

	// not supported in DirectDraw mode
	if (rendermode == DIRECTDRAW) 
		return;

	// when the window is destroyed, we lose its keyboard handler
	// so let's uninit it here and reinit after we create the new window.
	uninitinput();	
	
	if (screenmode == FULLSCREEN)	{   
		screenmode = WINDOW; /* switch to windowed mode */
	}
	else {  // switch to fullscreen   
  
		// ...also save the current windowed state
		// in case we need to return to it from fullscreen.
		GetWindowRect(hWndMain, &currentwindowrect);
		currentwindowstate.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(hWndMain, &currentwindowstate);

		screenmode = FULLSCREEN;  /* keep screenmode sync'ed with mode */
	}
 
	SuppressNextQuitMessage(); // prevents DestroyWindow from closing the application
	destroymenu();
	DestroyWindow(hWndMain);
	initwin();

	/* if we're in Direct3D rendering mode, we need to do a full 
	   shutdown and restart of the Direct3D engine. */
	if (rendermode == DIRECT3D) {
		shutdowndirect3d();	   

		if (screenmode == FULLSCREEN)
			startupdirect3d(fullscreenWidth, fullscreenHeight, FALSE, &frameparams);
		else 
			startupdirect3d(currentwindowrect.right - currentwindowrect.left,
					currentwindowrect.bottom - currentwindowrect.top, TRUE, &frameparams);
	}

	// if we're returning to windowed mode, restore it to its previous size and location.
    // If not, use the default metrics. 
	if (screenmode == WINDOW && currentwindowrect.bottom && currentwindowrect.right) 
	{
		MoveWindow(hWndMain, currentwindowrect.left, currentwindowrect.top,
				   currentwindowrect.right - currentwindowrect.left,
		    	   currentwindowrect.bottom - currentwindowrect.top, TRUE);
		
		if (currentwindowstate.showCmd == SW_SHOWMAXIMIZED)
			   showCmd = SW_MAXIMIZE;	

		changewindowsize(RESET, 0);
	}
	
	ShowWindow(hWndMain, showCmd);
	setmenu();
	
	// keep sound muted in UI
	if (UI_is_active)
		Sound_Pause();

	// reinit the keyboard input and clear it
	initinput();
	clearkb();
	
	setcursor(); 

	return;
}

/* 'scale' is expressed as a percentage (100 = 100%)     */
/* CHANGEWINDOWSIZE indicates whether to STEPUP or STEPDOWN */
/* by the scale percentage, or to just simply SET or RESET it. */
/* Note that when the RESET parameter is used, the scale parameter */
/* is ignored. */
void changewindowsize(CHANGEWINDOWSIZE cws, int scale)
{
	RECT rect = {0,0,0,0};
	WINDOWPLACEMENT wndpl;
	int xpos, ypos;
	int originalwindowscale = windowscale;

	// not supported in DirectDraw mode
	if (rendermode == DIRECTDRAW) 
		return;

	// in simple mode, force scale stepping to 100% jumps
	if (scalingmethod == SIMPLE && (cws == STEPUP || cws == STEPDOWN))
		scale = 100;
		
	// don't change window size if window is maximized, minimized, or fullscreen
	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hWndMain, &wndpl);
	if (wndpl.showCmd == SW_SHOWMAXIMIZED || wndpl.showCmd == SW_SHOWMINIMIZED || screenmode == FULLSCREEN)
		return;		

	switch (cws)
	{
		case STEPUP:
			// adjust current windowscale as an even multiple of the scale parameter
			windowscale = currentscale / scale * scale;
			windowscale += scale;		     
			break;

		case STEPDOWN:
			windowscale = currentscale / scale * scale;
			if (windowscale == currentscale)
				windowscale -= scale;
			if (windowscale < 100) 
			{
				windowscale = 100;
				return;
			}
			break;
			
		case SET:
			windowscale = scale;
			break;
			
		case RESET:
			windowscale = currentscale; 
	}

	getscaledrect(&rect);

	// we've oversized, so back off and get out
	if ((rect.bottom - rect.top) > GetSystemMetrics(SM_CYSCREEN))
	{
		windowscale = originalwindowscale;
		return;
	}

	// Position the new resized window relative to previous window position
	xpos = wndpl.rcNormalPosition.left+
	       (((wndpl.rcNormalPosition.right-wndpl.rcNormalPosition.left)-
		   (rect.right-rect.left)) / 2);
	
	ypos = wndpl.rcNormalPosition.top+
	       (((wndpl.rcNormalPosition.bottom-wndpl.rcNormalPosition.top)-
		   (rect.bottom-rect.top)) / 2);
	
	MoveWindow(hWndMain, xpos, ypos,
	           rect.right - rect.left, rect.bottom - rect.top, TRUE);
	
	return;
}

// Short helper function that computes an x,y position that can be used  
// by MoveWindow to center a window having the given rectangle.
void getcenteredcoords(RECT rect, int* x, int* y)
{
	*x = (GetSystemMetrics(SM_CXSCREEN) / 2) - ((rect.right - rect.left) / 2);
	*y = (GetSystemMetrics(SM_CYSCREEN) / 2) - ((rect.bottom - rect.top) / 2);
}

// Returns the native atari coordinates given a set of windows coordinates.
// These coordinates are relative to the full 384x240 screen buffer.
// a coordinate of -1,-1 indicates a click outside the screen buffer.
void getnativecoords(int mx, int my, int* nx, int* ny)
{	
	RECT rect;
	float fx;
	float fy;
	
	GetClientRect(hWndMain, &rect);
	
	// adjust input mouse coordinates based on client rectangle size
	my = my - ((rect.bottom - rect.top) - frameparams.height) / 2;
	mx = mx - ((rect.right - rect.left) - frameparams.width) / 2;
	
	// transform absolute mouse coordinates into relative coordinates
	fx = (float)mx/frameparams.width;
	fy = (float)my/frameparams.height;

	// compute the absolute atari coordinates relative to the 
	// 384x240 screen buffer accounting for offsets and cropping
	*nx = (int)((frameparams.view.right - frameparams.view.left) * fx
	            + frameparams.view.left);

	*ny = (int)((frameparams.view.bottom - frameparams.view.top) * fy
	            + frameparams.view.top);	

	// force any points determined to be outside the screen buffer to -1,-1
	*nx = (*nx < frameparams.view.left || *nx >= frameparams.view.right) ? -1 : *nx;
	*ny = (*ny < frameparams.view.top || *ny >= frameparams.view.bottom) ? -1 : *ny;
	
	if (*nx <= -1 || *ny <= -1 || *nx >= Screen_WIDTH || *ny >= Screen_HEIGHT)
		*nx = *ny = -1;	
}

// Parse the width and height from a string formated as
// widthxheight (such as 800x600, 1280x720, etc.)
BOOL getres(char res[], int *width, int *height)
{
	size_t pos;
	char sep[] = "x";

	pos = strcspn(res, sep);
	if (pos == strlen(res))
		return FALSE;	  // missing x delimiter	

	*width = atoi(strtok(res, sep));
	*height = atoi(strtok(NULL, sep));	

	if (*width == 0 || *height == 0)
		return FALSE;     // value could not be converted

	return TRUE;	
}

// Process the commandline, then initialize the selected renderer
int gron(int *argc, char *argv[])
{
	int i, j;
	int retvalue = 0;
	RECT rect = {0,0,0,0};
	int xpos, ypos;

	// set some defaults
	int width = 0;
	int help_only = FALSE;
	
	// process commandline parameters and arguments
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-windowed") == 0)
			screenmode = WINDOW;  
		else if (strcmp(argv[i], "-width") == 0) {
			rendermode = DIRECTDRAW;
			width = Util_sscandec(argv[++i]);
		}
		else if (strcmp(argv[i], "-blt") == 0) {
			rendermode = DIRECTDRAW;
			bltgfx = TRUE;
		}
		else if (strcmp(argv[i], "-hidecursor") == 0)
			hidecursor = TRUE;
		else if (strcmp(argv[i], "-lockaspect") == 0)
			lockaspect = TRUE;
		else if (strcmp(argv[i], "-fullscreen") == 0)
			screenmode = FULLSCREEN;
		else if (strcmp(argv[i], "-hidemenu") == 0)
			showmenu = FALSE;
		else if (strcmp(argv[i], "-winscale") == 0)
			windowscale = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-hcrop") == 0)
			crop.horizontal = atoi(argv[++i]);
		else if (strcmp(argv[i], "-vcrop") == 0)
			crop.vertical = atoi(argv[++i]);
		else if (strcmp(argv[i], "-crop") == 0) {
			crop.horizontal = atoi(argv[++i]);
			crop.vertical = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-hoffset") == 0)
			offset.horizontal = atoi(argv[++i]);
		else if (strcmp(argv[i], "-voffset") == 0)
			offset.vertical = atoi(argv[++i]);
		else if (strcmp(argv[i], "-scanlines") == 0)
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "low") == 0) 
				frameparams.scanlinemode = LOW;
			else if (strcmp(argv[i], "medium") == 0) 
				frameparams.scanlinemode = MEDIUM;
			else if (strcmp(argv[i], "high") == 0)
				frameparams.scanlinemode = HIGH;
		}
		else if (strcmp(argv[i], "-render") == 0) 
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "ddraw") == 0) 
			{
				rendermode = DIRECTDRAW;
				bltgfx = TRUE;
			}
			else if (strcmp(argv[i], "gdi") == 0) {
				frameparams.filter = NEARESTNEIGHBOR;
				rendermode = GDI;
			}
			else if (strcmp(argv[i], "gdiplus") == 0) {
				frameparams.filter = NEARESTNEIGHBOR;
				rendermode = GDIPLUS;
			}
			else if (strcmp(argv[i], "direct3d") == 0) {
				frameparams.filter = NEARESTNEIGHBOR;
				rendermode = DIRECT3D;				
			}
		}
		else if (strcmp(argv[i], "-filter") == 0) 
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "pixel") == 0) 
				frameparams.filter = NEARESTNEIGHBOR;
			else if (strcmp(argv[i], "bilinear") == 0)
				frameparams.filter = BILINEAR;
			else if (strcmp(argv[i], "bicubic") == 0)
				frameparams.filter = BICUBIC;
			else if (strcmp(argv[i], "hqbilinear") == 0)
				frameparams.filter = HQBILINEAR;
			else if (strcmp(argv[i], "hqbicubic") == 0)
				frameparams.filter = HQBICUBIC;
		}
		else if (strcmp(argv[i], "-scaling") == 0) 
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "off") == 0)
				scalingmethod = OFF;
			else if (strcmp(argv[i], "normal") == 0) 
				scalingmethod = NORMAL;
			else if (strcmp(argv[i], "simple") == 0) 
				scalingmethod = SIMPLE;
			else if (strcmp(argv[i], "adaptive") == 0)
				scalingmethod = ADAPTIVE;
		}
		else if (strcmp(argv[i], "-aspect") == 0) 
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "auto") == 0)
				aspectmode = AUTO;
			else if (strcmp(argv[i], "wide") == 0) 
				aspectmode = WIDE;
			else if (strcmp(argv[i], "cropped") == 0) 
				aspectmode = CROPPED;
			else if (strcmp(argv[i], "compressed") == 0) 
				aspectmode = COMPRESSED;
		}
		else if (strcmp(argv[i], "-fsres") == 0) {	
			getres(argv[++i], &fullscreenWidth, &fullscreenHeight);
			usecustomfsresolution = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-windowed        Run in a window");
				Log_print("\t-width <num>     Set display mode width (ddraw mode only)");
				Log_print("\t-blt             Use blitting to draw graphics (ddraw mode only)");
				Log_print("\t-hidecursor      Always hide mouse cursor in fullscreen");
				Log_print("\t-fullscreen      Run in fullscreen");
				Log_print("\t-winscale <size> Default window size <100> <200> <300>... etc.");
				Log_print("\t-scanlines <%%>   Scanline mode <low> <medium> <high>");
				Log_print("\t-render <mode>   Render mode <ddraw> <gdi> <gdiplus> <direct3d>");
				Log_print("\t-filter <mode>   Filter <bilinear> <bicubic> <hqbilinear> <hqbicubic>");
				Log_print("\t-scaling <mode>  Scaling method <off> <normal> <simple> <adaptive>");
				Log_print("\t-aspect <mode>   Aspect mode <auto> <wide> <cropped> <compressed>");
				Log_print("\t-fsres <res>     Fullscale resolution <widthxheight> viz. <640x480>");
				Log_print("\t-vcrop <amt>     Crop screen vertically <0..108>");
				Log_print("\t-hcrop <amt>     Crop screen horizontally <-24..150>");
				Log_print("\t-voffset <amt>   Offset screen vertically <-50..50>");
				Log_print("\t-hoffset <amt>   Offset screen horizontally <-24..24>");
				Log_print("\t-lockaspect      Lock aspect mode when trimming");
				Log_print("\t-hidemenu        Hide Windows menu");
				help_only = TRUE;
			}
			argv[j++] = argv[i];
		}
	}

	if (help_only)
		return FALSE;

	*argc = j;

	displaymode = GetActiveDisplayMode();
	
	initwin();  // initialize a window

	// Do render mode-specific initialization
	switch (rendermode)
	{
		case DIRECTDRAW:
			retvalue = startupdirectdraw(bltgfx, width);
			break;
		case GDIPLUS:
			startupgdiplus();
			break;
		case DIRECT3D:
			if (screenmode == WINDOW) 
			{
				startupdirect3d(scaledWidth, scaledHeight, TRUE, &frameparams);
			
				// Direct3D overrides the creation of the Window, so we must
				// manually rescale it to ensure it is the exact size we want.
				getscaledrect(&rect);
				getcenteredcoords(rect, &xpos, &ypos);	
				MoveWindow(hWndMain, xpos, ypos, rect.right - rect.left,
		   			       rect.bottom - rect.top, TRUE);
			}
			else if (screenmode == FULLSCREEN) 
			{
				startupdirect3d(fullscreenWidth, fullscreenHeight, FALSE, &frameparams);
			}
			break;
	}

	return retvalue;
}

// Refresh the frame -- called on every frame
void refreshv(UBYTE *scr_ptr)
{
	float fHorizAspect, fVertAspect;
	PAINTSTRUCT ps;
	INT nRectWidth, nRectHeight;
	INT nBaseWidth, nBaseHeight;
	INT i;
	RECT rt;
	
	/* DirectDraw mode does not support windowed modes, or aspect ratio processing
	   (nor much else for that matter), so redirect immediately to the DirectDraw renderer. */
	if (rendermode == DIRECTDRAW)
	{
		refreshv_directdraw(scr_ptr, bltgfx);
		return;
	}

	GetClientRect(hWndMain, &rt);
	
	if (refreshframeparams)
		InvalidateRect(hWndMain, &rt, TRUE);   // blank screen to avoid artifacts
	else
		InvalidateRect(hWndMain, &rt, FALSE);  // don't blank screen

	// open the device context for painting
	frameparams.hdc = BeginPaint(hWndMain, &ps);
	
	// Generic frame and window operations are performed inside this section.  
	// In order to avoid performing all of this work on every frame, we only process 
	// when a function has set the refreshframeparams flag to true.
	if (refreshframeparams) 
	{
		nRectWidth = rt.right - rt.left;
		nRectHeight = rt.bottom - rt.top;
		frameparams.width = nRectWidth;
		frameparams.height = nRectHeight;
		frameparams.y_origin = 0;
		frameparams.x_origin = 0;
		frameparams.d3dRefresh = TRUE;
		cropamount = 0;

		// Scale and enforce aspect ratio
		if (scalingmethod)
		{
			// Set standard 4:3 crop if we're running in crop mode.
	        if (aspectmode == CROPPED) 
	        {       
				// 4:3 aspect ratio in CROPPED and AUTO/FULLSCREEN modes
				// are created by specifying an 8 pixel horizontal crop.
		        cropamount = STD_CROP; 
			}
			else if (aspectmode == AUTO && screenmode == FULLSCREEN)
			{
				// if we're in AUTO mode and fullscreen, then test the fullscreen
				// aspect ratio to see whether we need to crop.
				if ((float)fullscreenWidth / (float)fullscreenHeight < 7.0f/5.0f)
					cropamount = STD_CROP; 
			}
			
			// Set the *display* aspect ratio.  
			// Wide modes are set nominally to an aspect ratio of 7:5         
			if (aspectmode == WIDE || (aspectmode == AUTO && screenmode == WINDOW) ||
			   (aspectmode == AUTO && screenmode == FULLSCREEN && cropamount == 0))
			{
				// Enforce a strict WIDE aspect ratio of 7:5 
				// if the aspect ratio is locked.
				if (lockaspect) {
					fHorizAspect = 7.0f;   
					fVertAspect = 5.0f;    
				}
				else { 
					// Otherwise, allow aspect ratio to vary dynamically based on the 
					// crop.horizontal and crop.vertical values.
					fHorizAspect = (float)SCREENWIDTH - crop.horizontal * 2;
					fVertAspect = (float)SCREENHEIGHT - crop.vertical * 2;
				}
			}
			else // aspect must be COMPRESSED, CROPPED, or AUTO/FULLSCREEN/CROP               
			{
				// Enforce a strict 4:3 aspect ratio if aspect is locked.
				if (lockaspect) { 
					fHorizAspect = 4.0f;
					fVertAspect = 3.0f;				
				}
				else {
					// Allow aspect ratio to vary dynamically based on the 
					// crop.horizontal and crop.vertical values if lockaspect is set to false.
					fHorizAspect = (float)CROPPEDWIDTH - crop.horizontal * 2;
					fVertAspect = (float)SCREENHEIGHT - crop.vertical * 2;
				}
			}
		
			if (screenmode == FULLSCREEN && scalingmethod == ADAPTIVE)
			{
				// ADAPTIVE scaling - 
				// ** Intended for widescreen monitors that stretch non-native resolutions **:
				// In fullscreen legacy (4:3) resolutions like VGA running on a 
				// widescreen monitor, some users may want to squeeze the screen back into a 
				// more normalized aspect if (and only if) the monitor stretches these resolutions. 
				// To do this, we create a compensation aspect ratio based on the assumption 
				// that the original screen resolution is the native resolution --  
				// or at least one that has square (or nearly square) pixels. 

				float fOrigAspect = (float)origScreenWidth / (float)origScreenHeight;
				float fFullAspect = (float)fullscreenWidth / (float)fullscreenHeight;
				float fAdjustedAspect = fFullAspect * fHorizAspect/fVertAspect;
				float fCompensationAspect = fAdjustedAspect / fOrigAspect; 
				float fCompensatedWidth = (float)fullscreenHeight * fCompensationAspect;

				frameparams.width = (int)fCompensatedWidth;
				frameparams.height = fullscreenHeight;
				frameparams.x_origin = (int)((nRectWidth - frameparams.width) / 2.0f); 
			}
			else if (scalingmethod == SIMPLE)
			{       
				// SIMPLE scaling
				// This fullscreen mode sizes the Atari screen by a simple even multiple
				// of its native size, and finds the largest that will fit within the 
				// currently set client rectangle size.
			
                // Set the base height/width of Atari screen
				// Account for possible cropping if the user has specified it.
				if (lockaspect) { 
					if (aspectmode == WIDE || (aspectmode == AUTO && cropamount == 0))
						nBaseWidth = SCREENWIDTH;
					else 
						nBaseWidth = CROPPEDWIDTH;
						
					nBaseHeight = SCREENHEIGHT;
				}
				else {
					if (aspectmode == WIDE || (aspectmode == AUTO && cropamount == 0))
						nBaseWidth = SCREENWIDTH - crop.horizontal * 2;
					else 
						nBaseWidth = CROPPEDWIDTH - crop.horizontal * 2;
					
					nBaseHeight = SCREENHEIGHT - crop.vertical * 2;
				}
                            
                // Find the largest even multiple of the Atari screen
                // that will fit inside the client rectangle.
                for (i = 1; ;i++) 
					if (nBaseWidth * i > nRectWidth || nBaseHeight * i > nRectHeight)
						break;
                            
                // Finally, set the frame size and location
                frameparams.height = nBaseHeight * (i-1);
                frameparams.width = nBaseWidth * (i-1);
                frameparams.y_origin = (int)((nRectHeight - frameparams.height) / 2.0f);
                frameparams.x_origin = (int)((nRectWidth - frameparams.width) / 2.0f);                        
			}
			else  // we're in either a window or fullscreen with NORMAL aspect processing
			{
				// we have a tall narrow window, so pin to the width and
				// adjust the height to force the aspect ratio.
				if (nRectHeight >= nRectWidth * fVertAspect/fHorizAspect)
				{
					frameparams.height = (int)(nRectWidth * fVertAspect/fHorizAspect);
					frameparams.width = nRectWidth;
					frameparams.y_origin = (int)((nRectHeight - frameparams.height) / 2.0f); 
				}
				// we have a wide, fat window, so pin to the height and 
				// adjust the width to force the aspect ratio. 
				else
				{
					frameparams.width = (int)(nRectHeight * fHorizAspect/fVertAspect);
					frameparams.height = nRectHeight;
					frameparams.x_origin = (int)((nRectWidth - frameparams.width) / 2.0f);
				}
			}
		}
		
		// Specify the viewport rectangle that will be sent to the renderer.  This is a
		// rectangle that encloses the portion of the screen buffer that we want to display.
		frameparams.view.left = STD_INDENT + cropamount + crop.horizontal;
		frameparams.view.top = crop.vertical;
		frameparams.view.right = Screen_WIDTH - frameparams.view.left;
		frameparams.view.bottom = Screen_HEIGHT - frameparams.view.top;
		
    	// If an offset has been set by the user, adjust the viewport by that amount.
		// The negative signs reverse the shift direction to make it more natural.
		OffsetRect(&frameparams.view, -offset.horizontal, -offset.vertical);
		
		/* When doing a refreshframeparams refresh, perform these direct3d specific  */
		/* operations.  This should be done regardless of aspect ratio settings.     */
		if (rendermode == DIRECT3D)
		{	
			// Update window dimensions 
			frameparams.d3dWidth = (float)frameparams.width / nRectWidth;
			frameparams.d3dHeight = (float)frameparams.height / nRectHeight;
		
			// Although Direct3D can handle window resizing with no intervention, it is not 
			// optimal since it does not automatically change the backbuffer size to match the new
			// window size.  This can result in lower quality after a resize.  However, if we take
			// the time to manually reset the device and change the backbuffer size, we can maintain
			// a high quality image.
			resetdevice(nRectWidth, nRectHeight, screenmode, &frameparams);
		}

		// Update the title bar with resolution, scale, scanline information
		if (screenmode == WINDOW)
		{
			char buffer[100];
			char scanlinetxt[25];
			
            // The displayed window scale is actually pixel based rather than screen size
			// based to make it more useful.  To compute this we use the vertical screen
			// dimension, since its relatively constant, and factor out any vertical trim.
			
			currentscale = (int)Util_round(((float)frameparams.height + 
			                                (crop.vertical * 2 * ((float)windowscale/100)))
			                               / 240.0f * 100);
			
			switch (frameparams.scanlinemode)
			{
				case NONE:
					strcpy(scanlinetxt, "");
					break;
				case LOW:
					strcpy(scanlinetxt, "; Scanlines: Low (1x)");
					break;
				case MEDIUM:
					strcpy(scanlinetxt, "; Scanlines: Medium (2x)");
					break;
				case HIGH:
					strcpy(scanlinetxt, "; Scanlines: High (3x)");
					break;
			}
			
			sprintf(buffer, "%s - %dx%d (%d%%)%s",myname, frameparams.width, frameparams.height, currentscale, scanlinetxt);
			SetWindowText(hWndMain, buffer);
		}

		refreshframeparams = FALSE;  // we're done refreshing so set the fp_refresh global to FALSE
	}

	// sanity check to make sure the window has some real estate
	if ((!frameparams.width) || (!frameparams.height))
		return;

	// Now that we've completed our generic screen processing, pass the screen buffer along
	// with the calculated frame parameters over to the specific renderer the user has specified
	// in order to actually render the frame.
	switch (rendermode)
	{
		case GDI:
			refreshv_gdi(scr_ptr, &frameparams);
			break;
		case GDIPLUS:
			refreshv_gdiplus(scr_ptr, &frameparams);
			break;
		case DIRECT3D:
			refreshv_direct3d(scr_ptr, &frameparams);
			break;
	}

	// We are finished writing to the DC
	EndPaint(hWndMain, &ps);

	ValidateRect(hWndMain, &rt);
}

