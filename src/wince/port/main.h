/* (C) 2001  Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */
#ifndef A800_WCE_MAIN_H
#define A800_WCE_MAIN_H

extern LPTSTR myname;
extern HWND hWndMain;
extern HINSTANCE myInstance;

/* Due to imperfection in Windows CE emulation we have to test
   different codepath on emulator.
*/
#ifdef _WIN32_WCE_EMULATION
#define MULTITHREADED
#endif

#ifndef MULTITHREADED
void MsgPump();
#endif

#endif
