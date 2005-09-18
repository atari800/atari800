#ifndef _MONITOR_H_
#define _MONITOR_H_

#include "config.h"
#include <stdio.h>

#include "atari.h"

int monitor(void);

#ifdef MONITOR_TRACE
extern FILE *trace_file;
#endif

#ifdef MONITOR_BREAK
extern UWORD break_addr;
extern UBYTE break_step;
extern UBYTE break_ret;
extern UBYTE break_brk;
extern int ret_nesting;
#endif

extern const UBYTE optype6502[256];

void show_state(FILE *fp, UWORD pc, UBYTE a, UBYTE x, UBYTE y, UBYTE s,
                char n, char v, char z, char c);

#ifdef MONITOR_BREAKPOINTS

/* Breakpoint conditions */

#define BREAKPOINT_OR          1
#define BREAKPOINT_FLAG_CLEAR  2
#define BREAKPOINT_FLAG_SET    3

/* these three may be ORed together and must be ORed with BREAKPOINT_PC .. BREAKPOINT_WRITE */
#define BREAKPOINT_LESS        1
#define BREAKPOINT_EQUAL       2
#define BREAKPOINT_GREATER     4

#define BREAKPOINT_PC          8
#define BREAKPOINT_A           16
#define BREAKPOINT_X           32
#define BREAKPOINT_Y           40
#define BREAKPOINT_S           48
#define BREAKPOINT_READ        64
#define BREAKPOINT_WRITE       128
#define BREAKPOINT_ACCESS      (BREAKPOINT_READ | BREAKPOINT_WRITE)

typedef struct {
	UBYTE enabled;
	UBYTE condition;
	UWORD value;
} breakpoint_cond;

#define BREAKPOINT_TABLE_MAX  20
extern breakpoint_cond breakpoint_table[BREAKPOINT_TABLE_MAX];
extern int breakpoint_table_size;
extern int breakpoints_enabled;

#endif /* MONITOR_BREAKPOINTS */

#endif /* _MONITOR_H_ */
