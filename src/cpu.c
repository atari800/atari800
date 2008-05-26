/*
 * cpu.c - 6502 CPU emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
	Configuration symbols
	=====================

	Define CPU65C02 if you don't want 6502 JMP() bug emulation.
	Define CYCLES_PER_OPCODE to update xpos in each opcode's emulation.
	Define MONITOR_BREAK if you want code breakpoints and execution history.
	Define MONITOR_BREAKPOINTS if you want user-defined breakpoints.
	Define MONITOR_PROFILE if you want 6502 opcode profiling.
	Define MONITOR_TRACE if you want the code to be disassembled while it is executed.
	Define NO_GOTO if you compile with GCC, but want switch() rather than goto *.
	Define NO_V_FLAG_VARIABLE to don't use local (static) variable V for the V flag.
	Define PC_PTR to emulate 6502 Program Counter using UBYTE *.
	Define PREFETCH_CODE to always fetch 2 bytes after the opcode.
	Define WRAP_64K to correctly emulate instructions that wrap at 64K.
	Define WRAP_ZPAGE to prevent incorrect access to the address 0x0100 in zeropage
	indirect mode.


	Limitations & Known bugs
	========================

	There is no emulation of the bug in the BRK instruction executed simultaneously
	with another interrupt.

	The 6502 emulation ignores memory attributes for instruction fetch.
	This is because the instruction must come from either RAM or ROM.
	A program that executes instructions from within hardware addresses will fail
	since there is never any usable code there.

	The 6502 emulation also ignores memory attributes for accesses to page 0 and page 1.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>	/* exit() */

#include "cpu.h"
#ifdef ASAP /* external project, see http://asap.sf.net */
#include "asap_internal.h"
#else
#include "antic.h"
#include "atari.h"
#include "memory.h"
#include "monitor.h"
#ifndef BASIC
#include "statesav.h"
#ifndef __PLUS
#include "ui.h"
#endif
#endif /* BASIC */
#endif /* ASAP */

#ifdef FALCON_CPUASM

extern UBYTE IRQ;

#ifdef PAGED_MEM
#error cpu_m68k.asm cannot work with paged memory
#endif

void CPU_Initialise(void)
{
	CPU_INIT();
}

void CPU_GetStatus(void)
{
	CPUGET();
}

void CPU_PutStatus(void)
{
	CPUPUT();
}

#else /* FALCON_CPUASM */

/* Windows headers define it */
#undef ABSOLUTE

#ifndef __GNUC__
#define NO_GOTO
#endif

/* #define CYCLES_PER_OPCODE */

/* #define MONITOR_PROFILE */

/* #define NO_V_FLAG_VARIABLE */

/* If PC_PTR is defined, local PC is "const UBYTE *", otherwise it's UWORD. */
/* #define PC_PTR */

/* If PREFETCH_CODE is defined, 2 bytes after the opcode are always fetched. */
/* #define PREFETCH_CODE */


/* 6502 stack handling */
#define PL                  dGetByte(0x0100 + ++S)
#define PH(x)               dPutByte(0x0100 + S--, x)
#define PHW(x)              PH((x) >> 8); PH((x) & 0xff)

/* 6502 code fetching */
#ifdef PC_PTR
#define GET_PC()            (PC - memory)
#define SET_PC(newpc)       (PC = memory + (newpc))
#define PHPC                { UWORD tmp = PC - memory; PHW(tmp); }
#define GET_CODE_BYTE()     (*PC++)
#define PEEK_CODE_BYTE()    (*PC)
#if !defined(WORDS_BIGENDIAN) && defined(WORDS_UNALIGNED_OK)
#define PEEK_CODE_WORD()    (*(const UWORD *) PC)
#else
#define PEEK_CODE_WORD()    (*PC + (PC[1] << 8))
#endif
#else /* PC_PTR */
#define GET_PC()            PC
#define SET_PC(newpc)       (PC = (newpc))
#define PHPC                PHW(PC)
#define GET_CODE_BYTE()     dGetByte(PC++)
#define PEEK_CODE_BYTE()    dGetByte(PC)
#define PEEK_CODE_WORD()    dGetWord(PC)
#endif /* PC_PTR */

/* Cycle-exact Read-Modify-Write instructions.
   RMW instructions: ASL, LSR, ROL, ROR, INC, DEC
   (+ some undocumented) write to the specified address
   *twice*: first the unmodified value, then the modified value.
   This can be observed only with some hardware registers. */
/* XXX: we do this only for GTIA, because NEW_CYCLE_EXACT does not correctly
   emulate INC $D400 (and INC $D40A wasn't tested) */
#ifdef NEW_CYCLE_EXACT
#ifndef PAGED_ATTRIB
#define RMW_GetByte(x, addr) \
	if (attrib[addr] == HARDWARE) { \
		x = Atari800_GetByte(addr); \
		if ((addr & 0xef00) == 0xc000) { \
			xpos--; \
			Atari800_PutByte(addr, x); \
			xpos++; \
		} \
	} else \
		x = dGetByte(addr);
#else /* PAGED_ATTRIB */
#define RMW_GetByte(x, addr) \
	x = GetByte(addr); \
	if ((addr & 0xef00) == 0xc000) { \
		xpos--; \
		PutByte(addr, x); \
		xpos++; \
	}
#endif /* PAGED_ATTRIB */
#else /* NEW_CYCLE_EXACT */
/* Don't emulate the first write */
#define RMW_GetByte(x, addr) x = GetByte(addr);
#endif /* NEW_CYCLE_EXACT */

/* 6502 registers. */
UWORD regPC;
UBYTE regA;
UBYTE regX;
UBYTE regY;
UBYTE regP;						/* Processor Status Byte (Partial) */
UBYTE regS;
UBYTE IRQ;

/* Transfer 6502 registers between global variables and local variables inside GO() */
#define UPDATE_GLOBAL_REGS  regPC = GET_PC(); regS = S; regA = A; regX = X; regY = Y
#define UPDATE_LOCAL_REGS   SET_PC(regPC); S = regS; A = regA; X = regX; Y = regY

/* 6502 flags local to this module */
static UBYTE N;					/* bit7 set => N flag set */
#ifndef NO_V_FLAG_VARIABLE
static UBYTE V;                 /* non-zero => V flag set */
#endif
static UBYTE Z;					/* zero     => Z flag set */
static UBYTE C;					/* must be 0 or 1 */
/* B, D, I are always in regP */

void CPU_GetStatus(void)
{
#ifndef NO_V_FLAG_VARIABLE
	regP = (N & 0x80) + (V ? 0x40 : 0) + (regP & 0x3c) + ((Z == 0) ? 0x02 : 0) + C;
#else
	regP = (N & 0x80) + (regP & 0x7c) + ((Z == 0) ? 0x02 : 0) + C;
#endif
}

void CPU_PutStatus(void)
{
	N = regP;
#ifndef NO_V_FLAG_VARIABLE
	V = (regP & 0x40);
#endif
	Z = (regP & 0x02) ^ 0x02;
	C = (regP & 0x01);
}

/* For Atari Basic loader */
void (*rts_handler)(void) = NULL;

/* 6502 instruction profiling */
#ifdef MONITOR_PROFILE
int instruction_count[256];
#endif

UBYTE cim_encountered = FALSE;

/* Execution history */
#ifdef MONITOR_BREAK
UWORD remember_PC[REMEMBER_PC_STEPS];
unsigned int remember_PC_curpos = 0;
int remember_xpos[REMEMBER_PC_STEPS];
UWORD remember_JMP[REMEMBER_JMP_STEPS];
unsigned int remember_jmp_curpos = 0;
#define INC_RET_NESTING ret_nesting++
#else /* MONITOR_BREAK */
#define INC_RET_NESTING
#endif /* MONITOR_BREAK */

/* Addressing modes */
#ifdef WRAP_ZPAGE
#define zGetWord(x) (dGetByte(x) + (dGetByte((UBYTE) ((x) + 1)) << 8))
#else
#define zGetWord(x) dGetWord(x)
#endif
#ifdef PREFETCH_CODE
#if defined(WORDS_BIGENDIAN) || !defined(WORDS_UNALIGNED_OK)
#warning PREFETCH_CODE is efficient only on little-endian machines with WORDS_UNALIGNED_OK
#endif
#define OP_BYTE     ((UBYTE) addr)
#define OP_WORD     addr
#define IMMEDIATE   (PC++, (UBYTE) addr)
#define ABSOLUTE    PC += 2
#define ZPAGE       PC++; addr &= 0xff
#define ABSOLUTE_X  addr += X; PC += 2
#define ABSOLUTE_Y  addr += Y; PC += 2
#define INDIRECT_X  PC++; addr = (UBYTE) (addr + X); addr = zGetWord(addr)
#define INDIRECT_Y  PC++; addr &= 0xff; addr = zGetWord(addr) + Y
#define ZPAGE_X     PC++; addr = (UBYTE) (addr + X)
#define ZPAGE_Y     PC++; addr = (UBYTE) (addr + Y)
#else /* PREFETCH_CODE */
#define OP_BYTE     PEEK_CODE_BYTE()
#define OP_WORD     PEEK_CODE_WORD()
#define IMMEDIATE   GET_CODE_BYTE()
#define ABSOLUTE    addr = PEEK_CODE_WORD(); PC += 2
#define ZPAGE       addr = GET_CODE_BYTE()
#define ABSOLUTE_X  addr = PEEK_CODE_WORD() + X; PC += 2
#define ABSOLUTE_Y  addr = PEEK_CODE_WORD() + Y; PC += 2
#define INDIRECT_X  addr = (UBYTE) (GET_CODE_BYTE() + X); addr = zGetWord(addr)
#define INDIRECT_Y  addr = GET_CODE_BYTE(); addr = zGetWord(addr) + Y
#define ZPAGE_X     addr = (UBYTE) (GET_CODE_BYTE() + X)
#define ZPAGE_Y     addr = (UBYTE) (GET_CODE_BYTE() + Y)
#endif /* PREFETCH_CODE */

/* Instructions */
#define AND(t_data) Z = N = A &= t_data
#define CMP(t_data) data = t_data; Z = N = A - data; C = (A >= data)
#define CPX(t_data) data = t_data; Z = N = X - data; C = (X >= data)
#define CPY(t_data) data = t_data; Z = N = Y - data; C = (Y >= data)
#define EOR(t_data) Z = N = A ^= t_data
#define LDA(t_data) Z = N = A = t_data
#define LDX(t_data) Z = N = X = t_data
#define LDY(t_data) Z = N = Y = t_data
#define ORA(t_data) Z = N = A |= t_data
#ifndef NO_V_FLAG_VARIABLE
#define PHP(x)      data = (N & 0x80) + (V ? 0x40 : 0) + (regP & (x)) + ((Z == 0) ? 0x02 : 0) + C; PH(data)
#define PHPB0       PHP(0x2c)  /* push flags with B flag clear (NMI, IRQ) */
#define PHPB1       PHP(0x3c)  /* push flags with B flag set (PHP, BRK) */
#define PLP         data = PL; N = data; V = (data & 0x40); Z = (data & 0x02) ^ 0x02; C = (data & 0x01); regP = (data & 0x0c) + 0x30
#else /* NO_V_FLAG_VARIABLE */
#define PHP(x)      data = (N & 0x80) + (regP & (x)) + ((Z == 0) ? 0x02 : 0) + C; PH(data)
#define PHPB0       PHP(0x6c)  /* push flags with B flag clear (NMI, IRQ) */
#define PHPB1       PHP(0x7c)  /* push flags with B flag set (PHP, BRK) */
#define PLP         data = PL; N = data; Z = (data & 0x02) ^ 0x02; C = (data & 0x01); regP = (data & 0x4c) + 0x30
#endif /* NO_V_FLAG_VARIABLE */
/* 1 or 2 extra cycles for conditional jumps */
#if 0
/* old, less efficient version */
#define BRANCH(cond) \
	if (cond) { \
		SWORD sdata = (SBYTE) GET_CODE_BYTE(); \
		if ((sdata + (UBYTE) GET_PC()) & 0xff00) \
			xpos++; \
		xpos++; \
		PC += sdata; \
		DONE \
	} \
	PC++; \
	DONE
#else
#define BRANCH(cond) \
	if (cond) { \
		addr = (UWORD) (SBYTE) IMMEDIATE; \
		addr += GET_PC(); \
		if ((addr ^ GET_PC()) & 0xff00) \
			xpos++; \
		xpos++; \
		SET_PC(addr); \
		DONE \
	} \
	PC++; \
	DONE
#endif

/* 1 extra cycle for X (or Y) index overflow */
#define NCYCLES_X   if ((UBYTE) addr < X) xpos++
#define NCYCLES_Y   if ((UBYTE) addr < Y) xpos++

/* Triggers a Non-Maskable Interrupt */
void NMI(void)
{
	UBYTE S = regS;
	UBYTE data;

	PHW(regPC);
	PHPB0;
	SetI;
	regPC = dGetWordAligned(0xfffa);
	regS = S;
	xpos += 7; /* handling an interrupt by 6502 takes 7 cycles */
	INC_RET_NESTING;
}

/* Check pending IRQ, helps in (not only) Lucasfilm games */
#define CPUCHECKIRQ \
	if (IRQ && !(regP & I_FLAG) && xpos < xpos_limit) { \
		PHPC; \
		PHPB0; \
		SetI; \
		SET_PC(dGetWordAligned(0xfffe)); \
		xpos += 7; \
		INC_RET_NESTING; \
	}

/* Enter monitor */
#ifdef __PLUS
#define ENTER_MONITOR  Atari800_Exit(TRUE)
#else
#define ENTER_MONITOR  if (!Atari800_Exit(TRUE)) exit(0)
#endif
#define DO_BREAK \
	UPDATE_GLOBAL_REGS; \
	CPU_GetStatus(); \
	ENTER_MONITOR; \
	CPU_PutStatus(); \
	UPDATE_LOCAL_REGS;


/*	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
static const int cycles[256] =
{
	7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,		/* 0x */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 1x */
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,		/* 2x */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 3x */

	6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,		/* 4x */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 5x */
	6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,		/* 6x */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* 7x */

	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,		/* 8x */
	2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,		/* 9x */
	2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,		/* Ax */
	2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,		/* Bx */

	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,		/* Cx */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,		/* Dx */
	2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,		/* Ex */
	2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7		/* Fx */
};

/* 6502 emulation routine */
#ifndef NO_GOTO
__extension__ /* suppress -ansi -pedantic warnings */
#endif
void GO(int limit)
{
#ifdef NO_GOTO
#define OPCODE_ALIAS(code)	case 0x##code:
#define DONE				break;
#else
#define OPCODE_ALIAS(code)	opcode_##code:
#define DONE				goto next;
	static const void *opcode[256] =
	{
		&&opcode_00, &&opcode_01, &&opcode_02, &&opcode_03,
		&&opcode_04, &&opcode_05, &&opcode_06, &&opcode_07,
		&&opcode_08, &&opcode_09, &&opcode_0a, &&opcode_0b,
		&&opcode_0c, &&opcode_0d, &&opcode_0e, &&opcode_0f,

		&&opcode_10, &&opcode_11, &&opcode_12, &&opcode_13,
		&&opcode_14, &&opcode_15, &&opcode_16, &&opcode_17,
		&&opcode_18, &&opcode_19, &&opcode_1a, &&opcode_1b,
		&&opcode_1c, &&opcode_1d, &&opcode_1e, &&opcode_1f,

		&&opcode_20, &&opcode_21, &&opcode_22, &&opcode_23,
		&&opcode_24, &&opcode_25, &&opcode_26, &&opcode_27,
		&&opcode_28, &&opcode_29, &&opcode_2a, &&opcode_2b,
		&&opcode_2c, &&opcode_2d, &&opcode_2e, &&opcode_2f,

		&&opcode_30, &&opcode_31, &&opcode_32, &&opcode_33,
		&&opcode_34, &&opcode_35, &&opcode_36, &&opcode_37,
		&&opcode_38, &&opcode_39, &&opcode_3a, &&opcode_3b,
		&&opcode_3c, &&opcode_3d, &&opcode_3e, &&opcode_3f,

		&&opcode_40, &&opcode_41, &&opcode_42, &&opcode_43,
		&&opcode_44, &&opcode_45, &&opcode_46, &&opcode_47,
		&&opcode_48, &&opcode_49, &&opcode_4a, &&opcode_4b,
		&&opcode_4c, &&opcode_4d, &&opcode_4e, &&opcode_4f,

		&&opcode_50, &&opcode_51, &&opcode_52, &&opcode_53,
		&&opcode_54, &&opcode_55, &&opcode_56, &&opcode_57,
		&&opcode_58, &&opcode_59, &&opcode_5a, &&opcode_5b,
		&&opcode_5c, &&opcode_5d, &&opcode_5e, &&opcode_5f,

		&&opcode_60, &&opcode_61, &&opcode_62, &&opcode_63,
		&&opcode_64, &&opcode_65, &&opcode_66, &&opcode_67,
		&&opcode_68, &&opcode_69, &&opcode_6a, &&opcode_6b,
		&&opcode_6c, &&opcode_6d, &&opcode_6e, &&opcode_6f,

		&&opcode_70, &&opcode_71, &&opcode_72, &&opcode_73,
		&&opcode_74, &&opcode_75, &&opcode_76, &&opcode_77,
		&&opcode_78, &&opcode_79, &&opcode_7a, &&opcode_7b,
		&&opcode_7c, &&opcode_7d, &&opcode_7e, &&opcode_7f,

		&&opcode_80, &&opcode_81, &&opcode_82, &&opcode_83,
		&&opcode_84, &&opcode_85, &&opcode_86, &&opcode_87,
		&&opcode_88, &&opcode_89, &&opcode_8a, &&opcode_8b,
		&&opcode_8c, &&opcode_8d, &&opcode_8e, &&opcode_8f,

		&&opcode_90, &&opcode_91, &&opcode_92, &&opcode_93,
		&&opcode_94, &&opcode_95, &&opcode_96, &&opcode_97,
		&&opcode_98, &&opcode_99, &&opcode_9a, &&opcode_9b,
		&&opcode_9c, &&opcode_9d, &&opcode_9e, &&opcode_9f,

		&&opcode_a0, &&opcode_a1, &&opcode_a2, &&opcode_a3,
		&&opcode_a4, &&opcode_a5, &&opcode_a6, &&opcode_a7,
		&&opcode_a8, &&opcode_a9, &&opcode_aa, &&opcode_ab,
		&&opcode_ac, &&opcode_ad, &&opcode_ae, &&opcode_af,

		&&opcode_b0, &&opcode_b1, &&opcode_b2, &&opcode_b3,
		&&opcode_b4, &&opcode_b5, &&opcode_b6, &&opcode_b7,
		&&opcode_b8, &&opcode_b9, &&opcode_ba, &&opcode_bb,
		&&opcode_bc, &&opcode_bd, &&opcode_be, &&opcode_bf,

		&&opcode_c0, &&opcode_c1, &&opcode_c2, &&opcode_c3,
		&&opcode_c4, &&opcode_c5, &&opcode_c6, &&opcode_c7,
		&&opcode_c8, &&opcode_c9, &&opcode_ca, &&opcode_cb,
		&&opcode_cc, &&opcode_cd, &&opcode_ce, &&opcode_cf,

		&&opcode_d0, &&opcode_d1, &&opcode_d2, &&opcode_d3,
		&&opcode_d4, &&opcode_d5, &&opcode_d6, &&opcode_d7,
		&&opcode_d8, &&opcode_d9, &&opcode_da, &&opcode_db,
		&&opcode_dc, &&opcode_dd, &&opcode_de, &&opcode_df,

		&&opcode_e0, &&opcode_e1, &&opcode_e2, &&opcode_e3,
		&&opcode_e4, &&opcode_e5, &&opcode_e6, &&opcode_e7,
		&&opcode_e8, &&opcode_e9, &&opcode_ea, &&opcode_eb,
		&&opcode_ec, &&opcode_ed, &&opcode_ee, &&opcode_ef,

		&&opcode_f0, &&opcode_f1, &&opcode_f2, &&opcode_f3,
		&&opcode_f4, &&opcode_f5, &&opcode_f6, &&opcode_f7,
		&&opcode_f8, &&opcode_f9, &&opcode_fa, &&opcode_fb,
		&&opcode_fc, &&opcode_fd, &&opcode_fe, &&opcode_ff,
	};
#endif	/* NO_GOTO */

#ifdef CYCLES_PER_OPCODE
#define OPCODE(code) OPCODE_ALIAS(code) xpos += cycles[0x##code];
#else
#define OPCODE(code) OPCODE_ALIAS(code)
#endif

#ifdef PC_PTR
	const UBYTE *PC;
#else
	UWORD PC;
#endif
	UBYTE A;
	UBYTE X;
	UBYTE Y;
	UBYTE S;

	UWORD addr;
	UBYTE data;
#define insn data

/*
   This used to be in the main loop but has been removed to improve
   execution speed. It does not seem to have any adverse effect on
   the emulation for two reasons:

   1. NMI's will can only be raised in antic.c - there is
      no way an NMI can be generated whilst in this routine.

   2. The timing of the IRQs are not that critical. */

	if (wsync_halt) {

#ifdef NEW_CYCLE_EXACT
		if (DRAWING_SCREEN) {
/* if WSYNC_C is a stolen cycle, antic2cpu_ptr will convert that to the nearest
   cpu cycle before that cycle.  The CPU will see this cycle, if WSYNC is not
   delayed. (Actually this cycle is the first cycle of the instruction after
   STA WSYNC, which was really executed one cycle after STA WSYNC because
   of an internal antic delay ).   delayed_wsync is added to this cycle to form
   the limit in the case that WSYNC is not early (does not allow this extra cycle) */

			if (limit < antic2cpu_ptr[WSYNC_C] + delayed_wsync)
				return;
			xpos = antic2cpu_ptr[WSYNC_C] + delayed_wsync;
		}
		else {
			if (limit < (WSYNC_C + delayed_wsync))
				return;
			xpos = WSYNC_C;
		}
		delayed_wsync = 0;

#else /* NEW_CYCLE_EXACT */

		if (limit < WSYNC_C)
			return;
		xpos = WSYNC_C;

#endif /* NEW_CYCLE_EXACT */

		wsync_halt = 0;
	}
	xpos_limit = limit;			/* needed for WSYNC store inside ANTIC */

	UPDATE_LOCAL_REGS;

	CPUCHECKIRQ;

	while (xpos < xpos_limit) {

#ifdef MONITOR_BREAKPOINTS
	breakpoint_return:
#endif

#ifdef PC_PTR
		/* must handle 64k wrapping */
		if (PC >= memory + 0xfffe) {
			if (PC >= memory + 0x10000)
				PC -= 0x10000;
			else {
				/* the opcode is before 0x10000, but the operand is past */
#ifdef WORDS_UNALIGNED_OK
				*(UWORD *) (memory + 0x10000) = *(UWORD *) memory;
#else
				memory[0x10000] = memory[0];
				memory[0x10001] = memory[1];
#endif /* WORDS_UNALIGNED_OK */
			}
		}
#endif /* PC_PTR */

#ifdef MONITOR_TRACE
		if (trace_file != NULL) {
			show_state(trace_file, GET_PC(), A, X, Y, S,
				(N & 0x80) ? 'N' : '-',
#ifndef NO_V_FLAG_VARIABLE
				V ? 'V' : '-',
#else
				(regP & V_FLAG) ? 'V' : '-',
#endif
				(Z == 0) ? 'Z' : '-',
				(C != 0) ? 'C' : '-');
		}
#endif

#ifdef MONITOR_BREAK
		remember_PC[remember_PC_curpos] = GET_PC();
#ifdef NEW_CYCLE_EXACT
		if (DRAWING_SCREEN)
			remember_xpos[remember_PC_curpos] = cpu2antic_ptr[xpos] + (ypos << 8);
		else
#endif
			remember_xpos[remember_PC_curpos] = xpos + (ypos << 8);
		remember_PC_curpos = (remember_PC_curpos + 1) % REMEMBER_PC_STEPS;

		if (break_addr == GET_PC() || break_ypos == ypos) {
			DO_BREAK;
		}
#endif /* MONITOR_BREAK */

#if defined(WRAP_64K) && !defined(PC_PTR)
		memory[0x10000] = memory[0];
#endif

		insn = GET_CODE_BYTE();

#ifdef MONITOR_BREAKPOINTS
		if (breakpoint_table_size > 0 && breakpoints_enabled) {
			UBYTE optype = optype6502[insn];
			int i;
			switch (optype >> 4) {
			case 1:
				addr = PEEK_CODE_WORD();
				break;
			case 2:
				addr = PEEK_CODE_BYTE();
				break;
			case 3:
				addr = PEEK_CODE_WORD() + X;
				break;
			case 4:
				addr = PEEK_CODE_WORD() + Y;
				break;
			case 5:
				addr = (UBYTE) (PEEK_CODE_BYTE() + X);
				addr = zGetWord(addr);
				break;
			case 6:
				addr = PEEK_CODE_BYTE();
				addr = zGetWord(addr) + Y;
				break;
			case 7:
				addr = (UBYTE) (PEEK_CODE_BYTE() + X);
				break;
			case 8:
				addr = (UBYTE) (PEEK_CODE_BYTE() + Y);
				break;
			/* XXX: case 13 */
			default:
				addr = 0;
				break;
			}
			for (i = 0; i < breakpoint_table_size; i++) {
				int cond;
				int value;
				if (!breakpoint_table[i].enabled)
					continue; /* skip */
				cond = breakpoint_table[i].condition;
				if (cond == BREAKPOINT_OR)
					break; /* fire */
				value = breakpoint_table[i].value;
				if (cond == BREAKPOINT_FLAG_CLEAR) {
					switch (value) {
					case N_FLAG:
						if ((N & 0x80) == 0)
							continue;
						break;
#ifndef NO_V_FLAG_VARIABLE
					case V_FLAG:
						if (V == 0)
							continue;
						break;
#endif
					case Z_FLAG:
						if (Z != 0)
							continue;
						break;
					case C_FLAG:
						if (C == 0)
							continue;
						break;
					default:
						if ((regP & value) == 0)
							continue;
						break;
					}
				}
				else if (cond == BREAKPOINT_FLAG_SET) {
					switch (value) {
					case N_FLAG:
						if ((N & 0x80) != 0)
							continue;
						break;
#ifndef NO_V_FLAG_VARIABLE
					case V_FLAG:
						if (V != 0)
							continue;
						break;
#endif
					case Z_FLAG:
						if (Z == 0)
							continue;
						break;
					case C_FLAG:
						if (C != 0)
							continue;
						break;
					default:
						if ((regP & value) != 0)
							continue;
						break;
					}
				}
				else {
					int val;
					switch (cond >> 3) {
					case BREAKPOINT_PC >> 3:
						val = GET_PC() - 1;
						break;
					case BREAKPOINT_A >> 3:
						val = A;
						break;
					case BREAKPOINT_X >> 3:
						val = X;
						break;
					case BREAKPOINT_Y >> 3:
						val = Y;
						break;
					case BREAKPOINT_S >> 3:
						val = S;
						break;
					case BREAKPOINT_READ >> 3:
						if ((optype & 4) == 0)
							goto cond_failed;
						val = addr;
						break;
					case BREAKPOINT_WRITE >> 3:
						if ((optype & 8) == 0)
							goto cond_failed;
						val = addr;
						break;
					case BREAKPOINT_ACCESS >> 3:
						if ((optype & 12) == 0)
							goto cond_failed;
						val = addr;
						break;
					default:
						/* shouldn't happen */
						continue;
					}
					if ((cond & BREAKPOINT_LESS) != 0 && val < value)
						continue;
					if ((cond & BREAKPOINT_EQUAL) != 0 && val == value)
						continue;
					if ((cond & BREAKPOINT_GREATER) != 0 && val > value)
						continue;
				cond_failed:
					;
				}
				/* a condition failed */
				/* quickly skip AND-connected conditions */
				do {
					if (++i >= breakpoint_table_size)
						goto no_breakpoint;
				} while (breakpoint_table[i].condition != BREAKPOINT_OR || !breakpoint_table[i].enabled);
			}
			/* fire breakpoint */
			PC--;
			DO_BREAK;
			goto breakpoint_return;
		no_breakpoint:
			;
		}
#endif /* MONITOR_BREAKPOINTS */

#ifndef CYCLES_PER_OPCODE
		xpos += cycles[insn];
#endif

#ifdef MONITOR_PROFILE
		instruction_count[insn]++;
#endif

#ifdef PREFETCH_CODE
		addr = PEEK_CODE_WORD();
#endif

#ifdef NO_GOTO
		switch (insn) {
#else
		goto *opcode[insn];
#endif

	OPCODE(00)				/* BRK */
#ifdef MONITOR_BREAK
		if (break_brk) {
			DO_BREAK;
		}
		else
#endif
		{
			PC++;
			PHPC;
			PHPB1;
			SetI;
			SET_PC(dGetWordAligned(0xfffe));
			INC_RET_NESTING;
		}
		DONE

	OPCODE(01)				/* ORA (ab,x) */
		INDIRECT_X;
		ORA(GetByte(addr));
		DONE

	OPCODE(03)				/* ASO (ab,x) [unofficial - ASL then ORA with Acc] */
		INDIRECT_X;

	aso:
		RMW_GetByte(data, addr);
		C = (data & 0x80) ? 1 : 0;
		data <<= 1;
		PutByte(addr, data);
		Z = N = A |= data;
		DONE

	OPCODE_ALIAS(04)		/* NOP ab [unofficial - skip byte] */
	OPCODE_ALIAS(44)
	OPCODE(64)
		PC++;
		DONE

	OPCODE_ALIAS(14)		/* NOP ab,x [unofficial - skip byte] */
	OPCODE_ALIAS(34)
	OPCODE_ALIAS(54)
	OPCODE_ALIAS(74)
	OPCODE_ALIAS(d4)
	OPCODE(f4)
		PC++;
		DONE

	OPCODE_ALIAS(80)		/* NOP #ab [unofficial - skip byte] */
	OPCODE_ALIAS(82)
	OPCODE_ALIAS(89)
	OPCODE_ALIAS(c2)
	OPCODE(e2)
		PC++;
		DONE

	OPCODE(05)				/* ORA ab */
		ZPAGE;
		ORA(dGetByte(addr));
		DONE

	OPCODE(06)				/* ASL ab */
		ZPAGE;
		data = dGetByte(addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(07)				/* ASO ab [unofficial - ASL then ORA with Acc] */
		ZPAGE;

	aso_zpage:
		data = dGetByte(addr);
		C = (data & 0x80) ? 1 : 0;
		data <<= 1;
		dPutByte(addr, data);
		Z = N = A |= data;
		DONE

	OPCODE(08)				/* PHP */
		PHPB1;
		DONE

	OPCODE(09)				/* ORA #ab */
		ORA(IMMEDIATE);
		DONE

	OPCODE(0a)				/* ASL */
		C = (A & 0x80) ? 1 : 0;
		Z = N = A <<= 1;
		DONE

	OPCODE_ALIAS(0b)		/* ANC #ab [unofficial - AND then copy N to C (Fox) */
	OPCODE(2b)
		AND(IMMEDIATE);
		C = N >= 0x80;
		DONE

	OPCODE(0c)				/* NOP abcd [unofficial - skip word] */
		PC += 2;
		DONE

	OPCODE(0d)				/* ORA abcd */
		ABSOLUTE;
		ORA(GetByte(addr));
		DONE

	OPCODE(0e)				/* ASL abcd */
		ABSOLUTE;
		RMW_GetByte(data, addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		PutByte(addr, Z);
		DONE

	OPCODE(0f)				/* ASO abcd [unofficial - ASL then ORA with Acc] */
		ABSOLUTE;
		goto aso;

	OPCODE(10)				/* BPL */
		BRANCH(!(N & 0x80))

	OPCODE(11)				/* ORA (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		ORA(GetByte(addr));
		DONE

	OPCODE(13)				/* ASO (ab),y [unofficial - ASL then ORA with Acc] */
		INDIRECT_Y;
		goto aso;

	OPCODE(15)				/* ORA ab,x */
		ZPAGE_X;
		ORA(dGetByte(addr));
		DONE

	OPCODE(16)				/* ASL ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(17)				/* ASO ab,x [unofficial - ASL then ORA with Acc] */
		ZPAGE_X;
		goto aso_zpage;

	OPCODE(18)				/* CLC */
		C = 0;
		DONE

	OPCODE(19)				/* ORA abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		ORA(GetByte(addr));
		DONE

	OPCODE(1b)				/* ASO abcd,y [unofficial - ASL then ORA with Acc] */
		ABSOLUTE_Y;
		goto aso;

	OPCODE_ALIAS(1c)		/* NOP abcd,x [unofficial - skip word] */
	OPCODE_ALIAS(3c)
	OPCODE_ALIAS(5c)
	OPCODE_ALIAS(7c)
	OPCODE_ALIAS(dc)
	OPCODE(fc)
		if (OP_BYTE + X >= 0x100)
			xpos++;
		PC += 2;
		DONE

	OPCODE(1d)				/* ORA abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		ORA(GetByte(addr));
		DONE

	OPCODE(1e)				/* ASL abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(data, addr);
		C = (data & 0x80) ? 1 : 0;
		Z = N = data << 1;
		PutByte(addr, Z);
		DONE

	OPCODE(1f)				/* ASO abcd,x [unofficial - ASL then ORA with Acc] */
		ABSOLUTE_X;
		goto aso;

	OPCODE(20)				/* JSR abcd */
		{
			UWORD retaddr = GET_PC() + 1;
#ifdef MONITOR_BREAK
			remember_JMP[remember_jmp_curpos] = GET_PC() - 1;
			remember_jmp_curpos = (remember_jmp_curpos + 1) % REMEMBER_JMP_STEPS;
			ret_nesting++;
#endif
			PHW(retaddr);
		}
		SET_PC(OP_WORD);
		DONE

	OPCODE(21)				/* AND (ab,x) */
		INDIRECT_X;
		AND(GetByte(addr));
		DONE

	OPCODE(23)				/* RLA (ab,x) [unofficial - ROL Mem, then AND with A] */
		INDIRECT_X;

	rla:
		RMW_GetByte(data, addr);
		if (C) {
			C = (data & 0x80) ? 1 : 0;
			data = (data << 1) + 1;
		}
		else {
			C = (data & 0x80) ? 1 : 0;
			data = (data << 1);
		}
		PutByte(addr, data);
		Z = N = A &= data;
		DONE

	OPCODE(24)				/* BIT ab */
		ZPAGE;
		N = dGetByte(addr);
#ifndef NO_V_FLAG_VARIABLE
		V = N & 0x40;
#else
		regP = (regP & 0xbf) + (N & 0x40);
#endif
		Z = (A & N);
		DONE

	OPCODE(25)				/* AND ab */
		ZPAGE;
		AND(dGetByte(addr));
		DONE

	OPCODE(26)				/* ROL ab */
		ZPAGE;
		data = dGetByte(addr);
		Z = N = (data << 1) + C;
		C = (data & 0x80) ? 1 : 0;
		dPutByte(addr, Z);
		DONE

	OPCODE(27)				/* RLA ab [unofficial - ROL Mem, then AND with A] */
		ZPAGE;

	rla_zpage:
		data = dGetByte(addr);
		if (C) {
			C = (data & 0x80) ? 1 : 0;
			data = (data << 1) + 1;
		}
		else {
			C = (data & 0x80) ? 1 : 0;
			data = (data << 1);
		}
		dPutByte(addr, data);
		Z = N = A &= data;
		DONE

	OPCODE(28)				/* PLP */
		PLP;
		CPUCHECKIRQ;
		DONE

	OPCODE(29)				/* AND #ab */
		AND(IMMEDIATE);
		DONE

	OPCODE(2a)				/* ROL */
		Z = N = (A << 1) + C;
		C = (A & 0x80) ? 1 : 0;
		A = Z;
		DONE

	OPCODE(2c)				/* BIT abcd */
		ABSOLUTE;
		N = GetByte(addr);
#ifndef NO_V_FLAG_VARIABLE
		V = N & 0x40;
#else
		regP = (regP & 0xbf) + (N & 0x40);
#endif
		Z = (A & N);
		DONE

	OPCODE(2d)				/* AND abcd */
		ABSOLUTE;
		AND(GetByte(addr));
		DONE

	OPCODE(2e)				/* ROL abcd */
		ABSOLUTE;
		RMW_GetByte(data, addr);
		Z = N = (data << 1) + C;
		C = (data & 0x80) ? 1 : 0;
		PutByte(addr, Z);
		DONE

	OPCODE(2f)				/* RLA abcd [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE;
		goto rla;

	OPCODE(30)				/* BMI */
		BRANCH(N & 0x80)

	OPCODE(31)				/* AND (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		AND(GetByte(addr));
		DONE

	OPCODE(33)				/* RLA (ab),y [unofficial - ROL Mem, then AND with A] */
		INDIRECT_Y;
		goto rla;

	OPCODE(35)				/* AND ab,x */
		ZPAGE_X;
		AND(dGetByte(addr));
		DONE

	OPCODE(36)				/* ROL ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		Z = N = (data << 1) + C;
		C = (data & 0x80) ? 1 : 0;
		dPutByte(addr, Z);
		DONE

	OPCODE(37)				/* RLA ab,x [unofficial - ROL Mem, then AND with A] */
		ZPAGE_X;
		goto rla_zpage;

	OPCODE(38)				/* SEC */
		C = 1;
		DONE

	OPCODE(39)				/* AND abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		AND(GetByte(addr));
		DONE

	OPCODE(3b)				/* RLA abcd,y [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE_Y;
		goto rla;

	OPCODE(3d)				/* AND abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		AND(GetByte(addr));
		DONE

	OPCODE(3e)				/* ROL abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(data, addr);
		Z = N = (data << 1) + C;
		C = (data & 0x80) ? 1 : 0;
		PutByte(addr, Z);
		DONE

	OPCODE(3f)				/* RLA abcd,x [unofficial - ROL Mem, then AND with A] */
		ABSOLUTE_X;
		goto rla;

	OPCODE(40)				/* RTI */
		PLP;
		data = PL;
		SET_PC((PL << 8) + data);
		CPUCHECKIRQ;
#ifdef MONITOR_BREAK
		if (break_ret && --ret_nesting <= 0)
			break_step = TRUE;
#endif
		DONE

	OPCODE(41)				/* EOR (ab,x) */
		INDIRECT_X;
		EOR(GetByte(addr));
		DONE

	OPCODE(43)				/* LSE (ab,x) [unofficial - LSR then EOR result with A] */
		INDIRECT_X;

	lse:
		RMW_GetByte(data, addr);
		C = data & 1;
		data >>= 1;
		PutByte(addr, data);
		Z = N = A ^= data;
		DONE

	OPCODE(45)				/* EOR ab */
		ZPAGE;
		EOR(dGetByte(addr));
		DONE

	OPCODE(46)				/* LSR ab */
		ZPAGE;
		data = dGetByte(addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		dPutByte(addr, Z);
		DONE

	OPCODE(47)				/* LSE ab [unofficial - LSR then EOR result with A] */
		ZPAGE;

	lse_zpage:
		data = dGetByte(addr);
		C = data & 1;
		data >>= 1;
		dPutByte(addr, data);
		Z = N = A ^= data;
		DONE

	OPCODE(48)				/* PHA */
		PH(A);
		DONE

	OPCODE(49)				/* EOR #ab */
		EOR(IMMEDIATE);
		DONE

	OPCODE(4a)				/* LSR */
		C = A & 1;
		Z = N = A >>= 1;
		DONE

	OPCODE(4b)				/* ALR #ab [unofficial - Acc AND Data, LSR result] */
		data = A & IMMEDIATE;
		C = data & 1;
		Z = N = A = (data >> 1);
		DONE

	OPCODE(4c)				/* JMP abcd */
#ifdef MONITOR_BREAK
		remember_JMP[remember_jmp_curpos] = GET_PC() - 1;
		remember_jmp_curpos = (remember_jmp_curpos + 1) % REMEMBER_JMP_STEPS;
#endif
		SET_PC(OP_WORD);
		DONE

	OPCODE(4d)				/* EOR abcd */
		ABSOLUTE;
		EOR(GetByte(addr));
		DONE

	OPCODE(4e)				/* LSR abcd */
		ABSOLUTE;
		RMW_GetByte(data, addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		PutByte(addr, Z);
		DONE

	OPCODE(4f)				/* LSE abcd [unofficial - LSR then EOR result with A] */
		ABSOLUTE;
		goto lse;

	OPCODE(50)				/* BVC */
#ifndef NO_V_FLAG_VARIABLE
		BRANCH(!V)
#else
		BRANCH(!(regP & 0x40))
#endif

	OPCODE(51)				/* EOR (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		EOR(GetByte(addr));
		DONE

	OPCODE(53)				/* LSE (ab),y [unofficial - LSR then EOR result with A] */
		INDIRECT_Y;
		goto lse;

	OPCODE(55)				/* EOR ab,x */
		ZPAGE_X;
		EOR(dGetByte(addr));
		DONE

	OPCODE(56)				/* LSR ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		dPutByte(addr, Z);
		DONE

	OPCODE(57)				/* LSE ab,x [unofficial - LSR then EOR result with A] */
		ZPAGE_X;
		goto lse_zpage;

	OPCODE(58)				/* CLI */
		ClrI;
		CPUCHECKIRQ;
		DONE

	OPCODE(59)				/* EOR abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		EOR(GetByte(addr));
		DONE

	OPCODE(5b)				/* LSE abcd,y [unofficial - LSR then EOR result with A] */
		ABSOLUTE_Y;
		goto lse;

	OPCODE(5d)				/* EOR abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		EOR(GetByte(addr));
		DONE

	OPCODE(5e)				/* LSR abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(data, addr);
		C = data & 1;
		Z = data >> 1;
		N = 0;
		PutByte(addr, Z);
		DONE

	OPCODE(5f)				/* LSE abcd,x [unofficial - LSR then EOR result with A] */
		ABSOLUTE_X;
		goto lse;

	OPCODE(60)				/* RTS */
		data = PL;
		SET_PC((PL << 8) + data + 1);
#ifdef MONITOR_BREAK
		if (break_ret && --ret_nesting <= 0)
			break_step = TRUE;
#endif
		if (rts_handler != NULL) {
			rts_handler();
			rts_handler = NULL;
		}
		DONE

	OPCODE(61)				/* ADC (ab,x) */
		INDIRECT_X;
		data = GetByte(addr);
		goto adc;

	OPCODE(63)				/* RRA (ab,x) [unofficial - ROR Mem, then ADC to Acc] */
		INDIRECT_X;

	rra:
		RMW_GetByte(data, addr);
		if (C) {
			C = data & 1;
			data = (data >> 1) + 0x80;
		}
		else {
			C = data & 1;
			data >>= 1;
		}
		PutByte(addr, data);
		goto adc;

	OPCODE(65)				/* ADC ab */
		ZPAGE;
		data = dGetByte(addr);
		goto adc;

	OPCODE(66)				/* ROR ab */
		ZPAGE;
		data = dGetByte(addr);
		Z = N = (C << 7) + (data >> 1);
		C = data & 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(67)				/* RRA ab [unofficial - ROR Mem, then ADC to Acc] */
		ZPAGE;

	rra_zpage:
		data = dGetByte(addr);
		if (C) {
			C = data & 1;
			data = (data >> 1) + 0x80;
		}
		else {
			C = data & 1;
			data >>= 1;
		}
		dPutByte(addr, data);
		goto adc;

	OPCODE(68)				/* PLA */
		Z = N = A = PL;
		DONE

	OPCODE(69)				/* ADC #ab */
		data = IMMEDIATE;
		goto adc;

	OPCODE(6a)				/* ROR */
		Z = N = (C << 7) + (A >> 1);
		C = A & 1;
		A = Z;
		DONE

	OPCODE(6b)				/* ARR #ab [unofficial - Acc AND Data, ROR result] */
		/* It does some 'BCD fixup' if D flag is set */
		/* MPC 05/24/00 */
		data = A & IMMEDIATE;
		if (regP & D_FLAG) {
			UBYTE temp = (data >> 1) + (C << 7);
			Z = N = temp;
#ifndef NO_V_FLAG_VARIABLE
			V = ((temp ^ data) & 0x40);
#else
			regP = (regP & 0xbf) + ((temp ^ data) & 0x40);
#endif
			if ((data & 0x0F) + (data & 0x01) > 5)
				temp = (temp & 0xF0) + ((temp + 0x6) & 0x0F);
			if (data + (data & 0x10) >= 0x60) {
				temp += 0x60;
				C = 1;
			}
			else
				C = 0;
			A = (UBYTE) temp;
		}
		else {
			Z = N = A = (data >> 1) + (C << 7);
			C = data >> 7;
#ifndef NO_V_FLAG_VARIABLE
			V = C ^ ((A >> 5) & 1);
#else
			regP = (regP & 0xbf) + ((A ^ data) & 0x40);
#endif
		}
		DONE

	OPCODE(6c)				/* JMP (abcd) */
#ifdef MONITOR_BREAK
		remember_JMP[remember_jmp_curpos] = GET_PC() - 1;
		remember_jmp_curpos = (remember_jmp_curpos + 1) % REMEMBER_JMP_STEPS;
#endif
		ABSOLUTE;
#ifdef CPU65C02
		/* XXX: if ((UBYTE) addr == 0xff) xpos++; */
		SET_PC(dGetWord(addr));
#else
		/* original 6502 had a bug in JMP (addr) when addr crossed page boundary */
		if ((UBYTE) addr == 0xff)
			SET_PC((dGetByte(addr - 0xff) << 8) + dGetByte(addr));
		else
			SET_PC(dGetWord(addr));
#endif
		DONE

	OPCODE(6d)				/* ADC abcd */
		ABSOLUTE;
		data = GetByte(addr);
		goto adc;

	OPCODE(6e)				/* ROR abcd */
		ABSOLUTE;
		RMW_GetByte(data, addr);
		Z = N = (C << 7) + (data >> 1);
		C = data & 1;
		PutByte(addr, Z);
		DONE

	OPCODE(6f)				/* RRA abcd [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE;
		goto rra;

	OPCODE(70)				/* BVS */
#ifndef NO_V_FLAG_VARIABLE
		BRANCH(V)
#else
		BRANCH(regP & 0x40)
#endif

	OPCODE(71)				/* ADC (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto adc;

	OPCODE(73)				/* RRA (ab),y [unofficial - ROR Mem, then ADC to Acc] */
		INDIRECT_Y;
		goto rra;

	OPCODE(75)				/* ADC ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		goto adc;

	OPCODE(76)				/* ROR ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		Z = N = (C << 7) + (data >> 1);
		C = data & 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(77)				/* RRA ab,x [unofficial - ROR Mem, then ADC to Acc] */
		ZPAGE_X;
		goto rra_zpage;

	OPCODE(78)				/* SEI */
		SetI;
		DONE

	OPCODE(79)				/* ADC abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto adc;

	OPCODE(7b)				/* RRA abcd,y [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE_Y;
		goto rra;

	OPCODE(7d)				/* ADC abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		data = GetByte(addr);
		goto adc;

	OPCODE(7e)				/* ROR abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(data, addr);
		Z = N = (C << 7) + (data >> 1);
		C = data & 1;
		PutByte(addr, Z);
		DONE

	OPCODE(7f)				/* RRA abcd,x [unofficial - ROR Mem, then ADC to Acc] */
		ABSOLUTE_X;
		goto rra;

	OPCODE(81)				/* STA (ab,x) */
		INDIRECT_X;
		PutByte(addr, A);
		DONE

	/* AXS doesn't change flags and SAX is better name for it (Fox) */
	OPCODE(83)				/* SAX (ab,x) [unofficial - Store result A AND X */
		INDIRECT_X;
		data = A & X;
		PutByte(addr, data);
		DONE

	OPCODE(84)				/* STY ab */
		ZPAGE;
		dPutByte(addr, Y);
		DONE

	OPCODE(85)				/* STA ab */
		ZPAGE;
		dPutByte(addr, A);
		DONE

	OPCODE(86)				/* STX ab */
		ZPAGE;
		dPutByte(addr, X);
		DONE

	OPCODE(87)				/* SAX ab [unofficial - Store result A AND X] */
		ZPAGE;
		data = A & X;
		dPutByte(addr, data);
		DONE

	OPCODE(88)				/* DEY */
		Z = N = --Y;
		DONE

	OPCODE(8a)				/* TXA */
		Z = N = A = X;
		DONE

	OPCODE(8b)				/* ANE #ab [unofficial - A AND X AND (Mem OR $EF) to Acc] (Fox) */
		data = IMMEDIATE;
		N = Z = A & X & data;
		A &= X & (data | 0xef);
		DONE

	OPCODE(8c)				/* STY abcd */
		ABSOLUTE;
		PutByte(addr, Y);
		DONE

	OPCODE(8d)				/* STA abcd */
		ABSOLUTE;
		PutByte(addr, A);
		DONE

	OPCODE(8e)				/* STX abcd */
		ABSOLUTE;
		PutByte(addr, X);
		DONE

	OPCODE(8f)				/* SAX abcd [unofficial - Store result A AND X] */
		ABSOLUTE;
		data = A & X;
		PutByte(addr, data);
		DONE

	OPCODE(90)				/* BCC */
		BRANCH(!C)

	OPCODE(91)				/* STA (ab),y */
		INDIRECT_Y;
		PutByte(addr, A);
		DONE

	OPCODE(93)				/* SHA (ab),y [unofficial, UNSTABLE - Store A AND X AND (H+1) ?] (Fox) */
		/* It seems previous memory value is important - also in 9f */
		ZPAGE;
		data = dGetByte((UBYTE) (addr + 1));	/* Get high byte from zpage */
		data = A & X & (data + 1);
		addr = dGetWord(addr) + Y;
		PutByte(addr, data);
		DONE

	OPCODE(94)				/* STY ab,x */
		ZPAGE_X;
		dPutByte(addr, Y);
		DONE

	OPCODE(95)				/* STA ab,x */
		ZPAGE_X;
		dPutByte(addr, A);
		DONE

	OPCODE(96)				/* STX ab,y */
		ZPAGE_Y;
		PutByte(addr, X);
		DONE

	OPCODE(97)				/* SAX ab,y [unofficial - Store result A AND X] */
		ZPAGE_Y;
		data = A & X;
		dPutByte(addr, data);
		DONE

	OPCODE(98)				/* TYA */
		Z = N = A = Y;
		DONE

	OPCODE(99)				/* STA abcd,y */
		ABSOLUTE_Y;
		PutByte(addr, A);
		DONE

	OPCODE(9a)				/* TXS */
		S = X;
		DONE

	OPCODE(9b)				/* SHS abcd,y [unofficial, UNSTABLE] (Fox) */
		/* Transfer A AND X to S, then store S AND (H+1)] */
		/* S seems to be stable, only memory values vary */
		ABSOLUTE;
		S = A & X;
		data = S & ((addr >> 8) + 1);
		addr += Y;
		PutByte(addr, data);
		DONE

	OPCODE(9c)				/* SHY abcd,x [unofficial - Store Y and (H+1)] (Fox) */
		/* Seems to be stable */
		ABSOLUTE;
		/* MPC 05/24/00 */
		data = Y & ((UBYTE) ((addr >> 8) + 1));
		addr += X;
		PutByte(addr, data);
		DONE

	OPCODE(9d)				/* STA abcd,x */
		ABSOLUTE_X;
		PutByte(addr, A);
		DONE

	OPCODE(9e)				/* SHX abcd,y [unofficial - Store X and (H+1)] (Fox) */
		/* Seems to be stable */
		ABSOLUTE;
		/* MPC 05/24/00 */
		data = X & ((UBYTE) ((addr >> 8) + 1));
		addr += Y;
		PutByte(addr, data);
		DONE

	OPCODE(9f)				/* SHA abcd,y [unofficial, UNSTABLE - Store A AND X AND (H+1) ?] (Fox) */
		ABSOLUTE;
		data = A & X & ((addr >> 8) + 1);
		addr += Y;
		PutByte(addr, data);
		DONE

	OPCODE(a0)				/* LDY #ab */
		LDY(IMMEDIATE);
		DONE

	OPCODE(a1)				/* LDA (ab,x) */
		INDIRECT_X;
		LDA(GetByte(addr));
		DONE

	OPCODE(a2)				/* LDX #ab */
		LDX(IMMEDIATE);
		DONE

	OPCODE(a3)				/* LAX (ab,x) [unofficial] */
		INDIRECT_X;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(a4)				/* LDY ab */
		ZPAGE;
		LDY(dGetByte(addr));
		DONE

	OPCODE(a5)				/* LDA ab */
		ZPAGE;
		LDA(dGetByte(addr));
		DONE

	OPCODE(a6)				/* LDX ab */
		ZPAGE;
		LDX(dGetByte(addr));
		DONE

	OPCODE(a7)				/* LAX ab [unofficial] */
		ZPAGE;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(a8)				/* TAY */
		Z = N = Y = A;
		DONE

	OPCODE(a9)				/* LDA #ab */
		LDA(IMMEDIATE);
		DONE

	OPCODE(aa)				/* TAX */
		Z = N = X = A;
		DONE

	OPCODE(ab)				/* ANX #ab [unofficial - AND #ab, then TAX] */
		Z = N = X = A &= IMMEDIATE;
		DONE

	OPCODE(ac)				/* LDY abcd */
		ABSOLUTE;
		LDY(GetByte(addr));
		DONE

	OPCODE(ad)				/* LDA abcd */
		ABSOLUTE;
		LDA(GetByte(addr));
		DONE

	OPCODE(ae)				/* LDX abcd */
		ABSOLUTE;
		LDX(GetByte(addr));
		DONE

	OPCODE(af)				/* LAX abcd [unofficial] */
		ABSOLUTE;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(b0)				/* BCS */
		BRANCH(C)

	OPCODE(b1)				/* LDA (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		LDA(GetByte(addr));
		DONE

	OPCODE(b3)				/* LAX (ab),y [unofficial] */
		INDIRECT_Y;
		NCYCLES_Y;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(b4)				/* LDY ab,x */
		ZPAGE_X;
		LDY(dGetByte(addr));
		DONE

	OPCODE(b5)				/* LDA ab,x */
		ZPAGE_X;
		LDA(dGetByte(addr));
		DONE

	OPCODE(b6)				/* LDX ab,y */
		ZPAGE_Y;
		LDX(GetByte(addr));
		DONE

	OPCODE(b7)				/* LAX ab,y [unofficial] */
		ZPAGE_Y;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(b8)				/* CLV */
#ifndef NO_V_FLAG_VARIABLE
		V = 0;
#else
		ClrV;
#endif
		DONE

	OPCODE(b9)				/* LDA abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		LDA(GetByte(addr));
		DONE

	OPCODE(ba)				/* TSX */
		Z = N = X = S;
		DONE

/* AXA [unofficial - original decode by R.Sterba and R.Petruzela 15.1.1998 :-)]
   AXA - this is our new imaginative name for instruction with opcode hex BB.
   AXA - Store Mem AND #$FD to Acc and X, then set stackpoint to value (Acc - 4)
   It's cool! :-)
   LAS - this is better name for this :) (Fox)
   It simply ANDs stack pointer with Mem, then transfers result to A and X
 */

	OPCODE(bb)				/* LAS abcd,y [unofficial - AND S with Mem, transfer to A and X (Fox) */
		ABSOLUTE_Y;
		NCYCLES_Y;
		Z = N = A = X = S &= GetByte(addr);
		DONE

	OPCODE(bc)				/* LDY abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		LDY(GetByte(addr));
		DONE

	OPCODE(bd)				/* LDA abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		LDA(GetByte(addr));
		DONE

	OPCODE(be)				/* LDX abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		LDX(GetByte(addr));
		DONE

	OPCODE(bf)				/* LAX abcd,y [unofficial] */
		ABSOLUTE_Y;
		NCYCLES_Y;
		Z = N = X = A = GetByte(addr);
		DONE

	OPCODE(c0)				/* CPY #ab */
		CPY(IMMEDIATE);
		DONE

	OPCODE(c1)				/* CMP (ab,x) */
		INDIRECT_X;
		CMP(GetByte(addr));
		DONE

	OPCODE(c3)				/* DCM (ab,x) [unofficial - DEC Mem then CMP with Acc] */
		INDIRECT_X;

	dcm:
		RMW_GetByte(data, addr);
		data--;
		PutByte(addr, data);
		CMP(data);
		DONE

	OPCODE(c4)				/* CPY ab */
		ZPAGE;
		CPY(dGetByte(addr));
		DONE

	OPCODE(c5)				/* CMP ab */
		ZPAGE;
		CMP(dGetByte(addr));
		DONE

	OPCODE(c6)				/* DEC ab */
		ZPAGE;
		Z = N = dGetByte(addr) - 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(c7)				/* DCM ab [unofficial - DEC Mem then CMP with Acc] */
		ZPAGE;

	dcm_zpage:
		data = dGetByte(addr) - 1;
		dPutByte(addr, data);
		CMP(data);
		DONE

	OPCODE(c8)				/* INY */
		Z = N = ++Y;
		DONE

	OPCODE(c9)				/* CMP #ab */
		CMP(IMMEDIATE);
		DONE

	OPCODE(ca)				/* DEX */
		Z = N = --X;
		DONE

	OPCODE(cb)				/* SBX #ab [unofficial - store ((A AND X) - Mem) in X] (Fox) */
		X &= A;
		data = IMMEDIATE;
		C = X >= data;
		/* MPC 05/24/00 */
		Z = N = X -= data;
		DONE

	OPCODE(cc)				/* CPY abcd */
		ABSOLUTE;
		CPY(GetByte(addr));
		DONE

	OPCODE(cd)				/* CMP abcd */
		ABSOLUTE;
		CMP(GetByte(addr));
		DONE

	OPCODE(ce)				/* DEC abcd */
		ABSOLUTE;
		RMW_GetByte(Z, addr);
		N = --Z;
		PutByte(addr, Z);
		DONE

	OPCODE(cf)				/* DCM abcd [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE;
		goto dcm;

	OPCODE(d0)				/* BNE */
		BRANCH(Z)

	OPCODE(d1)				/* CMP (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		CMP(GetByte(addr));
		DONE

	OPCODE(d3)				/* DCM (ab),y [unofficial - DEC Mem then CMP with Acc] */
		INDIRECT_Y;
		goto dcm;

	OPCODE(d5)				/* CMP ab,x */
		ZPAGE_X;
		CMP(dGetByte(addr));
		Z = N = A - data;
		C = (A >= data);
		DONE

	OPCODE(d6)				/* DEC ab,x */
		ZPAGE_X;
		Z = N = dGetByte(addr) - 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(d7)				/* DCM ab,x [unofficial - DEC Mem then CMP with Acc] */
		ZPAGE_X;
		goto dcm_zpage;

	OPCODE(d8)				/* CLD */
		ClrD;
		DONE

	OPCODE(d9)				/* CMP abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		CMP(GetByte(addr));
		DONE

	OPCODE(db)				/* DCM abcd,y [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE_Y;
		goto dcm;

	OPCODE(dd)				/* CMP abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		CMP(GetByte(addr));
		DONE

	OPCODE(de)				/* DEC abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(Z, addr);
		N = --Z;
		PutByte(addr, Z);
		DONE

	OPCODE(df)				/* DCM abcd,x [unofficial - DEC Mem then CMP with Acc] */
		ABSOLUTE_X;
		goto dcm;

	OPCODE(e0)				/* CPX #ab */
		CPX(IMMEDIATE);
		DONE

	OPCODE(e1)				/* SBC (ab,x) */
		INDIRECT_X;
		data = GetByte(addr);
		goto sbc;

	OPCODE(e3)				/* INS (ab,x) [unofficial - INC Mem then SBC with Acc] */
		INDIRECT_X;

	ins:
		RMW_GetByte(data, addr);
		++data;
		PutByte(addr, data);
		goto sbc;

	OPCODE(e4)				/* CPX ab */
		ZPAGE;
		CPX(dGetByte(addr));
		DONE

	OPCODE(e5)				/* SBC ab */
		ZPAGE;
		data = dGetByte(addr);
		goto sbc;

	OPCODE(e6)				/* INC ab */
		ZPAGE;
		Z = N = dGetByte(addr) + 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(e7)				/* INS ab [unofficial - INC Mem then SBC with Acc] */
		ZPAGE;

	ins_zpage:
		data = dGetByte(addr) + 1;
		dPutByte(addr, data);
		goto sbc;

	OPCODE(e8)				/* INX */
		Z = N = ++X;
		DONE

	OPCODE_ALIAS(e9)		/* SBC #ab */
	OPCODE(eb)				/* SBC #ab [unofficial] */
		data = IMMEDIATE;
		goto sbc;

	OPCODE_ALIAS(ea)		/* NOP */
	OPCODE_ALIAS(1a)		/* NOP [unofficial] */
	OPCODE_ALIAS(3a)
	OPCODE_ALIAS(5a)
	OPCODE_ALIAS(7a)
	OPCODE_ALIAS(da)
	OPCODE(fa)
		DONE

	OPCODE(ec)				/* CPX abcd */
		ABSOLUTE;
		CPX(GetByte(addr));
		DONE

	OPCODE(ed)				/* SBC abcd */
		ABSOLUTE;
		data = GetByte(addr);
		goto sbc;

	OPCODE(ee)				/* INC abcd */
		ABSOLUTE;
		RMW_GetByte(Z, addr);
		N = ++Z;
		PutByte(addr, Z);
		DONE

	OPCODE(ef)				/* INS abcd [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE;
		goto ins;

	OPCODE(f0)				/* BEQ */
		BRANCH(!Z)

	OPCODE(f1)				/* SBC (ab),y */
		INDIRECT_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto sbc;

	OPCODE(f3)				/* INS (ab),y [unofficial - INC Mem then SBC with Acc] */
		INDIRECT_Y;
		goto ins;

	OPCODE(f5)				/* SBC ab,x */
		ZPAGE_X;
		data = dGetByte(addr);
		goto sbc;

	OPCODE(f6)				/* INC ab,x */
		ZPAGE_X;
		Z = N = dGetByte(addr) + 1;
		dPutByte(addr, Z);
		DONE

	OPCODE(f7)				/* INS ab,x [unofficial - INC Mem then SBC with Acc] */
		ZPAGE_X;
		goto ins_zpage;

	OPCODE(f8)				/* SED */
		SetD;
		DONE

	OPCODE(f9)				/* SBC abcd,y */
		ABSOLUTE_Y;
		NCYCLES_Y;
		data = GetByte(addr);
		goto sbc;

	OPCODE(fb)				/* INS abcd,y [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE_Y;
		goto ins;

	OPCODE(fd)				/* SBC abcd,x */
		ABSOLUTE_X;
		NCYCLES_X;
		data = GetByte(addr);
		goto sbc;

	OPCODE(fe)				/* INC abcd,x */
		ABSOLUTE_X;
		RMW_GetByte(Z, addr);
		N = ++Z;
		PutByte(addr, Z);
		DONE

	OPCODE(ff)				/* INS abcd,x [unofficial - INC Mem then SBC with Acc] */
		ABSOLUTE_X;
		goto ins;

#ifdef ASAP

	OPCODE_ALIAS(d2)
	OPCODE_ALIAS(f2)

#else

	OPCODE(d2)				/* ESCRTS #ab (CIM) - on Atari is here instruction CIM [unofficial] !RS! */
		data = IMMEDIATE;
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();
		Atari800_RunEsc(data);
		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
		data = PL;
		SET_PC((PL << 8) + data + 1);
#ifdef MONITOR_BREAK
		if (break_ret && --ret_nesting <= 0)
			break_step = TRUE;
#endif
		DONE

	OPCODE(f2)				/* ESC #ab (CIM) - on Atari is here instruction CIM [unofficial] !RS! */
		/* OPCODE(ff: ESC #ab - opcode FF is now used for INS [unofficial] instruction !RS! */
		data = IMMEDIATE;
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();
		Atari800_RunEsc(data);
		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
		DONE

#endif /* ASAP */

	OPCODE_ALIAS(02)		/* CIM [unofficial - crash intermediate] */
	OPCODE_ALIAS(12)
	OPCODE_ALIAS(22)
	OPCODE_ALIAS(32)
	OPCODE_ALIAS(42)
	OPCODE_ALIAS(52)
	OPCODE_ALIAS(62)
	OPCODE_ALIAS(72)
	OPCODE_ALIAS(92)
	OPCODE(b2)

#ifdef ASAP

		ASAP_CIM();
		DONE

#else

	/* OPCODE(d2) Used for ESCRTS #ab (CIM) */
	/* OPCODE(f2) Used for ESC #ab (CIM) */
		PC--;
		UPDATE_GLOBAL_REGS;
		CPU_GetStatus();

#ifdef CRASH_MENU
		crash_address = GET_PC();
		crash_afterCIM = GET_PC() + 1;
		crash_code = insn;
		ui();
#else
		cim_encountered = TRUE;
		ENTER_MONITOR;
#endif /* CRASH_MENU */

		CPU_PutStatus();
		UPDATE_LOCAL_REGS;
		DONE

#endif /* ASAP */

/* ---------------------------------------------- */
/* ADC and SBC routines */

	adc:
		if (!(regP & D_FLAG)) {
			/* Binary mode */
			unsigned int tmp;
			tmp = A + data + C;
			C = tmp > 0xff;
			/* C = tmp >> 8; */
#ifndef NO_V_FLAG_VARIABLE
			V = !((A ^ data) & 0x80) && ((data ^ tmp) & 0x80);
#else
			ClrV;
			if (!((A ^ data) & 0x80) && ((data ^ tmp) & 0x80))
				SetV;
#endif
			Z = N = A = (UBYTE) tmp;
	    }
		else {
			/* Decimal mode */
			unsigned int tmp;
			tmp = (A & 0x0f) + (data & 0x0f) + C;
			if (tmp >= 10)
				tmp = (tmp - 10) | 0x10;
			tmp += (A & 0xf0) + (data & 0xf0);

			Z = A + data + C;
			N = (UBYTE) tmp;
#ifndef NO_V_FLAG_VARIABLE
			V = !((A ^ data) & 0x80) && ((data ^ tmp) & 0x80);
#else
			ClrV;
			if (!((A ^ data) & 0x80) && ((data ^ tmp) & 0x80))
				SetV;
#endif

			if (tmp > 0x9f)
				tmp += 0x60;
			C = tmp > 0xff;
			A = (UBYTE) tmp;
		}
		DONE

	sbc:
		if (!(regP & D_FLAG)) {
			/* Binary mode */
			unsigned int tmp;
			/* tmp = A - data - !C; */
			tmp = A - data - 1 + C;
			C = tmp < 0x100;
#ifndef NO_V_FLAG_VARIABLE
			V = ((A ^ tmp) & 0x80) && ((A ^ data) & 0x80);
#else
			ClrV;
			if (((A ^ tmp) & 0x80) && ((A ^ data) & 0x80))
				SetV;
#endif
			Z = N = A = (UBYTE) tmp;
		}
		else {
			/* Decimal mode */
			unsigned int al, ah, tmp;
			/* tmp = A - data - !C; */
			tmp = A - data - 1 + C;
			/* al = (A & 0x0f) - (data & 0x0f) - !C; */
			al = (A & 0x0f) - (data & 0x0f) - 1 + C;	/* Calculate lower nybble */
			ah = (A >> 4) - (data >> 4);		/* Calculate upper nybble */
			if (al & 0x10) {
				al -= 6;	/* BCD fixup for lower nybble */
				ah--;
			}
			if (ah & 0x10)
				ah -= 6;	/* BCD fixup for upper nybble */

			C = tmp < 0x100;			/* Set flags */
#ifndef NO_V_FLAG_VARIABLE
			V = ((A ^ tmp) & 0x80) && ((A ^ data) & 0x80);
#else
			ClrV;
			if (((A ^ tmp) & 0x80) && ((A ^ data) & 0x80))
				SetV;
#endif
			Z = N = (UBYTE) tmp;

			A = (ah << 4) + (al & 0x0f);	/* Compose result */
		}
		DONE

#ifdef NO_GOTO
	}
#else
	next:
#endif

#ifdef MONITOR_BREAK
		if (break_step) {
			DO_BREAK;
		}
#endif
		/* This "continue" does nothing here.
		   But it is necessary because, if we're not using NO_GOTO nor MONITOR_BREAK,
		   gcc can complain: "error: label at end of compound statement". */
		continue;
	}

	UPDATE_GLOBAL_REGS;
}

void CPU_Initialise(void)
{
}

#endif /* FALCON_CPUASM */

void CPU_Reset(void)
{
#ifdef MONITOR_PROFILE
	memset(instruction_count, 0, sizeof(instruction_count));
#endif

	IRQ = 0;

	regP = 0x34;				/* The unused bit is always 1, I flag set! */
	CPU_PutStatus();	/* Make sure flags are all updated */
	regS = 0xff;
	regPC = dGetWordAligned(0xfffc);
}

#if !defined(BASIC) && !defined(ASAP)

void CpuStateSave(UBYTE SaveVerbose)
{
	SaveUBYTE(&regA, 1);

	CPU_GetStatus();	/* Make sure flags are all updated */
	SaveUBYTE(&regP, 1);

	SaveUBYTE(&regS, 1);
	SaveUBYTE(&regX, 1);
	SaveUBYTE(&regY, 1);
	SaveUBYTE(&IRQ, 1);

	MemStateSave(SaveVerbose);

	SaveUWORD(&regPC, 1);
}

void CpuStateRead(UBYTE SaveVerbose, UBYTE StateVersion)
{
	ReadUBYTE(&regA, 1);

	ReadUBYTE(&regP, 1);
	CPU_PutStatus();	/* Make sure flags are all updated */

	ReadUBYTE(&regS, 1);
	ReadUBYTE(&regX, 1);
	ReadUBYTE(&regY, 1);
	ReadUBYTE(&IRQ, 1);

	MemStateRead(SaveVerbose, StateVersion);

	ReadUWORD(&regPC, 1);
}

#endif
