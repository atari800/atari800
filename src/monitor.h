#ifndef _MONITOR_H_
#define _MONITOR_H_

#include "atari.h"
#include "config.h"

int monitor(void);

#ifdef TRACE
extern int tron;
#endif

#ifdef MONITOR_BREAK
extern UWORD break_addr;
extern UWORD ypos_break_addr;
extern UBYTE break_step;
extern UBYTE break_ret;
extern UBYTE break_cim;
extern UBYTE break_here;
extern int ret_nesting;
extern int brkhere;
#endif

unsigned int disassemble(UWORD addr1, UWORD addr2);

#endif /* _MONITOR_H_ */
