#include "config.h"
#include <fcntl.h>
#include "atari.h"
#include "pokeysnd.h"
#include "allegro.h"
extern int timesync;
int speed_count = 0;

#define FALSE 0
#define TRUE 1
static sound_enabled = TRUE;
static int gain = 4;

static int sample_pos;
static unsigned char *buffer;
static int frames_per_second = 60;
static int real_updates_per_frame = 262;
static int stream_len;
static int stream_freq;
AUDIOSTREAM *stream;
   
void init_stream(){
#ifdef NEW_ALLEGRO312
   stream = play_audio_stream(stream_len, 8, 0, stream_freq , 255, 128); /* 0 = no_stereo, 255=vol, 128=pan, 8=bits */
#else
   stream = play_audio_stream(stream_len, 8, stream_freq , 255, 128); /* 255=vol 128=pan 8=bits */
#endif
   if (!stream) {
      printf("Error creating audio stream!\n");
      abort();
   }
}
static int updatecount;
void pokey_update(void)
{
	int newpos;
	if (sound_enabled == 0)
		return;
	if (updatecount >= real_updates_per_frame)
		return;
	newpos = stream_len * (updatecount) / real_updates_per_frame;
	if (newpos > stream_len)
		newpos = stream_len;

	Pokey_process(buffer + sample_pos, newpos - sample_pos);
	sample_pos = newpos;
	updatecount++;
}

int dossound_Initialise(int *argc, char *argv[])
{
	int i, j;
	if (tv_mode == TV_PAL) {
		frames_per_second = 50;
		real_updates_per_frame = 312;
	}
	for (i = j = 1; i < *argc; i++) {
		if (stricmp(argv[i], "-sound") == 0)
			sound_enabled = TRUE;
		else if (strcmp(argv[i], "-nosound") == 0)
			sound_enabled = FALSE;
		else
			argv[j++] = argv[i];
	}
	*argc = j;

	if (sound_enabled) {
         if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL) != 0) {
            printf("Error initialising sound system\n%s\n", allegro_error);
            return 1;
         }
	}
	if (sound_enabled) {
		timesync = FALSE;
		stream_len = (44100 - 1) / frames_per_second;
		stream_freq = stream_len * frames_per_second;
		sample_pos = 0;
		if ((buffer = (char *) malloc(stream_len)) == 0) {
			free(buffer);
			return 1;
		}
		memset(buffer, 0x00, stream_len);
		Pokey_sound_init(FREQ_17_EXACT, stream_freq, 1);
                init_stream();
	}
	else {
		timesync = TRUE;
	}
	return 0;
}

void dossound_Exit(void)
{
}

void dossound_UpdateSound(void)
{
	if (sound_enabled) {
             	unsigned char *p;
                updatecount = 0;
                if (sample_pos < stream_len)
		   Pokey_process(buffer + sample_pos, stream_len - sample_pos);
                sample_pos = 0;
                while (!(p=get_audio_stream_buffer(stream))){}
                memcpy(p, buffer, stream_len);
                free_audio_stream_buffer(stream);
	}
}

void Atari_AUDC(int channel, int byte)
{
	channel--;
	Update_pokey_sound(  1 + channel + channel, byte, 0, gain);
}

void Atari_AUDF(int channel, int byte)
{
	channel--;
	Update_pokey_sound(  channel + channel, byte, 0, gain);
}

void Atari_AUDCTL(int byte)
{
	Update_pokey_sound(  8, byte, 0, gain);
}


