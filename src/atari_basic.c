#include "atari.h"
#include "config.h"
#include "monitor.h"
#ifdef SOUND
#include "sound.h"
#endif
#include <unistd.h> /* for STDOUT_FILENO */
#include <termios.h>
#include <term.h>

struct termios old_termios;

void Atari_Initialise(int *argc, char *argv[])
{
   int term_echo = 1;
   int i;
   int j;
   struct termios new_termios;

   for (i = j = 1; i < *argc; i++)
   {
      if (strcmp(argv[i], "-noecho") == 0)
         term_echo =0;
      else
         argv[j++] = argv[i];
   }

   *argc = j;

#if (! defined(USE_GETCHAR)) && (! defined(DOS))
   tcgetattr(STDOUT_FILENO, &old_termios);
   /* set to non canonical mode */
   new_termios = old_termios;

   if(term_echo)
      new_termios.c_lflag &= ~( ICANON | IEXTEN);
   else
      new_termios.c_lflag &= ~( ECHO| ICANON | IEXTEN);
   new_termios.c_cc[VMIN] = 1;
   new_termios.c_cc[VTIME] = 0;

   tcsetattr(STDOUT_FILENO, TCSAFLUSH, &new_termios);
#endif
   /* termcap init */
#ifndef NO_TEXT_MODES
   setupterm((char*)0, 1, (int*)0);
#endif

#ifdef SOUND
   Sound_Initialise(argc, argv);
#endif

}

int Atari_Exit(int run_monitor)
{
#if (! defined(USE_GETCHAR)) && (! defined(DOS))
        /* restore old terminal settings */
        tcsetattr(STDOUT_FILENO, TCSAFLUSH, &old_termios);
#endif
	if (run_monitor)
		return monitor();
#ifdef SOUND
        else
           Sound_Exit();
#endif
	return FALSE;
}

int Atari_Keyboard(void)
{
	return AKEY_NONE;
}

int Atari_PORT(int num)
{
	return 0xff;
}

int Atari_TRIG(int num)
{
	return 1;
}

int Atari_POT(int num)
{
	return 228;
}

int Atari_CONSOL(void)
{
	return 7;
}

int Atari_PEN(int vertical)
{
	return vertical ? 0xff : 0;
}
