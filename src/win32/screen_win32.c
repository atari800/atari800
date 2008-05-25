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
#define DIRECTDRAW_VERSION 0x0500
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>

#include "atari.h"
#include "colours.h"
#include "log.h"
#include "util.h"

#include "main.h"
#include "screen_win32.h"

#define SHOWFRAME 0

int windowed = FALSE;

static LPDIRECTDRAW4 lpDD = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSPrimary = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSBack = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSsrc = NULL;
static LPDIRECTDRAWPALETTE lpDDPal = NULL;

#define MAX_CLR 0x100
static PALETTEENTRY pal[MAX_CLR]; /* palette */

static int linesize = 0;
static int scrwidth = 320;
static int scrheight = 240;
static UBYTE *scraddr = NULL;
static int bltgfx = 0;

void groff(void)
{
	if (lpDD != NULL) {
		if (lpDDSsrc != NULL) {
			IDirectDrawSurface4_Release(lpDDSsrc);
			lpDDSsrc = NULL;
		}
		if (lpDDSPrimary != NULL) {
			IDirectDrawSurface4_Release(lpDDSPrimary);
			lpDDSPrimary = NULL;
		}
		if (lpDDPal != NULL) {
			IDirectDrawPalette_Release(lpDDPal);
			lpDDPal = NULL;
		}
		IDirectDraw4_Release(lpDD);
		lpDD = NULL;
	}
}

LRESULT CALLBACK Atari_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static BOOL initwin(void)
{
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

	if (windowed) {
		hWndMain = CreateWindowEx(
			0, myname, myname, WS_TILEDWINDOW | WS_SIZEBOX | WS_DLGFRAME, CW_USEDEFAULT, CW_USEDEFAULT,
			GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2,
			NULL, NULL, myInstance, NULL);
	}
	else {
		hWndMain = CreateWindowEx(
			0, myname, myname, WS_POPUP, 0, 0,
			GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
			NULL, NULL, myInstance, NULL);
	}

	if (!hWndMain)
		return 1;
	return 0;
}

static int initFail(HWND hwnd, const char *func, HRESULT hr)
{
	char txt[256];
	sprintf(txt, "DirectDraw Init FAILED: %s returned 0x%x", func, (unsigned int)hr);
	MessageBox(hwnd, txt, myname, MB_OK);
	groff();
	DestroyWindow(hwnd);
	return 1;
}

int gron(int *argc, char *argv[])
{
	DDSURFACEDESC2 ddsd;
	DDSCAPS2 ddscaps;
	HRESULT ddrval;
	int i, j;
	int width = 0;
	int help = FALSE;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-windowed") == 0)
			windowed = TRUE;
		else if (strcmp(argv[i], "-width") == 0)
			width = Util_sscandec(argv[++i]);
		else if (strcmp(argv[i], "-blt") == 0)
			bltgfx = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-windowed        Run in a window");
				Aprint("\t-width <num>     Set display mode width");
				Aprint("\t-blt             Use blitting to draw graphics");
				help = TRUE;
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;

	initwin();
	if (help || windowed)
		return 0;

	if (width > 0) {
		scrwidth = width;
		scrheight = width * 3 / 4;
	}

	ddrval = DirectDrawCreate(NULL, (void *) &lpDD, NULL);
	if (FAILED(ddrval))
		return initFail(hWndMain, "DirectDrawCreate", ddrval);
	ddrval = IDirectDraw4_QueryInterface(lpDD, &IID_IDirectDraw4, (void *) &lpDD);
	if (FAILED(ddrval))
		return initFail(hWndMain, "QueryInterface", ddrval);
	ddrval = IDirectDraw4_SetCooperativeLevel(lpDD, hWndMain, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	if (FAILED(ddrval))
		return initFail(hWndMain, "SetCooperativeLevel", ddrval);

	if (bltgfx) {
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddrval = IDirectDraw4_GetDisplayMode(lpDD, &ddsd);
		if (FAILED(ddrval))
			return initFail(hWndMain, "GetDisplayMode", ddrval);
		ddrval = IDirectDraw4_SetDisplayMode(lpDD, ddsd.dwWidth, ddsd.dwHeight, 32, 0, 0);
	}
	else {
		ddrval = IDirectDraw4_SetDisplayMode(lpDD, scrwidth, scrheight, 8, 0, 0);
	}
	if (FAILED(ddrval)) {
		if ((ddrval == DDERR_INVALIDMODE || ddrval == DDERR_UNSUPPORTED) && !bltgfx && width != 640) {
			/* 320x240 results in DDERR_INVALIDMODE on my Win98SE / Radeon 9000 */
			/* 320x240 results in DDERR_UNSUPPORTED on my WinXP / Toshiba laptop */
			MessageBox(hWndMain,
				"DirectDraw does not support the requested display mode.\n"
				"Try running with \"-blt\" or \"-width 640\" on the command line.",
				myname, MB_OK);
			groff();
			DestroyWindow(hWndMain);
			return 1;
		}
		return initFail(hWndMain, "SetDisplayMode", ddrval);
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;
	ddrval = IDirectDraw4_CreateSurface(lpDD, &ddsd, &lpDDSPrimary, NULL);
	if (FAILED(ddrval))
		return initFail(hWndMain, "CreateSurface", ddrval);
	memset(&ddscaps, 0, sizeof(ddscaps));
	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	ddrval = IDirectDrawSurface4_GetAttachedSurface(lpDDSPrimary, &ddscaps, &lpDDSBack);
	if (FAILED(ddrval))
		return initFail(hWndMain, "GetAttachedSurface", ddrval);

	if (bltgfx) {
		ddrval = IDirectDraw4_GetDisplayMode(lpDD, &ddsd);
		if (FAILED(ddrval))
			return initFail(hWndMain, "GetDisplayMode", ddrval);

		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		ddsd.dwWidth = 336;
		ddsd.dwHeight = 252;
		ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
		ddrval = IDirectDraw4_CreateSurface(lpDD, &ddsd, &lpDDSsrc, NULL);
		if (FAILED(ddrval))
			return initFail(hWndMain, "CreateSurface", ddrval);

		ddrval = IDirectDrawSurface4_Lock(lpDDSsrc, NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
		if (FAILED(ddrval))
			return initFail(hWndMain, "Lock", ddrval);

		memset(ddsd.lpSurface, 0, ddsd.lPitch * ddsd.dwHeight);

		ddrval = IDirectDrawSurface4_Unlock(lpDDSsrc, NULL);
	}
	else {
		for (i = 0; i < MAX_CLR; i++)
			palette(i, Palette_GetR(i), Palette_GetG(i), Palette_GetB(i));
		IDirectDraw4_CreatePalette(lpDD, DDPCAPS_8BIT, pal, &lpDDPal, NULL);
		if (lpDDPal)
			IDirectDrawSurface4_SetPalette(lpDDSPrimary, lpDDPal);
	}

	return 0;
}

void palupd(int beg, int cnt)
{
	IDirectDrawPalette_SetEntries(lpDDPal, 0, beg, cnt, pal);
}

void palette(int ent, UBYTE r, UBYTE g, UBYTE b)
{
	if (ent >= MAX_CLR)
		return;
	pal[ent].peRed = r;
	pal[ent].peGreen = g;
	pal[ent].peBlue = b;
	pal[ent].peFlags = 0;
}

/* Platform-specific function to update the palette if it changed */
void Atari_PaletteUpdate(void)
{
	if(lpDDPal != NULL) {
		int i;
		for (i = 0; i < MAX_CLR; i++) {
			palette(i, Palette_GetR(i), Palette_GetG(i), Palette_GetB(i));
		}
		palupd(0, MAX_CLR);
	}
}

void refreshv_win32api(UBYTE *scr_ptr)
{
	PAINTSTRUCT ps;
	HDC hdc;
	HDC hCdc;
	BITMAPINFO bi;
	DWORD *bitmap_bits = NULL;
	ULONG   ulWindowWidth, ulWindowHeight;      /* window width/height */
	HBITMAP hBitmap;
	RECT rt;
	int i;
	int j;

	GetClientRect(hWndMain, &rt);
	InvalidateRect(hWndMain, &rt, FALSE);
	hdc = BeginPaint(hWndMain, &ps);
	ulWindowWidth = rt.right - rt.left;
	ulWindowHeight = rt.bottom - rt.top;

	/* make sure we have at least some window size */
	if ((!ulWindowWidth) || (!ulWindowHeight))
		return;

	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); /* structure size in bytes */
	bi.bmiHeader.biWidth = ATARI_WIDTH-48;
	bi.bmiHeader.biHeight = ATARI_HEIGHT;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB; /* BI_BITFIELDS OR BI_RGB??? */
	bi.bmiHeader.biSizeImage = 0; /* for BI_RGB set to 0 */
	bi.bmiHeader.biXPelsPerMeter = 2952; /* 75dpi=2952bpm */
	bi.bmiHeader.biYPelsPerMeter = 2952; /* 75dpi=2952bpm */
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	hCdc = CreateCompatibleDC(hdc);
	hBitmap = CreateDIBSection(hCdc, &bi, DIB_RGB_COLORS, (void *)&bitmap_bits, NULL, 0);
	if (!hBitmap) {
		MessageBox(hWndMain, "Could not create bitmap", myname, MB_OK);
		DestroyWindow(hWndMain);
		return;
	}

	/* Copying the atari screen to bitmap. */
	for (i = 0; i < ATARI_HEIGHT; i++) {
		for (j = 0; j < ATARI_WIDTH - 48; j++) {
			*bitmap_bits++ = colortable[*scr_ptr++];
		}
		/* The last 48 columns of the screen are not used */
		scr_ptr+=48;
	}

	SelectObject(hCdc, hBitmap);

	/* Draw the bitmap on the screen. The image must be flipped vertically */
	if (!StretchBlt(hdc, 0, 0, ulWindowWidth, ulWindowHeight,
			hCdc, 0, bi.bmiHeader.biHeight, bi.bmiHeader.biWidth, -bi.bmiHeader.biHeight, SRCCOPY)) {
		MessageBox(hWndMain, "Could not StretchBlt", myname, MB_OK);
		DestroyWindow(hWndMain);
	}

	DeleteDC(hCdc);
	DeleteObject(hBitmap);

	EndPaint(hWndMain, &ps);

	ValidateRect(hWndMain, &rt);
}

void refreshv(UBYTE *scr_ptr)
{
	DDSURFACEDESC2 desc0;
	int err;
	int x, y;
	UBYTE *srcb;
	ULONG *srcl;
	ULONG *dst;
	int h, w;
	DDBLTFX ddbltfx;

	if (windowed) {
		refreshv_win32api(scr_ptr);
		return;
	}

	desc0.dwSize = sizeof(desc0);
	err = IDirectDrawSurface4_Lock(bltgfx ? lpDDSsrc : lpDDSBack,
			NULL, &desc0, DDLOCK_WRITEONLY | DDLOCK_WAIT ,NULL);
	if (err == DD_OK) {
		linesize = desc0.lPitch;
		scrwidth = desc0.dwWidth;
		scrheight = desc0.dwHeight;
		scraddr = (UBYTE *) desc0.lpSurface + (bltgfx ? linesize * 6 : 0);

		if (bltgfx) {
			for (y = 0; y < ATARI_HEIGHT; y++) {
				dst = (ULONG *) (scraddr + y * linesize);
				srcb = scr_ptr + y * ATARI_WIDTH;
				for (x = 0; x < scrwidth; x++)
					*dst++ = colortable[*srcb++];
			}
		}
		else {
			w = (scrwidth - 336) / 2;
			h = (scrheight - ATARI_HEIGHT) / 2;
			if (w > 0)
				scraddr += w;
			else if (w < 0)
				scr_ptr -= w;
			if (h > 0)
				scraddr += linesize * h;
			for (y = 0; y < ATARI_HEIGHT; y++) {
				dst = (ULONG *) (scraddr + y * linesize);
				srcl = (ULONG *) (scr_ptr + y * ATARI_WIDTH);
				for (x = (w >= 0) ? (336 >> 2) : (scrwidth >> 2); x > 0; x--)
					*dst++ = *srcl++;
			}
		}

		IDirectDrawSurface4_Unlock(bltgfx ? lpDDSsrc : lpDDSBack, NULL);
		linesize = 0;
		scrwidth = 0;
		scrheight = 0;
		scraddr = 0;
	}
	else if (err == DDERR_SURFACELOST)
		err = IDirectDrawSurface4_Restore(bltgfx ? lpDDSsrc : lpDDSBack);
	else {
		char txt[256];
		sprintf(txt, "DirectDraw error 0x%x", err);
		MessageBox(hWndMain, txt, myname, MB_OK);
		/* printf("error: %x\n", err); */
		exit(1);
	}

	if (bltgfx) {
		memset(&ddbltfx, 0, sizeof(ddbltfx));
		ddbltfx.dwSize = sizeof(ddbltfx);
		err = IDirectDrawSurface4_Blt(lpDDSBack, NULL, lpDDSsrc,
				NULL, DDBLT_WAIT, &ddbltfx);
		if (err == DDERR_SURFACELOST)
			err = IDirectDrawSurface4_Restore(lpDDSBack);
	}

#if (SHOWFRAME > 0)
	palette(0, 0x20, 0x20, 0);
	palupd(CLR_BACK, 1);
#endif
	err = IDirectDrawSurface4_Flip(lpDDSPrimary, NULL, DDFLIP_WAIT);
	/* err = IDirectDrawSurface3_Flip(lpDDSPrimary, NULL, 0); */
	if (err == DDERR_SURFACELOST)
		err = IDirectDrawSurface4_Restore(lpDDSPrimary);
#if (SHOWFRAME > 0)
	palette(0, 0x0, 0x20, 0x20);
	palupd(CLR_BACK, 1);
#endif
#if (SHOWFRAME > 0)
	palette(0, 0x0, 0x0, 0x0);
	palupd(CLR_BACK, 1);
#endif
}
