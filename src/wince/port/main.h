/* (C) 2001  Vasyl Tsvirkunov */
/* Based on Win32 port by  Krzysztof Nikiel */
#ifndef A800_WCE_MAIN_H
#define A800_WCE_MAIN_H

extern LPTSTR myname;
extern HWND hWndMain;
extern HINSTANCE myInstance;

// Multi-threaded implementation has no benefit over single threaded and it
// has very odd codepath. I will leave code in place anyway */
//#define MULTITHREADED

#ifndef MULTITHREADED
void MsgPump();
#endif

#endif
