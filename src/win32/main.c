/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
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

char *myname = "Atari800";
HWND hWndMain;
HINSTANCE myInstance;

int bActive = 0;		/* activity indicator */
static ULONG hth = -1;		/* thread handle */
int vloopexit = 0;		/* exit vloop */
static char **gargv = NULL;
static int gargc = 0;

void exit(int code)
{
  groff();
#ifdef SOUND
  Sound_Exit();
#endif
  uninitinput();
  vloopexit |= 2;
  PostMessage(hWndMain, WM_CLOSE, 0, 0);
  _endthread(code);
}

extern int atari_main(int argc, char **argv);

static void vloop(void *p)
{
  atari_main(gargc, gargv);
  exit(0);
}

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
    case WM_SETCURSOR:
      SetCursor(NULL);
      return TRUE;
    case WM_CREATE:
      break;
    case WM_CLOSE:
      vloopexit |= 1;
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

  ShowWindow(hWndMain, nCmdShow);
  UpdateWindow(hWndMain);

  return 0;
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR
		   lpCmdLine, int nCmdShow)
{
  MSG msg;
  int i;
  static int argc = 0;
  static char args[0x400];
  static char *argv[100];

  myInstance = hInstance;
  if (initwin(hInstance, nCmdShow))
    {
      return 1;
    }

  if (lpCmdLine)
    strncpy(args, lpCmdLine, sizeof(args));
  else
    args[0] = 0;
  argv[argc++] = myname;
  for (i = 0; i < (sizeof(args) - 1) && args[i]; i++)
    {
      while (args[i] == ' ' && i < (sizeof(args) - 1))
	i++;
      if (args[i] && i < (sizeof(args) - 1))
	{
	  argv[argc++] = &args[i];
	  if ((argc + 1) >= (sizeof(argv) / sizeof(argv[0])))
	    break;
	}
      while (args[i] != ' ' && args[i] && i < (sizeof(args) - 1))
	i++;
      args[i] = 0;
    }
  argv[argc] = NULL;

  gargv = argv;
  gargc = argc;

  if ((hth = _beginthread(vloop, 0x10000, NULL)) == -1)
    return 1;

  while (GetMessage(&msg, NULL, 0, 0))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

  /* wait for the other thread to exit */
  WaitForSingleObject((HANDLE)hth, INFINITE);

  return msg.wParam;
}

/*
$Log$
Revision 1.3  2001/05/19 06:12:05  knik
show window before display change

Revision 1.2  2001/04/08 05:51:44  knik
sound calls update

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
