/*
 * screen.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2003 Atari800 development team (see DOC/CREDITS)
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

#define DIRECTDRAW_VERSION 0x0500

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include "screen_win32.h"
#include "main.h"
#include "atari.h"
#include "colours.h"
#include "log.h"

#define SHOWFRAME 0

static LPDIRECTDRAW4 lpDD = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSPrimary = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSBack = NULL;
static LPDIRECTDRAWSURFACE4 lpDDSsrc = NULL;
static LPDIRECTDRAWPALETTE lpDDPal = NULL;

#define MAX_CLR         0x100
static PALETTEENTRY pal[MAX_CLR];       /* palette */

static int linesize = 0;
static int scrwidth = 320;
static int scrheight = 240;
static void *scraddr = NULL;
static int bltgfx = 0;

void groff(void)
{
  if (lpDD != NULL)
    {
      if (lpDDSsrc != NULL)
      {
	IDirectDrawSurface4_Release(lpDDSsrc);
	lpDDSsrc = NULL;
      }
      if (lpDDSPrimary != NULL)
        {
	  IDirectDrawSurface4_Release(lpDDSPrimary);
          lpDDSPrimary = NULL;
        }
      if (lpDDPal != NULL)
        {
          IDirectDrawPalette_Release(lpDDPal);
          lpDDPal = NULL;
        }
      IDirectDraw4_Release(lpDD);
      lpDD = NULL;
    }
}

static int initFail(HWND hwnd)
{
  MessageBox(hwnd, "DirectDraw Init FAILED", myname, MB_OK);
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

  for (i = j = 1; i < *argc; i++)
    {
      if (strcmp(argv[i], "-width") == 0)
      {
        i++;
	width = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-blt") == 0)
      {
	bltgfx = !0;
      }
      else
      {
        if (strcmp(argv[i], "-help") == 0)
        {
	  Aprint("\t-width <num>     Set display mode width");
	  Aprint("\t-blt             Use blitting to draw graphics");
	  help = TRUE;
        }
        argv[j++] = argv[i];
      }
    }
  *argc = j;

  if (help)
    return 0;

  if (width)
  {
    scrwidth = width;
    scrheight = width * 3 / 4;
  }

  ddrval = DirectDrawCreate(NULL, (void *) &lpDD, NULL);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  ddrval = IDirectDraw4_QueryInterface(lpDD, &IID_IDirectDraw4, (void *) &lpDD);
  if (ddrval != DD_OK)
    return initFail(hWndMain);
  ddrval = IDirectDraw4_SetCooperativeLevel(lpDD, hWndMain,
                                            DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }

  if (bltgfx)
  {
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddrval = IDirectDraw4_GetDisplayMode(lpDD, &ddsd);
  if (ddrval != DD_OK)
      return initFail(hWndMain);
    ddrval = IDirectDraw4_SetDisplayMode(lpDD, ddsd.dwWidth, ddsd.dwHeight,
					 32, 0, 0);
  if (ddrval != DD_OK)
      return initFail(hWndMain);
  }
  else
    {
    ddrval = IDirectDraw4_SetDisplayMode(lpDD, scrwidth, scrheight, 8, 0, 0);
    if (ddrval != DD_OK)
      return initFail(hWndMain);
    }

  memset(&ddsd, 0, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
    DDSCAPS_FLIP |
    DDSCAPS_COMPLEX;
  ddsd.dwBackBufferCount = 1;
  ddrval = IDirectDraw4_CreateSurface(lpDD, &ddsd, &lpDDSPrimary, NULL);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  memset(&ddscaps, 0, sizeof(ddscaps));
  ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
  ddrval = IDirectDrawSurface4_GetAttachedSurface(lpDDSPrimary,
                                                  &ddscaps, &lpDDSBack);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }

  if (bltgfx)
  {
    ddrval = IDirectDraw4_GetDisplayMode(lpDD, &ddsd);
    if (ddrval != DD_OK)
      return initFail(hWndMain);

    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    ddsd.dwWidth = 336;
    ddsd.dwHeight = 252;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddrval = IDirectDraw4_CreateSurface(lpDD, &ddsd, &lpDDSsrc, NULL);
    if (ddrval != DD_OK)
      return initFail(hWndMain);

    ddrval = IDirectDrawSurface4_Lock(lpDDSsrc, NULL, &ddsd,
				      DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
    if (ddrval != DD_OK)
      return initFail(hWndMain);

    memset(ddsd.lpSurface, 0, ddsd.lPitch * ddsd.dwHeight);

    ddrval = IDirectDrawSurface4_Unlock(lpDDSsrc, NULL);
  }
  else
  {
  for (i = 0; i < MAX_CLR; i++)
    {
      palette(i, (colortable[i] >> 16) & 0xff,
              (colortable[i] >> 8) & 0xff,
              (colortable[i]) & 0xff);
    }
    IDirectDraw4_CreatePalette(lpDD, DDPCAPS_8BIT,
                             pal, &lpDDPal, NULL);
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

void refreshv(UBYTE * scr_ptr)
{
  DDSURFACEDESC2 desc0;
  int err;
  int x, y;
  UBYTE *src;
  long *dst;
  int h, w;
  DDBLTFX ddbltfx;

  desc0.dwSize = sizeof(desc0);
  if ((err = IDirectDrawSurface4_Lock(bltgfx ? lpDDSsrc : lpDDSBack,
				      NULL, &desc0,
                                      DDLOCK_WRITEONLY
                                      | DDLOCK_WAIT
                                      ,NULL)) == DD_OK)
    {
      linesize = desc0.lPitch;
      scrwidth = desc0.dwWidth;
      scrheight = desc0.dwHeight;
      scraddr = desc0.lpSurface + (bltgfx ? linesize * 6 : 0);

      if (bltgfx)
      {
	for (y = 0; y < ATARI_HEIGHT; y++)
	{
	  dst = (void *) scraddr + y * linesize;
	  src = scr_ptr + y * ATARI_WIDTH;
	  for (x = 0; x < scrwidth; x++)
	    *dst++ = colortable[*src++];
	}
      }
      else
      {
      w = (scrwidth - 336) / 2;
      h = (scrheight - ATARI_HEIGHT) / 2;
      if (w > 0)
        (UBYTE *)scraddr += w;
      if (w < 0)
        (UBYTE *)scr_ptr -= w;
      if (h > 0)
        (UBYTE *)scraddr += linesize * h;
      for (y = 0; y < ATARI_HEIGHT; y++)
        {
          dst = scraddr + y * linesize;
          src = (void *) scr_ptr + y * ATARI_WIDTH;
          for (x = (w >= 0) ? (336 >> 2) : (scrwidth >> 2); x > 0; x--)
	  *dst++ = *((unsigned *) src)++;
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
  else
    {
      char txt[0x100];
      sprintf(txt, "DirectDraw error 0x%x", err);
      MessageBox(hWndMain, txt, myname, MB_OK);
//      printf("error: %x\n", err);
      exit(1);
    }

  if (bltgfx)
  {
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
  //err = IDirectDrawSurface3_Flip(lpDDSPrimary, NULL, 0);
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

/*
$Log$
Revision 1.2  2005/05/13 23:28:15  emuslor
-help cleanup

Revision 1.1  2005/03/03 09:15:43  pfusik
renamed win32/screen.[ch] to win32/screen_win32.[ch]

Revision 1.6  2003/02/24 09:33:34  joy
header cleanup

Revision 1.5  2003/02/15 09:48:23  knik
added GPL notice
tiny code fix

Revision 1.4  2003/02/07 17:23:22  knik
updated DirectDraw interface
added optional blit stretching support
fixed -help option

Revision 1.3  2002/12/30 17:36:59  knik
don't initialise engine when printing help

Revision 1.2  2001/07/22 06:47:20  knik
DDSURFACEDESC u1 name removed

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
