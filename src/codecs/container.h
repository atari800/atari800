#ifndef CODECS_CONTAINER_H_
#define CODECS_CONTAINER_H_

#include "atari.h"

/* Open a container using the file externsion to determine type */
typedef int (*CONTAINER_Open)(const char *filename);

/* Save the audio samples to the container */
typedef int (*CONTAINER_SaveAudioFrame)(const UBYTE *buf, int num_samples);

/* Save current screen (possibly interlaced) to a buffer */
typedef int (*CONTAINER_SaveVideoFrame)(void);

/* Close current container forcing any final data to be written to the file */
typedef int (*CONTAINER_Close)(void);

typedef struct {
    char *container_id;
    char *description;
    CONTAINER_Open open;
    CONTAINER_SaveAudioFrame save_audio;
    CONTAINER_SaveVideoFrame save_video;
    CONTAINER_Close close;
} CONTAINER_t;

/* RIFF files (WAV, AVI) are limited to 4GB in size, so define a reasonable max
   that's lower than 4GB */
#define MAX_RECORDING_SIZE (0xfff00000)

/* number of bytes written to the currently open multimedia file */
extern ULONG byteswritten;

/* These variables are needed for statistics and on-screen information display. */
extern ULONG video_frame_count;
extern float fps;
extern char description[16];

/* Currently open container */
extern CONTAINER_t *container;

int CODECS_CONTAINER_Open(const char *filename);

#endif /* CODECS_CONTAINER_H_ */

