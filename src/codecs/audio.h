#ifndef CODECS_AUDIO_H_
#define CODECS_AUDIO_H_

#include "atari.h"

#define PUT_LE_WORD(p, val) do {         \
		UWORD d = (val);             \
		((UBYTE*)(p))[0] = (d);      \
		((UBYTE*)(p))[1] = (d)>>8;   \
		p += 2;                      \
	} while(0)

typedef struct {
    int sample_rate;
    int sample_size;
    int bits_per_sample;
    int bitrate;
    int num_channels;
    int block_align;
    int scale;
    int rate;
    int length;
    int extra_data_size;
    UBYTE extra_data[256];
    int samples_processed;
} AUDIO_OUT_t;

/* Audio codec initialization function. No codec functions are valid before a
   call to this function. Return the maximum size of the buffer needed to store
   the compressed audio frame, or -1 on error. */
typedef int (*AUDIO_CODEC_Init)(int sample_rate, float fps, int sample_size, int num_channels);

/* Return a pointer to the AUDIO_OUT_t structure that provides information on
   the audio output format. */
typedef AUDIO_OUT_t *(*AUDIO_CODEC_AudioOut)(void);

/* Audio codec frame creation function. Given the pointer to the sample data and
   the number of samples, store the compressed frame into buf. Return the size
   of the audio frame in bytes, 0 if there is not enough samples to fill a
   frame, or -1 on error. */
typedef int (*AUDIO_CODEC_CreateFrame)(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize);

/* Audio codec frame available function. Returns 1 if the audio codec has
   another frame to output, 0 otherwise. */
typedef int (*AUDIO_CODEC_AnotherFrame)(void);

/* Audio codec flush function, triggering codec to output any buffered or
   partial frames upon subsesquent calls to AUDIO_CODEC_CreateFrame. Return 1 on
   success, or zero on error. */
typedef int (*AUDIO_CODEC_Flush)(float duration);

/* Audio codec cleanup function. Free any data allocated in the init function.
   Audio codec must not be used after calling this function. Return 1 on
   success, or zero on error. */
typedef int (*AUDIO_CODEC_End)(void);

#define AUDIO_CODEC_FLAG_VBR_POSSIBLE 1
#define AUDIO_CODEC_FLAG_SUPPORTS_8_BIT_SAMPLES 2

typedef struct {
    char *codec_id;
    char *description;
    char fourcc[4];
    UWORD format_type;
    UWORD codec_flags;
    AUDIO_CODEC_Init init;
    AUDIO_CODEC_AudioOut audio_out;
    AUDIO_CODEC_CreateFrame frame;
    AUDIO_CODEC_AnotherFrame another_frame;
    AUDIO_CODEC_Flush flush;
    AUDIO_CODEC_End end;
} AUDIO_CODEC_t;

extern AUDIO_CODEC_t *audio_codec;
extern AUDIO_OUT_t *audio_out;
extern int audio_buffer_size;
extern UBYTE *audio_buffer;

#ifdef AUDIO_CODEC_MP3
extern int audio_param_bitrate;
extern int audio_param_samplerate;
extern int audio_param_quality;
#endif

int CODECS_AUDIO_Initialise(int *argc, char *argv[]);
int CODECS_AUDIO_ReadConfig(char *string, char *ptr);
void CODECS_AUDIO_WriteConfig(FILE *fp);
int CODECS_AUDIO_CheckType(char *codec_id);
int CODECS_AUDIO_Init(void);
void CODECS_AUDIO_End(void);


#endif /* CODECS_AUDIO_H_ */

