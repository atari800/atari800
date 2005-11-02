/*
 * joystick.c - Win32 port specific code
 *
 * Copyright (C) 2005 James Wilkinson
 * Copyright (C) 2005 Atari800 development team (see DOC/CREDITS)
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
#include "input.h"
#include "log.h"

#include "joystick.h"
#include "main.h"

HRESULT
SetDIDwordProperty(LPDIRECTINPUTDEVICE pdev, REFGUID guidProperty,
		   DWORD dwObject, DWORD dwHow, DWORD dwValue);

tjoystat joystat;
static LPDIRECTINPUTDEVICE2 dijoy[NUM_STICKS] = {NULL};

int joyreacquire(int num)
{
  if (!dijoy[num])
    return 1;
  if (IDirectInputDevice_Acquire(dijoy[num]) >= 0)
    return 0;
  else
    return 1;
}

int procjoy(int num)
{
  DIJOYSTATE js;
  HRESULT hRes;

  if (!dijoy[num])
    return 1;
  IDirectInputDevice2_Poll(dijoy[num]);

  hRes = IDirectInputDevice_GetDeviceState(dijoy[num], sizeof(DIJOYSTATE), &js);
  if (hRes != DI_OK)
  {
    if (hRes == DIERR_INPUTLOST)
      joyreacquire(num);
    return 1;
  }

  joystat.trig = (js.rgbButtons[ 0 ] & 0x80) ? 1 : 0;
  if (js.lX == 0)
  {
    if (js.lY == 0) joystat.stick = STICK_CENTRE;
    else if (js.lY < 0) joystat.stick = STICK_FORWARD;
    else joystat.stick = STICK_BACK;
  }
  else if (js.lX < 0)
  {
    if (js.lY == 0) joystat.stick = STICK_LEFT;
    else if (js.lY < 0) joystat.stick = STICK_UL;
    else joystat.stick = STICK_LL;
  }
  else
  {
    if (js.lY == 0) joystat.stick = STICK_RIGHT;
    else if (js.lY < 0) joystat.stick = STICK_UR;
    else joystat.stick = STICK_LR;
  }

  return 0;
}

static BOOL CALLBACK joycallback(LPCDIDEVICEINSTANCE pdevinst, LPVOID pv)
{
  DIPROPRANGE dipr;
  HRESULT hRes;
  LPDIRECTINPUTDEVICE pdev;
  LPDIRECTINPUT lpdi = pv;
  static int i = 0;

  if (i > 1) return DIENUM_STOP;

  if (IDirectInput_CreateDevice(lpdi, &pdevinst->guidInstance, &pdev, NULL) != DI_OK)
    {
      return DIENUM_STOP;
    }
  if (IDirectInputDevice_SetDataFormat(pdev, &c_dfDIJoystick) != DI_OK)
    {
      IDirectInputDevice_Release(pdev);
      return DIENUM_STOP;
    }
  if (IDirectInputDevice_SetCooperativeLevel(pdev, hWndMain,
   DISCL_NONEXCLUSIVE | DISCL_BACKGROUND) != DI_OK)
    {
      IDirectInputDevice_Release(pdev);
      return DIENUM_STOP;
    }

  dipr.diph.dwSize = sizeof(dipr);
  dipr.diph.dwHeaderSize = sizeof(dipr.diph);
  dipr.diph.dwObj = DIJOFS_X;
  dipr.diph.dwHow = DIPH_BYOFFSET;
  dipr.lMin = -1000;
  dipr.lMax = 1000;

  if (IDirectInputDevice_SetProperty(pdev, DIPROP_RANGE, &dipr.diph) != DI_OK)
  {
    IDirectInputDevice_Release(pdev);
    return DIENUM_STOP;
  }

  dipr.diph.dwObj = DIJOFS_Y;

  if (IDirectInputDevice_SetProperty(pdev, DIPROP_RANGE, &dipr.diph) != DI_OK)
  {
    IDirectInputDevice_Release(pdev);
    return DIENUM_STOP;
  }
  if (SetDIDwordProperty(pdev, DIPROP_DEADZONE, DIJOFS_X, DIPH_BYOFFSET, 5000) != DI_OK)
  {
    IDirectInputDevice_Release(pdev);
    return DIENUM_STOP;
  }
  if (SetDIDwordProperty(pdev, DIPROP_DEADZONE, DIJOFS_Y, DIPH_BYOFFSET, 5000) != DI_OK)
  {
    IDirectInputDevice_Release(pdev);
    return DIENUM_STOP;
  }

  hRes = pdev->lpVtbl->QueryInterface(pdev, &IID_IDirectInputDevice2,
				      (LPVOID*) &dijoy[i]);
  IDirectInputDevice_Release(pdev);
  if (hRes < 0)
    return DIENUM_STOP;
  joyreacquire(i);
  Aprint("joystick %d found!", i);
  i ++;
  return DIENUM_CONTINUE;
}

int initjoystick(void)
{
  LPDIRECTINPUT pdi;
  HRESULT hRes;

  if (DirectInputCreate(myInstance, DIRECTINPUT_VERSION, &pdi, NULL) != DI_OK)
    {
      return 1;
    }
  hRes = IDirectInput_EnumDevices(pdi, DIDEVTYPE_JOYSTICK, joycallback, pdi, DIEDFL_ATTACHEDONLY);
  IDirectInput_Release(pdi);
  if(!dijoy[0]/*hRes != DI_OK*/)
  {
    return 1;
  }
  return 0;
}

void uninitjoystick(void)
{
  int i;

  for (i = 0; i < NUM_STICKS; i ++)
  {
    if (dijoy[i])
    {
      IDirectInputDevice_Unacquire(dijoy[i]);
      IDirectInputDevice_Release(dijoy[i]);
      dijoy[i] = NULL;
    }
  }
}
