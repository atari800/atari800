#ifndef MAIN_H_
#define MAIN_H_

extern char *myname;
extern HWND hWndMain;
extern HMENU hMainMenu;
extern HINSTANCE myInstance;
LRESULT CALLBACK Atari_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void SuppressNextQuitMessage(void);

#endif /* MAIN_H_ */
