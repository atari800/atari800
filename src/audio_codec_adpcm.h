#ifndef AUDIO_CODEC_ADPCM_H_
#define AUDIO_CODEC_ADPCM_H_

#include "atari.h"
#include "file_export.h"

extern AUDIO_CODEC_t Audio_Codec_ADPCM;

typedef struct ADPCMChannelStatus {
	int sample1;
	int sample2;
	int coeff1;
	int coeff2;
	int idelta;
} ADPCMChannelStatus;


#endif /* AUDIO_CODEC_ADPCM_H_ */
