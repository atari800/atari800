#include <stdio.h>		/* for sscanf */
#include "config.h"

#include "pokeysnd.h"
#include "dos_sb.h"
#include "log.h"

#define FALSE 0
#define TRUE 1

static int sound_enabled = TRUE;

int playback_freq = FREQ_17_APPROX / 28 / 3;
int buffersize = 440;
int stereo = FALSE;
int bps = 8;

void Sound_Initialise(int *argc, char *argv[])
{
	int i, j;

#ifdef STEREO
	stereo = TRUE;
#endif

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else if (strcmp(argv[i], "-dsprate") == 0)
			sscanf(argv[++i], "%d", &playback_freq);
		else if (strcmp(argv[i], "-bufsize") == 0)
			sscanf(argv[++i], "%d", &buffersize);
		else
			argv[j++] = argv[i];
	}

	*argc = j;

	if (sound_enabled) {
		if (sb_init(&playback_freq, &bps, &buffersize, &stereo) < 0) {
			Aprint("Cannot init sound card\n");
			sound_enabled = FALSE;
		}
		else {

#ifdef STEREO
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, 2);
#else
			Pokey_sound_init(FREQ_17_APPROX, playback_freq, 1);
#endif
			sb_startoutput((sbmix_t) Pokey_process);
		}
	}
}

void Sound_Pause(void)
{
	if (sound_enabled)
		sb_stopoutput();
}

void Sound_Continue(void)
{
	if (sound_enabled)
		sb_startoutput((sbmix_t) Pokey_process);
}

void Sound_Exit(void)
{
	if (sound_enabled)
		sb_shutdown();
}
