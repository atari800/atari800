/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#define DIRECTDRAW_VERSION 0x0500

#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include "screen.h"
#include "main.h"
#include "atari.h"
#include "colours.h"
#include "log.h"

#define SHOWFRAME 0

static LPDIRECTDRAW lpDD = NULL;
static LPDIRECTDRAWSURFACE lpDDSPrimary = NULL;
static LPDIRECTDRAWSURFACE lpDDSBack = NULL;
static LPDIRECTDRAWPALETTE lpDDPal = NULL;

#define MAX_CLR         0x100
static PALETTEENTRY pal[MAX_CLR];       /* palette */

static int linesize = 0;
static int scrwidth = 320;
static int scrheight = 240;
static void *scraddr = NULL;

void groff(void)
{
  if (lpDD != NULL)
    {
      if (lpDDSPrimary != NULL)
        {
          IDirectDrawSurface3_Release(lpDDSPrimary);
          lpDDSPrimary = NULL;
        }
      if (lpDDPal != NULL)
        {
          IDirectDrawPalette_Release(lpDDPal);
          lpDDPal = NULL;
        }
      IDirectDraw2_Release(lpDD);
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
  DDSURFACEDESC ddsd;
  DDSCAPS ddscaps;
  HRESULT ddrval;
  int i, j;
  int mode = 0;
  int help = FALSE;

  for (i = j = 1; i < *argc; i++)
    {
      if (strcmp(argv[i], "-video") == 0)
      {
        i++;
        mode = atoi(argv[i]);
      }
      else
      {
        help = TRUE;
        if (strcmp(argv[i], "-help") == 0)
        {
          Aprint("\t-video <num>   set video mode #num");
        }
        argv[j++] = argv[i];
      }
    }
  *argc = j;

  if (help)
    return 0;

  switch (mode)
  {
    case 0:
      scrwidth  = 320;
      scrheight = 240;
      break;
    case 1:
      scrwidth  = 400;
      scrheight = 300;
      break;
    default:
      scrwidth  = 320;
      scrheight = 240;
      break;
  }

  ddrval = DirectDrawCreate(NULL, &lpDD, NULL);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  ddrval = IDirectDraw2_SetCooperativeLevel(lpDD, hWndMain,
                                            DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  ddrval = IDirectDraw_SetDisplayMode(lpDD, scrwidth, scrheight, 8);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
    DDSCAPS_FLIP |
    DDSCAPS_COMPLEX;
  ddsd.dwBackBufferCount = 1;
  ddrval = IDirectDraw2_CreateSurface(lpDD, &ddsd, &lpDDSPrimary, NULL);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }
  ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
  ddrval = IDirectDrawSurface3_GetAttachedSurface(lpDDSPrimary,
                                                  &ddscaps, &lpDDSBack);
  if (ddrval != DD_OK)
    {
      return initFail(hWndMain);
    }

  for (i = 0; i < MAX_CLR; i++)
    {
      palette(i, (colortable[i] >> 16) & 0xff,
              (colortable[i] >> 8) & 0xff,
              (colortable[i]) & 0xff);
    }
  IDirectDraw2_CreatePalette(lpDD, DDPCAPS_8BIT,
                             pal, &lpDDPal, NULL);
  if (lpDDPal)
    IDirectDrawSurface3_SetPalette(lpDDSPrimary, lpDDPal);

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

static HRESULT restoreAll(void)
{
  int err;
  err = IDirectDrawSurface3_Restore(lpDDSPrimary);
  return err;
}

void refreshv(UBYTE * scr_ptr)
{
  DDSURFACEDESC desc0;
  int err;
  int x, y;
  long *src, *dst;
  int h, w;

  desc0.dwSize = sizeof(DDSURFACEDESC);
  if ((err = IDirectDrawSurface3_Lock(lpDDSBack, NULL, &desc0,
                                      DDLOCK_WRITEONLY
                                      | DDLOCK_WAIT
                                      ,NULL)) == DD_OK)
    {
      linesize = desc0.lPitch;
      scrwidth = desc0.dwWidth;
      scrheight = desc0.dwHeight;
      scraddr = desc0.lpSurface;

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
            *dst++ = *src++;
        }

      IDirectDrawSurface3_Unlock(lpDDSBack, NULL);
      linesize = 0;
      scrwidth = 0;
      scrheight = 0;
      scraddr = 0;
    }
  else if (err == DDERR_SURFACELOST)
    err = restoreAll();
  else
    {
      char txt[0x100];
      sprintf(txt, "DirectDraw error 0x%x", err);
      MessageBox(hWndMain, txt, myname, MB_OK);
//      printf("error: %x\n", err);
      exit(1);
    }

#if (SHOWFRAME > 0)
  palette(0, 0x20, 0x20, 0);
  palupd(CLR_BACK, 1);
#endif
  err = IDirectDrawSurface3_Flip(lpDDSPrimary, NULL, DDFLIP_WAIT);
  //err = IDirectDrawSurface3_Flip(lpDDSPrimary, NULL, 0);
  if (err == DDERR_SURFACELOST)
    err = restoreAll();
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
Revision 1.3  2002/12/30 17:36:59  knik
don't initialise engine when printing help

Revision 1.2  2001/07/22 06:47:20  knik
DDSURFACEDESC u1 name removed

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
