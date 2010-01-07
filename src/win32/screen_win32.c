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

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include "atari.h"
#include "colours.h"
#include "log.h"
#include "util.h"
#include "keyboard.h"

#include "screen_win32.h"
#include "main.h"
#include "render_gdi.h"
#include "render_gdiplus.h"
#include "render_directdraw.h"
#include "render_direct3d.h"

static RECT currentwindowrect = {0,0,0,0};
static WINDOWPLACEMENT currentwindowstate;
static HCURSOR cursor;
static MENUITEMINFO menuitems;
static BOOL refreshframeparams = TRUE;  
static int scaledWidth = 0;
static int scaledHeight = 0;
static int bltgfx = 0;
static int scrwidth = SCREENWIDTH;
static int scrheight = SCREENHEIGHT;

extern RENDERMODE rendermode = GDI;
extern DISPLAYMODE displaymode = GDI_NEARESTNEIGHBOR;
extern FSRESOLUTION fsresolution = LOWRES;
extern SCREENMODE screenmode = WINDOW;
extern ASPECTMODE aspectmode = SMART;
extern FRAMEPARAMS frameparams;
extern BOOL showcursor = TRUE;
extern BOOL usecustomfsresolution = FALSE;
extern int windowscale = 200;
extern int fullscreenWidth = 0;
extern int fullscreenHeight = 0;
extern int origScreenWidth = 0;
extern int origScreenHeight = 0; 

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
			shutdowngdiplus();
			break;
		case DIRECT3D:
			DestroyWindow(hWndMain); /* needed by Server 2008 to prevent crash on exit */
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
	ShowCursor(showcursor);
}

RENDERMODE GetRenderMode() {
	return rendermode;
}

DISPLAYMODE GetDisplayMode() {
	return displaymode;
}

void GetDisplayModeName(char *name) {

	switch (displaymode) {
		case GDI_NEARESTNEIGHBOR:
			sprintf(name, "%s", "GDI");
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
		case GDIPLUS_NEARESTNEIGHBOR:
			sprintf(name, "%s", "GDI+");
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
	else if (rendermode == GDIPLUS && frameparams.filter == BILINEAR)
		retval = GDIPLUS_BILINEAR;
	else if (rendermode == GDIPLUS && frameparams.filter == HQBICUBIC)
		retval = GDIPLUS_HQBICUBIC;
	else if (rendermode == DIRECT3D && frameparams.filter == NEARESTNEIGHBOR)
		retval = DIRECT3D_NEARESTNEIGHBOR;
	else if (rendermode == DIRECT3D && frameparams.filter == BILINEAR)
		retval = DIRECT3D_BILINEAR;
	else if (rendermode == GDIPLUS && frameparams.filter == NEARESTNEIGHBOR)
		retval = GDIPLUS_NEARESTNEIGHBOR;
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
		case GDIPLUS_NEARESTNEIGHBOR:
			rendermode = GDIPLUS;
			frameparams.filter = NEARESTNEIGHBOR;
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

	wc.style = CS_HREDRAW | CS_VREDRAW;
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

	// save the original screen resolution settings
	if (!origScreenWidth && !origScreenHeight)
	{
		// Get current display settings
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dmDevMode);
		origScreenWidth = dmDevMode.dmPelsWidth;
		origScreenHeight = dmDevMode.dmPelsHeight;
	}

	if (rendermode != DIRECTDRAW && screenmode == WINDOW) // display to a window
	{
		// If the screen resolution has changed (due, perhaps, to switching into 
		// fullscreen mode), return to the original screen resolution.
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dmDevMode);
		if (!(dmDevMode.dmPelsWidth == origScreenWidth) &&
		    !(dmDevMode.dmPelsHeight == origScreenHeight))
		{
			dmDevMode.dmPelsWidth = origScreenWidth;
		      	dmDevMode.dmPelsHeight = origScreenHeight;

			retval = ChangeDisplaySettings(&dmDevMode, 0);
		}
		
		// compute a properly scaled client rect
		getscaledrect(&windowRect);
		scaledWidth = windowRect.right - windowRect.left;
		scaledHeight = windowRect.bottom - windowRect.top;

		// center the screen
		getcenteredcoords(windowRect, &xpos, &ypos);
	
		//initmenu();
	
		hWndMain = CreateWindowEx(0, myname, myname, DW_WINDOWSTYLE, xpos, ypos,
			                  scaledWidth, scaledHeight, NULL, hMainMenu, myInstance, NULL );
	}
	else  // display as fullscreen
	{
		if (usecustomfsresolution) // user has specified a custom screen resolution
		{
			// Change to the requested screen resolution
			dmDevMode.dmPelsWidth = fullscreenWidth;
			dmDevMode.dmPelsHeight = fullscreenHeight;

			// Change the resolution now.
			// Using CDS_FULLSCREEN means that this resolution is temporary, so Windows 
			// rather than us, will take care of resetting this back to the default when we
			// exit the application.
			retval = ChangeDisplaySettings(&dmDevMode, CDS_FULLSCREEN);
		}
		else  // no custom screen resolution was specified.
		{
			// Record the current resolution, but don't change it.
			fullscreenWidth = dmDevMode.dmPelsWidth;
			fullscreenHeight = dmDevMode.dmPelsHeight;
			retval = DISP_CHANGE_SUCCESSFUL;
		}

		// create an empty fullscreen window
		hWndMain = CreateWindowEx(
			WS_EX_TOPMOST, myname, myname, WS_POPUP, 0, 0,
			GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
			NULL, NULL, myInstance, NULL);
		
		// If we were unsuccessful in changing the resolution, report this to the user,
		// Then attempt to recover by using original screen resolution.
		if (retval != DISP_CHANGE_SUCCESSFUL) {
			sprintf(msgbuffer, "Could not change resolution\nto requested size: %dx%d.\n\nForcing resolution to %dx%d.\nPlease check your settings.",
					dmDevMode.dmPelsWidth, dmDevMode.dmPelsHeight,origScreenWidth,origScreenHeight);
			MessageBox(hWndMain, msgbuffer, myname, MB_OK);
			
			DestroyWindow(hWndMain);
			
			// Change to the original desktop screen resolution
			dmDevMode.dmPelsWidth = origScreenWidth;
			dmDevMode.dmPelsHeight = origScreenHeight;
			fullscreenWidth = dmDevMode.dmPelsWidth;
			fullscreenHeight = dmDevMode.dmPelsHeight;
			fsresolution = DESKTOPRES;			
			usecustomfsresolution = FALSE;
			
			retval = ChangeDisplaySettings(&dmDevMode, CDS_FULLSCREEN);
			
			hWndMain = CreateWindowEx(
				WS_EX_TOPMOST, myname, myname, WS_POPUP, 0, 0,
				origScreenWidth, origScreenHeight,
				NULL, NULL, myInstance, NULL);
			
			if (retval != DISP_CHANGE_SUCCESSFUL) {
				sprintf(msgbuffer, "Could not change resolution\nto requested size: %dx%d.",
				    dmDevMode.dmPelsWidth, dmDevMode.dmPelsHeight);
				MessageBox(hWndMain, msgbuffer, myname, MB_OK);
			}
		}

		// hide the cursor if -nocursor commandline option was specified
		if (!showcursor)
			ShowCursor(FALSE);	
	}
	
	if (!hWndMain)
		return 1;
	return 0;
}

static BOOL initmenu(void)
{
    // create menu
	menuitems.cbSize = sizeof(MENUITEMINFO);
	menuitems.fMask = MIIM_TYPE | MIIM_ID | MIIM_DATA;
	menuitems.fType = MFT_STRING;
	menuitems.wID = 0x0800;
	menuitems.dwTypeData = "File";
	menuitems.cch = lstrlen("File");
	
	hMainMenu = CreateMenu();
	SetMenu(hWndMain, hMainMenu);
		
	InsertMenuItem(hMainMenu, 0, 1, &menuitems);
}

// helper function that creates a scaled rectangle with the exact client area that we want.
void getscaledrect(RECT *rect)
{
	// calculate the scaled screen metrics 
	scaledWidth = (int)(scrwidth * windowscale / 100.0f);
	scaledHeight = (int)(scrheight * windowscale / 100.0f);

	// prevent the scaled size from exceeding the screen resolution
	if (scaledWidth > GetSystemMetrics(SM_CXSCREEN) || scaledHeight > GetSystemMetrics(SM_CYSCREEN))
	{
		scaledWidth = GetSystemMetrics(SM_CXSCREEN);
		scaledHeight = GetSystemMetrics(SM_CYSCREEN);
	}				
	
	rect->right = scaledWidth;
	rect->bottom = scaledHeight;

	// To avoid screen artifacts in windowed mode, especially when using scanlines, it is important
	// that the client area of the window is of the exact scaled dimensions.  CreateWindow/MoveWindow by itself
	// does not do this, therefore, we need the AdjustWindowRectEx() method to help enforce the dimensions.
	AdjustWindowRectEx(rect, DW_WINDOWSTYLE, FALSE, 0);
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
		if (frameparams.screensaver == TRUE)
			frameparams.screensaver = FALSE;
		else
			frameparams.screensaver = TRUE;

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
	DestroyWindow(hWndMain);
	initwin();

	/* if we're in Direct3D rendering mode, we need to do a full 
	   shutdown and restart of the Direct3D engine. */
	if (rendermode == DIRECT3D) {
		shutdowndirect3d();	   

		if (screenmode == FULLSCREEN)
			startupdirect3d(fullscreenWidth, fullscreenHeight, FALSE,
				        frameparams.scanlinemode, frameparams.filter);
		else 
			startupdirect3d(currentwindowrect.right - currentwindowrect.left,
					currentwindowrect.bottom - currentwindowrect.top, 
					TRUE, frameparams.scanlinemode, frameparams.filter);
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
	}
    
	ShowWindow(hWndMain, showCmd);

	// reinit the keyboard input and clear it
	initinput();
	clearkb();

	// Always show the cursor in windowed mode, but only show it
	// in fullscreen if the user is not using the -nocursor switch.
	if (screenmode == WINDOW)
		ShowCursor(TRUE);

	return;
}

/* 'scale' is expressed as a percentage (100 = 100%)     */
/* CHANGEWINDOWSIZE indicates whether to STEPUP or STEPDOWN */
/* by the scale percentage, or to just simply SET it.         */
void changewindowsize(CHANGEWINDOWSIZE cws, int scale)
{
	RECT rect = {0,0,0,0};
	WINDOWPLACEMENT wndpl;
	int xpos, ypos;
	int originalwindowscale = windowscale;

	// not supported in DirectDraw mode
	if (rendermode == DIRECTDRAW) 
		return;

	// don't change window size if window is maximized, minimized, or fullscreen
	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hWndMain, &wndpl);
	if (wndpl.showCmd == SW_SHOWMAXIMIZED || wndpl.showCmd == SW_SHOWMINIMIZED || screenmode == FULLSCREEN)
		return;		

	switch (cws)
	{
		case STEPUP:
			// adjust current windowscale as an even multiple of the scale parameter
			windowscale = windowscale / scale  * scale ;  
			windowscale += scale;		     
			break;

		case STEPDOWN:
			windowscale = windowscale / scale  * scale ;
			windowscale -= scale;
			if (windowscale < 100) 
			{
				windowscale = 100;
				return;
			}
			break;
			
		case SET:
			windowscale = scale;
	}

	getscaledrect(&rect);

	// we've oversized, so back off and get out
	if ((rect.bottom - rect.top) > GetSystemMetrics(SM_CYSCREEN))
	{
		windowscale = originalwindowscale;
		return;
	}

	// Center the screen
	getcenteredcoords(rect, &xpos, &ypos);

	MoveWindow(hWndMain, xpos, ypos,
		   rect.right - rect.left,
		   rect.bottom - rect.top, TRUE);	

	return;
}

// Short helper function that computes an x,y position that can be used  
// by MoveWindow to center a window having the given rectangle.
void getcenteredcoords(RECT rect, int* x, int* y)
{
	*x = (GetSystemMetrics(SM_CXSCREEN) / 2) - ((rect.right - rect.left) / 2);
	*y = (GetSystemMetrics(SM_CYSCREEN) / 2) - ((rect.bottom - rect.top) / 2);
}

// Parses the width and height from a string formated as
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
		else if (strcmp(argv[i], "-nocursor") == 0)
			showcursor = FALSE;
		else if (strcmp(argv[i], "-fullscreen") == 0)
		{
			screenmode = FULLSCREEN;
		}
		else if (strcmp(argv[i], "-winscale") == 0)
			windowscale = Util_sscandec(argv[++i]);
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
			else if (strcmp(argv[i], "gdi") == 0)
				rendermode = GDI;
			else if (strcmp(argv[i], "gdiplus") == 0)
				rendermode = GDIPLUS;
			else if (strcmp(argv[i], "direct3d") == 0)
				rendermode = DIRECT3D;
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
		else if (strcmp(argv[i], "-aspect") == 0) 
		{
			if (!checkparamarg(argv[++i]))
				cmdlineFail(argv[i-1]);
			else if (strcmp(argv[i], "off") == 0)
				aspectmode = OFF;
			else if (strcmp(argv[i], "normal") == 0) 
				aspectmode = NORMAL;
			else if (strcmp(argv[i], "smart") == 0)
				aspectmode = SMART;
		}
		else if (strcmp(argv[i], "-fsres") == 0) {	
			getres(argv[++i], &fullscreenWidth, &fullscreenHeight);
			usecustomfsresolution = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-windowed        Run in a window");
				Log_print("\t-width <num>     Set display mode width");
				Log_print("\t-blt             Use blitting to draw graphics");
				Log_print("\t-nocursor        Do not show mouse cursor in fullscreen");
				Log_print("\t-fullscreen      Run in fullscreen");
				Log_print("\t-winscale <size> Default window size <100> <200> <300>... etc.");
				Log_print("\t-scanlines <%%>   Scanline mode <low> <medium> <high>");
				Log_print("\t-render <mode>   Render mode <ddraw> <gdi> <gdiplus> <direct3d>");
				Log_print("\t-filter <mode>   Filter <bilinear> <bicubic> <hqbilinear> <hqbicubic>");
				Log_print("\t-aspect <mode>   Aspect control mode <off> <normal> <smart>");
				Log_print("\t-fsres <res>     Fullscale resolution <widthxheight> viz. <640x480>");
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
			if (rendermode != DIRECTDRAW && screenmode == WINDOW) 
			{
				startupdirect3d(scaledWidth, scaledHeight, TRUE,
                                frameparams.scanlinemode, frameparams.filter);
			
				// Direct3D overrides the creation of the Window, so we must
				// manually rescale it to ensure it is the exact size we want.
				getscaledrect(&rect);
				getcenteredcoords(rect, &xpos, &ypos);	
				MoveWindow(hWndMain, xpos, ypos, rect.right - rect.left,
		   			       rect.bottom - rect.top, TRUE);
			}
			else if (screenmode == FULLSCREEN) 
			{
				startupdirect3d(fullscreenWidth, fullscreenHeight, FALSE,
			                    frameparams.scanlinemode, frameparams.filter);
			}
			break;
	}

	return retvalue;
}

// Refresh the frame -- called on every frame
void refreshv(UBYTE *scr_ptr)
{
	PAINTSTRUCT ps;
	INT nRectWidth, nRectHeight;
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
	// when a function has set the fp_refresh flag to true.
	if (refreshframeparams) 
	{
		nRectWidth = rt.right - rt.left;
		nRectHeight = rt.bottom - rt.top;
		frameparams.width = nRectWidth;
		frameparams.height = nRectHeight;
		frameparams.vertoffset = 0;
		frameparams.horizoffset = 0;
		frameparams.d3dRefresh = TRUE;

		// Compute correct aspect ratio
		if (aspectmode)
		{
			if (screenmode == FULLSCREEN && aspectmode == SMART)
			{
				// SMART Aspect Ratio processing:
				// In full screen mode, we may need to compensate for non-square
				// pixels if the monitor stretches these lower resolutions.
				// Base the compensated aspect ratio on the assumption that the original
				// screen resolution is the native resolution -- or at least one that 
				// has square (or nearly square) pixels. 

				float fOrigAspect = (float)origScreenWidth / (float)origScreenHeight;
				float fFullAspect = (float)fullscreenWidth / (float)fullscreenHeight;
				float fAdjustedAspect = fFullAspect * 4.0f/3.0f;
				float fCompensationAspect = fAdjustedAspect / fOrigAspect; 
				float fCompensatedWidth = (float)fullscreenHeight * fCompensationAspect;

				frameparams.width = (int)fCompensatedWidth;
				frameparams.height = fullscreenHeight;
				frameparams.horizoffset = (int)((nRectWidth - frameparams.width) / 2.0f); 
			}
			else  // we're in either a window or fullscreen with NORMAL aspect processing
			{
				// we have a tall narrow window, so pin to the width and
				// adjust the height to force the 3:4 aspect ratio.
				if (nRectHeight >= nRectWidth * 3.0f/4.0f)
				{
					frameparams.height = (int)(nRectWidth * 3.0f/4.0f);
					frameparams.width = nRectWidth;
					frameparams.vertoffset = (int)((nRectHeight - frameparams.height) / 2.0f); 
				}
				// we have a wide, fat window, so pin to the height and 
				// adjust the width to force the 3:4 aspect ratio. 
				else
				{
					frameparams.width = (int)(nRectHeight * 4.0f/3.0f);
					frameparams.height = nRectHeight;
					frameparams.horizoffset = (int)((nRectWidth - frameparams.width) / 2.0f);
				}
			}
		}
		
		/* When doing a frame params refresh, perform these direct3d specific operations */
		/* This should be done regardless of aspect ratio settings                       */
		if (rendermode == DIRECT3D)
		{	
			// Update window dimensions 
			frameparams.d3dWidth = (float)frameparams.width / nRectWidth;
			frameparams.d3dHeight = 1.0f;
		
			// Although Direct3D can handle window resizing with no intervention, it is not 
			// optimal since it does not automatically change the backbuffer size to match the new
			// window size.  This can result in lower quality after a resize.  However, if we take
			// the time to manually reset the device and change the backbuffer size, we can maintain
			// a high quality image.
			resetdevice(nRectWidth, nRectHeight, screenmode, frameparams.scanlinemode, frameparams.filter);
		}

		// Update the title bar with resolution, scale, scanline information
		if (screenmode == WINDOW)
		{
			char buffer[100];
			char scanlinetxt[25];
			int winscale = (int)((float)frameparams.height / 240.0f * 100);
			windowscale = winscale;

			switch (frameparams.scanlinemode)
			{
				case NONE:
					strcpy(scanlinetxt, "");
					break;
				case LOW:
					strcpy(scanlinetxt, "; Scanlines: Low");
					break;
				case MEDIUM:
					strcpy(scanlinetxt, "; Scanlines: Medium");
					break;
				case HIGH:
					strcpy(scanlinetxt, "; Scanlines: High");
					break;
			}
			
			sprintf(buffer, "%s - %dx%d (%d%%)%s",myname, frameparams.width, frameparams.height, winscale, scanlinetxt);
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

