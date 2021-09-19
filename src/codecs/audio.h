#ifndef CODECS_AUDIO_H_
#define CODECS_AUDIO_H_

#include "atari.h"

typedef struct {
    int sample_rate;
    int sample_size;
    int bits_per_sample;
    int num_channels;
    int block_align;
    int scale;
    int rate;
    int length;
    int extra_data_size;
    UBYTE extra_data[256];
} AUDIO_OUT_t;

/* Audio codec initialization function. It must set up any internal
   configuration needed by the codec. Return the maximum size of the buffer
   needed to store the compressed audio frame, or -1 on error. */
typedef int (*AUDIO_CODEC_Init)(int sample_rate, float fps, int sample_size, int num_channels);

/* Audio codec output data function. Return a pointer to the AUDIO_OUT_t structure
   that provides information on the audio output format. */
typedef AUDIO_OUT_t *(*AUDIO_CODEC_AudioOut)(void);

/* Audio codec frame creation function. Given the pointer to the sample data and
   the number of samples, store the compressed frame into buf. Return the size
   of the compressed frame in bytes, or -1 on error. */
typedef int (*AUDIO_CODEC_CreateFrame)(const UBYTE *source, int num_samples, UBYTE *buf, int bufsize);

/* Audio codec has another frame to output. */
typedef int (*AUDIO_CODEC_FrameReady)(int final_frame);

/* Audio codec cleanup function. Free any data allocated in the init function. Return 1 on
   success, or zero on error. */
typedef int (*AUDIO_CODEC_End)(float duration);

typedef struct {
    char *codec_id;
    char *description;
    char fourcc[4];
    UWORD format_type;
    AUDIO_CODEC_Init init;
    AUDIO_CODEC_AudioOut audio_out;
    AUDIO_CODEC_CreateFrame frame;
    AUDIO_CODEC_FrameReady another_frame;
    AUDIO_CODEC_End end;
} AUDIO_CODEC_t;

extern AUDIO_CODEC_t *audio_codec;
extern AUDIO_OUT_t *audio_out;
extern int audio_buffer_size;
extern UBYTE *audio_buffer;

int CODECS_AUDIO_Initialise(int *argc, char *argv[]);
int CODECS_AUDIO_ReadConfig(char *string, char *ptr);
void CODECS_AUDIO_WriteConfig(FILE *fp);
int CODECS_AUDIO_Init(void);
void CODECS_AUDIO_End(float duration);


#endif /* CODECS_AUDIO_H_ */

