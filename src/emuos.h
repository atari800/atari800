#ifndef EMUOS_H_
#define EMUOS_H_

#include "config.h"
#include "atari.h"

#ifdef EMUOS_ALTIRRA
extern UBYTE const emuos_h[10240];
#define EMUOS_CHARSET_OFFSET 0x800
#else
extern UBYTE const emuos_h[8192];
#define EMUOS_CHARSET_OFFSET 0
#endif

#endif /* EMUOS_H_ */
