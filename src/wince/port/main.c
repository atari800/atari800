/*
 * main.c - WinCE port specific code
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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

/* Based on Win32 port by  Krzysztof Nikiel */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "main.h"
#include "screen_wince.h"
#include "keyboard.h"
#include "sound.h"
#include "ui.h"

LPTSTR myname = TEXT("Pocket Atari");
char* mynameb = "Pocket Atari";
HWND hWndMain;
HINSTANCE myInstance;
char issmartphone = 0;

extern int wince_main(int argc, char **argv);

static char **gargv = NULL;
static int gargc = 0;

void __cdecl atari_exit(int code)
{
#ifdef MULTITHREADED
	ExitThread(code);
#else
	MsgPump();
#endif
}

void wce_perror(char* s)
{
	TCHAR sUnc[100];
	gr_suspend();
	MultiByteToWideChar(CP_ACP, 0, s, -1, sUnc, 100);
	MessageBox(hWndMain, sUnc, TEXT("Error"), MB_OK|MB_ICONERROR);
	gr_resume();
}

DWORD WINAPI vloop(LPVOID p)
{
	srand(GetTickCount());
	wince_main(gargc, gargv);
	atari_exit(0);
	return 0;
}

static long FAR PASCAL WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static POINT lastClick;
	static PAINTSTRUCT ps;
	switch (message)
	{
	/* All right, I don't understand this. PPC2000 throws VK_LWIN every time button
	   is pressed and PPC2002 adds VK_F21 after each joystick press */
	case WM_KEYDOWN:
		if(wParam != VK_LWIN && wParam != VK_F21) // huh?
			hitbutton((short)wParam);
		return 0;
	case WM_KEYUP:
		if(wParam != VK_LWIN && wParam != VK_F21) // huh?
			releasebutton((short)wParam);
		return 0;
	case WM_LBUTTONDOWN:
		lastClick.x = LOWORD(lParam);
		lastClick.y = HIWORD(lParam);
		ClientToScreen(hWnd, &lastClick);
		tapscreen((short)lastClick.x, (short)lastClick.y);
		SetCapture(hWnd);
		return 0;
	case WM_LBUTTONUP:
		untapscreen((short)lastClick.x, (short)lastClick.y);
		ReleaseCapture();
		return 0;
	case WM_MOUSEMOVE:
		lastClick.x = LOWORD(lParam);
		lastClick.y = HIWORD(lParam);
		ClientToScreen(hWnd, &lastClick);
		dragscreen((short)lastClick.x, (short)lastClick.y);
		return 0;
	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SETFOCUS:
	case WM_ACTIVATE:
		gr_resume();
		return 0;
	case WM_KILLFOCUS:
	case WM_HIBERNATE:
		gr_suspend();
		return 0;
	}
/* In the first version all other events were merrily dropped on floor in singlethreaded
   model. That caused some troubles with debugging and suspected to cause spurious key up
   messages. Nevertheless, this call does not affect performance in any noticeable way and
   it is much nicer to the OS. */
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#ifndef MULTITHREADED
void MsgPump()
{
	MSG msg;
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		WindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
}
#endif


static BOOL initwin(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASS wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL; //LoadIcon(hInstance, IDI_APPLICATION);
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = myname;
	RegisterClass(&wc);

	hWndMain = CreateWindow(myname,
		myname,
		WS_VISIBLE,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		NULL,
		NULL,
		hInstance,
		NULL);

	if(!hWndMain)
		return 1;

	SetWindowPos(hWndMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef MULTITHREADED
	MSG msg;
	HANDLE hth;
#endif

	int i;
	static int argc = 0;
	static char args[0x400];
	static char *argv[100];
	TCHAR platform[100];

	myInstance = hInstance;
	if(initwin(hInstance, nCmdShow))
	{
		return 1;
	}

	if (SystemParametersInfo(SPI_GETPLATFORMTYPE, 100, platform, 0))
		if (wcsstr(platform, _T("martphone")))
			issmartphone = 1;

	if(lpCmdLine)
		WideCharToMultiByte(CP_ACP, 0, lpCmdLine, -1, args, 0x400, NULL, NULL);
	else
		args[0] = 0;
	argv[argc++] = mynameb;
	for(i = 0; i < (sizeof(args) - 1) && args[i]; i++)
	{
		while(args[i] == ' ' && i < (sizeof(args) - 1))
			i++;
		if(args[i] && i < (sizeof(args) - 1))
		{
			argv[argc++] = &args[i];
			if((argc + 1) >= (sizeof(argv) / sizeof(argv[0])))
				break;
		}
		while(args[i] != ' ' && args[i] && i < (sizeof(args) - 1))
			i++;
		args[i] = 0;
	}
	argv[argc] = NULL;

	gargv = argv;
	gargc = argc;

#ifdef MULTITHREADED
	if((hth = CreateThread(NULL, 0x10000, vloop, NULL, 0, NULL)) == NULL)
		return 1;

	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* wait for the other thread to exit */
	WaitForSingleObject((HANDLE)hth, INFINITE);
	return msg.wParam;
#else
	vloop(NULL);
	return 0;
#endif
}
