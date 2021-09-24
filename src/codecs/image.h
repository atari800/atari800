#ifndef CODECS_IMAGE_H_
#define CODECS_IMAGE_H_

#include "atari.h"

/* Save current screen (possibly interlaced) to a file */
typedef int (*IMAGE_CODEC_SaveToFile)(FILE *fp, UBYTE *ptr1, UBYTE *ptr2);

/* Save current screen (possibly interlaced) to a buffer */
typedef int (*IMAGE_CODEC_SaveToBuffer)(UBYTE *buf, int bufsize, UBYTE *ptr1, UBYTE *ptr2);

typedef struct {
    char *codec_id;
    char *description;
    IMAGE_CODEC_SaveToFile to_file;
    IMAGE_CODEC_SaveToBuffer to_buffer;
} IMAGE_CODEC_t;

extern IMAGE_CODEC_t *image_codec;
extern int image_codec_left_margin;
extern int image_codec_top_margin;
extern int image_codec_width;
extern int image_codec_height;

void CODECS_IMAGE_SetMargins(void);
int CODECS_IMAGE_Init(const char *filename);
int CODECS_IMAGE_SaveScreen(FILE *fp, UBYTE *ptr1, UBYTE *ptr2);

#endif /* CODECS_IMAGE_H_ */

