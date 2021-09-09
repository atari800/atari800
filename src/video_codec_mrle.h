#ifndef VIDEO_CODEC_MRLE_H_
#define VIDEO_CODEC_MRLE_H_

#include "atari.h"

int MRLE_CreateFrame(const UBYTE *source, int keyframe, UBYTE *buf, int bufsize, UBYTE *reference_screen, int width, int height, int left_margin, int top_margin);

#endif /* VIDEO_CODEC_MRLE_H_ */

