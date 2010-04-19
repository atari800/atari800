/*
 * joystick.c - Win32 port specific code
 *
 * Copyright (C) 2005 James Wilkinson
 * Copyright (C) 2005-2010 Atari800 development team (see DOC/CREDITS)
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

#define DIRECTINPUT_VERSION	    0x0500

#include "config.h"
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
  int i;

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

  /* process trigger */
  joystat.trig = (js.rgbButtons[ 0 ] & 0x80) ? 1 : 0;
  
  /* process programmable buttons */
  for (i = 0; i < MAX_PROG_BUTTONS; i++) { 
	 joystat.jsbutton[num][i][STATE] = (js.rgbButtons[ i + 1 ] & 0x80) ? 1 : 0;
  }
  
  /* process primary joystick. X and Y joystick axis*/
  if (js.lX == 0)
  {
    if (js.lY == 0) joystat.stick = INPUT_STICK_CENTRE;
    else if (js.lY < 0) joystat.stick = INPUT_STICK_FORWARD;
    else joystat.stick = INPUT_STICK_BACK;
  }
  else if (js.lX < 0)
  {
    if (js.lY == 0) joystat.stick = INPUT_STICK_LEFT;
    else if (js.lY < 0) joystat.stick = INPUT_STICK_UL;
    else joystat.stick = INPUT_STICK_LL;
  }
  else
  {
    if (js.lY == 0) joystat.stick = INPUT_STICK_RIGHT;
    else if (js.lY < 0) joystat.stick = INPUT_STICK_UR;
    else joystat.stick = INPUT_STICK_LR;
  }
  
  /* process second joystick on the same gamepad (for dual stick
     games like Robotron). Second stick must use Z-axis (throttle)
	 and Z-axis rotation (rudder) for it's X and Y motions.  */
  if (alternateJoystickMode == JOY_DUAL_MODE) 
  { 
	  if (js.lZ == 0)
	  {
		if (js.lRz == 0) joystat.stick_1 = INPUT_STICK_CENTRE;
		else if (js.lRz < 0) joystat.stick_1 = INPUT_STICK_FORWARD;
		else joystat.stick_1 = INPUT_STICK_BACK;
	  }
	  else if (js.lZ < 0)
	  {
		if (js.lRz == 0) joystat.stick_1 = INPUT_STICK_LEFT;
		else if (js.lRz < 0) joystat.stick_1 = INPUT_STICK_UL;
		else joystat.stick_1 = INPUT_STICK_LL;
	  }
	  else
	  {
		if (js.lRz == 0) joystat.stick_1 = INPUT_STICK_RIGHT;
		else if (js.lRz < 0) joystat.stick_1 = INPUT_STICK_UR;
		else joystat.stick_1 = INPUT_STICK_LR;
	  }
  }
  /* end dual stick processing */
  
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
  
  /* Initialize the Z and Z-Rotation axis for dual stick mode */  
  /* No reason to stop if these fail, even if GetCapabilities says they should not */

  dipr.diph.dwObj = DIJOFS_Z;
  IDirectInputDevice_SetProperty(pdev, DIPROP_RANGE, &dipr.diph);
  dipr.diph.dwObj = DIJOFS_RZ;
  IDirectInputDevice_SetProperty(pdev, DIPROP_RANGE, &dipr.diph);
  SetDIDwordProperty(pdev, DIPROP_DEADZONE, DIJOFS_Z, DIPH_BYOFFSET, 5000);
  SetDIDwordProperty(pdev, DIPROP_DEADZONE, DIJOFS_RZ, DIPH_BYOFFSET, 5000);

  /* End Z and Z-Rotation axis initialization */
  
  hRes = IDirectInputDevice_QueryInterface(pdev, &IID_IDirectInputDevice2,
				      (LPVOID*) &dijoy[i]);
  IDirectInputDevice_Release(pdev);
  if (hRes < 0)
    return DIENUM_STOP;
  joyreacquire(i);
  Log_print("joystick %d found!", i);
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
