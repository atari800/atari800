#ifndef _ANTIC_H_
#define _ANTIC_H_

#include "atari.h"

/*
 * Offset to registers in custom relative to start of antic memory addresses.
 */

#define _DMACTL 0x00
#define _CHACTL 0x01
#define _DLISTL 0x02
#define _DLISTH 0x03
#define _HSCROL 0x04
#define _VSCROL 0x05
#define _PMBASE 0x07
#define _CHBASE 0x09
#define _WSYNC 0x0a
#define _VCOUNT 0x0b
#define _PENH 0x0c
#define _PENV 0x0d
#define _NMIEN 0x0e
#define _NMIRES 0x0f
#define _NMIST 0x0f

extern UBYTE CHACTL;
extern UBYTE CHBASE;
extern UWORD dlist;
extern UBYTE DMACTL;
extern UBYTE HSCROL;
extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UBYTE PMBASE;
extern UBYTE VSCROL;

extern int break_ypos;
extern int ypos;
extern UBYTE wsync_halt;

#define NMIST_C	6
#define NMI_C	12

extern int global_artif_mode;

extern UBYTE PENH_input;
extern UBYTE PENV_input;

void ANTIC_Initialise(int *argc, char *argv[]);
void ANTIC_Reset(void);
void ANTIC_Frame(int draw_display);
UBYTE ANTIC_GetByte(UWORD addr);
void ANTIC_PutByte(UWORD addr, UBYTE byte);

UBYTE ANTIC_GetDLByte(UWORD *paddr);
UWORD ANTIC_GetDLWord(UWORD *paddr);

/* always call ANTIC_UpdateArtifacting after changing global_artif_mode */
void ANTIC_UpdateArtifacting(void);

/* Video memory access */
void video_memset(UBYTE *ptr, UBYTE val, ULONG size);
void video_putbyte(UBYTE *ptr, UBYTE val);

#ifdef NEW_CYCLE_EXACT
#define NOT_DRAWING -999
#define DRAWING_SCREEN (cur_screen_pos!=NOT_DRAWING)
extern int delayed_wsync;
extern int cur_screen_pos;
extern const int *cpu2antic_ptr;
extern const int *antic2cpu_ptr;
void update_scanline(void);
void update_scanline_prior(UBYTE byte);
#ifndef NO_GTIA11_DELAY
extern int prevline_prior_pos;
extern int curline_prior_pos;
extern int prior_curpos;
#define PRIOR_BUF_SIZE 40
extern UBYTE prior_val_buf[PRIOR_BUF_SIZE];
extern int prior_pos_buf[PRIOR_BUF_SIZE];
#endif /* NO_GTIA11_DELAY */

#define XPOS ( DRAWING_SCREEN ? cpu2antic_ptr[xpos] : xpos )
#else
#define XPOS xpos
#endif /* NEW_CYCLE_EXACT */

#endif /* _ANTIC_H_ */
