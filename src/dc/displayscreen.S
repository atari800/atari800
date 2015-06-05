/*
 * SEGA Dreamcast assembler support routines
 * (c) 2003,2004,2013 Christian Groessler (chris@groessler.org)
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

static void PLATFORM_DisplayScreen_singleb(UBYTE *screen)
{
	unsigned int x, m, j;
	uint16 *vram = screen_vram;

#ifdef SPEED_CHECK
	vid_border_color(0, 0, 0);
#endif
	for (m=START_VAL/8, x=START_VAL; m < ATARI_HEIGHT * ATARI_WIDTH / 8; m++) {
		if (Screen_dirty[m]) {
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			Screen_dirty[m] = 0;
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

#ifdef DIRTYRECT
_PLATFORM_DisplayScreen_singleb:
		.globl	_PLATFORM_DisplayScreen_singleb

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
 * r6 - Screen_dirty
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
		mov.l	Screen_dirty,r6
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

		mov.b	@(r0,r6),r0	/* Screen_dirty[m] */

		tst	#255,r0		/* is it dirty? */
		 mov	r2,r0		/* x, j = 0 -> r0 */
		bt/s	s_next		/* not dirty, skip screen update */
		 add	#8,r2		/* x += 8; */


!		 mov	r2,r0		/* x, j = 0 -> r0 */

		/* loop "for (j=0; j<8; j++)" unrolled */

		!add	#8,r2		/* x += 8; */

bim:
		mov.b	@(r0,r4),r13	/* *(screen + x + j) */
		and	r14,r13		/* and with 255 */
		shal	r13
		add	r12,r13		/* &mypal[*(screen + x + j)] */
		mov.w	@r13,r13
		shal	r0		/* index * 2 (words) */
		mov.w	r13,@(r0,r10)	/* *(vram + x + j) */
		shar	r0

		add	#1,r0		/* j = 1 */
!nop
		cmp/eq	r2,r0
		bf	bim
!nop

#if 0
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
#endif
		/* end of unrolled loop */

		/* Screen_dirty[m] = 0; */
		mov	r1,r0		/* m -> r0 */
		mov.b	r7,@(r0,r6)

		.align	5

s_next:		!add	#8,r2		/* x += 8; */
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

#else // DIRTYRECT




_PLATFORM_DisplayScreen_singleb:
		.globl	_PLATFORM_DisplayScreen_singleb

		mov.l	r8,@-r15
		sts.l	pr,@-r15	/* save return address */
		mov.l	r9,@-r15	/* save registers */
		mov.l	r10,@-r15
		mov.l	r11,@-r15
		mov.l	r12,@-r15
		mov.l	r13,@-r15
		mov.l	r14,@-r15

	!trapa #0x20

#ifdef SPEED_CHECK
		mov.l	r4,@-r15
		mov.l	vbc_ptr,r0
		mov	#0,r4
		mov	r4,r5
		jsr	@r0		/* vid_border_color(0, 0, 0); */
		 mov	r4,r6
		mov.l	@r15+,r4
#endif

/* @@@

	vram_wrk = vram + START_VAL;
	screen += START_VAL;
	for (y = 0; y < Screen_HEIGHT; y++) {
		for (x = 0; x < 320; x++)
			*(vram_wrk + x) = mypal[*(screen + x)];
		screen += Screen_WIDTH;
		vram_wrk += vid_mode->width;
	}
*/

/* r0
 * r1 - data
 * r2 - column counter (inner loop variable)
 * r4 - screen (function input argument)
 * r5
 * r6 - 320
 * r7 - 0
 * r8 - ATARI_WIDTH (Screen_WIDTH)
 * r9 - vid_mode_width
 * r10 - vram (work)
 * r11 - line counter (outer loop variable)
 * r12 - mypal
 * r13 - vram
 * r14 - 255 (for ANDing)
 */
		sub	r7,r7
		mov	#START_VAL,r2

		mov.l	atari_width,r8
		mov.l	vid_mode_width,r9
		mov.l	screen_vram,r10
		mov.l	@r9,r9
		mov.l	@r10,r10

		!add	#-1,r7

		mov.l	mypal,r12
		mov.l	three20,r6
		mov.l	two55,r14

		add	r2,r10
		mov.l	atari_height,r11	! line counter, outer loop variable
		add	r2,r4			! screen + START_VAL
		add	r2,r10			! vram + START_VAL (16bit frame buffer) -> vram_wrk

		!add	#-1,r11
		mov	r6,r2
		mov	r10,r13

		shal	r9

		!.align	5

s_loop:		
		mov.b	@r4+,r1			! *(screen + x); screen++;

		.align	5
s_loop1:
		and	r14,r1			! and with 255
		shal	r1			! r1: index into mypal
		add	r12,r1
		mov.w	@r1,r1			! mypal[*(screen + x)]
		mov.w	r1,@r10
		add	#2,r10			! vram++ (16bit fb)

		add	#-1,r2
		cmp/eq	r2,r7
!		bf	s_loop
		bf/s	s_loop1
		 mov.b	@r4+,r1

		! one line done, update "screen" and "vram" to point to the next line
		add	r9,r13
		add	#ATARI_WIDTH-320-1,r4	! screen
		mov	r13,r10
		mov	r6,r2

		add	#-1,r11
		cmp/eq	r11,r7
!		bf	s_loop
		bf/s	s_loop1
		 mov.b	@r4+,r1

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

#endif

/*******************************************************************************/

/*
 * implementation of the following C routine
 *
static void PLATFORM_DisplayScreen_doubleb(UBYTE *screen)
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
		nsd[m] = Screen_dirty[m];
		if (Screen_dirty[m] || osd[m]) {
			for (j=0; j<8; j++) {
				*(vram + x + j) = mypal[*(screen + x + j)];
			}
			Screen_dirty[m] = 0;
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

_PLATFORM_DisplayScreen_doubleb:
		.globl	_PLATFORM_DisplayScreen_doubleb

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
 * r6 - Screen_dirty
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
		mov.l	sbase,r0
		cmp/eq	#0,r0
		mova	sbase,r0
		bt/s	d_sbase_1
		 nop

/* sbase != 0 */

		mov	#0,r8
		mov.l	r8,@r0		/* sbase = 0 */

		mov.l	old_sd,r0
		mov	r0,r8		/* osd = old_sd[0] */
		add	r5,r0
		bra	d_sbase_2
		 mov	r0,r3		/* nsd = old_sd[1] */

/* sbase == 0 */

d_sbase_1:
		mov.l	sbase2_start,r8
		mov.l	r8,@r0		/* sbase = 1024 * 768 * 4 */

		add	r8,r10		/* vram = screen_vram + sbase / 2 */
					/* "/ 2" in the C code, since screen_vram is */
					/* a uint16 pointer; here we operate on byte */
					/* pointers */

		mov.l	old_sd,r0
		mov	r0,r3		/* nsd = old_sd[0] */
		add	r5,r0
		mov	r0,r8		/* osd = old_sd[1] */

d_sbase_2:
		sub	r7,r7
		mov	#START_VAL/8,r1
		mov	#START_VAL,r2
#ifdef DIRTYRECT
		mov.l	Screen_dirty,r6
#endif
		mov.l	@r6,r6
		mov.l	cmp_val2,r11
		mov.l	mypal,r12
		mov.l	two55,r14	/* 255 -> r14 */

		cmp/hs	r5,r1		/* m >= ATARI_HEIGHT * ATARI_WIDTH / 8 */

		.align	5

d_loop:		bt/s	d_done
		 mov	r1,r0		/* m -> r0 */

		mov.b	@(r0,r6),r9	/* Screen_dirty[m] */
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
		cmp/eq	#8,r0
		bf	d_do

#if 0
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

#endif
		/* end of unrolled loop */

		/* Screen_dirty[m] = 0; */
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
#ifdef DIRTYRECT
Screen_dirty:	.long	_Screen_dirty
#endif
cmp_val:	.long	(ATARI_HEIGHT * ATARI_WIDTH / 8)
atari_width:	.long	ATARI_WIDTH
atari_height:	.long	ATARI_HEIGHT
vid_mode_width:	.long	_vid_mode_width
screen_vram:	.long	_screen_vram
cmp_val2:	.long	320 + (ATARI_WIDTH - 320) / 2
mypal:		.long	_mypal
two55:		.long	255
three20:	.long	320
vid_set_start:	.long	_vid_set_start

sbase2_start:	.long	1024 * 768 * 4
old_sd:		.long	_old_sd
sbase:		.long	0

		.end
