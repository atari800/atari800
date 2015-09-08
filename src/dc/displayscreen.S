/*
 * SEGA Dreamcast assembler support routines
 * (c) 2003,2004,2013,2015 Christian Groessler (chris@groessler.org)
 */

#include "config.h"
#ifndef ATARI_WIDTH
#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240
#endif
#define START_VAL ((ATARI_WIDTH - 320) / 2)

/*******************************************************************************/

		.align	5

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

/*
	vram_wrk = vram + START_VAL;
	screen += START_VAL;
	for (y = 0; y < Screen_HEIGHT; y++) {
		for (x = 0; x < 320; x++)
			*(vram_wrk + x) = mypal[*(screen + x)];
		screen += Screen_WIDTH;
		vram_wrk += vid_mode->width;
	}
*/

		mov.l	screen_vram,r10
		sub	r8,r8			! sbase
		mov.l	@r10,r10

		.align	5

ext_entry:	! entered here from _PLATFORM_DisplayScreen_doubleb
		! r4 - screen
		! r8 - sbase
		! r10 - vram

		mov.l	mypal,r12

		mov.l	vid_mode_width,r9
		mov	#START_VAL,r2
		mov.l	@r9,r9

#ifdef STORE_QUEUE

		add	r2,r10			! vram + START_VAL (1st part, 16bit values)
		mov.l	atari_height,r11	! line counter, outer loop variable
		add	r2,r4			! screen + START_VAL
		add	r2,r10			! vram + START_VAL (2nd part, 16bit frame buffer) -> vram_wrk


		! build SQ address out of vram address
		mov.l	_03ffffe0,r13
		mov.l	_e0000000,r2
		and	r13,r10
		or	r2,r10

		shal	r9			! vid_mode_width (16bit)
		mov	r10,r13			! remember initial vram_wrk

/* r0 - data
 * r1 - data
 * r2
 * r4 - screen (function input argument)
 * r5 - data
 * r6
 * r7
 * r8
 * r9 - vid_mode_width
 * r10 - vram_wrk (during line)
 * r11 - line counter (outer loop variable)
 * r12 - mypal
 * r13 - vram_wrk (start of each line)
 * r14 - store queue addr for "pref"
 */


! Store-Queue
! Each of the two store queues is 32 bytes (8 words) long.
! Our pixels are 16bit, so 16 pixel fit into one store queue.
! Store queue can only be written with word (32bit) accesses.
! Line length is 320 pixel, so we need to fill the store queues 20 times for each line.


		mov	#20,r2			! we need to run the following part 20 times for one line

		.align	5

s_loop:		mov	r10,r14

		.rept	8

  		mov.b	@r4+,r0			! *(screen + x)
  		mov.b	@r4+,r1			! *(screen + x + 1)

		extu.b	r0,r0
		extu.b	r1,r1

		shal	r0
		shal	r1

		mov.w	@(r0,r12),r5		! mypal[*(screen + x)]
		mov	r1,r0
		extu.w	r5,r5

		mov.w	@(r0,r12),r0		! mypal[*(screen + x + 1)]
		shll16	r0			! move to high 16bits

		or	r0,r5
		mov.l	r5,@r10
		add	#4,r10

		.endr

		! one store queue is full, 16 pixels done

		pref	@r14			! flush store queue

		dt    	r2
		bf  	s_loop

		! one line done, update "screen" and "vram" to point to the next line

		add	r9,r13
		add	#ATARI_WIDTH-320,r4	! screen
		mov	r13,r10

		dt	r11			! all lines done?
		bf/s	s_loop			! branch taken if not
		 mov	#20,r2			! we need to run the following part 20 times for one line (see above...)


#else /* above STORE_QUEUE, below not */

/* r0
 * r1 - data
 * r2 - column counter (inner loop variable)
 * r4 - screen (function input argument)
 * r5
 * r6 - 320
 * r7
 * r8
 * r9 - vid_mode_width
 * r10 - vram (during line)
 * r11 - line counter (outer loop variable)
 * r12 - mypal
 * r13 - vram_wrk (start of each line)
 * r14
 */

		add	r2,r10			! vram + START_VAL (1st part, 16bit values)
		mov.l	atari_height,r11	! line counter, outer loop variable
		mov.l	_320,r6
		add	r2,r4			! screen + START_VAL
		add	r2,r10			! vram + START_VAL (16bit frame buffer) -> vram_wrk

		mov	r6,r2			! initialize column counter
		mov	r10,r13			! remember initial vram_wrk

		shal	r9			! vid_mode_width (16bit)

		.align	5

s_loop:		
		mov.b	@r4+,r1			! *(screen + x)
		extu.b	r1,r1
		shal	r1			! r1: index into mypal
		add	r12,r1
		mov.w	@r1,r1			! mypal[*(screen + x)]
		mov.w	r1,@r10
		add	#2,r10			! vram++ (16bit fb)

		dt	r2
		bf	s_loop

		! one line done, update "screen" and "vram" to point to the next line
		add	r9,r13
		add	#ATARI_WIDTH-320,r4	! screen
		mov	r13,r10
		mov	r6,r2

		dt	r11
		bf	s_loop

#endif /* not defined STORE_QUEUE */

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

		mov.l	vid_set_start,r0
		jsr	@r0		/* vid_set_start(sbase); */
		 mov	r8,r4		/* sbase */

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

		mov.l	screen_vram,r10
		mov.l	sbase,r0
		cmp/eq	#0,r0
		mova	sbase,r0
		bt/s	d_sbase_1
		 mov.l	@r10,r10	/* vram = screen_vram */

/* sbase != 0 */

		mov	#0,r8
		bra	ext_entry
		 mov.l	r8,@r0		/* sbase = 0 */

/* sbase == 0 */

d_sbase_1:
		mov.l	sbase2_start,r8
		mov.l	r8,@r0		/* sbase = 1024 * 768 * 4 */

		bra	ext_entry
		 add	r8,r10		/* vram = screen_vram + sbase / 2 */
					/* "/ 2" in the C code, since screen_vram is */
					/* a uint16 pointer; here we operate on byte */
					/* pointers */


		.align	2
vbc_ptr:	.long	_vid_border_color
vbl_wt:		.long	_vbl_wait
vram:		.long	_screen_vram
atari_height:	.long	ATARI_HEIGHT
vid_mode_width:	.long	_vid_mode_width
screen_vram:	.long	_screen_vram
mypal:		.long	_mypal
_320:		.long	320
vid_set_start:	.long	_vid_set_start
sbase2_start:	.long	1024 * 768 * 4
sbase:		.long	0

#ifdef STORE_QUEUE
_03ffffe0:	.long	0x03ffffe0
_e0000000:	.long	0xe0000000	/* store queue memory area */
#endif

		.end
