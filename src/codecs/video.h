#ifndef CODECS_VIDEO_H_
#define CODECS_VIDEO_H_

#include "atari.h"

/* Video codec initialization function. It must set up any internal
   configuration needed by the codec. Return the maximum size of the buffer
   needed to store the compressed video frame, or -1 on error. */
typedef int (*VIDEO_CODEC_Init)(int width, int height, int left_margin, int top_margin);

/* Video codec frame creation function. Given the pointer to the screen data and
   whether to produce a keyframe or interframe, store the compressed frame into
   buf. Return the size of the compressed frame in bytes, or -1 on error. */
typedef int (*VIDEO_CODEC_CreateFrame)(UBYTE *source, int keyframe, UBYTE *buf, int bufsize);

/* Video codec cleanup function. Free any data allocated in the init function. Return 1 on
   success, or zero on error. */
typedef int (*VIDEO_CODEC_End)(void);

typedef struct {
    char *codec_id;
    char *description;
    char fourcc[4];
    char avi_compression[4];
    int uses_interframes;
    VIDEO_CODEC_Init init;
    VIDEO_CODEC_CreateFrame frame;
    VIDEO_CODEC_End end;
} VIDEO_CODEC_t;

extern VIDEO_CODEC_t *video_codec;
extern int video_buffer_size;
extern UBYTE *video_buffer;
extern int video_codec_keyframe_interval;

int CODECS_VIDEO_Initialise(int *argc, char *argv[]);
int CODECS_VIDEO_ReadConfig(char *string, char *ptr);
void CODECS_VIDEO_WriteConfig(FILE *fp);
int CODECS_VIDEO_Init(void);
void CODECS_VIDEO_End(void);

#endif /* CODECS_VIDEO_H_ */

