/*
 * SEGA Dreamcast assembler support routines
 * (c) 2003,2004 Christian Groessler (chris@groessler.org)
 */

#include "config.h"
#ifndef ATARI_WIDTH
#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240
#endif
#define START_VAL ((ATARI_WIDTH - 320) / 2)

/*******************************************************************************/

/*
 * implementation of the following C routine
 *

static void Atari_DisplayScreen_singleb(UBYTE *screen)
{
	unsigned int x, m, j;
	uint16 *vram = screen_vram;

#ifdef SPEED_CHECK
	vid_border_color(0, 0, 0);
#endif
	for (m=START_VAL/8, x=START_VAL; m < ATARI_HEIGHT * ATARI_WIDTH / 8; m++) {
		if (screen_dirty[m]) {
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			screen_dirty[m] = 0;
		}
		x += 8;
		if (x >= 320 + (ATARI_WIDTH - 320) / 2) {
			vram += vid_mode->width;
			screen += ATARI_WIDTH;
			m += (ATARI_WIDTH - 320) / 8;
			x = START_VAL;
		}
	}
#ifdef SPEED_CHECK
	vid_border_color(127, 127, 127);
#endif
	vbl_wait();
#ifdef SPEED_CHECK
	vid_border_color(255, 255, 255);
#endif
}
 */

		.align	5

_Atari_DisplayScreen_singleb:
		.globl	_Atari_DisplayScreen_singleb

		mov.l	r8,@-r15
		sts.l	pr,@-r15	/* save return address */
		mov.l	r9,@-r15	/* save registers */
		mov.l	r10,@-r15
		mov.l	r11,@-r15
		mov.l	r12,@-r15
		mov.l	r13,@-r15
		mov.l	r14,@-r15

#ifdef SPEED_CHECK
		mov.l	r4,@-r15
		mov.l	vbc_ptr,r0
		mov	#0,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(0, 0, 0); */
		 mov	r4,r6
		mov.l	@r15+,r4
#endif

/* r1 - m
 * r2 - x
 * r4 - screen (function input argument)
 * r5 - ATARI_HEIGHT * ATARI_WIDTH / 8
 * r6 - screen_dirty
 * r7 - 0
 * r8 - ATARI_WIDTH
 * r9 - vid_mode_width
 * r10 - vram
 * r11 - 320 + (ATARI_WIDTH - 320) / 2
 * r12 - mypal
 * r13 - (x + j) * 2
 * r14 - 255 (for ANDing)
 */
		sub	r7,r7
		mov	#START_VAL/8,r1
		mov	#START_VAL,r2
		mov.l	cmp_val,r5
		mov.l	screen_dirty,r6
		mov.l	@r6,r6
		mov.l	atari_width,r8
		mov.l	vid_mode_width,r9
		mov.l	@r9,r9
		mov.l	screen_vram,r10
		mov.l	@r10,r10
		mov.l	cmp_val2,r11
		mov.l	mypal,r12
		mov.l	two55,r14	/* 255 -> r14 */

		cmp/hs	r5,r1		/* m >= ATARI_HEIGHT * ATARI_WIDTH / 8 */

		.align	5

s_loop:		bt/s	s_done
		 mov	r1,r0		/* m -> r0 */

		mov.b	@(r0,r6),r0	/* screen_dirty[m] */

		tst	#255,r0		/* is it dirty? */
		bt/s	s_next		/* not dirty, skip screen update */
		 mov	r2,r0		/* x, j = 0 -> r0 */

		/* loop "for (j=0; j<8; j++)" unrolled */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 1 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 2 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 3 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 4 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 5 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 6 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 7 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		/* end of unrolled loop */

		/* screen_dirty[m] = 0; */
		mov	r1,r0		/* m -> r0 */
		mov.b	r7,@(r0,r6)

		.align	5

s_next:		add	#8,r2		/* x += 8; */
		cmp/hs	r11,r2		/* x >= 320 + (ATARI_WIDTH - 320) / 2 */
		bf/s	s_next2		/* no... */
		 add	#1,r1		/* m++ */

		mov	#START_VAL,r2			/* x = START_VAL */
		add	#(ATARI_WIDTH - 320) / 8,r1	/* m += (ATARI_WIDTH - 320) / 8 */
		add	r8,r4				/* screen += ATARI_WIDTH */
		add	r9,r10		/* vram += vid_mode->width */
		add	r9,r10		/* dto. (vram points to 16bit values) */

s_next2:	bra	s_loop
		 cmp/hs	r5,r1		/* m >= ATARI_HEIGHT * ATARI_WIDTH / 8 */

s_done:

#ifdef SPEED_CHECK
		mov.l	vbc_ptr,r0
		mov	#127,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(127, 127, 127); */
		 mov	r4,r6
#endif

		mov.l	vbl_wt,r0
		jsr	@r0		/* vbl_wait(); */
		 nop

#ifdef SPEED_CHECK
		mov.l	vbc_ptr,r0
		mov	#255,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(255, 255, 255); */
		 mov	r4,r6
#endif

		mov.l	@r15+,r14
		mov.l	@r15+,r13
		mov.l	@r15+,r12
		mov.l	@r15+,r11
		mov.l	@r15+,r10
		mov.l	@r15+,r9
		lds.l	@r15+,pr	/* get return address */
		rts
		 mov.l	@r15+,r8


/*******************************************************************************/

/*
 * implementation of the following C routine
 *
static void Atari_DisplayScreen_doubleb(UBYTE *screen)
{
	unsigned int sbase = 0;
	unsigned int x, m, j;
	uint16 *vram;
	UBYTE *osd, *nsd;

#ifdef SPEED_CHECK
	vid_border_color(0, 0, 0);
#endif
	if (sbase) {
		sbase = 0;
                vram = screen_vram;
		osd = old_sd[0];
		nsd = old_sd[1];
	}
	else {
		sbase = 1024 * 768 * 4;
                vram = screen_vram + sbase / 2;
		osd = old_sd[1];
		nsd = old_sd[0];
	}

	for (m=START_VAL/8, x=START_VAL; m < ATARI_HEIGHT * ATARI_WIDTH / 8; m++) {
		nsd[m] = screen_dirty[m];
		if (screen_dirty[m] || osd[m]) {
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			screen_dirty[m] = 0;
		}
		x += 8;
		if (x >= 320 + (ATARI_WIDTH - 320) / 2) {
			vram += vid_mode->width;
			screen += ATARI_WIDTH;
			m += (ATARI_WIDTH - 320) / 8;
			x = START_VAL;
		}
	}

#ifdef SPEED_CHECK
	vid_border_color(127, 127, 127);
#endif
	vbl_wait();
	vid_set_start(sbase);
#ifdef SPEED_CHECK
	vid_border_color(255, 255, 255);
#endif
}
 */

		.align	5

_Atari_DisplayScreen_doubleb:
		.globl	_Atari_DisplayScreen_doubleb

		mov.l	r8,@-r15
		sts.l	pr,@-r15	/* save return address */
		mov.l	r9,@-r15	/* save registers */
		mov.l	r10,@-r15
		mov.l	r11,@-r15
		mov.l	r12,@-r15
		mov.l	r13,@-r15
		mov.l	r14,@-r15

#ifdef SPEED_CHECK
		mov.l	r4,@-r15
		mov.l	vbc_ptr,r0
		mov	#0,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(0, 0, 0); */
		 mov	r4,r6
		mov.l	@r15+,r4
#endif

/* r1 - m
 * r2 - x
 * r3 - nsd
 * r4 - screen (function input argument)
 * r5 - ATARI_HEIGHT * ATARI_WIDTH / 8
 * r6 - screen_dirty
 * r7 - 0
 * r8 - osd
 * r9 - scratch
 * r10 - vram
 * r11 - 320 + (ATARI_WIDTH - 320) / 2
 * r12 - mypal
 * r13 - (x + j) * 2
 * r14 - 255 (for ANDing)
 */
		mov.l	screen_vram,r10
		mov.l	@r10,r10	/* vram = screen_vram */
		
		mov.l	cmp_val,r5	/* ATARI_HEIGHT * ATARI_WIDTH / 8 */
		mov.l	sbase,r7	/* r7: temp ptr to "sbase" (local_sbase) */
		mov.l	@r7,r0		/* current sbase value */
		cmp/eq	#0,r0
		bt/s	d_sbase_1
		 nop

/* sbase != 0 */

		mov	#0,r8
		mov.l	r8,@r7		/* sbase = 0 */

		mov.l	old_sd,r0
		mov	r0,r8		/* osd = old_sd[0] */
		add	r5,r0
		bra	d_sbase_2
		 mov	r0,r3		/* nsd = old_sd[1] */

/* sbase == 0 */

d_sbase_1:
		mov.l	sbase2_start,r8
		mov.l	r8,@r7		/* sbase = 1024 * 768 * 4 */

		/*shar	r8*/
		add	r8,r10		/* vram = screen_vram + sbase / 2 */

		mov.l	old_sd,r0
		mov	r0,r3		/* nsd = old_sd[0] */
		add	r5,r0
		mov	r0,r8		/* osd = old_sd[1] */

d_sbase_2:
		sub	r7,r7
		mov	#START_VAL/8,r1
		mov	#START_VAL,r2
		mov.l	screen_dirty,r6
		mov.l	@r6,r6
		mov.l	cmp_val2,r11
		mov.l	mypal,r12
		mov.l	two55,r14	/* 255 -> r14 */

		cmp/hs	r5,r1		/* m >= ATARI_HEIGHT * ATARI_WIDTH / 8 */

		.align	5

d_loop:		bt/s	d_done
		 mov	r1,r0		/* m -> r0 */

		mov.b	@(r0,r6),r9	/* screen_dirty[m] */
		mov.b	r9,@(r0,r3)	/* nsd[m] = screen_dirty[m] */
		mov	r9,r0

		tst	#255,r0		/* is it dirty? */
		bf/s	d_do		/* dirty, do screen update */
		 mov	r2,r0		/* x, j = 0 -> r0 */

		mov	r1,r0		/* m -> r0 */
		mov.b	@(r0,r8),r0	/* osd[m] */

		tst	#255,r0		/* is it dirty? */
		bt/s	d_next		/* not dirty, skip screen update */
		 mov	r2,r0		/* x, j = 0 -> r0 */

		/* loop "for (j=0; j<8; j++)" unrolled */

d_do:		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 1 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 2 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 3 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 4 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 5 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 6 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 7 */

		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		/* end of unrolled loop */

		/* screen_dirty[m] = 0; */
		mov	r1,r0		/* m -> r0 */
		mov.b	r7,@(r0,r6)

		.align	5

d_next:		add	#8,r2		/* x += 8; */
		cmp/hs	r11,r2		/* x >= 320 + (ATARI_WIDTH - 320) / 2 */
		bf/s	d_next2		/* no... */
		 add	#1,r1		/* m++ */

		mov	#START_VAL,r2			/* x = START_VAL */
		add	#(ATARI_WIDTH - 320) / 8,r1	/* m += (ATARI_WIDTH - 320) / 8 */
		mov.l	atari_width,r0
		add	r0,r4				/* screen += ATARI_WIDTH */
		mov.l	vid_mode_width,r0
		mov.l	@r0,r0
		add	r0,r10		/* vram += vid_mode->width */
		add	r0,r10		/* dto. (vram points to 16bit values) */

d_next2:	bra	d_loop
		 cmp/hs	r5,r1		/* m >= ATARI_HEIGHT * ATARI_WIDTH / 8 */

d_done:

#ifdef SPEED_CHECK
		mov.l	vbc_ptr,r0
		mov	#127,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(127, 127, 127); */
		 mov	r4,r6
#endif

		mov.l	vbl_wt,r0
		jsr	@r0		/* vbl_wait(); */
		 nop

		mov.l	vid_set_start,r0
		mov.l	sbase,r4
		mov.l	@r4,r4
		jsr	@r0		/* vid_set_start(sbase); */
		 nop

#ifdef SPEED_CHECK
		mov.l	vbc_ptr,r0
		mov	#255,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(255, 255, 255); */
		 mov	r4,r6
#endif

		mov.l	@r15+,r14
		mov.l	@r15+,r13
		mov.l	@r15+,r12
		mov.l	@r15+,r11
		mov.l	@r15+,r10
		mov.l	@r15+,r9
		lds.l	@r15+,pr	/* get return address */
		rts
		 mov.l	@r15+,r8


		.align	2
vbc_ptr:	.long	_vid_border_color
vbl_wt:		.long	_vbl_wait
vram:		.long	_screen_vram
screen_dirty:	.long	_screen_dirty
cmp_val:	.long	(ATARI_HEIGHT * ATARI_WIDTH / 8)
atari_width:	.long	ATARI_WIDTH
vid_mode_width:	.long	_vid_mode_width
screen_vram:	.long	_screen_vram
cmp_val2:	.long	320 + (ATARI_WIDTH - 320) / 2
mypal:		.long	_mypal
two55:		.long	255
vid_set_start:	.long	_vid_set_start

sbase2_start:	.long	1024 * 768 * 4
old_sd:		.long	_old_sd
sbase:		.long	local_sbase


.section	.bss

local_sbase:	.ds.l	1


		.end
