#include "atari.h"
#include "config.h"
#include "input.h"
#include "monitor.h"
#include "log.h"

void Atari_Initialise(int *argc, char *argv[])
{
}

int Atari_Exit(int run_monitor)
{
	if (run_monitor)
		return monitor();

	Aflushlog();
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

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	while (TRUE) {
		Atari800_Frame(EMULATE_BASIC);
#ifndef	DONT_SYNC_WITH_HOST
		atari_sync();
#endif
	}
}
