/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <dinput.h>

#include "config.h"
#include "platform.h"
#include "input.h"
#include "screen.h"
#include "keyboard.h"
#include "main.h"
#include "sound.h"
#include "monitor.h"
#include "diskled.h"

static int usesnd = 1;

extern int refresh_rate;

extern int alt_function;
static int kbjoy = 1;
static UBYTE joydefs[] =
{
  DIK_NUMPAD0,			/* fire */
  DIK_NUMPAD7,			/* up/left */
  DIK_NUMPAD8,			/* up */
  DIK_NUMPAD9,			/* up/right */
  DIK_NUMPAD4,			/* left */
  DIK_NUMPAD6,			/* right */
  DIK_NUMPAD1,			/* down/left */
  DIK_NUMPAD2,			/* down */
  DIK_NUMPAD3,			/* down/right */
#ifdef USE_CURSORBLOCK
  DIK_UP,
  DIK_LEFT,
  DIK_RIGHT,
  DIK_DOWN,
#endif
};

static UBYTE joymask[] =
{
  0,				/* not used */
  ~1 & ~4,			/* up/left */
  ~1,				/* up */
  ~1 & ~8,			/* up/right */
  ~4,				/* left */
  ~8,				/* right */
  ~2 & ~4,			/* down/left */
  ~2,				/* down */
  ~2 & ~8,			/* down/right */
#ifdef USE_CURSORBLOCK
  ~1,				/* up */
  ~4,				/* left */
  ~8,				/* right */
  ~2,				/* down */
#endif
};

static int trig0;
static int stick0;

extern double deltatime;

int Atari_Keyboard(void)
{
  int keycode;
  int i;

  prockb();

  if (kbjoy)
    {
      /* fire */
#ifdef USE_CURSORBLOCK
      trig0 = (kbhits[joydefs[0]] ? 0 : 1) & (kbhits[DIK_LCONTROL] ? 0 : 1);
#else
      trig0 = kbhits[joydefs[0]] ? 0 : 1;
#endif
      stick0 |= 0xf;
      for (i = 1; i < sizeof(joydefs) / sizeof(joydefs[0]); i++)
	if (kbhits[joydefs[i]])
	  stick0 &= joymask[i];
    }

  key_consol = (key_consol & ~CONSOL_NONE)
    | (kbhits[DIK_F2] ? 0 : CONSOL_OPTION)
    | (kbhits[DIK_F3] ? 0 : CONSOL_SELECT)
    | (kbhits[DIK_F4] ? 0 : CONSOL_START);

  if (pause_hit)
    {
      pause_hit = 0;
      return AKEY_BREAK;
    }

  key_shift = (kbhits[DIK_LSHIFT] | kbhits[DIK_RSHIFT]) ? 1 : 0;

  alt_function = -1;		/* no alt function */
  if (kbhits[DIK_LMENU])
    {				/* left Alt key is pressed */
      if (kbcode == DIK_R)
	alt_function = MENU_RUN;	/* ALT+R .. Run file */
      else if (kbcode == DIK_Y)
	alt_function = MENU_SYSTEM;	/* ALT+Y .. Select system */
      else if (kbcode == DIK_O)
	alt_function = MENU_SOUND;	/* ALT+O .. mono/stereo sound */
      else if (kbcode == DIK_A)
	alt_function = MENU_ABOUT;	/* ALT+A .. About */
      else if (kbcode == DIK_S)
	alt_function = MENU_SAVESTATE;	/* ALT+S .. Save state */
      else if (kbcode == DIK_D)
	alt_function = MENU_DISK;	/* ALT+D .. Disk management */
      else if (kbcode == DIK_L)
	alt_function = MENU_LOADSTATE;	/* ALT+L .. Load state */
      else if (kbcode == DIK_C)
	alt_function = MENU_CARTRIDGE;	/* ALT+C .. Cartridge management */
    }
  if (alt_function != -1)
    return AKEY_UI;

  /* need to set shift mask here to avoid conflict with PC layout */
  keycode = (key_shift ? 0x40 : 0)
    | ((kbhits[DIK_LCONTROL]
	| kbhits[DIK_RCONTROL]) ? 0x80 : 0);

  switch (kbcode)
    {
    case DIK_F9:
      return AKEY_EXIT;
      break;
    case DIK_F8:
      keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;
      kbcode = 0;
      break;
    case DIK_SCROLL:		/* pause */
      while (kbhits[DIK_SCROLL])
	prockb();
      while (!kbhits[DIK_SCROLL])
	prockb();
      kbcode = 0;
      break;
    case DIK_SYSRQ:
    case DIK_F10:
      keycode = key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
      kbcode = 0;
      break;

    case DIK_F1:
      return AKEY_UI;
      break;
    case DIK_F5:
      return key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
      break;
    case DIK_F11:
      for (i = 0; i < 4; i++)
	{
	  if (++joy_autofire[i] > 2)
	    joy_autofire[i] = 0;
	}
      kbcode = 0;
      break;

#define KBSCAN(name) \
	  case DIK_##name: \
	    keycode |= (AKEY_##name & ~(AKEY_CTRL | AKEY_SHFT)); \
	    break;
      KBSCAN(ESCAPE)
	KBSCAN(1)
    case DIK_2:
      if (key_shift)
	keycode = AKEY_AT;
      else
	keycode |= AKEY_2;
      break;
      KBSCAN(3)
	KBSCAN(4)
	KBSCAN(5)
    case DIK_6:
      if (key_shift)
	keycode = AKEY_CARET;
      else
	keycode |= AKEY_6;
      break;
    case DIK_7:
      if (key_shift)
	keycode = AKEY_AMPERSAND;
      else
	keycode |= AKEY_7;
      break;
    case DIK_8:
      if (key_shift)
	keycode = AKEY_ASTERISK;
      else
	keycode |= AKEY_8;
      break;
      KBSCAN(9)
	KBSCAN(0)
	KBSCAN(MINUS)
    case DIK_EQUALS:
      if (key_shift)
	keycode = AKEY_PLUS;
      else
	keycode |= AKEY_EQUAL;
      break;
      KBSCAN(BACKSPACE)
	KBSCAN(TAB)
	KBSCAN(Q)
	KBSCAN(W)
	KBSCAN(E)
	KBSCAN(R)
	KBSCAN(T)
	KBSCAN(Y)
	KBSCAN(U)
	KBSCAN(I)
	KBSCAN(O)
	KBSCAN(P)
    case DIK_LBRACKET:
      keycode |= AKEY_BRACKETLEFT;
      break;
    case DIK_RBRACKET:
      keycode |= AKEY_BRACKETRIGHT;
      break;
    case DIK_RETURN:
      keycode |= AKEY_RETURN;
      break;
      KBSCAN(A)
	KBSCAN(S)
	KBSCAN(D)
	KBSCAN(F)
	KBSCAN(G)
	KBSCAN(H)
	KBSCAN(J)
	KBSCAN(K)
	KBSCAN(L)
	KBSCAN(SEMICOLON)
    case DIK_APOSTROPHE:
      if (key_shift)
	keycode = AKEY_DBLQUOTE;
      else
	keycode |= AKEY_QUOTE;
      break;
    case DIK_GRAVE:
      keycode |= AKEY_ATARI;
      break;
    case DIK_BACKSLASH:
      if (key_shift)
	keycode = AKEY_EQUAL | AKEY_SHFT;
      else
	keycode |= AKEY_BACKSLASH;
      break;
      KBSCAN(Z)
	KBSCAN(X)
	KBSCAN(C)
	KBSCAN(V)
	KBSCAN(B)
	KBSCAN(N)
	KBSCAN(M)
    case DIK_COMMA:
      if (key_shift)
	keycode = AKEY_LESS;
      else
	keycode |= AKEY_COMMA;
      break;
    case DIK_PERIOD:
      if (key_shift)
	keycode = AKEY_GREATER;
      else
	keycode |= AKEY_FULLSTOP;
      break;
      KBSCAN(SLASH)
	KBSCAN(SPACE)
	KBSCAN(CAPSLOCK)
    case DIK_UP:
      keycode = AKEY_UP;
      break;
    case DIK_DOWN:
      keycode = AKEY_DOWN;
      break;
    case DIK_LEFT:
      keycode = AKEY_LEFT;
      break;
    case DIK_RIGHT:
      keycode = AKEY_RIGHT;
      break;
    case DIK_DELETE:
      if (key_shift)
	keycode = AKEY_DELETE_LINE;
      else
	keycode |= AKEY_DELETE_CHAR;
      break;
    case DIK_INSERT:
      if (key_shift)
	keycode = AKEY_INSERT_LINE;
      else
	keycode |= AKEY_INSERT_CHAR;
      break;
    case DIK_HOME:
      keycode = AKEY_CLEAR;
      break;
    case DIK_END:
      keycode = AKEY_HELP;
      break;
    default:
      keycode = AKEY_NONE;
    }

  return keycode;
}

void Atari_Initialise(int *argc, char *argv[])
{
  ShowWindow(hWndMain, SW_RESTORE);

#ifdef SOUND
  if (usesnd)
    Sound_Initialise(argc, argv);
#endif
  if (initinput())
  {
    MessageBox(hWndMain, "DirectInput Init FAILED",
		myname,
		MB_ICONEXCLAMATION
		| MB_OK);
    exit(1);
  }
  if (gron(argc, argv))
    exit(1);

  clearkb();

  trig0 = 1;
  stick0 = 15;
  key_consol = CONSOL_NONE;
}

int Atari_Exit(int run_monitor)
{
  int i;

  if (run_monitor)
    {
#ifdef SOUND
      Sound_Pause();
#endif
      ShowWindow(hWndMain, SW_MINIMIZE);
      i = monitor();
      ShowWindow(hWndMain, SW_RESTORE);
#ifdef SOUND
      Sound_Continue();
#endif
      if (i)
	return 1;	      /* return to emulation */
    }

#ifdef BUFFERED_LOG
  Aflushlog();
#endif

  return 0;
}

void Atari_DisplayScreen(UBYTE * ascreen)
{
  refreshv(ascreen + 24);
}

int Atari_PORT(int num)
{
  if (num == 0)
    {
      return 0xf0 | stick0;
    }
  else
    return 0xff;
}


int Atari_TRIG(int num)
{
  if (num == 0)
    {
      return trig0;
    }
  else
    return 1;
}

int Atari_POT(int num)
{
  return 228;
}

/*
$Log$
Revision 1.6  2001/12/04 13:07:22  joy
LED_lastline apparently disappeared from the Atari800 core so remove it here, too (suggested by Nathan)

Revision 1.5  2001/10/03 16:16:42  knik
keyboard update

Revision 1.4  2001/09/25 17:40:16  knik
keyboard and display update

Revision 1.3  2001/04/08 05:51:44  knik
sound calls update

Revision 1.2  2001/03/24 10:15:13  knik
included missing header

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
