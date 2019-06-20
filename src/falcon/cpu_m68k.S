/*
 *  cpu_m68k.S - 6502 CPU emulation for Atari Falcon port
 *
 *  Copyright (C) 2001 Karel Rous (empty head)
 *  Copyright (C) 2001-2003 Atari800 development team (see DOC/CREDITS)
 *
 *  This file is part of the Atari800 emulator project which emulates
 *  the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 *  Atari800 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Atari800 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Atari800; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef P65C02
#define P65C02 0        /* set to 1 to emulate this version of processor (6502 has a bug in jump code, */
                        /* you can emulate this bug by commenting out this line :) */
#endif


#ifndef PROFILE
#define PROFILE 0           /* set to 1 to fill the 'CPU_instruction_count' array for instruction profiling */
#endif

#ifndef MONITOR_BREAK
#define MONITOR_BREAK 0     /* set to 1 to jump to monitor at break */
#endif

#ifndef CRASH_MENU
#define CRASH_MENU 0        /* enable crash menu output */
#endif

#ifndef NEW_CYCLE_EXACT
#define NEW_CYCLE_EXACT 0   /* set to 1 to use the new cycle exact CPU emulation */
#endif

  .globl _CARTRIDGE_BountyBob2
  .globl _CARTRIDGE_BountyBob1
  .globl _GTIA_GetByte
  .globl _POKEY_GetByte
  .globl _PIA_GetByte
  .globl _ANTIC_GetByte
  .globl _CARTRIDGE_GetByte
  .globl _GTIA_PutByte
  .globl _POKEY_PutByte
  .globl _PIA_PutByte
  .globl _ANTIC_PutByte
  .globl _CARTRIDGE_PutByte
  .globl _ESC_Run
  .globl _Atari800_Exit
  .globl _exit
  .globl _ANTIC_wsync_halt /* CPU is stopped */
#if NEW_CYCLE_EXACT
  .globl _ANTIC_delayed_wsync
  .globl _ANTIC_antic2cpu_ptr
  .globl _ANTIC_cpu2antic_ptr
  .globl _ANTIC_cur_screen_pos
#endif
  .globl _ANTIC_xpos
  .globl _ANTIC_xpos_limit
  .globl _CPU_regPC
  .globl _CPU_regA
  .globl _CPU_regP /* Processor Status Byte (Partial) */
  .globl _CPU_regS
  .globl _CPU_regX
  .globl _CPU_regY
  .globl _MEMORY_mem
  .globl _MEMORY_attrib
#if PROFILE
  .globl _CPU_instruction_count
#endif
#if MONITOR_BREAK
  .globl _CPU_remember_PC
  .globl _CPU_remember_op
  .globl _CPU_remember_PC_curpos
  .globl _CPU_remember_xpos
  .globl _CPU_remember_JMP
  .globl _CPU_remember_jmp_curpos
  .globl _ANTIC_break_ypos
  .globl _ANTIC_ypos
  .globl _MONITOR_break_addr
  .globl _MONITOR_break_step
  .globl _MONITOR_break_ret
  .globl _MONITOR_break_brk
  .globl _MONITOR_ret_nesting
#endif
#if CRASH_MENU
  .globl _UI_crash_code
  .globl _UI_crash_address
  .globl _UI_crash_afterCIM
  .globl _UI_Run
#endif
  .globl _CPU_IRQ
  .globl _CPU_GO_m68k
  .globl _CPU_GetStatus
  .globl _CPU_PutStatus
  .globl _CPU_cim_encountered
  .globl _CPU_rts_handler

#if MONITOR_BREAK
#define rem_pc_steps  64  /* has to be equal to REMEMBER_PC_STEPS */
#define rem_jmp_steps 16  /* has to be equal to REMEMBER_JMP_STEPS */
#endif

  .even

  .p2align 2         /* doubleword alignment */

regP:
  .ds.b 1
_CPU_regP:  .ds.b 1   /* CCR */

regA:
  .ds.b 1
_CPU_regA:  .ds.b 1   /* A */

regX:
  .ds.b 1
_CPU_regX:  .ds.b 1   /* X */

regY:
  .ds.b 1
_CPU_regY:  .ds.b 1   /* Y */

regPC:
_CPU_regPC: .ds.w 1  /* PC */

regS:
  .dc.b 0x01
_CPU_regS:  .ds.b 1   /* stack */

  .even

#define memory_pointer a5
#define attrib_pointer a4
#define PC6502 a2

#define CD     a6 /* cycles counter up */
#define ZFLAG  d1 /* Bit 0..7 */
#define NFLAG  d1 /* Bit 8..15 */
#define VFLAG  d6 /* Bit 7 */
#define DFLAG  d6 /* Bit 15 */
#define CFLAG  d5 /* Bit 0..7, ( 1 = ff ) */
#define A      d2
#define X      d3
#define Y      d4

/*
 * d0  contains usually adress where we are working or temporary value
 * d7  contains is a working register or adress
 * a3  contains OPMODE_TABLE or OPMODE_TABLE_D (depending on the D flag)
 */
/* change order of lo and hi byte (address) */
#define LoHi(r) ror.w #8,r

/*
 * ==========================================================
 * Emulated Registers and Flags are kept local to this module
 * ==========================================================
 */

/* regP=processor flags; regPC=PC; regA=A; regX=X; regY=Y */
#define UPDATE_GLOBAL_REGS \
  sub.l   memory_pointer,PC6502; \
  movem.w d0/d2-d4/a2,regP /* d0->regP, d2-d4 (A,X,Y) a2 (regPC) */

/* PC=regPC; A=regA; X=regX; Y=regY */
#define UPDATE_LOCAL_REGS \
  moveq   #0,d7; \
  move.w  regP.l,d0; \
  move.w  regA.l,d2; \
  move.w  regX.l,d3; \
  move.w  regY.l,d4; \
  move.w  regPC.l,d7; \
  move.l  memory_pointer,PC6502; \
  add.l   d7,PC6502

_Local_GetByte:
  move.l d7,d1
  moveq  #0,d0
  move.b d1,d0
  lsr.w  #8,d1
  move.b (HIxTable,d1.l),d1
/* jmp    ([GetTable,PC,d1.l*4]) */
  move.w (GetTable,PC,d1.l*2),d1
  jmp    (GetTable,d1.w)

GetTable:
  dc.w GetNone-GetTable,GetGTIA-GetTable
  dc.w GetPOKEY-GetTable,GetPIA-GetTable
  dc.w GetANTIC-GetTable,GetCART-GetTable
  dc.w ItsBob1-GetTable,ItsBob2-GetTable

GetNone:
  st     d0        /* higher bytes are 0 from before */
  rts
GetGTIA:
  clr.l -(a7)      /* FALSE (no side effects) */
  move.l d0,-(a7)
#if   NEW_CYCLE_EXACT
  move.l CD,_ANTIC_xpos
#endif
  jsr    _GTIA_GetByte
  addq.l #8,a7
  rts
GetPOKEY:
  clr.l -(a7)      /* FALSE (no side effects) */
  move.l d0,-(a7)
  move.l CD,_ANTIC_xpos
  jsr    _POKEY_GetByte
  addq.l #8,a7
  rts
GetPIA:
  clr.l -(a7)      /* FALSE (no side effects) */
  move.l d0,-(a7)
  jsr    _PIA_GetByte
  addq.l #8,a7
  rts
GetANTIC:
  clr.l -(a7)      /* FALSE (no side effects) */
  move.l d0,-(a7)
  move.l CD,_ANTIC_xpos
  jsr    _ANTIC_GetByte
  addq.l #8,a7
  rts
GetCART:
  clr.l -(a7)      /* FALSE (no side effects) */
  move.l d0,-(a7)
  jsr    _CARTRIDGE_GetByte
  addq.l #8,a7
  rts
ItsBob2:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CARTRIDGE_BountyBob2
  addq.l #4,a7
  moveq  #0,d0
  rts
ItsBob1:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CARTRIDGE_BountyBob1
  addq.l #4,a7
  moveq  #0,d0
  rts

_Local_PutByte:
  moveq  #0,d1
  move.w d7,d1
  lsr.w  #8,d1
  move.b (HIxTable,d1.l),d1
  jmp    ([PutTable.w,PC,d1.l*4])

PutTable:
  dc.l PutNone,PutGTIA,PutPOKEY,PutPIA
  dc.l PutANTIC,PutCART,ItsBob1,ItsBob2

PutNone:
  moveq  #0,d0
  rts
PutGTIA:
  move.b d0,d1
  move.l d1,-(a7)
  move.b d7,d1
  move.l d1,-(a7)
  move.l CD,_ANTIC_xpos
  jsr    _GTIA_PutByte
  addq.l #8,a7
  rts
PutPOKEY:
  move.b d0,d1
  move.l d1,-(a7)
  move.b d7,d1
  move.l d1,-(a7)
  jsr    _POKEY_PutByte
  addq.l #8,a7
  rts
PutPIA:
  move.b d0,d1
  move.l d1,-(a7)
  move.b d7,d1
  move.l d1,-(a7)
  jsr    _PIA_PutByte
  addq.l #8,a7
  rts
PutANTIC:
  move.b d0,d1
  move.l d1,-(a7)
  move.b d7,d1
  move.l d1,-(a7)
  move.l CD,_ANTIC_xpos
  jsr    _ANTIC_PutByte
  move.l _ANTIC_xpos,CD
  addq.l #8,a7
  rts
PutCART:
  move.b d0,d1
  move.l d1,-(a7)
  move.b d7,d1
  move.l d1,-(a7)
  jsr    _CARTRIDGE_PutByte
  addq.l #8,a7
  rts

#define HIxNone   0
#define HIxGTIA8  1
#define HIxGTIA5  1
#define HIxPOKEY8 2
#define HIxPOKEY5 2
#define HIxPIA8   3
#define HIxANTIC8 4
#define HIxCART   5
#define HIxBob1   6
#define HIxBob2   7

HIxTable:
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 00..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 04..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 08..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 0c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 10..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 14..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 18..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 1c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 20..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 24..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 28..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 2c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 30..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 34..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 38..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 3c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 40..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 44..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 48..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxBob1     /* 4c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 50..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 54..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 58..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxBob2     /* 5c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 60..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 64..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 68..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 6c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 70..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 74..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 78..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 7c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 80..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 84..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 88..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxBob1     /* 8c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 90..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 94..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* 98..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxBob2     /* 9c..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* a0..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* a4..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* a8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* ac..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* b0..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* b4..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* b8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* bc..f */
  dc.b HIxGTIA5,HIxNone,HIxNone,HIxNone    /* c0..3 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* c4..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* c8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* cc..f */
  dc.b HIxGTIA8,HIxNone,HIxPOKEY8,HIxPIA8  /* d0..3 */
  dc.b HIxANTIC8,HIxCART,HIxNone,HIxNone   /* d4..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* d8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* dc..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* e0..3 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* e4..7 */
  dc.b HIxPOKEY5,HIxNone,HIxNone,HIxPOKEY5 /* e8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* ec..f */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* f0..3 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* f4..7 */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* f8..b */
  dc.b HIxNone,HIxNone,HIxNone,HIxNone     /* fc..f */

#define EXE_GETBYTE \
/* move.l d7,-(a7); */ \
  jsr    _Local_GetByte.l; \
/* addq.l #4,a7 ;put stack onto right place */

#define EXE_PUTBYTE(r) \
/* clr.l  -(a7); */ \
/* move.b r,3(a7) ;byte */ \
  jsr    _Local_PutByte.l; \
/* addq.l #8,a7 */


/* XXX: we do this only for GTIA, because NEW_CYCLE_EXACT does not correctly */
/* emulate INC $D400 (and INC $D40A wasn't tested) */ */
#if   NEW_CYCLE_EXACT
#define RMW_GETBYTE \
  move.w d7,d0; \
  and.w  #0xef00,d0; \
  cmp.w  #0xc000,d0; \
  bne.s  1f; \
  EXE_GETBYTE; \
  subq.l #1,CD; \
  move.l d0,-(a7); \
  EXE_PUTBYTE(d0); \
  move.l (a7)+,d0; \
  addq.l #1,CD; \
  bra.s  2f; \
1:; \
  EXE_GETBYTE; \
2:
#else
#define RMW_GETBYTE \
  EXE_GETBYTE
#endif

/* these are bit in MC68000 CCR register */
#define NB68  3
#define EB68  4 /* X */
#define ZB68  2
#define OB68  1
#define CB68  0

#define WSYNC_C 106

#define N_FLAG 0x80
#define N_FLAGN 0x7f
#define N_FLAGB 7
#define V_FLAG 0x40
#define V_FLAGN 0xbf
#define V_FLAGB 6
#define G_FLAG 0x20
#define G_FLAGB 5
#define B_FLAG 0x10
#define B_FLAGN 0xef
#define B_FLAGB 4
#define D_FLAG 0x08
#define D_FLAGN 0xf7
#define D_FLAGB 3
#define I_FLAG 0x04
#define I_FLAGN 0xfb
#define I_FLAGB 2
#define Z_FLAG 0x02
#define Z_FLAGN 0xfd
#define Z_FLAGB 1
#define C_FLAG 0x01
#define C_FLAGN 0xfe
#define C_FLAGB 0
#define VCZN_FLAGS 0xc3
#define VCZN_FLAGSN 0x3c

#define SetI  ori.b  #I_FLAG,_CPU_regP

#define ClrI  andi.b #I_FLAGN,_CPU_regP

#define SetB  ori.b  #B_FLAG,_CPU_regP

#define SetD \
  ori.b  #D_FLAG,_CPU_regP; \
  lea    OPMODE_TABLE_D.l,a3

#define ClrD \
  andi.b #D_FLAGN,_CPU_regP; \
  lea    OPMODE_TABLE.l,a3

/* static UBYTE  N; */  /* bit7 zero (0) or bit 7 non-zero (1) */
/* static UBYTE  Z; */  /* zero (0) or non-zero (1) */
/* static UBYTE  V; */
/* static UBYTE  C; */  /* zero (0) or one(1) */

#define isRAM      0
#define isROM      1
#define isHARDWARE 2

/*
 * The following array is used for 6502 instruction profiling
 */

/* int instruction_count[256]; */

/* UBYTE memory[65536]; */
/* UBYTE attrib[65536]; */


#define cy_CIM 2
#define cy_NOP 2
#define cy_NOP2 2
#define cy_NOP3 3
#define cy_NOP4 4
#define cy_SKW 4
#define cy_BRK 7
#define cy_Sub 6
#define cy_Bcc 2
#define cy_Bcc1 3
#define cy_Bcc2 4
#define cy_JmpAbs 3
#define cy_JmpInd 5
#define cy_IndX 6    /* indirect X */
#define cy_IndY 5    /* indirect Y */
#define cy_IndY2 6   /* indirect Y (+) */
#define cy_IndX_RW 8 /* indirect X read/write ( all inofficial ) */
#define cy_IndY_RW 8 /* indirect Y read/write ( all inofficial ) */
#define cy_Abs 4     /* absolute */
#define cy_Abs_RW 6  /* absolute read/write */
#define cy_AbsX 4    /* absolute X */
#define cy_AbsX2 5   /* absolute X (+) */
#define cy_AbsX_RW 7 /* absolute X read/write */
#define cy_AbsY 4    /* absolute Y */
#define cy_AbsY2 5   /* absolute X (+) */
#define cy_AbsY_RW 7 /* absolute Y read/write ( all inofficial ) */
#define cy_ZP 3      /* zero page */
#define cy_ZP_RW 5   /* zero page read/write */
#define cy_ZPX 4     /* zero page X */
#define cy_ZPX_RW 6  /* zero page X read/write */
#define cy_ZPY 4     /* zero page X */
#define cy_Imm 2     /* immediate */
#define cy_FlagCS 2  /* flag clear/set */
#define cy_RegChg 2  /* register only manipulation */
#define cy_RegPH 3   /* push register to stack */
#define cy_RegPL 4   /* pull register from stack */

/*
  ===============================================================
  Z flag: This actually contains the result of an operation which
    would modify the Z flag. The value is tested for
    equality by the BEQ and BNE instruction.
  ===============================================================
*/

/* Bit    : 76543210 */
/* 68000  : ***XNZVC */
/* _RegP  : NV*BDIZC */

#define ConvertSTATUS_RegP(r) \
  move.b _CPU_regP.l,r; /* put flag BDI into d0 */ \
  andi.b #VCZN_FLAGSN,r; /* clear overflow, carry, zero & negative flag */ \
  tst.b  CFLAG; \
  beq.s  1f; \
  addq.b #1,r; \
1:; \
  tst.w  NFLAG; \
  bpl.s  2f; \
  tas    r; \
2:; \
  tst.b  ZFLAG; \
  bne.s  3f; \
  addq.b #2,r; \
3:; \
  tst.b  VFLAG; \
  bpl.s  4f; \
  ori.b  #V_FLAG,r; \
4:

#define ConvertSTATUS_RegP_destroy(r) \
  move.b _CPU_regP.l,r; /* put flag BDI into d0 */ \
  andi.b #VCZN_FLAGSN,r; /* clear overflow, carry, zero & negative flag */ \
  lsr.b  #7,CFLAG; \
  or.b   CFLAG,r; \
  tst.w  NFLAG; \
  bpl.s  1f; \
  tas    r; \
1:; \
  tst.b  ZFLAG; \
  bne.s  2f; \
  addq.b #2,r; \
2:; \
  tst.b  VFLAG; \
  bpl.s  3f; \
  ori.b  #V_FLAG,r; \
3:

#define ConvertRegP_STATUS(r); \
  btst   #V_FLAGB,r; \
  sne    VFLAG; \
  btst   #C_FLAGB,r; \
  sne    CFLAG; \
  move.b r,NFLAG; \
  lsl.w  #8,NFLAG;  /* sets NFLAG and clears ZFLAG */ \
  btst   #Z_FLAGB,r; \
  seq    ZFLAG; \
  lea    OPMODE_TABLE.l,a3; \
  btst   #D_FLAGB,r; \
  beq.s  1f; \
  lea    OPMODE_TABLE_D.l,a3; \
1:

#define Call_Atari800_RunEsc \
/* move.l d7,-(a7);    !!!TEST!!! */ \
  clr.l  -(a7);     /* !!!TEST!!! */ \
  move.b d7,(3,a7); /* !!!TEST!!! */ \
  ConvertSTATUS_RegP_destroy(d0); \
  UPDATE_GLOBAL_REGS; \
  jsr _ESC_Run; \
  addq.l #4,a7; \
  UPDATE_LOCAL_REGS; \
  ConvertRegP_STATUS(d0)

#define Call_Atari800_Exit_true \
  pea    0x1.W; \
  jsr    _Atari800_Exit; \
  addq.l #4,a7; \
  tst.l  d0; \
  bne.s  1f; \
  clr.l  -(a7); \
  jsr    _exit; \
1:

#define PLW(src,dst) \
  moveq  #0,dst; \
  move.w regS.l,dst; \
  addq.b #2,dst;     /* wrong way around */ \
  move.b (memory_pointer,dst.l),src; \
  asl.w  #8,src; \
  subq.b #1,dst; \
  or.b   (memory_pointer,dst.l),src; \
  addq.b #1,dst; \
  move.b dst,_CPU_regS

#define SetVFLAG st     VFLAG

#define ClrVFLAG clr.b  VFLAG

#define SetCFLAG st     CFLAG

#define ClrCFLAG clr.b  CFLAG

_CPU_GetStatus:
  ConvertSTATUS_RegP(d0)
  move.b d0,_CPU_regP
  rts

_CPU_PutStatus:
  move.b _CPU_regP.l,d0
  ConvertRegP_STATUS(d0)
  rts

NMI:
  lea    _MEMORY_mem,a0
  moveq  #0,d1
  move.w regS.l,d1
  move.b _CPU_regPC.l,(a0,d1.l)
  subq.b #1,d1
  move.b _CPU_regPC+1.l,(a0,d1.l)
  subq.b #1,d1
  move.b _CPU_regP.l,d0
  andi.b #B_FLAGN,d0
  move.b d0,(a0,d1.l)
  subq.b #1,d1
  move.b d1,_CPU_regS
  SetI
  /* put regPC & Stack pointer adress on its place */
  move.w (a0,0xfffa.l),d1
  LoHi(d1)
  move.w d1,_CPU_regPC
  addq.l #7,_ANTIC_xpos
#if   MONITOR_BREAK
  addq.l #1,_MONITOR_ret_nesting
#endif
  rts

_CPU_GO_m68k:
  move.l  4(a7),d0       /* limit */

  tst.b  _ANTIC_wsync_halt
  beq.s  NO_WS_HALT
#if   NEW_CYCLE_EXACT
  moveq  #WSYNC_C-1,d1  /* TEST : no -1 if bpl.s */
  cmp.l  #-999,_ANTIC_cur_screen_pos    /* ANTIC_NOT_DRAWING */
  beq.s  1f
  move.l _ANTIC_antic2cpu_ptr,a0
  move.l (a0,d1.w*4),d1
1:
  add.l  _ANTIC_delayed_wsync,d1
  cmp.l  d0,d1
/* bpl.s  TERM_GO          TEST */
  bge    TERM_GO        /* TEST */
  addq.l #1,d1          /* TEST : not necessary if bpl.s */
  move.l d1,_ANTIC_xpos
  clr.b  _ANTIC_wsync_halt
  clr.l  _ANTIC_delayed_wsync
#else /* NEW_CYCLE_EXACT */
  moveq  #WSYNC_C-1,d1  /* TEST : no -1 if bpl.s */
  cmp.l  d0,d1
/* bpl.s  TERM_GO          TEST */
  bge    TERM_GO        /* TEST */
  addq.l #1,d1          /* TEST : not necessary if bpl.s */
  move.l d1,_ANTIC_xpos
#endif
  clr.b  _ANTIC_wsync_halt
NO_WS_HALT:
  move.l  d0,_ANTIC_xpos_limit /*  needed for WSYNC store inside ANTIC */
  movem.l d2-d7/a2-a6,-(a7)
  move.l _ANTIC_xpos,CD
  lea    _MEMORY_mem,memory_pointer
  UPDATE_LOCAL_REGS
  ConvertRegP_STATUS(d0)
  lea    _MEMORY_attrib,attrib_pointer
  tst.b  _CPU_IRQ          /* CPUCHECKIRQ */
  beq    NEXTCHANGE_WITHOUT
  cmp.l  _ANTIC_xpos_limit,CD
  bge    NEXTCHANGE_WITHOUT
  move.b d0,d7
/* and.b  #I_FLAG,d0 ;is interrupt active */
  btst   #I_FLAG,d0
  bne    NEXTCHANGE_WITHOUT /* yes, no other interrupt */
  moveq  #0,d0
  move.w regS.l,d0  /* push PC and P to stack ( PHW + PHB ) start */
  move.b _CPU_regPC.l,(memory_pointer,d0.l)
  subq.b #1,d0
  move.b _CPU_regPC+1.l,(memory_pointer,d0.l)
  subq.b #1,d0
  andi.b #B_FLAGN,d7
  move.b d7,(memory_pointer,d0.l)
  subq.b #1,d0
  move.b d0,_CPU_regS      /* push PC and P to stack ( PHW + PHB ) end */
  SetI
  move.w (memory_pointer,0xfffe.l),d0  /* d0 already cleared from before */
  LoHi(d0)
  move.l d0,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
  clr.b  _CPU_IRQ /* clear interrupt..... */
#if   MONITOR_BREAK
  addq.l  #1,_MONITOR_ret_nesting
#endif
  bra    NEXTCHANGE_WITHOUT

/*
   =====================================
   Extract Address if Required by Opcode
   =====================================
*/

/* d0 contains final value for use in program */

/* addressing macros */

#define NCYCLES_XY(addr) \
  cmp.b  addr,d7; /* if ( (UBYTE) addr < X,Y ) ncycles++; */ \
/* bpl.s  1f; */ \
  bcc.s  1f;             /* !!! */ \
  addq.l #1,CD; \
1:

#define ABSOLUTE \
  move.w (PC6502)+,d7; \
  LoHi(d7) /* d7 contains reversed value */

#define ABSOLUTE_X \
  ABSOLUTE; \
  add.w  X,d7

#define ABSOLUTE_X_NCY \
  ABSOLUTE_X; \
  NCYCLES_XY(X)

#define ABSOLUTE_Y \
  ABSOLUTE; \
  add.w  Y,d7

#define ABSOLUTE_Y_NCY \
  ABSOLUTE_Y; \
  NCYCLES_XY(Y)

#define IMMEDIATE(x) \
  move.b (PC6502)+,x

#define INDIRECT_X \
  move.b (PC6502)+,d7; \
  add.b  X,d7; \
  move.w (memory_pointer,d7.l),d7; \
  LoHi(d7)

#define INDIRECT_Y \
  move.b (PC6502)+,d7; \
  move.w (memory_pointer,d7.l),d7; \
  LoHi(d7);      /* swap bytes */ \
  add.w  Y,d7

#define INDIRECT_Y_NCY \
  INDIRECT_Y; \
  NCYCLES_XY(Y)

#define ZPAGE \
  move.b (PC6502)+,d7

#define ZPAGE_X \
  move.b (PC6502)+,d7; \
  add.b  X,d7

#define ZPAGE_Y \
  move.b (PC6502)+,d7; \
  add.b  Y,d7

/* miscellaneous macros */

#define NEXTCHANGE_REG(r) \
  move.b r,ZFLAG; \
  bra.w  NEXTCHANGE_N

/* command macros */

#define ROL_C(r) \
  add.b  CFLAG,CFLAG; \
  addx.b r,r; /* left */ \
  scs    CFLAG

#define ROR_C(r) \
  add.b  CFLAG,CFLAG; \
  roxr.b #1,r; \
  scs    CFLAG

#define ASL_C(r) \
  add.b  r,r; /* left */ \
  scs    CFLAG

#define LSR_C(r) \
  lsr.b  #1,r; \
  scs    CFLAG

/* opcodes */

/* inofficial opcodes */

/*   unstable inofficial opcodes */

opcode_93: /* SHA (ab),y [unofficial, UNSTABLE - Store A AND X AND (H+1) ?] */
  /* It seems previous memory value is important - also in 9f */
  addq.l #cy_IndY2,CD
  move.b (PC6502)+,d7
  addq.b #1,d7
  move.b (memory_pointer,d7.l),d0
  addq.b #1,d0
  and.b  A,d0
  and.b  X,d0
  move.w (memory_pointer,d7.l),d7
  LoHi(d7)      /* swap bytes */
  add.b  Y,d7
  bcc.s  1f
  LoHi(d7)
  move.b d0,d7
  LoHi(d7)
1:
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_9f: /* SHA abcd,y [unofficial, UNSTABLE - Store A AND X AND (H+1) ?] */
  addq.l #cy_IndY2,CD
  move.w (PC6502)+,d7
  move.b d7,d0
  LoHi(d7) /* d7 contains reversed value */
  addq.b #1,d0
  and.b  A,d0
  and.b  X,d0
  add.b  Y,d7
  bcc.s  1f
  LoHi(d7)
  move.b d0,d7
  LoHi(d7)
1:
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_9e: /* SHX abcd,y [unofficial - Store X and (H+1)] (Fox) */
  /* Seems to be stable */
  addq.l #cy_IndY2,CD
  move.w (PC6502)+,d7
  move.b d7,d0
  LoHi(d7) /* d7 contains reversed value */
  addq.b #1,d0
  and.b  X,d0
  add.b  Y,d7
  bcc.s  1f
  LoHi(d7)
  move.b d0,d7
  LoHi(d7)
1:
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_9c: /* SHY abcd,x [unofficial - Store Y and (H+1)] (Fox) */
  /* Seems to be stable */
  addq.l #cy_AbsX2,CD
  move.w (PC6502)+,d7
  move.b d7,d0
  LoHi(d7) /* d7 contains reversed value */
  addq.b #1,d0
  and.b  Y,d0
  add.b  X,d7
  bcc.s  1f
  LoHi(d7)
  move.b d0,d7
  LoHi(d7)
1:
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_9b: /* SHS abcd,y [unofficial, UNSTABLE] (Fox) */
  /* Transfer A AND X to S, then store S AND (H+1)] */
  /* S seems to be stable, only memory values vary */
  addq.l #cy_IndY2,CD
  move.w (PC6502)+,d7
  move.b d7,d0
  LoHi(d7) /* d7 contains reversed value */
  move.b A,_CPU_regS
  and.b  X,_CPU_regS
  addq.b #1,d0
  and.b  _CPU_regS.l,d0
  add.b  Y,d7
  bcc.s  1f
  LoHi(d7)
  move.b d0,d7
  LoHi(d7)
1:
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

/*   stable inofficial opcodes */

opcode_6b: /* ARR #ab [unofficial - Acc AND Data, ROR result] */
/* not optimized because I think it will never be executed anyway */
/* commit 01a618e7 seems to optimize it a bit, ignoring */
  addq.l #cy_Imm,CD
  IMMEDIATE(ZFLAG)
  and.b  A,ZFLAG
  btst   #D_FLAGB,_CPU_regP.l
  beq.s  .6b_noBCD
/* 'BCD fixup' */
  move.b ZFLAG,d7
  ROR_C(ZFLAG)
  move.b ZFLAG,A
  move.b d7,VFLAG  /* VFLAG */
  eor.b  ZFLAG,VFLAG
  and.b  #0x40,VFLAG
  sne    VFLAG
  move.b A,d7
  move.b A,d7
  move.b d7,d0
  andi.b #15,d0
  move.b d7,CFLAG
  andi.b #1,CFLAG
  add.b  CFLAG,d0
  cmpi.b #6,d0     /* check for >5 */
  bmi.s  .6b_bcd1  /* <=5 */
  move.b A,CFLAG
  and.b  #0xf0,CFLAG
  move.b A,d0
  addq.b #6,d0
  and.b  #15,d0
  move.b CFLAG,A
  or.b   d0,A
.6b_bcd1:
  move.b d7,d0
  andi.b #0xf0,d0
  move.b d7,CFLAG
  andi.b #16,CFLAG
  cmpi.b #0x51,d0   /* check for >$50 */
  bmi.s  .6b_bcd2  /* <=$50 */
  move.b A,CFLAG
  and.b  #15,CFLAG
  move.b A,d0
  add.b  #0x60,d0
  and.b  #0xf0,d0
  move.b CFLAG,A
  or.w   d0,A
  SetCFLAG
  bra.w  NEXTCHANGE_N
.6b_bcd2:
  ClrCFLAG
  bra.w  NEXTCHANGE_N
/* Binary */
.6b_noBCD:
  ROR_C(ZFLAG)
  move.b ZFLAG,A
  move.b A,VFLAG   /* VFLAG */
  lsr.b  #6,VFLAG
  move.b A,CFLAG
  lsr.b  #5,CFLAG
  eor.b  CFLAG,VFLAG
  and.b  #1,VFLAG
  sne    VFLAG
  move.b A,CFLAG   /* CFLAG */
  and.b  #0x40,CFLAG
  sne    CFLAG
  bra.w  NEXTCHANGE_N

opcode_02: /* CIM [unofficial - crash immediate] */
opcode_12:
opcode_22:
opcode_32:
opcode_42:
opcode_52:
opcode_62:
opcode_72:
opcode_92:
opcode_b2:
  addq.l #cy_CIM,CD
  subq.w #1,PC6502
  ConvertSTATUS_RegP_destroy(d0)
  UPDATE_GLOBAL_REGS
#if   CRASH_MENU
  move.w PC6502,_UI_crash_address
  addq.w #1,PC6502
  move.w PC6502,_UI_crash_afterCIM
  move.l d7,_UI_crash_code
  jsr    _UI_Run
#else
  move.b #1,_CPU_cim_encountered
  Call_Atari800_Exit_true
#endif
  UPDATE_LOCAL_REGS
  ConvertRegP_STATUS(d0)
  bra.w  NEXTCHANGE_WITHOUT

opcode_07: /* ASO ab [unofficial - ASL then ORA with Acc] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ASL_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  or.b   d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_17: /* ASO ab,x [unofficial - ASL then ORA with Acc] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ASL_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  or.b   d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define ASO_C_CONT /* [unofficial - ASL Mem, then ORA with A] */ \
  move.b (attrib_pointer,d7.l),d0; \
  bne.s  ASO_Getbyte_ROMHW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  ASL_C(d0); \
  jbra  ASO_STORE_MEM

opcode_03: /* ASO (ab,x) [unofficial - ASL then ORA with Acc] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  ASO_C_CONT

opcode_13: /* ASO (ab),y [unofficial - ASL then ORA with Acc] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  ASO_C_CONT

opcode_0f: /* ASO abcd [unofficial - ASL then ORA with Acc] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  ASO_C_CONT

opcode_1b: /* ASO abcd,y [unofficial - ASL then ORA with Acc] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  ASO_C_CONT

opcode_1f: /* ASO abcd,x [unofficial - ASL then ORA with Acc] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  ASO_C_CONT

ASO_Getbyte_ROMHW:
  cmp.b  #isHARDWARE,d0
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  ASL_C(d0)
  bra.s  ASO_NOW_ORA
1:
  RMW_GETBYTE
  ASL_C(d0)
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  bra.s  ASO_NOW_ORA
ASO_STORE_MEM:
  move.b d0,(memory_pointer,d7.l)
ASO_NOW_ORA:
  or.b   d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_27: /* RLA ab [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ROL_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  and.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_37: /* RLA ab,x [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ROL_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  and.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define RLA_C_CONT /* [unofficial - ROL Mem, then AND with A] */ \
  move.b (attrib_pointer,d7.l),d0; \
  jbne  RLA_Getbyte_ROMHW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  ROL_C(d0); \
  jbra  RLA_STORE_MEM

opcode_23: /* RLA (ab,x) [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  RLA_C_CONT

opcode_33: /* RLA (ab),y [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  RLA_C_CONT

opcode_2f: /* RLA abcd [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  RLA_C_CONT

opcode_3b: /* RLA abcd,y [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  RLA_C_CONT

opcode_3f: /* RLA abcd,x [unofficial - ROL Mem, then AND with A] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  RLA_C_CONT

RLA_Getbyte_ROMHW:
  cmp.b  #isHARDWARE,d0
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  ROL_C(d0)
  bra.s  RLA_NOW_AND
1:
  RMW_GETBYTE
  ROL_C(d0)
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  bra.s  RLA_NOW_AND
RLA_STORE_MEM:
  move.b d0,(memory_pointer,d7.l)
RLA_NOW_AND:
  and.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_47: /* LSE ab [unofficial - LSR then EOR result with A] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  LSR_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  eor.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_57: /* LSE ab,x [unofficial - LSR then EOR result with A] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  LSR_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  eor.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define LSE_C_CONT /* [unofficial - LSR Mem then EOR with A] */ \
  move.b (attrib_pointer,d7.l),d0; \
  bne.s  LSE_Getbyte_ROMHW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  LSR_C(d0); \
  jbra    LSE_STORE_MEM

opcode_43: /* LSE (ab,x) [unofficial] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  LSE_C_CONT

opcode_53: /* LSE (ab),y [unofficial] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  LSE_C_CONT

opcode_4f: /* LSE abcd [unofficial] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  LSE_C_CONT

opcode_5b: /* LSE abcd,y [unofficial] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  LSE_C_CONT

opcode_5f: /* LSE abcd,x [unofficial] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  LSE_C_CONT

LSE_Getbyte_ROMHW:
  cmp.b  #isHARDWARE,d0
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  LSR_C(d0)
  bra.s  LSE_NOW_EOR
1:
  RMW_GETBYTE
  LSR_C(d0)
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  bra.s  LSE_NOW_EOR
LSE_STORE_MEM:
  move.b d0,(memory_pointer,d7.l)
LSE_NOW_EOR:
  eor.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_4b: /* ALR #ab [unofficial - Acc AND Data, LSR result] */
  addq.l #cy_Imm,CD
  and.b  (PC6502)+,A
  LSR_C(A)
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_67: /* RRA ab [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ROR_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  bra.w  adc

opcode_77: /* RRA ab,x [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  ROR_C(d0)
  move.b d0,(memory_pointer,d7.l) /* PUTZPBYTE */
  bra.w  adc

#define GETANYBYTE_RRA  \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  jbne    RRA_RAMROM; \
  RMW_GETBYTE; \
  jbra    RRA_C_CONT

opcode_63: /* RRA (ab,x) [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  GETANYBYTE_RRA

opcode_73: /* RRA (ab),y [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  GETANYBYTE_RRA

opcode_6f: /* RRA abcd [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  GETANYBYTE_RRA

opcode_7b: /* RRA abcd,y [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  GETANYBYTE_RRA

opcode_7f: /* RRA abcd,x [unofficial - ROR Mem, then ADC to Acc] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  GETANYBYTE_RRA

RRA_RAMROM:
  move.b (memory_pointer,d7.l),d0 /* get byte */
RRA_C_CONT:        /* [unofficial - ROR Mem, then ADC to Acc] */
  ROR_C(d0)
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW
  move.b d0,(memory_pointer,d7.l)
  bra.w  adc
.ROM_OR_HW:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.w  adc       /* ROM ? */
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  bra.w  adc

opcode_87: /* SAX ab [unofficial - Store result A AND X] */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b A,d0
  and.b  X,d0
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_97: /* SAX ab,y [unofficial - Store result A AND X] */
  addq.l #cy_ZPY,CD
  ZPAGE_Y
  move.b A,d0
  and.b  X,d0
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_83: /* SAX (ab,x) [unofficial - Store result A AND X] */
  addq.l #cy_IndX,CD
  INDIRECT_X
  move.b A,d0
  and.b  X,d0
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_8f: /* SAX abcd [unofficial - Store result A AND X] */
  addq.l #cy_Abs,CD
  ABSOLUTE
  move.b A,d0
  and.b  X,d0
  tst.b  (attrib_pointer,d7.l)    /* PUTANYBYTE */
  bne.w  A800PUTB
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT

opcode_a7: /* LAX ab [unofficial] - LDA + LDX */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),A  /* GETZPBYTE */
  move.b A,X
  NEXTCHANGE_REG(A)

opcode_b7: /* LAX ab,y [unofficial] - LDA + LDX */
  addq.l #cy_ZPY,CD
  ZPAGE_Y
  move.b (memory_pointer,d7.l),A  /* GETZPBYTE */
  move.b A,X
  NEXTCHANGE_REG(A)

#define GETANYBYTE_LAX \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  LAX_HW; \
  move.b (memory_pointer,d7.l),A;  /* get byte */ \
  move.b A,X; \
  NEXTCHANGE_REG(A)

opcode_a3: /* LAX (ind,x) [unofficial] - LDA + LDX */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_LAX

opcode_b3: /* LAX (ind),y [unofficial] - LDA + LDX */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_LAX

opcode_af: /* LAX abcd [unofficial] - LDA + LDX */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_LAX

opcode_bf: /* LAX abcd,y [unofficial] - LDA + LDX */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_LAX

LAX_HW:
  EXE_GETBYTE
  move.b d0,A
  move.b A,X
  NEXTCHANGE_REG(A)

opcode_bb: /* LAS abcd,y [unofficial - AND S with Mem, transfer to A and X */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  bne.s  1f
  EXE_GETBYTE
  bra.s  2f
1:
  move.b (memory_pointer,d7.l),d0 /* get byte */
2:
  and.b  _CPU_regS.l,d0
  move.b d0,A
  move.b d0,X
  move.b d0,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_c7: /* DCM ab [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  subq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  jbra   COMPARE_A

opcode_d7: /* DCM ab,x [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  subq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  jbra   COMPARE_A

#define DCM_C_CONT /* [unofficial - DEC Mem then CMP with Acc] */ \
  tst.b  (attrib_pointer,d7.l); \
  bne.s  DCM_ROM_HW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  subq.b #1,d0; \
  move.b d0,(memory_pointer,d7.l); \
  jbra   COMPARE_A

opcode_c3: /* DCM (ab,x) [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  DCM_C_CONT

opcode_d3: /* DCM (ab),y [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  DCM_C_CONT

opcode_cf: /* DCM abcd [unofficial] - DEC Mem then CMP with Acc] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  DCM_C_CONT

opcode_db: /* DCM abcd,y [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  DCM_C_CONT

DCM_ROM_HW:
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  subq.b #1,d0
  jbra   COMPARE_A
1:
  RMW_GETBYTE
  subq.b #1,d0
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  jbra  COMPARE_A

opcode_df: /* DCM abcd,x [unofficial - DEC Mem then CMP with Acc] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  DCM_C_CONT

opcode_cb: /* SBX #ab [unofficial - store (A AND X - Mem) in X] */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  and.b  A,X
  subq.b #1,CFLAG
  subx.b d0,X
  scc    CFLAG
  NEXTCHANGE_REG(X)

opcode_e7: /* INS ab [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  addq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  bra sbc

opcode_f7: /* INS ab,x [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0 /* GETZPBYTE */
  addq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  bra sbc

#define INS_C_CONT /* [unofficial - INC Mem then SBC with Acc] */ \
  tst.b  (attrib_pointer,d7.l); \
  bne.s  INS_ROM_HW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  addq.b #1,d0; \
  move.b d0,(memory_pointer,d7.l); \
  bra.w  sbc

opcode_e3: /* INS (ab,x) [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_IndX_RW,CD
  INDIRECT_X
  INS_C_CONT

opcode_f3: /* INS (ab),y [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_IndY_RW,CD
  INDIRECT_Y
  INS_C_CONT

opcode_ef: /* INS abcd [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  INS_C_CONT

opcode_fb: /* INS abcd,y [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_AbsY_RW,CD
  ABSOLUTE_Y
  INS_C_CONT

INS_ROM_HW:
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  addq.b #1,d0
  bra.w  sbc
1:
  RMW_GETBYTE
  addq.b #1,d0
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,d0
  bra.w  sbc

opcode_ff: /* INS abcd,x [unofficial] - INC Mem then SBC with Acc] */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  INS_C_CONT

opcode_80: /* NOP #ab [unofficial - skip byte] */
opcode_82:
opcode_89:
opcode_c2:
opcode_e2:
  addq.l #cy_NOP2,CD
  addq.l #1,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_04: /* NOP ab [unofficial - skip byte] */
opcode_44:
opcode_64:
  addq.l #cy_NOP3,CD
  addq.l #1,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_14: /* NOP ab,x [unofficial - skip byte] */
opcode_34:
opcode_54:
opcode_74:
opcode_d4:
opcode_f4:
  addq.l #cy_NOP4,CD
  addq.l #1,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_0b: /* ANC #ab [unofficial - AND then copy N to C */
opcode_2b:
  addq.l #cy_Imm,CD
  and.b  (PC6502)+,A
  move.b A,ZFLAG
  smi    CFLAG
  bra.w  NEXTCHANGE_N

opcode_ab: /* ANX #ab [unofficial - AND #ab, then TAX] */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  and.b  d0,A
  move.b A,X
  NEXTCHANGE_REG(A)

opcode_8b: /* ANE #ab [unofficial - A AND X AND (Mem OR $EF) to Acc] */
  addq.l #cy_Imm,CD
  move.b (PC6502)+,d0
  and.b  X,A
  move.b A,ZFLAG
  and.b  d0,ZFLAG
  or.b   #0xef,d0
  and.b  d0,A
  bra.w  NEXTCHANGE_N

opcode_0c: /* NOP abcd [unofficial - skip word] */
  addq.l #cy_SKW,CD
  addq.l #2,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_1c: /* NOP abcd,x [unofficial - skip word] */
opcode_3c:
opcode_5c:
opcode_7c:
opcode_dc:
opcode_fc:
  addq.l #cy_SKW,CD
  move.b (PC6502),d7
  add.l  X,d7
  bcs.s  .SOLVE_PB
  addq.l #cy_Bcc1,CD
  addq.l #2,PC6502
  bra.w  NEXTCHANGE_WITHOUT
.SOLVE_PB:
  addq.l #cy_Bcc2,CD
  addq.l #2,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_1a: /* NOP [unofficial] */
opcode_3a:
opcode_5a:
opcode_7a:
opcode_da:
opcode_fa:
  addq.l #cy_NOP,CD
  bra.w  NEXTCHANGE_WITHOUT

/* official opcodes */

opcode_00: /* BRK */
#if   MONITOR_BREAK
  tst.b  _MONITOR_break_brk
  beq.s  .oc_00_norm
  bsr    go_monitor
  bra.w  NEXTCHANGE_WITHOUT
.oc_00_norm:
#endif
  addq.l #cy_BRK,CD
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  addq.w #1,d7
  moveq  #0,d0                    /* PHW + PHP */
  move.w regS.l,d0
  subq.b #1,d0     /* wrong way around */
  move.b d7,(memory_pointer,d0.l)
  addq.b #1,d0
  LoHi(d7)
  move.b d7,(memory_pointer,d0.l)
  subq.b #2,d0
  ConvertSTATUS_RegP(d7)
  move.b d7,(memory_pointer,d0.l)
  subq.b #1,d0
  move.b d0,_CPU_regS
  SetI
  move.w (memory_pointer,0xfffe.l),d7
  LoHi(d7)
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
#if   MONITOR_BREAK
  addq.l #1,_MONITOR_ret_nesting
#endif
  bra.w  NEXTCHANGE_WITHOUT

opcode_08: /* PHP */
  addq.l #cy_RegPH,CD
  move.w regS.l,d7
  ConvertSTATUS_RegP(d0)
  move.b d0,(memory_pointer,d7.l)
  subq.b #1,d7
  move.b d7,_CPU_regS
  bra.w  NEXTCHANGE_WITHOUT

opcode_28: /* PLP */
  addq.l #cy_RegPL,CD
  moveq  #0,d0          /* PLP */
  move.w regS.l,d0
  addq.b #1,d0
  move.b (memory_pointer,d0.l),d7
  ori.b  #0x30,d7
  move.b d7,_CPU_regP
  ConvertRegP_STATUS(d7)
  move.b d0,_CPU_regS
  tst.b  _CPU_IRQ           /* CPUCHECKIRQ */
  beq.w  NEXTCHANGE_WITHOUT
  cmp.l   _ANTIC_xpos_limit,CD
  bge     NEXTCHANGE_WITHOUT
  btst   #I_FLAGB,d7
  bne.w  NEXTCHANGE_WITHOUT
/* moveq  #0,d0 */
/* move.w regS.l,d0        ; push PC and P to stack ( PHW + PHB ) start */
  subq.b #2,d0          /* but do it the wrong way around for optim. */
  andi.b  #B_FLAGN,d7
  move.b  d7,(memory_pointer,d0.l) /* Push P */
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  addq.b #1,d0     /* wrong way around */
  move.b d7,(memory_pointer,d0.l)  /* Push High */
  addq.b #1,d0
  LoHi(d7)
  move.b d7,(memory_pointer,d0.l)  /* Push Low */
  subq.b #3,d0
  move.b d0,_CPU_regS       /* push PC and P to stack ( PHW + PHB ) end */
  SetI
  move.w (memory_pointer,0xfffe.l),d7
  LoHi(d7)
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
#if   MONITOR_BREAK
  addq.l #1,_MONITOR_ret_nesting
#endif
  bra.w  NEXTCHANGE_WITHOUT

opcode_48: /* PHA */
  addq.l #cy_RegPH,CD
  move.w regS.l,d7
  move.b A,(memory_pointer,d7.l)
  subq.b #1,d7
  move.b d7,_CPU_regS
  bra.w  NEXTCHANGE_WITHOUT

opcode_68: /* PLA */
  addq.l #cy_RegPL,CD
  move.w regS.l,d7
  addq.b #1,d7
  move.b (memory_pointer,d7.l),A
  move.b d7,_CPU_regS
  NEXTCHANGE_REG(A)

#define OR_ANYBYTE \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  OR_HW; \
  or.b   (memory_pointer,d7.l),A; \
  move.b A,ZFLAG; \
  bra.w  NEXTCHANGE_N

opcode_01: /* ORA (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  OR_ANYBYTE

opcode_11: /* ORA (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  OR_ANYBYTE

OR_HW:
  EXE_GETBYTE
  or.b   d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_0d: /* ORA abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  OR_ANYBYTE

opcode_19: /* ORA abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  OR_ANYBYTE

opcode_1d: /* ORA abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  OR_ANYBYTE

opcode_05: /* ORA ab */
  addq.l #cy_ZP,CD
  ZPAGE
  or.b   (memory_pointer,d7.l),A  /* OR ZPBYTE */
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_15: /* ORA ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  or.b   (memory_pointer,d7.l),A  /* OR ZPBYTE */
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_09: /* ORA #ab */
  addq.l #cy_Imm,CD
  or.b   (PC6502)+,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define AND_ANYBYTE \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  AND_HW; \
  and.b  (memory_pointer,d7.l),A; \
  move.b A,ZFLAG; \
  bra.w  NEXTCHANGE_N

opcode_21: /* AND (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  AND_ANYBYTE

opcode_31: /* AND (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  AND_ANYBYTE

AND_HW:
  EXE_GETBYTE
  and.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_2d: /* AND abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  AND_ANYBYTE

opcode_39: /* AND abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  AND_ANYBYTE

opcode_3d: /* AND abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  AND_ANYBYTE

opcode_25: /* AND ab */
  addq.l #cy_ZP,CD
  ZPAGE
  and.b  (memory_pointer,d7.l),A  /* AND ZPBYTE */
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_35: /* AND ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  and.b  (memory_pointer,d7.l),A  /* AND ZPBYTE */
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_29: /* AND #ab */
  addq.l #cy_Imm,CD
  and.b  (PC6502)+,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define EOR_C_CONT \
  eor.b  d0,A; \
  move.b A,ZFLAG; \
  bra.w  NEXTCHANGE_N

#define GETANYBYTE_EOR \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  EOR_HW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  EOR_C_CONT

opcode_41: /* EOR (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_EOR

opcode_51: /* EOR (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_EOR

EOR_HW:
  EXE_GETBYTE
  EOR_C_CONT

opcode_4d: /* EOR abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_EOR

opcode_59: /* EOR abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_EOR

opcode_5d: /* EOR abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_EOR

opcode_45: /* EOR ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0
  EOR_C_CONT

opcode_55: /* EOR ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0
  EOR_C_CONT

opcode_49: /* EOR #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)     /* because eor only works with registers ! */
  EOR_C_CONT

opcode_0a: /* ASLA */
  addq.l #cy_RegChg,CD
  ASL_C(A)
  NEXTCHANGE_REG(A)

opcode_06: /* ASL ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),ZFLAG
  ASL_C(ZFLAG)
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

opcode_16: /* ASL ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),ZFLAG
  ASL_C(ZFLAG)
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

#define RPW_ASL_C \
  move.b  (attrib_pointer,d7.l),d0; \
  bne.s  RPW_HW_ASL; \
  move.b (memory_pointer,d7.l),ZFLAG; /* get byte */ \
  ASL_C(ZFLAG); \
  move.b ZFLAG,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_N

opcode_0e: /* ASL abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  RPW_ASL_C

opcode_1e: /* ASL abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  RPW_ASL_C

RPW_HW_ASL:
  cmp.b  #isROM,d0
  beq.s  RPW_ROM_ASL
  RMW_GETBYTE
  ASL_C(d0)
  ext.w  d0
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ASL:
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  ASL_C(ZFLAG)
  bra.w  NEXTCHANGE_N

opcode_2a: /* ROLA */
  addq.l #cy_RegChg,CD
  ROL_C(A)
  NEXTCHANGE_REG(A)

opcode_26: /* ROL ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  ROL_C(ZFLAG)
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

opcode_36: /* ROL ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  ROL_C(ZFLAG)
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

#define RPW_ROL_C \
  move.b (attrib_pointer,d7.l),d0; \
  bne.s  RPW_HW_ROL; \
  move.b (memory_pointer,d7.l),ZFLAG; /* get byte */ \
  ROL_C(ZFLAG); \
  move.b ZFLAG,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_N

opcode_2e: /* ROL abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  RPW_ROL_C

opcode_3e: /* ROL abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  RPW_ROL_C

RPW_HW_ROL:
  cmp.b  #isROM,d0
  beq.s  RPW_ROM_ROL
  RMW_GETBYTE
  ROL_C(d0)
  ext.w  d0
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ROL:
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  ROL_C(ZFLAG)
  bra.w  NEXTCHANGE_N

opcode_4a: /* LSRA */
  addq.l #cy_RegChg,CD
  clr.w  NFLAG
  lsr.b  #1,A
  scs    CFLAG
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT

opcode_46: /* LSR ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  clr.w  NFLAG
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  lsr.b  #1,ZFLAG
  scs    CFLAG
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

opcode_56: /* LSR ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  clr.w  NFLAG
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  lsr.b  #1,ZFLAG
  scs    CFLAG
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

#define RPW_LSR_C \
  clr.w  NFLAG; \
  move.b (attrib_pointer,d7.l),d0; \
  bne.s  RPW_HW_LSR; \
  move.b (memory_pointer,d7.l),ZFLAG; /* get byte */ \
  LSR_C(ZFLAG); \
  move.b ZFLAG,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_WITHOUT

opcode_4e: /* LSR abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  RPW_LSR_C

opcode_5e: /* LSR abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  RPW_LSR_C

RPW_HW_LSR:
  cmp.b  #isROM,d0
  beq.s  RPW_ROM_LSR
  RMW_GETBYTE
  LSR_C(d0)
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_LSR:
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  LSR_C(ZFLAG)
  bra.w  NEXTCHANGE_WITHOUT

opcode_6a: /* RORA */
  addq.l #cy_RegChg,CD
  ROR_C(A)
  NEXTCHANGE_REG(A)

opcode_66: /* ROR ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  add.b  CFLAG,CFLAG
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  roxr.b #1,ZFLAG
  scs    CFLAG
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

opcode_76: /* ROR ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  add.b  CFLAG,CFLAG
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  roxr.b #1,ZFLAG
  scs    CFLAG
  move.b ZFLAG,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_N

#define RPW_ROR_C \
  move.b (attrib_pointer,d7.l),d0; \
  bne.s  RPW_HW_ROR; \
  move.b (memory_pointer,d7.l),ZFLAG; /* get byte */ \
  ROR_C(ZFLAG); \
  move.b ZFLAG,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_N

opcode_6e: /* ROR abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  RPW_ROR_C

opcode_7e: /* ROR abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  RPW_ROR_C

RPW_HW_ROR:
  cmp.b  #isROM,d0
  beq.s  RPW_ROM_ROR
  RMW_GETBYTE
  ROR_C(d0)
  ext.w  d0
  move.l d0,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ROR:
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  ROR_C(ZFLAG)
  bra.w  NEXTCHANGE_N

opcode_18: /* CLC */
  addq.l #cy_FlagCS,CD
  ClrCFLAG
  bra.w  NEXTCHANGE_WITHOUT

opcode_38: /* SEC */
  addq.l #cy_FlagCS,CD
  SetCFLAG
  bra.w  NEXTCHANGE_WITHOUT

opcode_58: /* CLI */
  addq.l #cy_FlagCS,CD
  ClrI
  tst.b  _CPU_IRQ      /* ~ CPUCHECKIRQ */
  beq.w  NEXTCHANGE_WITHOUT
  cmp.l   _ANTIC_xpos_limit,CD
  bge     NEXTCHANGE_WITHOUT
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  moveq  #0,d0                    /* PHW + PHP (B0) */
  move.w regS.l,d0
  subq.b #1,d0     /* wrong way around */
  move.b d7,(memory_pointer,d0.l)
  addq.b #1,d0
  LoHi(d7)
  move.b d7,(memory_pointer,d0.l)
  subq.b #2,d0
  ConvertSTATUS_RegP(d7)
  andi.b #B_FLAGN,d7
  move.b d7,(memory_pointer,d0.l)
  subq.b #1,d0
  move.b d0,_CPU_regS
  SetI
  move.w (memory_pointer,0xfffe.l),d7
  LoHi(d7)
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  clr.b  _CPU_IRQ
  addq.l #7,CD
#if   MONITOR_BREAK
  addq.l #1,_MONITOR_ret_nesting
#endif
  bra.w  NEXTCHANGE_WITHOUT

opcode_78: /* SEI */
  addq.l #cy_FlagCS,CD
  SetI
  bra.w  NEXTCHANGE_WITHOUT

opcode_b8: /* CLV */
  addq.l #cy_FlagCS,CD
  ClrVFLAG
  bra.w  NEXTCHANGE_WITHOUT

opcode_d8: /* CLD */
  addq.l #cy_FlagCS,CD
  ClrD
  bra.w  NEXTCHANGE_WITHOUT

opcode_f8: /* SED */
  addq.l #cy_FlagCS,CD
  SetD
  bra.w  NEXTCHANGE_WITHOUT

#define JMP_C \
  move.w (PC6502)+,d7; \
  LoHi(d7);   /* (in d7 adress where we want to jump) */ \
  lea (memory_pointer,d7.l),PC6502; \
  bra.w  NEXTCHANGE_WITHOUT

opcode_4c: /* JMP abcd */
#if   MONITOR_BREAK
  move.l PC6502,d7 /* current pointer */
  sub.l  memory_pointer,d7
  subq.l #1,d7
  lea    _CPU_remember_JMP,a0
  move.l _CPU_remember_jmp_curpos,d0
  move.w d7,(a0,d0*2)
  addq.l #1,d0
  cmp.l  #rem_jmp_steps,d0
  bmi.s  .point_rem_jmp
  moveq  #0,d0
.point_rem_jmp:
  move.l d0,_CPU_remember_jmp_curpos
#endif
  addq.l #cy_JmpAbs,CD
  JMP_C

opcode_6c: /* JMP (abcd) */
#if   MONITOR_BREAK
  move.l PC6502,d7 /* current pointer */
  sub.l  memory_pointer,d7
  subq.l #1,d7
  lea    _CPU_remember_JMP,a0
  move.l _CPU_remember_jmp_curpos,d0
  move.w d7,(a0,d0*2)
  addq.l #1,d0
  cmp.l  #rem_jmp_steps,d0
  bmi.s  .point_rem_jmp
  moveq  #0,d0
.point_rem_jmp:
  move.l d0,_CPU_remember_jmp_curpos
#endif
  move.w (PC6502)+,d7
  LoHi(d7)
#if   P65C02
  move.w (memory_pointer,d7.l),d7
  LoHi(d7)
  lea    (memory_pointer,d7.l),PC6502
#else
  /* original 6502 had a bug in jmp (addr) when addr crossed page boundary */
  cmp.b  #0xff,d7
  beq.s  .PROBLEM_FOUND /* when problematic jump is found */
  move.w (memory_pointer,d7.l),d7
  LoHi(d7)
  lea    (memory_pointer,d7.l),PC6502
  addq.l #cy_JmpInd,CD
  bra.w  NEXTCHANGE_WITHOUT
.PROBLEM_FOUND:
  move.l d7,d0 /* we have to use both of them */
  clr.b  d7 /* instead of reading right this adress, */
            /* we read adress at this start of page */
  move.b (memory_pointer,d7.l),d7
  LoHi(d7)
  move.b (memory_pointer,d0.l),d7
  lea    (memory_pointer,d7.l),PC6502
#endif
  addq.l #cy_JmpInd,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_20: /* JSR abcd */
  addq.l #cy_Sub,CD
  move.l PC6502,d7 /* current pointer */
  sub.l  memory_pointer,d7
#if   MONITOR_BREAK
  subq.l #1,d7
  lea    _CPU_remember_JMP,a0
  move.l _CPU_remember_jmp_curpos,d0
  move.w d7,(a0,d0*2)
  addq.l #1,d7     /* restore to PC */
  addq.l #1,d0
  cmp.l  #rem_jmp_steps,d0
  bmi.s  .point_rem_jmp
  moveq  #0,d0
.point_rem_jmp:
  move.l d0,_CPU_remember_jmp_curpos
  addq.l #1,_MONITOR_ret_nesting
#endif
  addq.l #1,d7 /* return address */
  moveq  #0,d0                    /* PHW */
  move.w regS.l,d0
  subq.b #1,d0     /* wrong way around */
  move.b d7,(memory_pointer,d0.l)
  addq.b #1,d0
  LoHi(d7)
  move.b d7,(memory_pointer,d0.l)
  subq.b #2,d0
  move.b d0,_CPU_regS
  JMP_C

opcode_60: /* RTS */
  addq.l #cy_Sub,CD
  PLW(d7,d0)
  lea    1(memory_pointer,d7.l),PC6502
#if   MONITOR_BREAK
  tst.b  _MONITOR_break_ret
  beq.s  .mb_end
  subq.l #1,_MONITOR_ret_nesting
  bgt.s  .mb_end
  move.b #1,_MONITOR_break_step
.mb_end:
#endif
  move.l _CPU_rts_handler,a0
  tst.l  a0
  beq.b  .no_rts
  UPDATE_GLOBAL_REGS
  jsr	 (a0)
  UPDATE_LOCAL_REGS
.no_rts:
  bra.w  NEXTCHANGE_WITHOUT

opcode_40: /* RTI */
  addq.l #cy_Sub,CD
  moveq  #0,d0                    /* PLP + PLW */
  move.w regS.l,d0
  addq.b #1,d0
  move.b (memory_pointer,d0.l),d7
  ori.b  #0x30,d7
  move.b d7,_CPU_regP
  ConvertRegP_STATUS(d7)
  addq.b #2,d0     /* wrong way around */
  move.b (memory_pointer,d0.l),d7
  asl.w  #8,d7
  subq.b #1,d0
  or.b   (memory_pointer,d0.l),d7
  addq.b #1,d0
  move.b d0,_CPU_regS
  lea    (memory_pointer,d7.l),PC6502
  tst.b  _CPU_IRQ           /* CPUCHECKIRQ */
  jbeq   .no_irq
  cmp.l  _ANTIC_xpos_limit,CD
  jbge    .no_irq
  move.b _CPU_regP.l,d7
/* andi.b #I_FLAG,d7 */
  btst   #I_FLAGB,d7
  jbne   .no_irq
  moveq  #0,d0
  move.w regS.l,d0        /* push PC and P to stack ( PHW + PHB ) start */
  subq.b #2,d0
  andi.b #B_FLAGN,d7
  move.b d7,(memory_pointer,d0.l) /* Push P */
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  addq.b #1,d0          /* wrong way around */
  move.b d7,(memory_pointer,d0.l)
  addq.b #1,d0
  LoHi(d7)
  move.b d7,(memory_pointer,d0.l)
  subq.b #3,d0
  move.b d0,_CPU_regS       /* push PC and P to stack ( PHW + PHB ) end */
  SetI
  move.w (memory_pointer,0xfffe.l),d7
  LoHi(d7)
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
#if   MONITOR_BREAK
  addq.l #1,_MONITOR_ret_nesting
#endif
.no_irq:
#if   MONITOR_BREAK
  tst.b  _MONITOR_break_ret
  beq.s  .mb_end
  subq.l #1,_MONITOR_ret_nesting
  bgt.s  .mb_end
  move.b #1,_MONITOR_break_step
.mb_end:
#endif
  bra.w  NEXTCHANGE_WITHOUT

#define BIT_C_CONT \
  ext.w  NFLAG; \
  btst   #V_FLAGB,ZFLAG; \
  sne    VFLAG; \
  and.b  A,ZFLAG; \
  bra.w  NEXTCHANGE_WITHOUT

opcode_24: /* BIT ab */
  addq.l #cy_ZP,CD
  ZPAGE
BIT_RAMROM:
  move.b (memory_pointer,d7.l),ZFLAG   /* GETZPBYTE */
  BIT_C_CONT

opcode_2c: /* BIT abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  bne.s  BIT_RAMROM
  EXE_GETBYTE
  move.b d0,ZFLAG
  BIT_C_CONT

#define STOREANYBYTE_A \
  tst.b  (attrib_pointer,d7.l); \
  bne.s  STA_HW; \
  move.b A,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_WITHOUT

opcode_81: /* STA (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  STOREANYBYTE_A

opcode_91: /* STA (ab),y */
  addq.l #cy_IndY2,CD
  INDIRECT_Y
  STOREANYBYTE_A

opcode_8d: /* STA abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  STOREANYBYTE_A

opcode_99: /* STA abcd,y */
  addq.l #cy_IndY2,CD
  ABSOLUTE_Y
  STOREANYBYTE_A

opcode_9d: /* STA abcd,x */
  addq.l #cy_AbsX2,CD
  ABSOLUTE_X
  STOREANYBYTE_A

STA_HW:
  move.b A,d0
  bra.w  A800PUTB

opcode_85: /* STA ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b A,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

opcode_95: /* STA ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b A,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

#define STOREANYBYTE(r); \
  tst.b  (attrib_pointer,d7.l); \
  bne.s  1f; \
  move.b r,(memory_pointer,d7.l); \
  bra.w  NEXTCHANGE_WITHOUT; \
1:; \
  move.b r,d0; \
  bra.w  A800PUTB

opcode_8e: /* STX abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  STOREANYBYTE(X)

opcode_86: /* STX ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b X,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

opcode_96: /* STX ab,y */
  addq.l #cy_ZPY,CD
  ZPAGE_Y
  move.b X,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

opcode_8c: /* STY abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  STOREANYBYTE(Y)

opcode_84: /* STY ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b Y,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

opcode_94: /* STY ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b Y,(memory_pointer,d7.l)   /* PUTZPBYTE */
  bra.w  NEXTCHANGE_WITHOUT

#define LOADANYBYTE_A \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  LDA_HW; \
  move.b (memory_pointer,d7.l),A;  /* get byte */ \
  NEXTCHANGE_REG(A)

opcode_a1: /* LDA (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  LOADANYBYTE_A

opcode_b1: /* LDA (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  LOADANYBYTE_A

LDA_HW:
  EXE_GETBYTE
  move.b d0,A
  NEXTCHANGE_REG(A)

opcode_ad: /* LDA abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  LOADANYBYTE_A

opcode_b9: /* LDA abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  LOADANYBYTE_A

opcode_bd: /* LDA abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  LOADANYBYTE_A

opcode_a5: /* LDA ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),A  /* GETZPBYTE */
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_b5: /* LDA ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),A  /* GETZPBYTE */
  NEXTCHANGE_REG(A)

opcode_a9: /* LDA #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(A)
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N

#define LOADANYBYTE_X \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  LDX_HW; \
  move.b (memory_pointer,d7.l),X;  /* get byte */ \
  NEXTCHANGE_REG(X)

opcode_ae: /* LDX abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  LOADANYBYTE_X

opcode_be: /* LDX abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  LOADANYBYTE_X

LDX_HW:
  EXE_GETBYTE
  move.b d0,X
  NEXTCHANGE_REG(X)

opcode_a6: /* LDX ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),X  /* GETZPBYTE */
  move.b X,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_b6: /* LDX ab,y */
  addq.l #cy_ZPY,CD
  ZPAGE_Y
  move.b (memory_pointer,d7.l),X  /* GETZPBYTE */
  NEXTCHANGE_REG(X)

opcode_a2: /* LDX #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(X)
  move.b X,ZFLAG
  bra.w  NEXTCHANGE_N

#define LOADANYBYTE_Y \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  LDY_HW; \
  move.b (memory_pointer,d7.l),Y;  /* get byte */ \
  NEXTCHANGE_REG(Y)

opcode_ac: /* LDY abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  LOADANYBYTE_Y

opcode_bc: /* LDY abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  LOADANYBYTE_Y

LDY_HW:
  EXE_GETBYTE
  move.b d0,Y
  NEXTCHANGE_REG(Y)

opcode_a4: /* LDY ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),Y  /* GETZPBYTE */
  move.b Y,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_b4: /* LDY ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),Y       /* GETZPBYTE */
  NEXTCHANGE_REG(Y)

opcode_a0: /* LDY #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(Y)
  move.b Y,ZFLAG
  bra.w  NEXTCHANGE_N

opcode_8a: /* TXA */
  addq.l #cy_RegChg,CD
  move.b X,A
  NEXTCHANGE_REG(A)

opcode_aa: /* TAX */
  addq.l #cy_RegChg,CD
  move.b A,X
  NEXTCHANGE_REG(A)

opcode_98: /* TYA */
  addq.l #cy_RegChg,CD
  move.b Y,A
  NEXTCHANGE_REG(A)

opcode_a8: /* TAY */
  addq.l #cy_RegChg,CD
  move.b A,Y
  NEXTCHANGE_REG(A)

opcode_9a: /* TXS */
  addq.l #cy_RegChg,CD
  move.b X,_CPU_regS
  bra.w  NEXTCHANGE_WITHOUT

opcode_ba: /* TSX */
  addq.l #cy_RegChg,CD
  move.b _CPU_regS.l,X
  NEXTCHANGE_REG(X)

opcode_d2: /* ESCRTS #ab (JAM) - on Atari is here instruction CIM */
           /* [unofficial] !RS! */
  addq.l #cy_CIM,CD
  move.b (PC6502)+,d7
  Call_Atari800_RunEsc
  PLW(d7,d0)
  lea    (memory_pointer,d7.l),PC6502
  addq.l #1,PC6502
#if   MONITOR_BREAK
  tst.b  _MONITOR_break_ret
  beq.s .mb_end
  subq.l #1,_MONITOR_ret_nesting
  bgt.s  .mb_end
  move.b #1,_MONITOR_break_step
.mb_end:
#endif
  bra.w  NEXTCHANGE_WITHOUT

opcode_f2: /* ESC #ab (JAM) - on Atari is here instruction CIM */
           /* [unofficial] !RS! */
  addq.l #cy_CIM,CD
  move.b (PC6502)+,d7
  Call_Atari800_RunEsc
  bra.w  NEXTCHANGE_WITHOUT

opcode_ea: /* NOP */ /* official */
  addq.l #cy_NOP,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_c6: /* DEC ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  subq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_d6: /* DEC ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  subq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_ce: /* DEC abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  move.b (attrib_pointer,d7.l),d0
  bne.s  DEC_Byte_ROMHW
  subq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N
DEC_Byte_ROMHW:
  cmp.b  #isHARDWARE,d0
  beq.s  1f
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  subq.b #1,ZFLAG
  bra.w  NEXTCHANGE_N
1:
  RMW_GETBYTE
  move.b d0,ZFLAG
  subq.b #1,ZFLAG
/* bra.w  A800PUTB_Ld0_N */
A800PUTB_Ld0_N:
  ext.w  NFLAG
A800PUTB_Ld0:
  move.b ZFLAG,d0
A800PUTB:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  A800PUTBE
  move.l ZFLAG,-(a7)
  EXE_PUTBYTE(d7)
  move.l (a7)+,ZFLAG
A800PUTBE:
  bra.w  NEXTCHANGE_WITHOUT

opcode_de: /* DEC abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  move.b (attrib_pointer,d7.l),d0
  bne.s  DEC_Byte_ROMHW
  subq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_ca: /* DEX */
  addq.l #cy_RegChg,CD
  subq.b #1,X
  NEXTCHANGE_REG(X)

opcode_88: /* DEY */
  addq.l #cy_RegChg,CD
  subq.b #1,Y
  NEXTCHANGE_REG(Y)

opcode_e6: /* INC ab */
  addq.l #cy_ZP_RW,CD
  ZPAGE
  addq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_f6: /* INC ab,x */
  addq.l #cy_ZPX_RW,CD
  ZPAGE_X
  addq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_ee: /* INC abcd */
  addq.l #cy_Abs_RW,CD
  ABSOLUTE
  move.b (attrib_pointer,d7.l),d0
  bne.s  INC_Byte_ROMHW
  addq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N
INC_Byte_ROMHW:
  cmp.b  #isHARDWARE,d0
  beq.s  1f
  move.b (memory_pointer,d7.l),ZFLAG /* get byte */
  addq.b #1,ZFLAG
  bra.w  NEXTCHANGE_N
1:
  RMW_GETBYTE
  move.b d0,ZFLAG
  addq.b #1,ZFLAG
  bra.w  A800PUTB_Ld0_N

opcode_fe: /* INC abcd,x */
  addq.l #cy_AbsX_RW,CD
  ABSOLUTE_X
  move.b (attrib_pointer,d7.l),d0
  bne.s  INC_Byte_ROMHW
  addq.b #1,(memory_pointer,d7.l)
  move.b (memory_pointer,d7.l),ZFLAG
  bra.w  NEXTCHANGE_N

opcode_e8: /* INX */
  addq.l #cy_RegChg,CD
  addq.b #1,X
  NEXTCHANGE_REG(X)

opcode_c8: /* INY */
  addq.l #cy_RegChg,CD
  addq.b #1,Y
  NEXTCHANGE_REG(Y)

#define DONT_BRA \
  addq.l #cy_Bcc,CD; \
  addq.l #1,PC6502; \
  bra.w  NEXTCHANGE_WITHOUT

opcode_10: /* BPL */
  tst.w  NFLAG
  bpl.s  SOLVE
  DONT_BRA

opcode_30: /* BMI */
  tst.w  NFLAG
  bmi.s  SOLVE
  DONT_BRA

opcode_d0: /* BNE */
  tst.b ZFLAG
  bne.s SOLVE
  DONT_BRA

opcode_f0: /* BEQ */
  tst.b ZFLAG
  beq.s SOLVE
  DONT_BRA

SOLVE:
  move.b (PC6502)+,d7
  extb.l d7
  move.l PC6502,d0
  add.l  d7,PC6502
  sub.l  memory_pointer,d0
  and.w  #255,d0                  /* !!! */
  add.w  d7,d0
  and.w  #0xff00,d0
  bne.s  SOLVE_PB
  addq.l #cy_Bcc1,CD
  bra.w  NEXTCHANGE_WITHOUT
SOLVE_PB:
  addq.l #cy_Bcc2,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_90: /* BCC */
  tst.b  CFLAG
  beq.s  SOLVE
  DONT_BRA

opcode_b0: /* BCS */
  tst.b  CFLAG
  bne.s  SOLVE
  DONT_BRA

opcode_50: /* BVC */
  tst.b  VFLAG
  beq.s  SOLVE
  DONT_BRA

opcode_70: /* BVS */
  tst.b  VFLAG
  bne.s  SOLVE
  DONT_BRA

#define GETANYBYTE_ADC_NOBRA \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  ADC_HW; \
  move.b (memory_pointer,d7.l),d0 /* get byte */ \

#define GETANYBYTE_ADC \
  GETANYBYTE_ADC_NOBRA; \
  jbra  adcb

adc:                         /* !!! put it where it's needed !!! */
  btst   #D_FLAGB,_CPU_regP.l
  jbne  BCD_ADC
  jbra  adcb

opcode_61: /* ADC (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_ADC

opcode_71: /* ADC (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_ADC

ADC_HW:
  EXE_GETBYTE
  bra.s  adcb

opcode_6d: /* ADC abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_ADC

opcode_79: /* ADC abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_ADC

opcode_7d: /* ADC abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_ADC_NOBRA

adcb:
  add.b  CFLAG,CFLAG
  addx.b d0,A
  svs    VFLAG
  scs    CFLAG
  NEXTCHANGE_REG(A)

opcode_65: /* ADC ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  adcb

opcode_75: /* ADC ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  adcb

opcode_69: /* ADC #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  bra.s  adcb

#define GETANYBYTE_ADC_D_NOBRA \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  ADC_HW_D; \
  move.b (memory_pointer,d7.l),d0 /* get byte */ \

#define GETANYBYTE_ADC_D \
  GETANYBYTE_ADC_D_NOBRA; \
  bra.s  BCD_ADC

opcode_61_D: /* ADC (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_ADC_D

opcode_6d_D: /* ADC abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_ADC_D

opcode_71_D: /* ADC (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_ADC_D

ADC_HW_D:
  EXE_GETBYTE
  bra.s  BCD_ADC

opcode_79_D: /* ADC abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_ADC_D

opcode_7d_D: /* ADC abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_ADC_D_NOBRA

BCD_ADC:
  unpk   A,d7,#0
  unpk   d0,ZFLAG,#0
  add.b  CFLAG,CFLAG
  addx.w ZFLAG,d7
  cmp.b  #0x0a,d7
  blo.s  1f
  add.w  #0x0106,d7
1:
  pack   d7,d7,#0
  move.b d7,ZFLAG
  ext.w  NFLAG
  move.b A,ZFLAG
  add.b  CFLAG,CFLAG
  addx.b d0,ZFLAG
  eor.b  d0,A
  not.b  A
  eor.b  d7,d0
  and.b  A,d0
  smi    VFLAG
  move.b d7,A
  cmp.w  #0x0a00,d7
  scc    CFLAG
  blo.s  2f
  add.b  #0x60,A
2:
  bra    NEXTCHANGE_WITHOUT

opcode_65_D: /* ADC ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  BCD_ADC

opcode_75_D: /* ADC ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  BCD_ADC

opcode_69_D: /* ADC #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  bra.s  BCD_ADC

#define GETANYBYTE_SBC_NOBRA \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  SBC_HW; \
  move.b (memory_pointer,d7.l),d0 /* get byte */ \

#define GETANYBYTE_SBC \
  GETANYBYTE_SBC_NOBRA; \
  bra.s  sbcb

sbc:                         /* !!! put it where it's needed !!! */
  btst   #D_FLAGB,_CPU_regP.l
  bne.w  BCD_SBC
  bra.w  sbcb

opcode_e1: /* SBC (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_SBC

opcode_f1: /* SBC (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_SBC

SBC_HW:
  EXE_GETBYTE
  bra.s  sbcb

opcode_ed: /* SBC abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_SBC

opcode_f9: /* SBC abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_SBC

opcode_fd: /* SBC abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_SBC_NOBRA

sbcb:
  subq.b #1,CFLAG
  subx.b d0,A
  svs    VFLAG
  scc    CFLAG
  NEXTCHANGE_REG(A)

opcode_e5: /* SBC ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  sbcb

opcode_f5: /* SBC ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  sbcb

opcode_eb: /* SBC #ab [unofficial] */
opcode_e9: /* SBC #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  bra.s  sbcb

#define GETANYBYTE_SBC_D_NOBRA \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  SBC_HW_D; \
  move.b (memory_pointer,d7.l),d0 /* get byte */ \

#define GETANYBYTE_SBC_D \
  GETANYBYTE_SBC_D_NOBRA; \
  bra.s  BCD_SBC

opcode_e1_D: /* SBC (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_SBC_D

opcode_ed_D: /* SBC abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_SBC_D

opcode_f1_D: /* SBC (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_SBC_D

SBC_HW_D:
  EXE_GETBYTE
  bra.s  BCD_SBC

opcode_f9_D: /* SBC abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_SBC_D

opcode_fd_D: /* SBC abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_SBC_D_NOBRA

BCD_SBC:
  move.b A,ZFLAG
  unpk   A,A,#0
  unpk   d0,d7,#0
  not.b  CFLAG
  add.b  CFLAG,CFLAG
  subx.w d7,A
  tst.b  A
  bpl.s  1f
  subq.w #6,A
1:
  pack   A,A,#0
  tst.w  A
  bpl.s  2f
  sub.b  #0x60,A
2:
  add.b  CFLAG,CFLAG
  subx.b d0,ZFLAG
  svs    VFLAG
  scc    CFLAG
  bra    NEXTCHANGE_N

opcode_e5_D: /* SBC ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  BCD_SBC

opcode_f5_D: /* SBC ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  bra.s  BCD_SBC

opcode_eb_D: /* SBC #ab [unofficial] */
opcode_e9_D: /* SBC #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  bra.s  BCD_SBC

opcode_cc: /* CPY abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l) /* GETANYBYTE */
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  move.b Y,ZFLAG
  bra COMPARE
1:
  EXE_GETBYTE
  move.b Y,ZFLAG
  bra COMPARE

opcode_c4: /* CPY ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  move.b Y,ZFLAG
  bra COMPARE

opcode_c0: /* CPY #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  move.b Y,ZFLAG
  bra COMPARE

opcode_ec: /* CPX abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l) /* GETANYBYTE */
  beq.s  1f
  move.b (memory_pointer,d7.l),d0 /* get byte */
  move.b X,ZFLAG
  bra COMPARE
1:
  EXE_GETBYTE
  move.b X,ZFLAG
  bra COMPARE

opcode_e4: /* CPX ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  move.b X,ZFLAG
  bra COMPARE

opcode_e0: /* CPX #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
  move.b X,ZFLAG
  bra COMPARE

#define GETANYBYTE_CMP \
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l); \
  beq.s  CMP_HW; \
  move.b (memory_pointer,d7.l),d0; /* get byte */ \
  jbra   COMPARE_A

opcode_c1: /* CMP (ab,x) */
  addq.l #cy_IndX,CD
  INDIRECT_X
  GETANYBYTE_CMP

opcode_d1: /* CMP (ab),y */
  addq.l #cy_IndY,CD
  INDIRECT_Y_NCY
  GETANYBYTE_CMP

CMP_HW:
  EXE_GETBYTE
  jbra  COMPARE_A

opcode_cd: /* CMP abcd */
  addq.l #cy_Abs,CD
  ABSOLUTE
  GETANYBYTE_CMP

opcode_d9: /* CMP abcd,y */
  addq.l #cy_AbsY,CD
  ABSOLUTE_Y_NCY
  GETANYBYTE_CMP

opcode_dd: /* CMP abcd,x */
  addq.l #cy_AbsX,CD
  ABSOLUTE_X_NCY
  GETANYBYTE_CMP

opcode_d5: /* CMP ab,x */
  addq.l #cy_ZPX,CD
  ZPAGE_X
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  jbra  COMPARE_A

opcode_c5: /* CMP ab */
  addq.l #cy_ZP,CD
  ZPAGE
  move.b (memory_pointer,d7.l),d0      /* GETZPBYTE */
  jbra  COMPARE_A

opcode_c9: /* CMP #ab */
  addq.l #cy_Imm,CD
  IMMEDIATE(d0)
/* jbra  COMPARE_A */

COMPARE_A:
  move.b A,ZFLAG
COMPARE:
  sub.b  d0,ZFLAG
  scc    CFLAG
/* bra.w  NEXTCHANGE_N */

/* MAIN LOOP , where we are counting cycles and working with other STUFF */

NEXTCHANGE_N:
  ext.w  NFLAG
NEXTCHANGE_WITHOUT:
  cmp.l _ANTIC_xpos_limit,CD
  bge.s END_OF_CYCLE
****************************************
#if   MONITOR_BREAK  /* following block of code allows you to enter */
                     /* a break address */
  move.l _CPU_remember_PC_curpos,d0
  lea    _CPU_remember_PC,a0
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  move.w d7,(a0,d0.l*2) /* remember program counter */

  lea	 _CPU_remember_op,a0
  mulu.w #3,d0
  add.l  d0,a0
  move.b (0.b,memory_pointer,d7.l),(a0)+
  move.b (1.b,memory_pointer,d7.l),(a0)+
  move.b (2.b,memory_pointer,d7.l),(a0)+

  move.l _CPU_remember_PC_curpos,d0
  lea    _CPU_remember_xpos,a0
  lea    (a0,d0.l*4),a0
#if   NEW_CYCLE_EXACT
  cmp.l   #-999,_ANTIC_cur_screen_pos
  bne.s   .not_drawing
  move.l  _ANTIC_cpu2antic_ptr,a1
  move.l  (a1,CD.l*4),a1
  bra.s   .drawing
.not_drawing:
#endif
  move.l  CD,a1
.drawing:
  move.l  _ANTIC_ypos,d0
  lsl.w   #8,d0
  add.l   d0,a1
  move.l  a1,(a0)

  move.l _CPU_remember_PC_curpos,d0
  addq.l #1,d0
  cmp.l  #rem_pc_steps,d0
  bmi.s  .point_rem_pc
  moveq  #0,d0
.point_rem_pc:
  move.l d0,_CPU_remember_PC_curpos

  cmp.w  _MONITOR_break_addr,d7 /* break address reached ? */
  beq.s  .go_monitor
  move.l _ANTIC_ypos,d0
  cmp.l  _ANTIC_break_ypos,d0 /* break address reached ? */
  beq.s  .go_monitor
  tst.b  _MONITOR_break_step /* step mode active ? */
  beq.s  .get_first
.go_monitor:
  bsr    go_monitor  /* on break monitor is invoked */
.get_first
#endif
****************************************
  moveq  #0,d7
  move.b (PC6502)+,d7
#if   PROFILE
  lea    _CPU_instruction_count,a0
  addq.l #1,(a0,d7.l*4)
#endif
  move.w (a3,d7.l*2),d0
  jmp    (a3,d0.w)

END_OF_CYCLE:
  ConvertSTATUS_RegP_destroy(d0)
  UPDATE_GLOBAL_REGS
  move.l CD,_ANTIC_xpos /* returned value */
  movem.l (a7)+,d2-d7/a2-a6
TERM_GO:
  rts

go_monitor:
  ConvertSTATUS_RegP_destroy(d0)
  UPDATE_GLOBAL_REGS
  Call_Atari800_Exit_true
  UPDATE_LOCAL_REGS
  ConvertRegP_STATUS(d0)
  rts

  .p2align 2         /* doubleword alignment */

OPMODE_TABLE:
OP_T:
  dc.w opcode_00-OP_T
  dc.w opcode_01-OP_T
  dc.w opcode_02-OP_T
  dc.w opcode_03-OP_T
  dc.w opcode_04-OP_T
  dc.w opcode_05-OP_T
  dc.w opcode_06-OP_T
  dc.w opcode_07-OP_T
  dc.w opcode_08-OP_T
  dc.w opcode_09-OP_T
  dc.w opcode_0a-OP_T
  dc.w opcode_0b-OP_T
  dc.w opcode_0c-OP_T
  dc.w opcode_0d-OP_T
  dc.w opcode_0e-OP_T
  dc.w opcode_0f-OP_T
  dc.w opcode_10-OP_T
  dc.w opcode_11-OP_T
  dc.w opcode_12-OP_T
  dc.w opcode_13-OP_T
  dc.w opcode_14-OP_T
  dc.w opcode_15-OP_T
  dc.w opcode_16-OP_T
  dc.w opcode_17-OP_T
  dc.w opcode_18-OP_T
  dc.w opcode_19-OP_T
  dc.w opcode_1a-OP_T
  dc.w opcode_1b-OP_T
  dc.w opcode_1c-OP_T
  dc.w opcode_1d-OP_T
  dc.w opcode_1e-OP_T
  dc.w opcode_1f-OP_T
  dc.w opcode_20-OP_T
  dc.w opcode_21-OP_T
  dc.w opcode_22-OP_T
  dc.w opcode_23-OP_T
  dc.w opcode_24-OP_T
  dc.w opcode_25-OP_T
  dc.w opcode_26-OP_T
  dc.w opcode_27-OP_T
  dc.w opcode_28-OP_T
  dc.w opcode_29-OP_T
  dc.w opcode_2a-OP_T
  dc.w opcode_2b-OP_T
  dc.w opcode_2c-OP_T
  dc.w opcode_2d-OP_T
  dc.w opcode_2e-OP_T
  dc.w opcode_2f-OP_T
  dc.w opcode_30-OP_T
  dc.w opcode_31-OP_T
  dc.w opcode_32-OP_T
  dc.w opcode_33-OP_T
  dc.w opcode_34-OP_T
  dc.w opcode_35-OP_T
  dc.w opcode_36-OP_T
  dc.w opcode_37-OP_T
  dc.w opcode_38-OP_T
  dc.w opcode_39-OP_T
  dc.w opcode_3a-OP_T
  dc.w opcode_3b-OP_T
  dc.w opcode_3c-OP_T
  dc.w opcode_3d-OP_T
  dc.w opcode_3e-OP_T
  dc.w opcode_3f-OP_T
  dc.w opcode_40-OP_T
  dc.w opcode_41-OP_T
  dc.w opcode_42-OP_T
  dc.w opcode_43-OP_T
  dc.w opcode_44-OP_T
  dc.w opcode_45-OP_T
  dc.w opcode_46-OP_T
  dc.w opcode_47-OP_T
  dc.w opcode_48-OP_T
  dc.w opcode_49-OP_T
  dc.w opcode_4a-OP_T
  dc.w opcode_4b-OP_T
  dc.w opcode_4c-OP_T
  dc.w opcode_4d-OP_T
  dc.w opcode_4e-OP_T
  dc.w opcode_4f-OP_T
  dc.w opcode_50-OP_T
  dc.w opcode_51-OP_T
  dc.w opcode_52-OP_T
  dc.w opcode_53-OP_T
  dc.w opcode_54-OP_T
  dc.w opcode_55-OP_T
  dc.w opcode_56-OP_T
  dc.w opcode_57-OP_T
  dc.w opcode_58-OP_T
  dc.w opcode_59-OP_T
  dc.w opcode_5a-OP_T
  dc.w opcode_5b-OP_T
  dc.w opcode_5c-OP_T
  dc.w opcode_5d-OP_T
  dc.w opcode_5e-OP_T
  dc.w opcode_5f-OP_T
  dc.w opcode_60-OP_T
  dc.w opcode_61-OP_T
  dc.w opcode_62-OP_T
  dc.w opcode_63-OP_T
  dc.w opcode_64-OP_T
  dc.w opcode_65-OP_T
  dc.w opcode_66-OP_T
  dc.w opcode_67-OP_T
  dc.w opcode_68-OP_T
  dc.w opcode_69-OP_T
  dc.w opcode_6a-OP_T
  dc.w opcode_6b-OP_T
  dc.w opcode_6c-OP_T
  dc.w opcode_6d-OP_T
  dc.w opcode_6e-OP_T
  dc.w opcode_6f-OP_T
  dc.w opcode_70-OP_T
  dc.w opcode_71-OP_T
  dc.w opcode_72-OP_T
  dc.w opcode_73-OP_T
  dc.w opcode_74-OP_T
  dc.w opcode_75-OP_T
  dc.w opcode_76-OP_T
  dc.w opcode_77-OP_T
  dc.w opcode_78-OP_T
  dc.w opcode_79-OP_T
  dc.w opcode_7a-OP_T
  dc.w opcode_7b-OP_T
  dc.w opcode_7c-OP_T
  dc.w opcode_7d-OP_T
  dc.w opcode_7e-OP_T
  dc.w opcode_7f-OP_T
  dc.w opcode_80-OP_T
  dc.w opcode_81-OP_T
  dc.w opcode_82-OP_T
  dc.w opcode_83-OP_T
  dc.w opcode_84-OP_T
  dc.w opcode_85-OP_T
  dc.w opcode_86-OP_T
  dc.w opcode_87-OP_T
  dc.w opcode_88-OP_T
  dc.w opcode_89-OP_T
  dc.w opcode_8a-OP_T
  dc.w opcode_8b-OP_T
  dc.w opcode_8c-OP_T
  dc.w opcode_8d-OP_T
  dc.w opcode_8e-OP_T
  dc.w opcode_8f-OP_T
  dc.w opcode_90-OP_T
  dc.w opcode_91-OP_T
  dc.w opcode_92-OP_T
  dc.w opcode_93-OP_T
  dc.w opcode_94-OP_T
  dc.w opcode_95-OP_T
  dc.w opcode_96-OP_T
  dc.w opcode_97-OP_T
  dc.w opcode_98-OP_T
  dc.w opcode_99-OP_T
  dc.w opcode_9a-OP_T
  dc.w opcode_9b-OP_T
  dc.w opcode_9c-OP_T
  dc.w opcode_9d-OP_T
  dc.w opcode_9e-OP_T
  dc.w opcode_9f-OP_T
  dc.w opcode_a0-OP_T
  dc.w opcode_a1-OP_T
  dc.w opcode_a2-OP_T
  dc.w opcode_a3-OP_T
  dc.w opcode_a4-OP_T
  dc.w opcode_a5-OP_T
  dc.w opcode_a6-OP_T
  dc.w opcode_a7-OP_T
  dc.w opcode_a8-OP_T
  dc.w opcode_a9-OP_T
  dc.w opcode_aa-OP_T
  dc.w opcode_ab-OP_T
  dc.w opcode_ac-OP_T
  dc.w opcode_ad-OP_T
  dc.w opcode_ae-OP_T
  dc.w opcode_af-OP_T
  dc.w opcode_b0-OP_T
  dc.w opcode_b1-OP_T
  dc.w opcode_b2-OP_T
  dc.w opcode_b3-OP_T
  dc.w opcode_b4-OP_T
  dc.w opcode_b5-OP_T
  dc.w opcode_b6-OP_T
  dc.w opcode_b7-OP_T
  dc.w opcode_b8-OP_T
  dc.w opcode_b9-OP_T
  dc.w opcode_ba-OP_T
  dc.w opcode_bb-OP_T
  dc.w opcode_bc-OP_T
  dc.w opcode_bd-OP_T
  dc.w opcode_be-OP_T
  dc.w opcode_bf-OP_T
  dc.w opcode_c0-OP_T
  dc.w opcode_c1-OP_T
  dc.w opcode_c2-OP_T
  dc.w opcode_c3-OP_T
  dc.w opcode_c4-OP_T
  dc.w opcode_c5-OP_T
  dc.w opcode_c6-OP_T
  dc.w opcode_c7-OP_T
  dc.w opcode_c8-OP_T
  dc.w opcode_c9-OP_T
  dc.w opcode_ca-OP_T
  dc.w opcode_cb-OP_T
  dc.w opcode_cc-OP_T
  dc.w opcode_cd-OP_T
  dc.w opcode_ce-OP_T
  dc.w opcode_cf-OP_T
  dc.w opcode_d0-OP_T
  dc.w opcode_d1-OP_T
  dc.w opcode_d2-OP_T
  dc.w opcode_d3-OP_T
  dc.w opcode_d4-OP_T
  dc.w opcode_d5-OP_T
  dc.w opcode_d6-OP_T
  dc.w opcode_d7-OP_T
  dc.w opcode_d8-OP_T
  dc.w opcode_d9-OP_T
  dc.w opcode_da-OP_T
  dc.w opcode_db-OP_T
  dc.w opcode_dc-OP_T
  dc.w opcode_dd-OP_T
  dc.w opcode_de-OP_T
  dc.w opcode_df-OP_T
  dc.w opcode_e0-OP_T
  dc.w opcode_e1-OP_T
  dc.w opcode_e2-OP_T
  dc.w opcode_e3-OP_T
  dc.w opcode_e4-OP_T
  dc.w opcode_e5-OP_T
  dc.w opcode_e6-OP_T
  dc.w opcode_e7-OP_T
  dc.w opcode_e8-OP_T
  dc.w opcode_e9-OP_T
  dc.w opcode_ea-OP_T
  dc.w opcode_eb-OP_T
  dc.w opcode_ec-OP_T
  dc.w opcode_ed-OP_T
  dc.w opcode_ee-OP_T
  dc.w opcode_ef-OP_T
  dc.w opcode_f0-OP_T
  dc.w opcode_f1-OP_T
  dc.w opcode_f2-OP_T
  dc.w opcode_f3-OP_T
  dc.w opcode_f4-OP_T
  dc.w opcode_f5-OP_T
  dc.w opcode_f6-OP_T
  dc.w opcode_f7-OP_T
  dc.w opcode_f8-OP_T
  dc.w opcode_f9-OP_T
  dc.w opcode_fa-OP_T
  dc.w opcode_fb-OP_T
  dc.w opcode_fc-OP_T
  dc.w opcode_fd-OP_T
  dc.w opcode_fe-OP_T
  dc.w opcode_ff-OP_T

OPMODE_TABLE_D:
OP_T_D:
  dc.w opcode_00-OP_T_D
  dc.w opcode_01-OP_T_D
  dc.w opcode_02-OP_T_D
  dc.w opcode_03-OP_T_D
  dc.w opcode_04-OP_T_D
  dc.w opcode_05-OP_T_D
  dc.w opcode_06-OP_T_D
  dc.w opcode_07-OP_T_D
  dc.w opcode_08-OP_T_D
  dc.w opcode_09-OP_T_D
  dc.w opcode_0a-OP_T_D
  dc.w opcode_0b-OP_T_D
  dc.w opcode_0c-OP_T_D
  dc.w opcode_0d-OP_T_D
  dc.w opcode_0e-OP_T_D
  dc.w opcode_0f-OP_T_D
  dc.w opcode_10-OP_T_D
  dc.w opcode_11-OP_T_D
  dc.w opcode_12-OP_T_D
  dc.w opcode_13-OP_T_D
  dc.w opcode_14-OP_T_D
  dc.w opcode_15-OP_T_D
  dc.w opcode_16-OP_T_D
  dc.w opcode_17-OP_T_D
  dc.w opcode_18-OP_T_D
  dc.w opcode_19-OP_T_D
  dc.w opcode_1a-OP_T_D
  dc.w opcode_1b-OP_T_D
  dc.w opcode_1c-OP_T_D
  dc.w opcode_1d-OP_T_D
  dc.w opcode_1e-OP_T_D
  dc.w opcode_1f-OP_T_D
  dc.w opcode_20-OP_T_D
  dc.w opcode_21-OP_T_D
  dc.w opcode_22-OP_T_D
  dc.w opcode_23-OP_T_D
  dc.w opcode_24-OP_T_D
  dc.w opcode_25-OP_T_D
  dc.w opcode_26-OP_T_D
  dc.w opcode_27-OP_T_D
  dc.w opcode_28-OP_T_D
  dc.w opcode_29-OP_T_D
  dc.w opcode_2a-OP_T_D
  dc.w opcode_2b-OP_T_D
  dc.w opcode_2c-OP_T_D
  dc.w opcode_2d-OP_T_D
  dc.w opcode_2e-OP_T_D
  dc.w opcode_2f-OP_T_D
  dc.w opcode_30-OP_T_D
  dc.w opcode_31-OP_T_D
  dc.w opcode_32-OP_T_D
  dc.w opcode_33-OP_T_D
  dc.w opcode_34-OP_T_D
  dc.w opcode_35-OP_T_D
  dc.w opcode_36-OP_T_D
  dc.w opcode_37-OP_T_D
  dc.w opcode_38-OP_T_D
  dc.w opcode_39-OP_T_D
  dc.w opcode_3a-OP_T_D
  dc.w opcode_3b-OP_T_D
  dc.w opcode_3c-OP_T_D
  dc.w opcode_3d-OP_T_D
  dc.w opcode_3e-OP_T_D
  dc.w opcode_3f-OP_T_D
  dc.w opcode_40-OP_T_D
  dc.w opcode_41-OP_T_D
  dc.w opcode_42-OP_T_D
  dc.w opcode_43-OP_T_D
  dc.w opcode_44-OP_T_D
  dc.w opcode_45-OP_T_D
  dc.w opcode_46-OP_T_D
  dc.w opcode_47-OP_T_D
  dc.w opcode_48-OP_T_D
  dc.w opcode_49-OP_T_D
  dc.w opcode_4a-OP_T_D
  dc.w opcode_4b-OP_T_D
  dc.w opcode_4c-OP_T_D
  dc.w opcode_4d-OP_T_D
  dc.w opcode_4e-OP_T_D
  dc.w opcode_4f-OP_T_D
  dc.w opcode_50-OP_T_D
  dc.w opcode_51-OP_T_D
  dc.w opcode_52-OP_T_D
  dc.w opcode_53-OP_T_D
  dc.w opcode_54-OP_T_D
  dc.w opcode_55-OP_T_D
  dc.w opcode_56-OP_T_D
  dc.w opcode_57-OP_T_D
  dc.w opcode_58-OP_T_D
  dc.w opcode_59-OP_T_D
  dc.w opcode_5a-OP_T_D
  dc.w opcode_5b-OP_T_D
  dc.w opcode_5c-OP_T_D
  dc.w opcode_5d-OP_T_D
  dc.w opcode_5e-OP_T_D
  dc.w opcode_5f-OP_T_D
  dc.w opcode_60-OP_T_D
  dc.w opcode_61_D-OP_T_D
  dc.w opcode_62-OP_T_D
  dc.w opcode_63-OP_T_D
  dc.w opcode_64-OP_T_D
  dc.w opcode_65_D-OP_T_D
  dc.w opcode_66-OP_T_D
  dc.w opcode_67-OP_T_D
  dc.w opcode_68-OP_T_D
  dc.w opcode_69_D-OP_T_D
  dc.w opcode_6a-OP_T_D
  dc.w opcode_6b-OP_T_D
  dc.w opcode_6c-OP_T_D
  dc.w opcode_6d_D-OP_T_D
  dc.w opcode_6e-OP_T_D
  dc.w opcode_6f-OP_T_D
  dc.w opcode_70-OP_T_D
  dc.w opcode_71_D-OP_T_D
  dc.w opcode_72-OP_T_D
  dc.w opcode_73-OP_T_D
  dc.w opcode_74-OP_T_D
  dc.w opcode_75_D-OP_T_D
  dc.w opcode_76-OP_T_D
  dc.w opcode_77-OP_T_D
  dc.w opcode_78-OP_T_D
  dc.w opcode_79_D-OP_T_D
  dc.w opcode_7a-OP_T_D
  dc.w opcode_7b-OP_T_D
  dc.w opcode_7c-OP_T_D
  dc.w opcode_7d_D-OP_T_D
  dc.w opcode_7e-OP_T_D
  dc.w opcode_7f-OP_T_D
  dc.w opcode_80-OP_T_D
  dc.w opcode_81-OP_T_D
  dc.w opcode_82-OP_T_D
  dc.w opcode_83-OP_T_D
  dc.w opcode_84-OP_T_D
  dc.w opcode_85-OP_T_D
  dc.w opcode_86-OP_T_D
  dc.w opcode_87-OP_T_D
  dc.w opcode_88-OP_T_D
  dc.w opcode_89-OP_T_D
  dc.w opcode_8a-OP_T_D
  dc.w opcode_8b-OP_T_D
  dc.w opcode_8c-OP_T_D
  dc.w opcode_8d-OP_T_D
  dc.w opcode_8e-OP_T_D
  dc.w opcode_8f-OP_T_D
  dc.w opcode_90-OP_T_D
  dc.w opcode_91-OP_T_D
  dc.w opcode_92-OP_T_D
  dc.w opcode_93-OP_T_D
  dc.w opcode_94-OP_T_D
  dc.w opcode_95-OP_T_D
  dc.w opcode_96-OP_T_D
  dc.w opcode_97-OP_T_D
  dc.w opcode_98-OP_T_D
  dc.w opcode_99-OP_T_D
  dc.w opcode_9a-OP_T_D
  dc.w opcode_9b-OP_T_D
  dc.w opcode_9c-OP_T_D
  dc.w opcode_9d-OP_T_D
  dc.w opcode_9e-OP_T_D
  dc.w opcode_9f-OP_T_D
  dc.w opcode_a0-OP_T_D
  dc.w opcode_a1-OP_T_D
  dc.w opcode_a2-OP_T_D
  dc.w opcode_a3-OP_T_D
  dc.w opcode_a4-OP_T_D
  dc.w opcode_a5-OP_T_D
  dc.w opcode_a6-OP_T_D
  dc.w opcode_a7-OP_T_D
  dc.w opcode_a8-OP_T_D
  dc.w opcode_a9-OP_T_D
  dc.w opcode_aa-OP_T_D
  dc.w opcode_ab-OP_T_D
  dc.w opcode_ac-OP_T_D
  dc.w opcode_ad-OP_T_D
  dc.w opcode_ae-OP_T_D
  dc.w opcode_af-OP_T_D
  dc.w opcode_b0-OP_T_D
  dc.w opcode_b1-OP_T_D
  dc.w opcode_b2-OP_T_D
  dc.w opcode_b3-OP_T_D
  dc.w opcode_b4-OP_T_D
  dc.w opcode_b5-OP_T_D
  dc.w opcode_b6-OP_T_D
  dc.w opcode_b7-OP_T_D
  dc.w opcode_b8-OP_T_D
  dc.w opcode_b9-OP_T_D
  dc.w opcode_ba-OP_T_D
  dc.w opcode_bb-OP_T_D
  dc.w opcode_bc-OP_T_D
  dc.w opcode_bd-OP_T_D
  dc.w opcode_be-OP_T_D
  dc.w opcode_bf-OP_T_D
  dc.w opcode_c0-OP_T_D
  dc.w opcode_c1-OP_T_D
  dc.w opcode_c2-OP_T_D
  dc.w opcode_c3-OP_T_D
  dc.w opcode_c4-OP_T_D
  dc.w opcode_c5-OP_T_D
  dc.w opcode_c6-OP_T_D
  dc.w opcode_c7-OP_T_D
  dc.w opcode_c8-OP_T_D
  dc.w opcode_c9-OP_T_D
  dc.w opcode_ca-OP_T_D
  dc.w opcode_cb-OP_T_D
  dc.w opcode_cc-OP_T_D
  dc.w opcode_cd-OP_T_D
  dc.w opcode_ce-OP_T_D
  dc.w opcode_cf-OP_T_D
  dc.w opcode_d0-OP_T_D
  dc.w opcode_d1-OP_T_D
  dc.w opcode_d2-OP_T_D
  dc.w opcode_d3-OP_T_D
  dc.w opcode_d4-OP_T_D
  dc.w opcode_d5-OP_T_D
  dc.w opcode_d6-OP_T_D
  dc.w opcode_d7-OP_T_D
  dc.w opcode_d8-OP_T_D
  dc.w opcode_d9-OP_T_D
  dc.w opcode_da-OP_T_D
  dc.w opcode_db-OP_T_D
  dc.w opcode_dc-OP_T_D
  dc.w opcode_dd-OP_T_D
  dc.w opcode_de-OP_T_D
  dc.w opcode_df-OP_T_D
  dc.w opcode_e0-OP_T_D
  dc.w opcode_e1_D-OP_T_D
  dc.w opcode_e2-OP_T_D
  dc.w opcode_e3-OP_T_D
  dc.w opcode_e4-OP_T_D
  dc.w opcode_e5_D-OP_T_D
  dc.w opcode_e6-OP_T_D
  dc.w opcode_e7-OP_T_D
  dc.w opcode_e8-OP_T_D
  dc.w opcode_e9_D-OP_T_D
  dc.w opcode_ea-OP_T_D
  dc.w opcode_eb_D-OP_T_D
  dc.w opcode_ec-OP_T_D
  dc.w opcode_ed_D-OP_T_D
  dc.w opcode_ee-OP_T_D
  dc.w opcode_ef-OP_T_D
  dc.w opcode_f0-OP_T_D
  dc.w opcode_f1_D-OP_T_D
  dc.w opcode_f2-OP_T_D
  dc.w opcode_f3-OP_T_D
  dc.w opcode_f4-OP_T_D
  dc.w opcode_f5_D-OP_T_D
  dc.w opcode_f6-OP_T_D
  dc.w opcode_f7-OP_T_D
  dc.w opcode_f8-OP_T_D
  dc.w opcode_f9_D-OP_T_D
  dc.w opcode_fa-OP_T_D
  dc.w opcode_fb-OP_T_D
  dc.w opcode_fc-OP_T_D
  dc.w opcode_fd_D-OP_T_D
  dc.w opcode_fe-OP_T_D
  dc.w opcode_ff-OP_T_D
