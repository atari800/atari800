#include "config.h"

#undef PAGED_MEM

#define INLINE_GETBYTE
#ifndef _A_MEM_H
#define _A_MEM_H
#ifdef PAGED_MEM
#include "memory-p.h"
#else
#include "memory-d.h"
#endif
#endif
