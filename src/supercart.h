#ifndef __SUPERCART__
#define __SUPERCART__

#include "atari.h"

UBYTE SuperCart_GetByte(UWORD addr);
int SuperCart_PutByte(UWORD addr, UBYTE byte);

#endif
