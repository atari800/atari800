#ifndef MAIN_H_
#define MAIN_H_

extern char *myname;
extern HWND hWndMain;
extern HINSTANCE myInstance;
extern BOOL useconsole;
extern int internalkey;

extern HMENU hMainMenu;
extern HMENU hFileMenu;
extern HMENU hManageMenu;
extern HMENU hSystemMenu;
extern HMENU hDisplayMenu;
extern HMENU hHelpMenu;

LRESULT CALLBACK Atari_WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void SuppressNextQuitMessage(void);
void DoEvents(void);
void SetTVModeMenuItem(int mode);

typedef struct {
  int keystroke;
  int function;
} keycommand_t;

void GetCommandKey(keycommand_t *keycommand);

#endif /* MAIN_H_ */
