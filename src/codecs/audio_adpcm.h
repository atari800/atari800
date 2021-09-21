#ifndef CODECS_AUDIO_ADPCM_H_
#define CODECS_AUDIO_ADPCM_H_

#include "atari.h"
#include "codecs/audio.h"

extern AUDIO_CODEC_t Audio_Codec_ADPCM;
extern AUDIO_CODEC_t Audio_Codec_ADPCM_IMA;
extern AUDIO_CODEC_t Audio_Codec_ADPCM_MS;
extern AUDIO_CODEC_t Audio_Codec_ADPCM_YAMAHA;

typedef struct ADPCMChannelStatus {
    int predictor;
    int step;
	int sample1;
	int sample2;
	int coeff1;
	int coeff2;
	int idelta;
} ADPCMChannelStatus;


#endif /* CODECS_AUDIO_ADPCM_H_ */
