#ifndef _MZPOKEYSND_H_
#define _MZPOKEYSND_H_

#include "config.h"
#include "pokeysnd.h"

int Pokey_sound_init_mz(uint32 freq17,
                        uint16 playback_freq,
                        uint8 num_pokeys,
                        int flags,
                        int quality
#ifdef __PLUS
                        , int clear_regs
#endif
                       );

#endif /* _MZPOKEYSND_H_ */
