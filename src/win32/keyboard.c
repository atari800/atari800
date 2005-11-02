/*
 * keyboard.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2005 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"
#define DIRECTINPUT_VERSION	    0x0500
#include <windows.h>
#include <dinput.h>

#include "atari.h"

#include "main.h"
#include "keyboard.h"

#define KEYBUFSIZE 0x40

static LPDIRECTINPUTDEVICE2 dikb0 = NULL;
int pause_hit;

int kbcode;
UBYTE kbhits[KBCODES];

int kbreacquire(void)
{
  if (!dikb0)
    return 1;
  if (IDirectInputDevice_Acquire(dikb0) >= 0)
    return 0;
  else
    return 1;
}

int prockb(void)
{
  DIDEVICEOBJECTDATA que[KEYBUFSIZE];
  DWORD dwEvents;
  DWORD i;
  HRESULT hRes;

  dwEvents = KEYBUFSIZE;
  hRes = IDirectInputDevice_GetDeviceData(dikb0,
					  sizeof(DIDEVICEOBJECTDATA),
					  que, &dwEvents, 0);
  if (hRes != DI_OK)
    {
      if ((hRes == DIERR_INPUTLOST))
	kbreacquire();
      return 1;
    }

  for (i = 0; i < dwEvents; i++)
    {
#if 0
      printf("%02x(%02x)\n", que[i].dwOfs, que[i].dwData);
#endif
      if (que[i].dwOfs >= KBCODES)
	continue;
      if (que[i].dwOfs == DIK_PAUSE)
	{
	  if (que[i].dwData)
	    pause_hit = 1;
	  continue;
	}
      if (que[i].dwData)
	kbhits[kbcode = que[i].dwOfs] = 1;
      else
	{
	  kbhits[kbcode = que[i].dwOfs] = 0;
	  kbcode |= 0x100;
	}
    }
  return 0;
}

void uninitinput(void)
{
  if (dikb0)
    {
      IDirectInputDevice_Unacquire(dikb0);
      IDirectInputDevice_Release(dikb0);
      dikb0 = NULL;
    }
}

HRESULT
SetDIDwordProperty(LPDIRECTINPUTDEVICE pdev, REFGUID guidProperty,
		   DWORD dwObject, DWORD dwHow, DWORD dwValue)
{
  DIPROPDWORD dipdw;

  dipdw.diph.dwSize = sizeof(dipdw);
  dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
  dipdw.diph.dwObj = dwObject;
  dipdw.diph.dwHow = dwHow;
  dipdw.dwData = dwValue;

  return pdev->lpVtbl->SetProperty(pdev, guidProperty, &dipdw.diph);
}

static int initkb(LPDIRECTINPUT pdi)
{
  LPDIRECTINPUTDEVICE pdev;
  HRESULT hRes;

  if (IDirectInput_CreateDevice(pdi,
				&GUID_SysKeyboard, &pdev, NULL) != DI_OK)
    {
      return 1;
    }
  if (IDirectInputDevice_SetDataFormat(pdev, &c_dfDIKeyboard) != DI_OK)
    {
      IDirectInputDevice_Release(pdev);
      return 1;
    }
  if (IDirectInputDevice_SetCooperativeLevel(pdev, hWndMain,
					     DISCL_NONEXCLUSIVE |
      DISCL_FOREGROUND) != DI_OK)
    {
      IDirectInputDevice_Release(pdev);
      return 1;
    }
  if (SetDIDwordProperty(pdev, DIPROP_BUFFERSIZE, 0, DIPH_DEVICE,
      KEYBUFSIZE) != DI_OK)
    {
      IDirectInputDevice_Release(pdev);
      return 1;
    }

  hRes = pdev->lpVtbl->QueryInterface(pdev, &IID_IDirectInputDevice2,
				      (LPVOID *) & dikb0);
  if (hRes < 0)
    return 1;
  IDirectInputDevice_Release(pdev);
  kbreacquire();
  return 0;
}

int initinput(void)
{
  int i;
  LPDIRECTINPUT pdi;

  if (DirectInputCreate(myInstance, 0x0300, &pdi, NULL) != DI_OK)
    {
      return 1;
    }
  i = initkb(pdi);
  IDirectInput_Release(pdi);
  if (i)
    return i;
  return 0;
}

void clearkb(void)
{
  int i;
  for (i = 0; i < KBCODES; i++)
    kbhits[i] = 0;
  pause_hit = 0;
  kbcode = 0;
}
