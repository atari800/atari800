/*
 * cpu.h - 6502 CPU emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2025 Atari800 development team (see DOC/CREDITS)
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

#ifndef CPU_H_
#define CPU_H_

#include "config.h"
#ifdef ASAP /* external project, see http://asap.sf.net */
#include "asap_internal.h"
#else
#include "atari.h"
#endif

#define CPU_N_FLAG 0x80
#define CPU_V_FLAG 0x40
#define CPU_B_FLAG 0x10
#define CPU_D_FLAG 0x08
#define CPU_I_FLAG 0x04
#define CPU_Z_FLAG 0x02
#define CPU_C_FLAG 0x01

void CPU_GetStatus(void);
void CPU_PutStatus(void);
void CPU_Reset(void);
void CPU_StateSave(UBYTE SaveVerbose);
void CPU_StateRead(UBYTE SaveVerbose, UBYTE StateVersion);
void CPU_NMI(void);
void CPU_GO(int limit);
#define CPU_GenerateIRQ() (CPU_IRQ = 1)

extern UWORD CPU_regPC;
extern UBYTE CPU_regA;
extern UBYTE CPU_regP;
extern UBYTE CPU_regS;
extern UBYTE CPU_regY;
extern UBYTE CPU_regX;

#define CPU_SetN CPU_regP |= CPU_N_FLAG
#define CPU_ClrN CPU_regP &= (~CPU_N_FLAG)
#define CPU_SetV CPU_regP |= CPU_V_FLAG
#define CPU_ClrV CPU_regP &= (~CPU_V_FLAG)
#define CPU_SetB CPU_regP |= CPU_B_FLAG
#define CPU_ClrB CPU_regP &= (~CPU_B_FLAG)
#define CPU_SetD CPU_regP |= CPU_D_FLAG
#define CPU_ClrD CPU_regP &= (~CPU_D_FLAG)
#define CPU_SetI CPU_regP |= CPU_I_FLAG
#define CPU_ClrI CPU_regP &= (~CPU_I_FLAG)
#define CPU_SetZ CPU_regP |= CPU_Z_FLAG
#define CPU_ClrZ CPU_regP &= (~CPU_Z_FLAG)
#define CPU_SetC CPU_regP |= CPU_C_FLAG
#define CPU_ClrC CPU_regP &= (~CPU_C_FLAG)

extern UBYTE CPU_IRQ;

extern void (*CPU_rts_handler)(void);

extern UBYTE CPU_cim_encountered;

#define CPU_REMEMBER_PC_STEPS 64
extern UWORD CPU_remember_PC[CPU_REMEMBER_PC_STEPS];
extern UBYTE CPU_remember_op[CPU_REMEMBER_PC_STEPS][3];
extern unsigned int CPU_remember_PC_curpos;
extern int CPU_remember_xpos[CPU_REMEMBER_PC_STEPS];

#define CPU_REMEMBER_JMP_STEPS 16
extern UWORD CPU_remember_JMP[CPU_REMEMBER_JMP_STEPS];
extern unsigned int CPU_remember_jmp_curpos;

#ifdef MONITOR_PROFILE
extern int CPU_instruction_count[256];
#endif

#endif /* CPU_H_ */
