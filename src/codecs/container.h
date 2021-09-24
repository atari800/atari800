#ifndef CODECS_CONTAINER_H_
#define CODECS_CONTAINER_H_

#include "atari.h"

/* Prepare a file for writing video and audio frames */
typedef int (*CONTAINER_Prepare)(FILE *fp);

/* Save the audio samples to the container */
typedef int (*CONTAINER_SaveAudioFrame)(FILE *fp, const UBYTE *buf, int bufsize);

/* Save current screen (possibly interlaced) to a buffer */
typedef int (*CONTAINER_SaveVideoFrame)(FILE *fp, const UBYTE *buf, int bufsize, int is_keyframe);

/* Verify the file size is not about to exceed maximum size limits */
typedef int (*CONTAINER_SizeCheck)(int current_size);

/* Create a valid file by forcing any final data to be written to the file */
typedef int (*CONTAINER_Finalize)(FILE *fp);

typedef struct {
    char *container_id;
    char *description;
    CONTAINER_Prepare prepare;
    CONTAINER_SaveAudioFrame audio_frame;
    CONTAINER_SaveVideoFrame video_frame;
    CONTAINER_SizeCheck size_check;
    CONTAINER_Finalize finalize;
} CONTAINER_t;

/* RIFF files (WAV, AVI) are limited to 4GB in size, so define a reasonable max
   that's lower than 4GB */
#define MAX_RIFF_FILE_SIZE (0xfff00000)

/* number of bytes written to the currently open multimedia file */
extern ULONG byteswritten;

/* These variables are needed for statistics and on-screen information display. */
extern ULONG video_frame_count;
extern float fps;
extern char description[32];

/* Currently open container */
extern CONTAINER_t *container;

int CONTAINER_IsSupported(const char *filename);
int CONTAINER_Open(const char *filename);
int CONTAINER_AddAudioSamples(const UBYTE *buf, int num_samples);
int CONTAINER_AddVideoFrame(void);
int CONTAINER_Close(int file_ok);

#endif /* CODECS_CONTAINER_H_ */

