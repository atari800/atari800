/*
 * main.c - Win32 port specific code
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include "config.h"
#include "main.h"
#include "screen.h"
#include "keyboard.h"
#include "sound.h"
#include "input.h"
#include "ataripcx.h"
#include "ui.h"
#include "rt-config.h"
#include "platform.h"

char *myname = "Atari800";
HWND hWndMain;
HINSTANCE myInstance;

static int bActive = 0;		/* activity indicator */

#if 1
void exit(int code)
{
  MSG msg;

  PostMessage(hWndMain, WM_CLOSE, 0, 0);

  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  ExitProcess(code);
}
#endif

static long FAR PASCAL WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_ACTIVATEAPP:
      bActive = wParam;
      if (bActive)
	{
	  kbreacquire();
#ifdef SOUND
	  Sound_Continue();
#endif
	}
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
      mouse_buttons = ((wParam & MK_LBUTTON) ? 1 : 0)
	| ((wParam & MK_RBUTTON) ? 2 : 0);
      break;
    case WM_SETCURSOR:
      SetCursor(NULL);
      return TRUE;
    case WM_CREATE:
      break;
    case WM_CLOSE:
      groff();
#ifdef SOUND
      Sound_Exit();
#endif
      uninitinput();
      break;
    case WM_DESTROY:
      PostQuitMessage(10);
      break;
    }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

static BOOL initwin(HINSTANCE hInstance, int nCmdShow)
{
  WNDCLASS wc;

  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(BLACK_BRUSH);
  wc.lpszMenuName = myname;
  wc.lpszClassName = myname;
  RegisterClass(&wc);

  hWndMain = CreateWindowEx(
			     0,
			     myname,
			     myname,
			     WS_POPUP,
			     0,
			     0,
			     GetSystemMetrics(SM_CXSCREEN),
			     GetSystemMetrics(SM_CYSCREEN),
			     NULL,
			     NULL,
			     hInstance,
			     NULL);

  if (!hWndMain)
    {
      return 1;
    }

  return 0;
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR
		   lpCmdLine, int nCmdShow)
{
  MSG msg;
  POINT mouse;
  static int mouse_center_x = 100;
  static int mouse_center_y = 100;

  myInstance = hInstance;
  if (initwin(hInstance, nCmdShow))
    {
      return 1;
    }

  /* initialise Atari800 core */
  if (!Atari800_Initialise(&_argc, _argv))
    return 3;

  msg.message = WM_NULL;

  /* main loop */
  while (TRUE) {
    static int test_val = 0;
    int keycode;

  start:
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    if (msg.message == WM_QUIT)
      break;

    if (!bActive)
      goto start;

    keycode = Atari_Keyboard();

    switch (keycode) {
    case AKEY_COLDSTART:
      Coldstart();
      break;
    case AKEY_WARMSTART:
      Warmstart();
      break;
    case AKEY_EXIT:
      Atari800_Exit(FALSE);
      exit(1);
    case AKEY_UI:
#ifdef SOUND
      Sound_Pause();
#endif
      ui((UBYTE *)atari_screen);
#ifdef SOUND
      Sound_Continue();
#endif
      break;
    case AKEY_SCREENSHOT:
      Save_PCX_file(FALSE, Find_PCX_name());
      break;
    case AKEY_SCREENSHOT_INTERLACE:
      Save_PCX_file(TRUE, Find_PCX_name());
      break;
    case AKEY_BREAK:
      key_break = 1;
      break;
    default:
      key_break = 0;
      key_code = keycode;
      break;
    }

    GetCursorPos(&mouse);
    mouse_delta_x = mouse.x - mouse_center_x;
    mouse_delta_y = mouse.y - mouse_center_y;
    if (mouse_delta_x | mouse_delta_y)
      SetCursorPos(mouse_center_x, mouse_center_y);

    if (++test_val == refresh_rate) {
      Atari800_Frame(EMULATE_FULL);
#ifndef DONT_SYNC_WITH_HOST
      atari_sync(); /* here seems to be the best place to sync */
#endif
      Atari_DisplayScreen((UBYTE *) atari_screen);
      test_val = 0;
    }
    else {
#ifdef VERY_SLOW
      Atari800_Frame(EMULATE_BASIC);
#else	/* VERY_SLOW */
      Atari800_Frame(EMULATE_NO_SCREEN);
#ifndef DONT_SYNC_WITH_HOST
      atari_sync();
#endif
#endif	/* VERY_SLOW */
    }
  }

  return msg.wParam;
}

/*
$Log$
Revision 1.9  2003/02/24 09:33:33  joy
header cleanup

Revision 1.8  2002/07/11 16:23:21  knik
shutdown code moved from exit() to WindowProc()

Revision 1.7  2002/07/10 16:59:54  knik
Enabled and updated exit(). Atari exited without closing graphics and sound.
Now should exit more nicely.

Revision 1.6  2001/10/03 16:17:20  knik
mouse input

Revision 1.5  2001/09/25 17:38:27  knik
added main loop; threading removed

Revision 1.4  2001/08/27 04:48:28  knik
_endthread() called without parameter

Revision 1.3  2001/05/19 06:12:05  knik
show window before display change

Revision 1.2  2001/04/08 05:51:44  knik
sound calls update

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
