#ifndef _MAIN_H_
#define _MAIN_H_

extern LPTSTR myname;
extern HWND hWndMain;
extern HINSTANCE myInstance;

// Multi-threaded implementation has no benefit over single threaded and it
// has very odd codepath. I will leave code in place anyway */
//#define MULTITHREADED

#ifndef MULTITHREADED
void MsgPump();
#endif

#endif /* _MAIN_H_ */
