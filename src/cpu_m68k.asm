; Original Author :   empty head
; Last changes    :   26th November 2000, gerhard.janka

P65C02 ; we emulate this version of processor (6502 has a bug in jump code,
       ; you can emulate this bug by commenting out this line :)
; PROFILE  ; fills the 'instruction_count' array for instruction profiling
; MONITOR_BREAK  ; jump to monitor at break
; CRASH_MENU  ; enable crash menu output

; TODO :
;
;   CRASH_MENU
;   Changes at inofficial opcodes

  OPT    P=68040,L1,O+,W-
  output cpu_m68k.o

  xref _CART_BountyBob2
  xref _CART_BountyBob1
  xref _GTIA_GetByte
  xref _POKEY_GetByte
  xref _PIA_GetByte
  xref _ANTIC_GetByte
  xref _CART_GetByte
  xref _GTIA_PutByte
  xref _POKEY_PutByte
  xref _PIA_PutByte
  xref _ANTIC_PutByte
  xref _CART_PutByte
  xref _AtariEscape
  xref _break_addr
  xref _wsync_halt ;CPU is stopped
  xref _xpos
  xref _xpos_limit
  xdef _regPC
  xdef _regA
  xdef _regP ;/* Processor Status Byte (Partial) */
  xdef _regS
  xdef _regX
  xdef _regY
  xref _memory
  xref _attrib
  ifd PROFILE
  xref _instruction_count
  endc
  ifd MONITOR_BREAK
  xdef _remember_PC
  xdef _remember_JMP
  xref _break_addr
  xref _break_step
  xref _break_ret
  xref _break_cim
  xref _break_here
  xref _brkhere
  xref _ret_nesting
  endc
  ifd CRASH_MENU
  xref _crash_code
  xref _crash_address
  xref _crash_afterCIM
  endc
  xdef _IRQ
  xdef _NMI
  xdef _RTI
  xdef _GO
  xdef _CPUGET
  xdef _CPUPUT
  xdef _CPU_INIT
  xdef _cycles ;temporarily needed outside :)

  ifd MONITOR_BREAK

rem_pc_steps  equ 16  ; has to be equal to REMEMBER_PC_STEPS
rem_jmp_steps equ 16  ; has to be equal to REMEMBER_JMP_STEPS

remember_PC
_remember_PC
  ds.w rem_pc_steps   ;REMEMBER_PC_STEPS

remember_JMP
_remember_JMP
  ds.w rem_jmp_steps  ;REMEMBER_JMP_STEPS

  endc

regA
  ds.b 1
_regA  ds.b 1   ;d2 -A

regX
  ds.b 1
_regX  ds.b 1   ;d3 -X

regY
  ds.b 1
_regY  ds.b 1   ;d4 -Y

regPC
_regPC  ds.w 1   ;a2 PC

regS
  ds.b 1
_regS  ds.b 1   ;a4 stack

regP  ds.b 1
BDIFLAG         ;same as regP
_regP  ds.b 1   ;   -CCR

IRQ
_IRQ  ds.b 1

  even

memory_pointer equr a5
attrib_pointer equr a3
stack_pointer equr a4
PC6502  equr a2

CD  equr d6 ; cycles counter up
ZFLAG  equr d1 ; Bit 0..7
NFLAG  equr d1 ; Bit 8..15
VFLAG  equr d5 ; Bit 31
CCR6502  equr d5 ; Bit 0..15, only carry is usable (Extended on 68000)
A  equr d2
X  equr d3
Y  equr d4

;d0  contains usually adress where we are working or temporary value
;d7  contains is a working register or adress

LoHi  macro    ;change order of lo and hi byte (address)
      ror.w #8,\1
      endm

;  ==========================================================
;  Emulated Registers and Flags are kept local to this module
;  ==========================================================

;#define UPDATE_GLOBAL_REGS regPC=PC;regA=A;regX=X;regY=Y
UPDATE_GLOBAL_REGS  macro
  sub.l memory_pointer,PC6502
  movem.w d2-d4/a2,regA ;d2-d4 (A,X,Y) a2 (regPC)
  endm

;#define UPDATE_LOCAL_REGS PC=regPC;A=regA;X=regX;Y=regY
UPDATE_LOCAL_REGS macro
  clr.l   d7
  move.w  regA,d2
  move.w  regX,d3
  move.w  regY,d4
; lea     $100(memory_pointer),stack_pointer
  move.w  regPC,d7
  move.l  memory_pointer,PC6502
  add.l   d7,PC6502
  endm

_Local_GetByte:
  move.l d7,d1
  clr.l  d0
  move.b d1,d0
  lsr.w  #8,d1
  sub.l  #$c0,d1
  bmi    CouldBeBob
  jmp    ([GetTable,PC,d1.l*4])

GetTable:
  dc.l GetGTIA5,GetNone,GetNone,GetNone        ; c0..3
  dc.l GetNone,GetNone,GetNone,GetNone         ; c4..7
  dc.l GetNone,GetNone,GetNone,GetNone         ; c8..b
  dc.l GetNone,GetNone,GetNone,GetNone         ; cc..f
  dc.l GetGTIA8,GetNone,GetPOKEY8,GetPIA8      ; d0..3
  dc.l GetANTIC8,GetRTIME8,GetNone,GetNone     ; d4..7
  dc.l GetNone,GetNone,GetNone,GetNone         ; d8..b
  dc.l GetNone,GetNone,GetNone,GetNone         ; dc..f
  dc.l GetNone,GetNone,GetNone,GetNone         ; e0..3
  dc.l GetNone,GetNone,GetNone,GetNone         ; e4..7
  dc.l GetPOKEY5,GetNone,GetNone,GetPOKEY5     ; e8..b
  dc.l GetNone,GetNone,GetNone,GetNone         ; ec..f
  dc.l GetNone,GetNone,GetNone,GetNone         ; f0..3
  dc.l GetNone,GetNone,GetNone,GetNone         ; f4..7
  dc.l GetNone,GetNone,GetNone,GetNone         ; f8..b
  dc.l GetNone,GetNone,GetNone,GetNone         ; fc..f

CouldBeBob:
  add.l  #$c0,d1
  cmp.w  #$4f,d1
  beq    ItsBob1
  cmp.w  #$5f,d1
  bne    GetNone
ItsBob2:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CART_BountyBob2
  addq.l #4,a7
  clr.l  d0
  rts
ItsBob1:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CART_BountyBob1
  addq.l #4,a7
  clr.l  d0
  rts
GetNone:
  move.l #255,d0
  rts
GetGTIA8:
GetGTIA5:
  move.l d0,-(a7)
  jsr    _GTIA_GetByte
  addq.l #4,a7
  rts
GetPOKEY8:
GetPOKEY5:
  move.l d0,-(a7)
  jsr    _POKEY_GetByte
  addq.l #4,a7
  rts
GetPIA8:
  move.l d0,-(a7)
  jsr    _PIA_GetByte
  addq.l #4,a7
  rts
GetANTIC8:
  move.l d0,-(a7)
  jsr    _ANTIC_GetByte
  addq.l #4,a7
  rts
GetRTIME8:
  move.l d0,-(a7)
  jsr    _CART_GetByte
  addq.l #4,a7
  rts

_Local_PutByte:
  clr.l  d1
  move.w d7,d1
  lsr.w  #8,d1
  sub.l  #$c0,d1
  bmi    CouldBeBobP
  jmp    ([PutTable,PC,d1.l*4])

PutTable:
  dc.l PutGTIA5,PutNone,PutNone,PutNone        ; c0..3
  dc.l PutNone,PutNone,PutNone,PutNone         ; c4..7
  dc.l PutNone,PutNone,PutNone,PutNone         ; c8..b
  dc.l PutNone,PutNone,PutNone,PutNone         ; cc..f
  dc.l PutGTIA8,PutNone,PutPOKEY8,PutPIA8      ; d0..3
  dc.l PutANTIC8,PutSCart,PutNone,PutNone      ; d4..7
  dc.l PutNone,PutNone,PutNone,PutNone         ; d8..b
  dc.l PutNone,PutNone,PutNone,PutNone         ; dc..f
  dc.l PutNone,PutNone,PutNone,PutNone         ; e0..3
  dc.l PutNone,PutNone,PutNone,PutNone         ; e4..7
  dc.l PutPOKEY5,PutNone,PutNone,PutPOKEY5     ; e8..b
  dc.l PutNone,PutNone,PutNone,PutNone         ; ec..f
  dc.l PutNone,PutNone,PutNone,PutNone         ; f0..3
  dc.l PutNone,PutNone,PutNone,PutNone         ; f4..7
  dc.l PutNone,PutNone,PutNone,PutNone         ; f8..b
  dc.l PutNone,PutNone,PutNone,PutNone         ; fc..f

CouldBeBobP:
  add.l  #$c0,d1
  cmp.w  #$4f,d1
  beq    ItsBob1P
  cmp.w  #$5f,d1
  bne    PutNone
ItsBob2P:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CART_BountyBob2
  addq.l #4,a7
  clr.l  d0
  rts
ItsBob1P:
  move.w d7,-(a7)
  clr.w  -(a7)
  jsr    _CART_BountyBob1
  addq.l #4,a7
  clr.l  d0
  rts
PutNone:
  clr.l  d0
  rts
PutGTIA8:
PutGTIA5:
  clr.l  -(a7)
  move.b d0,3(a7)
  clr.l  -(a7)
  move.b d7,3(a7)
  jsr    _GTIA_PutByte
  addq.l #8,a7
  rts
PutPOKEY8:
PutPOKEY5:
  clr.l  -(a7)
  move.b d0,3(a7)
  clr.l  -(a7)
  move.b d7,3(a7)
  jsr    _POKEY_PutByte
  addq.l #8,a7
  rts
PutPIA8:
  clr.l  -(a7)
  move.b d0,3(a7)
  clr.l  -(a7)
  move.b d7,3(a7)
  jsr    _PIA_PutByte
  addq.l #8,a7
  rts
PutANTIC8:
  clr.l  -(a7)
  move.b d0,3(a7)
  clr.l  -(a7)
  move.b d7,3(a7)
  move.l CD,_xpos
  jsr    _ANTIC_PutByte
  move.l _xpos,CD
  addq.l #8,a7
  rts
PutSCart:
  clr.l  -(a7)
  move.b d0,3(a7)
  clr.l  -(a7)
  move.b d7,3(a7)
  jsr    _CART_PutByte
  addq.l #8,a7
  rts

EXE_GETBYTE macro
; move.l d7,-(a7)
  jsr    _Local_GetByte
; addq.l #4,a7 ;put stack onto right place
  endm

EXE_PUTBYTE macro
; clr.l  -(a7)
; move.b \1,3(a7) ;byte
  move.b \1,d0
  jsr    _Local_PutByte
; addq.l #8,a7
  endm

EXE_PUTBYTE_d0 macro
; clr.l  -(a7)
; move.b d0,3(a7) ;byte
  jsr    _Local_PutByte
; addq.l #8,a7
  endm

_CPU_INIT:
  rts

;these are bit in MC68000 CCR register
NB68  equ 3
EB68  equ 4 ;used as a carry in 6502 emulation
ZB68  equ 2
OB68  equ 1
CB68  equ 0

WSYNC_C equ 110

N_FLAG equ $80
N_FLAGN equ $7f
N_FLAGB equ 7
V_FLAG equ $40
V_FLAGN equ $bf
V_FLAGB equ 6
G_FLAG equ $20
G_FLAGB equ $5
B_FLAG equ $10
B_FLAGN equ $ef
B_FLAGB equ 4
D_FLAG equ $08
D_FLAGN equ $f7
D_FLAGB equ 3
I_FLAG equ $04
I_FLAGN equ $fb
I_FLAGB equ 2
Z_FLAG equ $02
Z_FLAGN equ $fd
Z_FLAGB equ 1
C_FLAG equ $01
C_FLAGN equ $fe
C_FLAGB equ 0
VCZN_FLAGS equ $c3

SetI  macro
  ori.b  #I_FLAG,_regP
  endm

ClrI  macro
  andi.b #I_FLAGN,_regP
  endm

SetB  macro
  ori.b  #B_FLAG,_regP
  endm

SetD  macro
  ori.b  #D_FLAG,_regP
  endm

ClrD  macro
  andi.b #D_FLAGN,_regP
  endm

;static UBYTE  N;  /* bit7 zero (0) or bit 7 non-zero (1) */
;static UBYTE  Z;  /* zero (0) or non-zero (1) */
;static UBYTE  V;
;static UBYTE  C;  /* zero (0) or one(1) */

isRAM      equ 0
isROM      equ 1
isHARDWARE equ 2

;/*
; * The following array is used for 6502 instruction profiling
; */

;int instruction_count[256];

;UBYTE memory[65536];
;UBYTE attrib[65536];

;/*
;  ===============================================================
;  Z flag: This actually contains the result of an operation which
;    would modify the Z flag. The value is tested for
;    equality by the BEQ and BNE instruction.
;  ===============================================================
;*/

; Bit    : 76543210
; CCR6502: ***XNZVC
; _RegP  : NV*BDIZC

ConvertSTATUS_RegP macro
  move.b _regP,\1 ;put flag BDI into d0
  ori.b  #VCZN_FLAGS,\1 ; set overflow, carry, zero & negative flag
  btst   #EB68,CCR6502
  bne.s  .SETC\@
  andi.b #C_FLAGN,\1
.SETC\@
  tst.w  NFLAG
  bmi.s  .SETN\@
  andi.b #N_FLAGN,\1
.SETN\@
  tst.b  ZFLAG
  beq.s  .SETZ\@   ; beware! reverse compare is ok
  andi.b #Z_FLAGN,\1
.SETZ\@
  tst.l  VFLAG
  bmi.s  .SETV\@        ; !!!
  andi.b #V_FLAGN,\1
.SETV\@
  move.b \1,_regP       ;put result to _regP ! TEST
  endm

ConvertRegP_STATUS macro
  move.b _regP,\1
  clr.l  VFLAG          ; !!!
  btst   #V_FLAGB,\1
  beq.s  .SETV\@
  not.l  VFLAG          ; much faster than setting bit
.SETV\@
  btst   #C_FLAGB,\1
  sne    CCR6502
  move.b \1,NFLAG
  lsl.w  #8,NFLAG  ; sets NFLAG and clears ZFLAG
  btst   #Z_FLAGB,\1
  seq    ZFLAG
  endm

CPU_GetStatus macro
  ConvertSTATUS_RegP \1
; move.b \1,_regP
  endm

CPU_PutStatus macro
  ConvertRegP_STATUS \1
  endm

PHP  macro
  clr.l  \2
  move.b _regS,\2
  ConvertSTATUS_RegP \1
  move.b \1,(stack_pointer,\2.l)
  subq.b #1,\2
  move.b \2,_regS
  endm

PLP  macro
  clr.l  \2
  move.b _regS,\2
  addq.b #1,\2
; move.b (stack_pointer,\2.l),_regP
  move.b (stack_pointer,\2.l),\1  ; TEST
  ori.b  #$30,\1                  ; TEST
  move.b \1,_regP                 ; TEST
  ConvertRegP_STATUS \1
  move.b \2,_regS
  endm

PHB  macro
  clr.l  \2
  move.b _regS,\2
  move.b \1,(stack_pointer,\2.l)
  subq.b #1,\2
  move.b \2,_regS
  endm

PLB  macro
  clr.l  \2
  move.b _regS,\2
  addq.b #1,\2
  move.b (stack_pointer,\2.l),\1
  move.b \2,_regS
  endm

PHW  macro
  clr.l  \2
  move.b _regS,\2
  subq.b #1,\2     ; wrong way around
  move.b \1,(stack_pointer,\2.l)
  addq.b #1,\2
  LoHi \1
  move.b \1,(stack_pointer,\2.l)
  subq.b #2,\2
  move.b \2,_regS
  endm

PLW  macro
  clr.l  \2
  move.b _regS,\2
  addq.b #2,\2     ; wrong way around
  move.b (stack_pointer,\2.l),\1
  asl.w  #8,\1
  subq.b #1,\2
  or.b   (stack_pointer,\2.l),\1
  addq.b #1,\2
  move.b \2,_regS
  endm

PHW_AND_P macro
  clr.l  \2
  move.b _regS,\2
  subq.b #1,\2     ; wrong way around
  move.b \1,(stack_pointer,\2.l)
  addq.b #1,\2
  LoHi \1
  move.b \1,(stack_pointer,\2.l)
  subq.b #2,\2
  ConvertSTATUS_RegP \1
  move.b \1,(stack_pointer,\2.l)
  subq.b #1,\2
  move.b \2,_regS
  endm

PHW_AND_P_B0  macro
  clr.l  \2
  move.b _regS,\2
  subq.b #1,\2     ; wrong way around
  move.b \1,(stack_pointer,\2.l)
  addq.b #1,\2
  LoHi \1
  move.b \1,(stack_pointer,\2.l)
  subq.b #2,\2
  ConvertSTATUS_RegP \1
  andi.b #B_FLAGN,\1              ; TEST
  move.b \1,(stack_pointer,\2.l)
  subq.b #1,\2
  move.b \2,_regS
  endm

PLP_AND_W macro
  clr.l  \2
  move.b _regS,\2
  addq.b #1,\2
; move.b (stack_pointer,\2.l),_regP
  move.b (stack_pointer,\2.l),\1  ; TEST
  ori.b  #$30,\1                  ; TEST
  move.b \1,_regP                 ; TEST
  ConvertRegP_STATUS \1
  addq.b #2,\2     ; wrong way around
  move.b (stack_pointer,\2.l),\1
  asl.w  #8,\1
  subq.b #1,\2
  or.b   (stack_pointer,\2.l),\1
  addq.b #1,\2
  move.b \2,_regS
  endm

CPUCHECKIRQ macro
  tst.b  _IRQ
  beq.w  NEXTCHANGE_WITHOUT
  move.b _regP,d0
  andi.b #I_FLAG,d0
  bne.w  NEXTCHANGE_WITHOUT
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  PHW_AND_P_B0 d7,d0              ; TEST
  SetI
  move.w (memory_pointer,$fffe.l),d7
  LoHi d7
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT
  endm

CPUGET:
_CPUGET:
  CPU_GetStatus d0
  rts

CPUPUT:
_CPUPUT:
  CPU_PutStatus d0
  rts

NMI:
_NMI:
  lea    _memory,a0
  lea    $100(a0),a1
  clr.l  d1
  move.b _regS,d1
  move.b _regPC,(a1,d1)
  subq.b #1,d1
  move.b _regPC+1,(a1,d1)
  subq.b #1,d1
; move.b _regP,(a1,d1)  ;put P onto stack
  move.b _regP,d0       ; Test
  andi.b #B_FLAGN,d0    ; Test
  move.b d0,(a1,d1)     ; Test
  subq.b #1,d1
  move.b d1,_regS
  SetI
  ;put regPC & Stack pointer adress on its place
  move.w (a0,$fffa.l),d1
  LoHi d1
  move.w d1,_regPC
  addq.l #7,_xpos
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  rts

_GO: ;cycles (d0)

;  UWORD PC;
;  UBYTE S;
;  UBYTE A;
;  UBYTE X;
;  UBYTE Y;
;
;  UWORD  addr;
;  UBYTE  data;

;/*
;   This used to be in the main loop but has been removed to improve
;   execution speed. It does not seem to have any adverse effect on
;   the emulation for two reasons:-
;
;   1. NMI's will can only be raised in atari_custom.c - there is
;      no way an NMI can be generated whilst in this routine.
;
;   2. The timing of the IRQs are not that critical.
;*/

  move.l  4(a7),d0
  tst.b   _wsync_halt
  beq.s   NO_WS_HALT
  moveq.l #WSYNC_C-1,d1  ; TEST : no -1 if bpl.s
  cmp.l   d0,d1
; bpl.s   TERM_GO        ; TEST
  bge     TERM_GO        ; TEST
  subq.l  #1,d1          ; TEST : not necessary if bpl.s
  move.l  d1,_xpos
  clr.b   _wsync_halt
NO_WS_HALT:
  move.l  d0,_xpos_limit ;  needed for WSYNC store inside ANTIC
  movem.l d2-d7/a2-a5,-(a7)
  move.l  _xpos,CD
  lea     _memory,memory_pointer
  lea     $100(memory_pointer),stack_pointer
  UPDATE_LOCAL_REGS
  ConvertRegP_STATUS d0
  lea     _attrib,attrib_pointer
  tst.b   _IRQ          ; CPUCHECKIRQ
  beq     NEXTCHANGE_WITHOUT
  move.b  _regP,d0
  and.b   #I_FLAG,d0 ;is interrupt active
  bne     NEXTCHANGE_WITHOUT ;yes, no other interrupt
  clr.l   d0            ; push PC and P to stack ( PHW + PHB ) start
  move.b  _regS,d0
  move.b  _regPC,(stack_pointer,d0.l)
  subq.b  #1,d0
  move.b  _regPC+1,(stack_pointer,d0.l)
  subq.b  #1,d0
; move.b  _regP,(stack_pointer,d0.l) ;put P onto stack
  move.b  _regP,d7                 ; TEST
  andi.b  #B_FLAGN,d7              ; TEST
  move.b  d7,(stack_pointer,d0.l)  ; TEST
  subq.b  #1,d0
  move.b  d0,_regS      ; push PC and P to stack ( PHW + PHB ) end
  SetI
  move.w  (memory_pointer,$fffe.l),d0  ; d0 already cleared from before
  LoHi d0
  move.l  d0,PC6502
  add.l   memory_pointer,PC6502
  addq.l #7,CD
  clr.b   _IRQ ;clear interrupt.....
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  bra     NEXTCHANGE_WITHOUT

;/*
;   =====================================
;   Extract Address if Required by Opcode
;   =====================================
;*/

;d0 contains final value for use in program

; addressing macros

NCYCLES_XY macro
  cmp.b  \1,d7 ; if ( (UBYTE) addr < X,Y ) ncycles++;
; bpl.s  .NCY_XY_NC\@
  bcc.s  .NCY_XY_NC\@             ; !!!
  addq.l #1,CD
.NCY_XY_NC\@:
  endm

ABSOLUTE macro
  move.w (PC6502)+,d7
  LoHi d7 ;d7 contains reversed value
  endm

ABSOLUTE_X macro
  ABSOLUTE
  add.w  X,d7
  endm

ABSOLUTE_X_NCY macro
  ABSOLUTE_X \1
  NCYCLES_XY X
  endm

ABSOLUTE_Y macro
  ABSOLUTE
  add.w  Y,d7
  endm

ABSOLUTE_Y_NCY macro
  ABSOLUTE_Y \1
; addq.l #\1,CD
  NCYCLES_XY Y
  endm

IMMEDIATE macro
  move.b (PC6502)+,\1
  endm

INDIRECT_X macro
  move.b (PC6502)+,d7
  add.b  X,d7
  move.w (memory_pointer,d7.l),d7
  LoHi d7
  endm

INDIRECT_Y macro
  move.b (PC6502)+,d7
  move.w (memory_pointer,d7.l),d7
  LoHi d7      ;swap bytes
  add.w  Y,d7
  endm

INDIRECT_Y_NCY macro
  INDIRECT_Y
; addq.l #\1,CD
  NCYCLES_XY Y
  endm

ZPAGE macro
  move.b (PC6502)+,d7
  endm

ZPAGE_X macro
  move.b (PC6502)+,d7
  add.b  X,d7
  endm

ZPAGE_Y macro
  move.b (PC6502)+,d7
  add.b  Y,d7
  endm

; branching macros

NEXTCHANGE_REG macro
  move.b \1,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

DONT_BRA macro
  addq.l #1,PC6502
  addq.l #cy_Bcc,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

JMP_C macro
  move.w (PC6502)+,d7
  LoHi d7   ;(in d7 adress where we want to jump)
  lea (memory_pointer,d7.l),PC6502
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

; get byte macros

LOAD_IMMEDIATE macro
  move.b (PC6502)+,\2
  move.b \2,ZFLAG
  ext.w  NFLAG
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

LOAD_ZPAGE macro
  move.b (PC6502)+,d7
  move.b (memory_pointer,d7.l),\2 ;get byte
  move.b \2,ZFLAG
  ext.w  NFLAG
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

GETZPBYTE macro
  move.b (memory_pointer,d7.l),\1 ;get byte
  endm

GETANYBYTE macro
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  bne.s  .Getbyte_RAMROM\@
  EXE_GETBYTE
  move.b d0,\1
  bra.s  .AFTER_READ\@
.Getbyte_RAMROM\@
  move.b (memory_pointer,d7.l),\1 ;get byte
.AFTER_READ\@
  endm

GETANYBYTE_d0 macro
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  bne.s  .Getbyte_RAMROM\@
  EXE_GETBYTE
  bra.s  .AFTER_READ\@
.Getbyte_RAMROM\@
  move.b (memory_pointer,d7.l),d0 ;get byte
.AFTER_READ\@
  endm

LOADANYBYTE macro
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  beq.s  .LoadByte_HW\@
  move.b (memory_pointer,d7.l),\1 ;get byte
  NEXTCHANGE_REG \1
.LoadByte_HW\@
  EXE_GETBYTE
  move.b d0,\1
  NEXTCHANGE_REG \1
  endm

; put byte macros

PUTZPBYTE macro
  move.b \2,(memory_pointer,d7.l)
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

PUTZPBYTE_Z macro
  move.b ZFLAG,(memory_pointer,d7.l)
; addq.l #\1,CD
  bra.w  NEXTCHANGE_N
  endm

STOREANYBYTE macro
  tst.b  (attrib_pointer,d7.l)
  bne.s  .GO_PUTBYTE\@
  move.b \2,(memory_pointer,d7.l)
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
.GO_PUTBYTE\@:
  move.b \2,d0
; addq.l #\1,CD
  bra.w  A800PUTB
  endm

PUTANYBYTE_N macro
  ext.w  NFLAG
  tst.b  (attrib_pointer,d7.l)
  bne.s  .GO_PUTBYTE\@
  move.b ZFLAG,(memory_pointer,d7.l)
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
.GO_PUTBYTE\@:
; addq.l #\1,CD
  bra.w  A800PUTB_Ld0
  endm

PUTANYBYTE macro
  tst.b  (attrib_pointer,d7.l)
  bne.s  .GO_PUTBYTE\@
  move.b d0,(memory_pointer,d7.l)
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
.GO_PUTBYTE\@:
; addq.l #\1,CD
  bra.w  A800PUTB
  endm

A800PUTB_Ld0:
  move.b ZFLAG,d0
A800PUTB:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  A800PUTBE
  move.l d1,-(a7)
  EXE_PUTBYTE_d0
  move.l (a7)+,d1
A800PUTBE:
  bra.w  NEXTCHANGE_WITHOUT

; command macros

ROL_C macro
  move.w CCR6502,CCR
  addx.b \1,\1 ;left
  move.w CCR,CCR6502
  endm

RPW_ROL_C macro
  tst.b  (attrib_pointer,d7.l)
  bne.s  .RPW_NO_RAM_ROL\@
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ROL_C ZFLAG
  move.b ZFLAG,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_N
.RPW_NO_RAM_ROL\@:
  bra.w  RPW_HW_ROL
  endm

RPW_HW_ROL:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  RPW_ROM_ROL
  EXE_GETBYTE
  move.b d0,ZFLAG
  ROL_C ZFLAG
  ext.w  NFLAG
  move.l ZFLAG,-(a7)
  EXE_PUTBYTE ZFLAG
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ROL:
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ROL_C ZFLAG
  bra.w  NEXTCHANGE_N

ROR_C macro
  move.w CCR6502,CCR
  roxr.b #1,\1
  move.w CCR,CCR6502
  endm

RPW_ROR_C macro
  tst.b  (attrib_pointer,d7.l)
  bne.s  .RPW_NO_RAM_ROR\@
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ROR_C ZFLAG
  move.b ZFLAG,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_N
.RPW_NO_RAM_ROR\@:
  bra.w  RPW_HW_ROR
  endm

RPW_HW_ROR:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  RPW_ROM_ROR
  EXE_GETBYTE
  move.b d0,ZFLAG
  ROR_C ZFLAG
  ext.w  NFLAG
  move.l ZFLAG,-(a7)
  EXE_PUTBYTE ZFLAG
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ROR:
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ROR_C ZFLAG
  bra.w  NEXTCHANGE_N

ASL_C macro
  add.b  \1,\1 ;left
  move.w CCR,CCR6502
  endm

RPW_ASL_C macro
  tst.b  (attrib_pointer,d7.l)
  bne.s  .RPW_NO_RAM_ASL\@
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ASL_C ZFLAG
  move.b ZFLAG,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_N
.RPW_NO_RAM_ASL\@:
  bra.w  RPW_HW_ASL
  endm

RPW_HW_ASL:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  RPW_ROM_ASL
  EXE_GETBYTE
  move.b d0,ZFLAG
  ASL_C ZFLAG
  ext.w  NFLAG
  move.l ZFLAG,-(a7)
  EXE_PUTBYTE ZFLAG
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_ASL:
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  ASL_C ZFLAG
  bra.w  NEXTCHANGE_N

LSR_C macro
  lsr.b  #1,\1
  move.w CCR,CCR6502
  endm

RPW_LSR_C macro
  clr.w  NFLAG
  tst.b  (attrib_pointer,d7.l)
  bne.s  .RPW_NO_RAM_LSR\@
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  LSR_C ZFLAG
  move.b ZFLAG,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT
.RPW_NO_RAM_LSR\@:
  bra.w  RPW_HW_LSR
  endm

RPW_HW_LSR:
  cmp.b  #isROM,(attrib_pointer,d7.l)
  beq.s  RPW_ROM_LSR
  EXE_GETBYTE
  move.b d0,ZFLAG
  LSR_C ZFLAG
  move.l ZFLAG,-(a7)
  EXE_PUTBYTE ZFLAG
  move.l (a7)+,ZFLAG
  bra.w  NEXTCHANGE_WITHOUT
RPW_ROM_LSR:
  move.b (memory_pointer,d7.l),ZFLAG ;get byte
  LSR_C ZFLAG
  bra.w  NEXTCHANGE_WITHOUT

ASO_C_CONT macro   ;/* [unofficial - ASL Mem then ORA with Acc] */
  ASL_C d0
  or.b   d0,A
  move.b A,ZFLAG
  ext.w  NFLAG
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW\@
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT
.ROM_OR_HW\@
  cmp.b  #isROM,(attrib_pointer,d7.l)
  bne.w  A800PUTB
  bra.w  NEXTCHANGE_WITHOUT
  endm

LSE_C_CONT macro   ;/* [unofficial - LSR Mem then EOR with A] */
  LSR_C d0
  eor.b  d0,A
  move.b A,ZFLAG
  ext.w  NFLAG
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW\@
  move.b d0,(memory_pointer,d7.l)
  bra.w  NEXTCHANGE_WITHOUT
.ROM_OR_HW\@
  cmp.b  #isROM,(attrib_pointer,d7.l)
  bne.w  A800PUTB
  bra.w  NEXTCHANGE_WITHOUT
  endm

SKB_C_CONT macro   ;/* [unofficial - skip byte] */
  addq.l #1,PC6502
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

SKW_C_CONT macro   ;/* [unofficial - skip word] */
  addq.l #2,PC6502
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

XA1_C_CONT macro   ;/* [unofficial - X AND A AND 1 to Mem] */
  move.b X,d0
  and.b  A,d0
  and.b  #1,d0
  PUTANYBYTE \1
  endm

AXS_C_CONT macro   ;/* [unofficial - Store result A AND X] */
  move.b X,ZFLAG
  and.b  A,ZFLAG
  PUTANYBYTE_N \1
  endm

RLA_C macro        ;/* [unofficial - ROL Mem, then AND with A] */
  ROL_C d0
  move.b d0,ZFLAG
  and.b  A,ZFLAG
  ext.w  NFLAG
  endm

RRA_C_CONT macro   ;/* [unofficial - ROR Mem, then ADC to Acc] */
  ROR_C d0
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW\@
  move.b d0,(memory_pointer,d7.l)
  bra.w  adc
.ROM_OR_HW\@
  cmp.b  #isROM,(attrib_pointer,d7.l)
  bne.w  Putbyte_ADC
  bra.w  adc
  endm

DCM_C_CONT macro   ;/* [unofficial - DEC Mem then CMP with Acc] */
  subq.b #1,d0
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW\@
  move.b d0,(memory_pointer,d7.l)
  bra.w  COMPARE_A
.ROM_OR_HW\@
  cmp.b  #isROM,(attrib_pointer,d7.l)
  bne.w  Putbyte_CMP
  bra.w  COMPARE_A
  endm

INS_C_CONT macro   ;/* [unofficial - INC Mem then SBC with Acc] */
  addq.b #1,d0
  tst.b  (attrib_pointer,d7.l)
  bne.s  .ROM_OR_HW\@
  move.b d0,(memory_pointer,d7.l)
  bra.w  sbc
.ROM_OR_HW\@
  cmp.b  #isROM,(attrib_pointer,d7.l)
  bne.w  Putbyte_SBC
  bra.w  sbc
  endm

OR_IMMEDIATE macro
; addq.l #\1,CD
  or.b   (PC6502)+,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

OR_ZPBYTE macro
  or.b   (memory_pointer,d7.l),A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

OR_ANYBYTE macro
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  beq.s  .Getbyte_HW\@
  or.b   (memory_pointer,d7.l),A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
.Getbyte_HW\@:
  EXE_GETBYTE
  or.b   d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

EOR_C_CONT macro
  eor.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

AND_IMMEDIATE macro
; addq.l #\1,CD
  and.b  (PC6502)+,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

AND_ZPBYTE macro
  and.b  (memory_pointer,d7.l),A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

AND_ANYBYTE macro
  cmp.b  #isHARDWARE,(attrib_pointer,d7.l)
  beq.s  .Getbyte_HW\@
  and.b  (memory_pointer,d7.l),A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
.Getbyte_HW\@:
  EXE_GETBYTE
  and.b  d0,A
  move.b A,ZFLAG
  bra.w  NEXTCHANGE_N
  endm

BIT_C_CONT macro
  btst   #V_FLAGB,ZFLAG
  beq.s  .CLEAR_IT\@
  bset   #31,VFLAG
  ext.w  NFLAG
  and.b  A,ZFLAG
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
.CLEAR_IT\@:
  bclr   #31,VFLAG
  ext.w  NFLAG
  and.b  A,ZFLAG
; addq.l #\1,CD
  bra.w  NEXTCHANGE_WITHOUT
  endm

; opcodes

; inofficial opcodes

;opcode_02:  ;/* CIM [unofficial] */
instr_CIM:
  subq.w #1,PC6502
  addq.l #cy_CIM,CD
  clr.l d7
  ifd MONITOR_BREAK
  move.b #1,_break_cim
  move.b #$ff,d7
  UPDATE_GLOBAL_REGS
  CPU_GetStatus d0
; ifd CRASH_MENU
; ...
; else
  move.l d7,-(a7)
  jsr    _AtariEscape
  addq.l #4,a7
; endc             ;
  CPU_PutStatus d0
  UPDATE_LOCAL_REGS
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_03:  ;/* ASO (ab,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_03,CD
  GETANYBYTE_d0
  ASO_C_CONT

; opcode_04:  ;/* SKB [unofficial] */
instr_SKB:
  addq.l #cy_04,CD
  SKB_C_CONT cy_SKB

opcode_07:  ;/* ASO ZPAGE [unofficial] */
  addq.l #cy_07,CD
  ZPAGE
  GETZPBYTE d0
  ASO_C_CONT

;opcode_0b:  ;/* AND #ab [unofficial] ( at _29 ) */

; opcode_0c: ;/* SKW [unofficial] */
instr_SKW:
  addq.l #cy_0c,CD
  SKW_C_CONT cy_SKW

opcode_0f:  ;/* ASO abcd [unofficial] */
  ABSOLUTE
  addq.l #cy_0f,CD
  GETANYBYTE_d0
  ASO_C_CONT

; opcode_12:  ;/* CIM [unofficial] ( at _02 ) */

opcode_13:  ;/* ASO (ab),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_13,CD
  GETANYBYTE_d0
  ASO_C_CONT

opcode_14:
  addq.l #cy_14,CD
  SKB_C_CONT cy_14

opcode_17:  ;/* ASO zpage,x [unofficial] */
  addq.l #cy_17,CD
  ZPAGE_X
  GETZPBYTE d0
  ASO_C_CONT

; opcode_1a:  ;/* NOP [unofficial] ( at _ea ) */

opcode_1b:  ;/* ASO abcd,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_1b,CD
  GETANYBYTE_d0
  ASO_C_CONT

; opcode_1c:  ;/* SKW [unofficial] ( at _0c ) */

opcode_1f:  ;/* ASO abcd,x [unofficial] */
  ABSOLUTE_X
  addq.l #cy_1f,CD
  GETANYBYTE_d0
  ASO_C_CONT

; opcode_22:  ;/* CIM [unofficial] ( at _02 ) */

opcode_23: ;/* RLA (ab,x) [unofficial] */
  addq.l #cy_23,CD
  INDIRECT_X
  GETANYBYTE_d0
  RLA_C
  PUTANYBYTE cy_23

opcode_27: ;/* RLA ZPAGE [unofficial] */
  addq.l #cy_27,CD
  ZPAGE
  GETZPBYTE d0
  RLA_C
  PUTZPBYTE cy_27,d0

;opcode_2b: ;/* AND #ab [unofficial] ( at _29 ) */

opcode_2f: ;/* RLA abcd [unofficial] */
  addq.l #cy_2f,CD
  ABSOLUTE
  GETANYBYTE_d0
  RLA_C
  PUTANYBYTE cy_2f

; opcode_32:  ;/* CIM [unofficial] ( at _02 ) */

opcode_33: ;/* RLA (ab),y [unofficial] */
  addq.l #cy_33,CD
  INDIRECT_Y
  GETANYBYTE_d0
  RLA_C
  PUTANYBYTE cy_33

opcode_34:
  addq.l #cy_34,CD
  SKB_C_CONT cy_34

opcode_37: ;/* RLA zpage,x [unofficial] */
  addq.l #cy_37,CD
  ZPAGE_X
  GETZPBYTE d0
  RLA_C
  PUTZPBYTE cy_37,d0

; opcode_3a:  ;/* NOP [unofficial] ( at _ea ) */

opcode_3b: ;/* RLA abcd,y [unofficial] */
  addq.l #cy_3b,CD
  ABSOLUTE_Y
  GETANYBYTE_d0
  RLA_C
  PUTANYBYTE cy_3b

; opcode_3c:  ;/* SKW [unofficial] ( at _0c ) */

opcode_3f: ;/* RLA abcd,x [unofficial] */
  addq.l #cy_3f,CD
  ABSOLUTE_X
  GETANYBYTE_d0
  RLA_C
  PUTANYBYTE cy_3f

; opcode_42:  ;/* CIM [unofficial] ( at _02 ) */

opcode_43:  ;/* LSE (ab,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_43,CD
  GETANYBYTE_d0
  LSE_C_CONT

; opcode_44:  ;/* SKB [unofficial] ( at _04 ) */

opcode_47:  ;/* LSE ZPAGE [unofficial] */
  addq.l #cy_47,CD
  ZPAGE
  GETZPBYTE d0
  LSE_C_CONT

opcode_4b:  ;/* ALR #ab [unofficial - Acc AND Data, LSR result] */
  IMMEDIATE ZFLAG
  addq.l #cy_4b,CD
  and.b  A,ZFLAG
  LSR_C ZFLAG
  bra.w  NEXTCHANGE_N

opcode_4f:  ;/* LSE abcd [unofficial] */
  ABSOLUTE
  addq.l #cy_4f,CD
  GETANYBYTE_d0
  LSE_C_CONT

; opcode_52:  ;/* CIM [unofficial] ( at _02 ) */

opcode_53:  ;/* LSE (ab),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_53,CD
  GETANYBYTE_d0
  LSE_C_CONT

opcode_54:
  addq.l #cy_54,CD
  SKB_C_CONT cy_54

opcode_57:  ;/* LSE zpage,x [unofficial] */
  addq.l #cy_57,CD
  ZPAGE_X
  GETZPBYTE d0
  LSE_C_CONT

; opcode_5a:  ;/* NOP [unofficial] ( at _ea ) */

opcode_5b:  ;/* LSE abcd,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_5b,CD
  GETANYBYTE_d0
  LSE_C_CONT

; opcode_5c:  ;/* SKW [unofficial] ( at _0c ) */

opcode_5f:  ;/* LSE abcd,x [unofficial] */
  ABSOLUTE_X
  addq.l #cy_5f,CD
  GETANYBYTE_d0
  LSE_C_CONT

; opcode_62:  ;/* CIM [unofficial] ( at _02 ) */

opcode_63:  ;/* RRA (ab,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_63,CD
  GETANYBYTE_d0
  RRA_C_CONT

; opcode_64:  ;/* SKB [unofficial] ( at _04 ) */

opcode_67:  ;/* RRA ZPAGE [unofficial] */
  addq.l #cy_67,CD
  ZPAGE
  GETZPBYTE d0
  RRA_C_CONT

opcode_6b:  ;/* ARR #ab [unofficial - Acc AND Data, ROR result] */
  IMMEDIATE ZFLAG
  addq.l #cy_6b,CD
  and.b  A,ZFLAG
  ROR_C ZFLAG
  bra.w  NEXTCHANGE_N

opcode_6f:  ;/* RRA abcd [unofficial] */
  ABSOLUTE
  addq.l #cy_6f,CD
  GETANYBYTE_d0
  RRA_C_CONT

; opcode_72:  ;/* CIM [unofficial] ( at _02 ) */

opcode_73:  ;/* RRA (ab),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_73,CD
  GETANYBYTE_d0
  RRA_C_CONT

opcode_74:
  SKB_C_CONT cy_74

opcode_77:  ;/* RRA zpage,x [unofficial] */
  addq.l #cy_77,CD
  ZPAGE_X
  GETZPBYTE d0
  RRA_C_CONT

; opcode_7a:  ;/* NOP [unofficial] ( at _ea ) */

opcode_7b:  ;/* RRA abcd,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_7b,CD
  GETANYBYTE_d0
  RRA_C_CONT

; opcode_7c:  ;/* SKW [unofficial] ( at _0c ) */

opcode_7f:  ;/* RRA abcd,x [unofficial] */
  ABSOLUTE_X
  addq.l #cy_7f,CD
  GETANYBYTE_d0
  RRA_C_CONT

opcode_80:
  addq.l #cy_80,CD
  SKB_C_CONT cy_80

opcode_82:
  addq.l #cy_82,CD
  SKB_C_CONT cy_82

opcode_83:  ;/* AXS (ab,x) [unofficial] */
  addq.l #cy_83,CD
  INDIRECT_X
  AXS_C_CONT cy_83

opcode_87:  ;/* AXS ZPAGE [unofficial] */
  addq.l #cy_87,CD
  ZPAGE
  move.b X,ZFLAG
  and.b  A,ZFLAG
  PUTZPBYTE_Z cy_87

opcode_89:
  addq.l #cy_89,CD
  SKB_C_CONT cy_89

;opcode_8b:  ;/* AND #ab [unofficial] ( at _29 ) */

opcode_8f:  ;/* AXS abcd [unofficial] */
  addq.l #cy_8f,CD
  ABSOLUTE
  AXS_C_CONT cy_8f

; opcode_92:  ;/* CIM [unofficial] ( at _02 ) */

opcode_93:  ;/* AXS (ab),y [unofficial] */
  addq.l #cy_93,CD
  INDIRECT_Y
  AXS_C_CONT cy_93

opcode_97:  ;/* AXS zpage,y [unofficial] */
  addq.l #cy_97,CD
  ZPAGE_Y
  move.b X,ZFLAG
  and.b  A,ZFLAG
  PUTZPBYTE_Z cy_97

opcode_9b:  ;/* XA1 abcd,y [unofficial] */
  addq.l #cy_9b,CD
  ABSOLUTE_Y
  XA1_C_CONT cy_9b

opcode_9c:
  addq.l #cy_9c,CD
  SKW_C_CONT cy_9c

;opcode_9e:  ;/* XA1 abcd,y (_9b)[unofficial] */

;opcode_9f:  ;/* XA1 abcd,y (_9b)[unofficial] */

opcode_a3: ;/* LAX (ind,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_a3,CD
  GETANYBYTE A
  move.b A,X
  NEXTCHANGE_REG A

opcode_a7: ;/* LAX ZPAGE [unofficial] */
  addq.l #cy_a7,CD
  ZPAGE
  GETZPBYTE A
  move.b A,X
  NEXTCHANGE_REG A

opcode_ab:  ;/* ANX #ab [unofficial - AND #ab, TAX] */
  IMMEDIATE d0
  addq.l #cy_ab,CD
  and.b  d0,A
  move.b A,X
  NEXTCHANGE_REG A

opcode_af: ;/* LAX absolute [unofficial] */
  ABSOLUTE
  addq.l #cy_af,CD
  GETANYBYTE A
  move.b A,X
  NEXTCHANGE_REG A

; opcode_b2:  ;/* CIM [unofficial] ( at _02 ) */

opcode_b3: ;/* LAX (ind),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_b3,CD
  GETANYBYTE A
  move.b A,X
  NEXTCHANGE_REG A

opcode_b7: ;/* LAX zpage,y [unofficial] */
  addq.l #cy_b7,CD
  ZPAGE_Y
  GETZPBYTE A
  move.b A,X
  NEXTCHANGE_REG A

opcode_bb:  ;/* AXA abcd,y [unofficial - Store Mem AND #$FD to Acc and X,
            ;then set stackpoint to value (Acc - 4) */
  ABSOLUTE_Y
  GETANYBYTE_d0
  and.b  #$fd,d0
  clr.l  A
  move.b d0,A
  move.b d0,X
  move.b d0,ZFLAG
  ext.w  NFLAG
  move.b A,d0
  subq.b #4,d0
  move.b d0,_regS
  addq.l #cy_bb,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_bf: ;/* LAX absolute,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_bf,CD
  GETANYBYTE A
  move.b A,X
  NEXTCHANGE_REG A

opcode_c2:
  addq.l #cy_c2,CD
  SKB_C_CONT cy_c2

opcode_c3:  ;/* DCM (ab,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_c3,CD
  GETANYBYTE_d0
  DCM_C_CONT

opcode_c7:  ;/* DCM ZPAGE [unofficial] */
  addq.l #cy_c7,CD
  ZPAGE
  GETZPBYTE d0
  DCM_C_CONT

opcode_cb:  ;/* SAX #ab [unofficial - A AND X, then SBC Mem, store to X] */
  IMMEDIATE d0
  addq.l #cy_cb,CD
  and.b  A,X
  btst   #D_FLAGB,_regP
  bne.s  .BCD_SBC_X
  not.w  CCR6502
  move.w CCR6502,CCR
  subx.b d0,X
  move.w CCR,CCR6502
  bvs.s  .SET_V
  bclr   #31,VFLAG
  not.w  CCR6502
  NEXTCHANGE_REG X
.SET_V:
  bset   #31,VFLAG
  not.w  CCR6502
  NEXTCHANGE_REG X
.BCD_SBC_X:
  move.b X,ZFLAG
  not.w  CCR6502
  move.w CCR6502,CCR
  subx.b d0,ZFLAG
  bvs.s  .SET_V_X
  bclr   #31,VFLAG
  bra.s  .END_V_X
.SET_V_X:
  bset   #31,VFLAG
.END_V_X:
  ext.w  NFLAG
  move.w CCR6502,CCR
  sbcd   d0,X
  move.w CCR,CCR6502
  not.w  CCR6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_cf:  ;/* DCM abcd [unofficial] */
  ABSOLUTE
  addq.l #cy_cf,CD
  GETANYBYTE_d0
  DCM_C_CONT

opcode_d3:  ;/* DCM (ab),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_d3,CD
  GETANYBYTE_d0
  DCM_C_CONT

opcode_d4:
  addq.l #cy_d4,CD
  SKB_C_CONT cy_d4

opcode_d7:  ;/* DCM zpage,x [unofficial] */
  addq.l #cy_d7,CD
  ZPAGE_X
  GETZPBYTE d0
  DCM_C_CONT

; opcode_da:  ;/* NOP [unofficial] ( at _ea ) */

opcode_db:  ;/* DCM abcd,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_db,CD
  GETANYBYTE_d0
  DCM_C_CONT

; opcode_dc:  ;/* SKW [unofficial] ( at _0c ) */

opcode_df:  ;/* DCM abcd,x [unofficial] */
  ABSOLUTE_X
  addq.l #cy_df,CD
  GETANYBYTE_d0
  DCM_C_CONT

opcode_e2:
  addq.l #cy_e2,CD
  SKB_C_CONT cy_e2

opcode_e3:  ;/* INS (ab,x) [unofficial] */
  INDIRECT_X
  addq.l #cy_e3,CD
  GETANYBYTE_d0
  INS_C_CONT

opcode_e7:  ;/* INS ZPAGE [unofficial] */
  addq.l #cy_e7,CD
  ZPAGE
  GETZPBYTE d0
  addq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  bra sbc

;opcode_eb: ;/* SBC #ab (_e9)[unofficial] */

opcode_ef:  ;/* INS abcd [unofficial] */
  ABSOLUTE
  addq.l #cy_ef,CD
  GETANYBYTE_d0
  INS_C_CONT

opcode_f3:  ;/* INS (ab),y [unofficial] */
  INDIRECT_Y
  addq.l #cy_f3,CD
  GETANYBYTE_d0
  INS_C_CONT

opcode_f4:
  addq.l #cy_f4,CD
  SKB_C_CONT cy_f4

opcode_f7:  ;/* INS zpage,x [unofficial] */
  addq.l #cy_f7,CD
  ZPAGE_X
  GETZPBYTE d0
  addq.b #1,d0
  move.b d0,(memory_pointer,d7.l)
  bra sbc

; opcode_fa:  ;/* NOP [unofficial] ( at _ea ) */

opcode_fb:  ;/* INS abcd,y [unofficial] */
  ABSOLUTE_Y
  addq.l #cy_fb,CD
  GETANYBYTE_d0
  INS_C_CONT

; opcode_fc:  ;/* SKW [unofficial] ( at _0c ) */

opcode_ff:  ;/* INS abcd,x [unofficial] */
  ABSOLUTE_X
  addq.l #cy_ff,CD
  GETANYBYTE_d0
  INS_C_CONT

; official opcodes

opcode_00: ;/* BRK */
  ifd MONITOR_BREAK
  tst.l _brkhere
  beq.s .oc_00_norm
  move.b #1,_break_here
  move.b #$ff,d7
  UPDATE_GLOBAL_REGS
  CPU_GetStatus d0
  move.l d7,-(a7)
  jsr    _AtariEscape
  addq.l #4,a7
  CPU_PutStatus d0
  UPDATE_LOCAL_REGS
  bra.w  NEXTCHANGE_WITHOUT
.oc_00_norm:
  endc
  addq.l #cy_00,CD
; btst   #I_FLAGB,_regP
; bne.w  NEXTCHANGE_WITHOUT
  SetB
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  addq.w #1,d7
  PHW_AND_P d7,d0
  SetI
  move.w (memory_pointer,$fffe.l),d7
  LoHi d7
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  bra.w  NEXTCHANGE_WITHOUT

opcode_01: ;/* ORA (ab,x) */
  INDIRECT_X
  addq.l #cy_01,CD
  OR_ANYBYTE

opcode_05: ;/* ORA ab */
  addq.l #cy_05,CD
  ZPAGE
  OR_ZPBYTE

opcode_06: ;/* ASL ab */
  addq.l #cy_06,CD
  ZPAGE
  GETZPBYTE ZFLAG
  ASL_C ZFLAG
  PUTZPBYTE_Z cy_06

opcode_08: ;/* PHP */
  PHP d7,d0
  addq.l #cy_08,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_09: ;/* ORA #ab */
  addq.l #cy_09,CD
  OR_IMMEDIATE cy_09

opcode_0a: ;/* ASLA */
  addq.l #cy_0a,CD
  ASL_C A
  NEXTCHANGE_REG A

opcode_0d: ;/* ORA abcd */
  ABSOLUTE
  addq.l #cy_0d,CD
  OR_ANYBYTE

opcode_0e: ;/* ASL abcd */
  ABSOLUTE
  addq.l #cy_0e,CD
  RPW_ASL_C

opcode_10: ;/* BPL */
  tst.w  NFLAG
  bpl    SOLVE
  DONT_BRA

opcode_11: ;/* ORA (ab),y */
  addq.l #cy_11,CD
  INDIRECT_Y_NCY cy_11
  OR_ANYBYTE

opcode_15: ;/* ORA ab,x */
  addq.l #cy_15,CD
  ZPAGE_X
  OR_ZPBYTE

opcode_16: ;/* ASL ab,x */
  addq.l #cy_16,CD
  ZPAGE_X
  GETZPBYTE ZFLAG
  ASL_C ZFLAG
  PUTZPBYTE_Z cy_16

opcode_18: ;/* CLC */
  clr.w  CCR6502
  addq.l #cy_18,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_19: ;/* ORA abcd,y */
  addq.l #cy_19,CD
  ABSOLUTE_Y_NCY cy_19
  OR_ANYBYTE

opcode_1d: ;/* ORA abcd,x */
  addq.l #cy_1d,CD
  ABSOLUTE_X_NCY cy_1d
  OR_ANYBYTE

opcode_1e: ;/* ASL abcd,x */
  ABSOLUTE_X
  addq.l #cy_1e,CD
  RPW_ASL_C

opcode_20: ;/* JSR abcd */
  addq.l #cy_20,CD
  move.l PC6502,d7 ;current pointer
  sub.l memory_pointer,d7
  ifd MONITOR_BREAK
  move.l #rem_jmp_steps,d0
  subq.l #1,d0
  lea _remember_JMP,a0
.shift_rem_jmp:
  move.w 2(a0),(a0)+
  dbra   d0,.shift_rem_jmp
  move.w d7,-(a0)  ; remember program counter
  addq.l #1,_ret_nesting
  endc
  addq.l #1,d7 ; return address
  PHW d7,d0
  JMP_C cy_20

opcode_21: ;/* AND (ab,x) */
  INDIRECT_X
  addq.l #cy_21,CD
  AND_ANYBYTE

opcode_24: ;/* BIT ab */
  addq.l #cy_24,CD
  ZPAGE
  GETZPBYTE ZFLAG
  BIT_C_CONT cy_24

opcode_25: ;/* AND ab */
  addq.l #cy_25,CD
  ZPAGE
  AND_ZPBYTE

opcode_26: ;/* ROL ab */
  addq.l #cy_26,CD
  ZPAGE
  GETZPBYTE ZFLAG
  ROL_C ZFLAG
  PUTZPBYTE_Z cy_26

opcode_28: ;/* PLP */
  addq.l #cy_28,CD
  PLP d7,d0
  tst.b  _IRQ           ; CPUCHECKIRQ
  beq.w  NEXTCHANGE_WITHOUT
  move.b _regP,d0
  andi.b #I_FLAG,d0
  bne.w  NEXTCHANGE_WITHOUT
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  clr.l  d0             ; push PC and P to stack ( PHW + PHB ) start
  move.b _regS,d0
  subq.b #1,d0     ; wrong way around
  move.b d7,(stack_pointer,d0.l)
  addq.b #1,d0
  LoHi d7
  move.b d7,(stack_pointer,d0.l)
  subq.b #2,d0
; move.b _regP,(stack_pointer,d0.l)
  move.b  _regP,d7                 ; TEST
  andi.b  #B_FLAGN,d7              ; TEST
  move.b  d7,(stack_pointer,d0.l)  ; TEST
  subq.b #1,d0
  move.b d0,_regS       ; push PC and P to stack ( PHW + PHB ) end
  SetI
  move.w (memory_pointer,$fffe.l),d7
  LoHi d7
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_29: ;/* AND #ab */
  addq.l #cy_29,CD
  AND_IMMEDIATE cy_29

opcode_2a: ;/* ROLA */
  addq.l #cy_2a,CD
  ROL_C A
  NEXTCHANGE_REG A

opcode_2c: ;/* BIT abcd */
  addq.l #cy_2c,CD
  ABSOLUTE
  GETANYBYTE ZFLAG
  BIT_C_CONT cy_2c

opcode_2d: ;/* AND abcd */
  ABSOLUTE
  addq.l #cy_2d,CD
  AND_ANYBYTE

opcode_2e: ;/* ROL abcd */
  ABSOLUTE
  addq.l #cy_2e,CD
  RPW_ROL_C

opcode_30: ;/* BMI */
  tst.w  NFLAG
  bmi    SOLVE
  DONT_BRA

opcode_31: ;/* AND (ab),y */
  addq.l #cy_31,CD
  INDIRECT_Y_NCY cy_31
  AND_ANYBYTE

opcode_35: ;/* AND ab,x */
  addq.l #cy_35,CD
  ZPAGE_X
  AND_ZPBYTE

opcode_36: ;/* ROL ab,x */
  addq.l #cy_36,CD
  ZPAGE_X
  GETZPBYTE ZFLAG
  ROL_C ZFLAG
  PUTZPBYTE_Z cy_36

opcode_38: ;/* SEC */
  clr.w  CCR6502
  not.w  CCR6502
  addq.l #cy_38,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_39: ;/* AND abcd,y */
  addq.l #cy_39,CD
  ABSOLUTE_Y_NCY cy_39
  AND_ANYBYTE

opcode_3d: ;/* AND abcd,x */
  addq.l #cy_3d,CD
  ABSOLUTE_X_NCY cy_3d
  AND_ANYBYTE

opcode_3e: ;/* ROL abcd,x */
  ABSOLUTE_X
  addq.l #cy_3e,CD
  RPW_ROL_C

opcode_40: ;/* RTI */
_RTI:
  addq.l #cy_40,CD
  PLP_AND_W d7,d0
  lea    (memory_pointer,d7.l),PC6502
  ifd MONITOR_BREAK
  tst.b _break_ret
  beq.s .mb_end
  tst.l _ret_nesting
  bmi.s .mb_end
  move.b #1,_break_step
.mb_end:
  subq.l #1,_ret_nesting
  endc
  tst.b  _IRQ           ; CPUCHECKIRQ
  beq.w  NEXTCHANGE_WITHOUT
  move.b _regP,d0
  andi.b #I_FLAG,d0
  bne.w  NEXTCHANGE_WITHOUT
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  clr.l  d0             ; push PC and P to stack ( PHW + PHB ) start
  move.b _regS,d0
  subq.b #1,d0     ; wrong way around
  move.b d7,(stack_pointer,d0.l)
  addq.b #1,d0
  LoHi d7
  move.b d7,(stack_pointer,d0.l)
  subq.b #2,d0
; move.b _regP,(stack_pointer,d0.l)
  move.b  _regP,d7                 ; TEST
  andi.b  #B_FLAGN,d7              ; TEST
  move.b  d7,(stack_pointer,d0.l)  ; TEST
  subq.b #1,d0
  move.b d0,_regS       ; push PC and P to stack ( PHW + PHB ) end
  SetI
  move.w (memory_pointer,$fffe.l),d7
  LoHi d7
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  addq.l #7,CD
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_41: ;/* EOR (ab,x) */
  INDIRECT_X
  addq.l #cy_41,CD
  GETANYBYTE_d0
  EOR_C_CONT

opcode_45: ;/* EOR ab */
  addq.l #cy_45,CD
  ZPAGE
  GETZPBYTE d0
  EOR_C_CONT

opcode_46: ;/* LSR ab */
  addq.l #cy_46,CD
  ZPAGE
  clr.w  NFLAG
  GETZPBYTE ZFLAG
  LSR_C ZFLAG
  PUTZPBYTE cy_46,ZFLAG

opcode_48: ;/* PHA */
  PHB A,d0
  addq.l #cy_48,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_49: ;/* EOR #ab */
  IMMEDIATE d0
  addq.l #cy_49,CD
  EOR_C_CONT

opcode_4a: ;/* LSRA */
  clr.w  NFLAG
  LSR_C A
  move.b A,ZFLAG
  addq.l #cy_4a,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_4c: ;/* JMP abcd */
  ifd MONITOR_BREAK
  move.l #rem_jmp_steps,d0
  subq.l #1,d0
  lea _remember_JMP,a0
.shift_rem_jmp:
  move.w 2(a0),(a0)+
  dbra   d0,.shift_rem_jmp
  move.l PC6502,d0
  sub.l  memory_pointer,d0
  move.w d0,-(a0)  ; remember program counter
  addq.l #1,_ret_nesting
  endc
  addq.l #cy_4c,CD
  JMP_C cy_4c

opcode_4d: ;/* EOR abcd */
  ABSOLUTE
  addq.l #cy_4d,CD
  GETANYBYTE_d0
  EOR_C_CONT

opcode_4e: ;/* LSR abcd */
  ABSOLUTE
  addq.l #cy_4e,CD
  RPW_LSR_C

opcode_50: ;/* BVC */
  tst.l  VFLAG
  beq SOLVE
  DONT_BRA

opcode_51: ;/* EOR (ab),y */
  addq.l #cy_51,CD
  INDIRECT_Y_NCY cy_51
  GETANYBYTE_d0
  EOR_C_CONT

opcode_55: ;/* EOR ab,x */
  addq.l #cy_55,CD
  ZPAGE_X
  GETZPBYTE d0
  EOR_C_CONT

opcode_56: ;/* LSR ab,x */
  addq.l #cy_56,CD
  ZPAGE_X
  clr.w  NFLAG
  GETZPBYTE ZFLAG
  LSR_C ZFLAG
  PUTZPBYTE cy_56,ZFLAG

opcode_58: ;/* CLI */
  addq.l #cy_58,CD
  ClrI
  tst.b  _IRQ      ; ~ CPUCHECKIRQ
  beq.w  NEXTCHANGE_WITHOUT
  move.l PC6502,d7
  sub.l  memory_pointer,d7
  PHW_AND_P_B0 d7,d0
  SetI
  move.w (memory_pointer,$fffe.l),d7
  LoHi d7
  move.l d7,PC6502
  add.l  memory_pointer,PC6502
  clr.b  _IRQ
  addq.l #7,CD
  ifd MONITOR_BREAK
  addq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_59: ;/* EOR abcd,y */
  addq.l #cy_59,CD
  ABSOLUTE_Y_NCY cy_59
  GETANYBYTE_d0
  EOR_C_CONT

opcode_5d: ;/* EOR abcd,x */
  addq.l #cy_5d,CD
  ABSOLUTE_X_NCY cy_5d
  GETANYBYTE_d0
  EOR_C_CONT

opcode_5e: ;/* LSR abcd,x */
  ABSOLUTE_X
  addq.l #cy_5e,CD
  RPW_LSR_C

opcode_60: ;/* RTS */
  PLW d7,d0
  lea    1(memory_pointer,d7.l),PC6502
  addq.l #cy_60,CD
  ifd MONITOR_BREAK
  tst.b _break_ret
  beq.s .mb_end
  tst.l _ret_nesting
  bmi.s .mb_end
  move.b #1,_break_step
.mb_end:
  subq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_61: ;/* ADC (ab,x) */
  INDIRECT_X
  addq.l #cy_61,CD
  GETANYBYTE_d0
  bra adc

opcode_65: ;/* ADC ab */
  addq.l #cy_65,CD
  ZPAGE
  GETZPBYTE d0
  bra adc

opcode_66: ;/* ROR ab */
  addq.l #cy_66,CD
  ZPAGE
  GETZPBYTE ZFLAG
  ROR_C ZFLAG
  PUTZPBYTE_Z cy_66

opcode_68: ;/* PLA */
  addq.l #cy_68,CD
  PLB A,d0
  NEXTCHANGE_REG A

opcode_69: ;/* ADC #ab */
  IMMEDIATE d0
  addq.l #cy_69,CD
  bra adc

opcode_6a: ;/* RORA */
  addq.l #cy_6a,CD
  ROR_C A
  NEXTCHANGE_REG A

opcode_6c: ;/* JMP (abcd) */
  ifd MONITOR_BREAK
  move.l #rem_jmp_steps,d0
  subq.l #1,d0
  lea _remember_JMP,a0
.shift_rem_jmp:
  move.w 2(a0),(a0)+
  dbra   d0,.shift_rem_jmp
  move.l PC6502,d0
  sub.l  memory_pointer,d0
  move.w d0,-(a0)  ; remember program counter
  addq.l #1,_ret_nesting
  endc
  move.w (PC6502)+,d7
  LoHi d7
  ifd P65C02
  move.w (memory_pointer,d7.l),d7
  LoHi d7
  lea    (memory_pointer,d7.l),PC6502
  else
  ;/* original 6502 had a bug in jmp (addr) when addr crossed page boundary */
  cmp.b  #$ff,d7
  beq.s  .PROBLEM_FOUND ;when problematic jump is found
  move.w (memory_pointer,d7.l),d7
  LoHi d7
  lea    (memory_pointer,d7.l),PC6502
  addq.l #cy_6c,CD
  bra.w  NEXTCHANGE_WITHOUT
.PROBLEM_FOUND:
  move.l d7,d0 ;we have to use both of them
  clr.b  d7 ;instead of reading right this adress,
            ;we read adress at this start of page
  move.b (memory_pointer,d7.l),d7
  LoHi d7
  move.b (memory_pointer,d0.l),d7
  lea    (memory_pointer,d7.l),PC6502
  endc
  addq.l #cy_6c,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_6d: ;/* ADC abcd */
  ABSOLUTE
  addq.l #cy_6d,CD
  GETANYBYTE_d0
  bra adc

opcode_6e: ;/* ROR abcd */
  ABSOLUTE
  addq.l #cy_6e,CD
  RPW_ROR_C

opcode_70: ;/* BVS */
  tst.l  VFLAG
  bne SOLVE
  DONT_BRA

opcode_71: ;/* ADC (ab),y */
  addq.l #cy_71,CD
  INDIRECT_Y_NCY cy_71
  GETANYBYTE_d0
  bra adc

opcode_75: ;/* ADC ab,x */
  addq.l #cy_75,CD
  ZPAGE_X
  GETZPBYTE d0
  bra adc

opcode_76: ;/* ROR ab,x */
  addq.l #cy_76,CD
  ZPAGE_X
  GETZPBYTE ZFLAG
  ROR_C ZFLAG
  PUTZPBYTE_Z cy_76

opcode_78: ;/* SEI */
  SetI
  addq.l #cy_78,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_79: ;/* ADC abcd,y */
  addq.l #cy_79,CD
  ABSOLUTE_Y_NCY cy_79
  GETANYBYTE_d0
  bra adc

opcode_7d: ;/* ADC abcd,x */
  addq.l #cy_7d,CD
  ABSOLUTE_X_NCY cy_7d
  GETANYBYTE_d0
  bra adc

opcode_7e: ;/* ROR abcd,x */
  ABSOLUTE_X
  addq.l #cy_7e,CD
  RPW_ROR_C

opcode_81: ;/* STA (ab,x) */
  addq.l #cy_81,CD
  INDIRECT_X
  STOREANYBYTE cy_81,A

opcode_84: ;/* STY ab */
  addq.l #cy_84,CD
  ZPAGE
  PUTZPBYTE cy_84,Y

opcode_85: ;/* STA ab */
  addq.l #cy_85,CD
  ZPAGE
  PUTZPBYTE cy_85,A

opcode_86: ;/* STX ab */
  addq.l #cy_86,CD
  ZPAGE
  PUTZPBYTE cy_86,X

opcode_88: ;/* DEY */
  addq.l #cy_88,CD
  subq.b #1,Y
  NEXTCHANGE_REG Y

opcode_8a: ;/* TXA */
  addq.l #cy_8a,CD
  move.b X,A
  NEXTCHANGE_REG A

opcode_8c: ;/* STY abcd */
  addq.l #cy_8c,CD
  ABSOLUTE
  STOREANYBYTE cy_8c,Y

opcode_8d: ;/* STA abcd */
  addq.l #cy_8d,CD
  ABSOLUTE
  STOREANYBYTE cy_8d,A

opcode_8e: ;/* STX abcd */
  addq.l #cy_8e,CD
  ABSOLUTE
  STOREANYBYTE cy_8e,X

opcode_90: ;/* BCC */
  btst #EB68,CCR6502
  beq SOLVE
  DONT_BRA

opcode_91: ;/* STA (ab),y */
  addq.l #cy_91,CD
  INDIRECT_Y
  STOREANYBYTE cy_91,A

opcode_94: ;/* STY ab,x */
  addq.l #cy_94,CD
  ZPAGE_X
  PUTZPBYTE cy_94,Y

opcode_95: ;/* STA ab,x */
  addq.l #cy_95,CD
  ZPAGE_X
  PUTZPBYTE cy_95,A

opcode_96: ;/* STX ab,y */
  addq.l #cy_96,CD
  ZPAGE_Y
  PUTZPBYTE cy_96,X

opcode_98: ;/* TYA */
  addq.l #cy_98,CD
  move.b Y,A
  NEXTCHANGE_REG A

opcode_99: ;/* STA abcd,y */
  addq.l #cy_99,CD
  ABSOLUTE_Y
  STOREANYBYTE cy_99,A

opcode_9a: ;/* TXS */
  move.b X,_regS
  addq.l #cy_9a,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_9d: ;/* STA abcd,x */
  addq.l #cy_9d,CD
  ABSOLUTE_X
  STOREANYBYTE cy_9d,A

opcode_a0: ;/* LDY #ab */
  addq.l #cy_a0,CD
  LOAD_IMMEDIATE cy_a0,Y

opcode_a1: ;/* LDA (ab,x) */
  INDIRECT_X
  addq.l #cy_a1,CD
  LOADANYBYTE A

opcode_a2: ;/* LDX #ab */
  addq.l #cy_a2,CD
  LOAD_IMMEDIATE cy_a2,X

opcode_a4: ;/* LDY ab */
  addq.l #cy_a4,CD
  LOAD_ZPAGE cy_a4,Y

opcode_a5: ;/* LDA ab */
  addq.l #cy_a5,CD
  LOAD_ZPAGE cy_a5,A

opcode_a6: ;/* LDX ab */
  addq.l #cy_a6,CD
  LOAD_ZPAGE cy_a6,X

opcode_a8: ;/* TAY */
  addq.l #cy_a8,CD
  move.b A,Y
  NEXTCHANGE_REG A

opcode_a9: ;/* LDA #ab */
  addq.l #cy_a9,CD
  LOAD_IMMEDIATE cy_a9,A

opcode_aa: ;/* TAX */
  addq.l #cy_aa,CD
  move.b A,X
  NEXTCHANGE_REG A

opcode_ac: ;/* LDY abcd */
  ABSOLUTE
  addq.l #cy_ac,CD
  LOADANYBYTE Y

opcode_ad: ;/* LDA abcd */
  ABSOLUTE
  addq.l #cy_ad,CD
  LOADANYBYTE A

opcode_ae: ;/* LDX abcd */
  ABSOLUTE
  addq.l #cy_ae,CD
  LOADANYBYTE X

opcode_b0: ;/* BCS */
  btst #EB68,CCR6502
  bne SOLVE
  DONT_BRA

opcode_b1: ;/* LDA (ab),y */
  addq.l #cy_b1,CD
  INDIRECT_Y_NCY cy_b1
  LOADANYBYTE A

opcode_b4: ;/* LDY ab,x */
  addq.l #cy_b4,CD
  ZPAGE_X
  GETZPBYTE Y
  NEXTCHANGE_REG Y

opcode_b5: ;/* LDA ab,x */
  addq.l #cy_b5,CD
  ZPAGE_X
  GETZPBYTE A
  NEXTCHANGE_REG A

opcode_b6: ;/* LDX ab,y */
  addq.l #cy_b6,CD
  ZPAGE_Y
  GETZPBYTE X
  NEXTCHANGE_REG X

opcode_b8: ;/* CLV */
  bclr   #31,VFLAG
  addq.l #cy_b8,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_b9: ;/* LDA abcd,y */
  addq.l #cy_b9,CD
  ABSOLUTE_Y_NCY cy_b9
  LOADANYBYTE A

opcode_ba: ;/* TSX */
  addq.l #cy_ba,CD
  move.b _regS,X
  NEXTCHANGE_REG X

opcode_bc: ;/* LDY abcd,x */
  addq.l #cy_bc,CD
  ABSOLUTE_X_NCY cy_bc
  LOADANYBYTE Y

opcode_bd: ;/* LDA abcd,x */
  addq.l #cy_bd,CD
  ABSOLUTE_X_NCY cy_bd
  LOADANYBYTE A

opcode_be: ;/* LDX abcd,y */
  addq.l #cy_be,CD
  ABSOLUTE_Y_NCY cy_be
  LOADANYBYTE X

opcode_c0: ;/* CPY #ab */
  IMMEDIATE d0
  addq.l #cy_c0,CD
  move.b Y,ZFLAG
  bra COMPARE

opcode_c1: ;/* CMP (ab,x) */
  INDIRECT_X
  addq.l #cy_c1,CD
  GETANYBYTE_d0
  bra COMPARE_A

opcode_c4: ;/* CPY ab */
  addq.l #cy_c4,CD
  ZPAGE
  GETZPBYTE d0
  move.b Y,ZFLAG
  bra COMPARE

opcode_c5: ;/* CMP ab */
  addq.l #cy_c5,CD
  ZPAGE
  GETZPBYTE d0
  bra COMPARE_A

opcode_c6: ;/* DEC ab */
  addq.l #cy_c6,CD
  ZPAGE
  GETZPBYTE ZFLAG
  subq.b #1,ZFLAG
  PUTZPBYTE_Z cy_c6

opcode_c8: ;/* INY */
  addq.l #cy_c8,CD
  addq.b #1,Y
  NEXTCHANGE_REG Y

opcode_c9: ;/* CMP #ab */
  IMMEDIATE d0
  addq.l #cy_c9,CD
  bra COMPARE_A

opcode_ca: ;/* DEX */
  addq.l #cy_ca,CD
  subq.b #1,X
  NEXTCHANGE_REG X

opcode_cc: ;/* CPY abcd */
  ABSOLUTE
  addq.l #cy_cc,CD
  GETANYBYTE_d0
  move.b Y,ZFLAG
  bra COMPARE

opcode_cd: ;/* CMP abcd */
  ABSOLUTE
  addq.l #cy_cd,CD
  GETANYBYTE_d0
  bra COMPARE_A

opcode_ce: ;/* DEC abcd */
  addq.l #cy_ce,CD
  ABSOLUTE
  GETANYBYTE ZFLAG
  subq.b #1,ZFLAG
  PUTANYBYTE_N cy_ce

opcode_d0: ;/* BNE */
  tst.b ZFLAG
  bne SOLVE
  DONT_BRA

opcode_d1: ;/* CMP (ab),y */
  addq.l #cy_d1,CD
  INDIRECT_Y_NCY cy_d1
  GETANYBYTE_d0
  bra COMPARE_A

opcode_d2: ;/* ESCRTS #ab (JAM) - on Atari is here instruction CIM
           ;[unofficial] !RS! */
  move.b (PC6502)+,d7
  move.l d7,-(a7)
  UPDATE_GLOBAL_REGS
  CPU_GetStatus d0
  jsr _AtariEscape /*in atari c*/
  addq.l #4,a7
  CPU_PutStatus d0
  UPDATE_LOCAL_REGS
  PLW d7,d0
  lea (memory_pointer,d7.l),PC6502
  addq.l #1,PC6502
  addq.l #cy_d2,CD
  ifd MONITOR_BREAK
  tst.b _break_ret
  beq.s .mb_end
  tst.l _ret_nesting
  bmi.s .mb_end
  move.b #1,_break_step
.mb_end:
  subq.l #1,_ret_nesting
  endc
  bra.w  NEXTCHANGE_WITHOUT

opcode_d5: ;/* CMP ab,x */
  addq.l #cy_d5,CD
  ZPAGE_X
  GETZPBYTE d0
  bra COMPARE_A

opcode_d6: ;/* DEC ab,x */
  addq.l #cy_d6,CD
  ZPAGE_X
  GETZPBYTE ZFLAG
  subq.b #1,ZFLAG
  PUTZPBYTE_Z cy_d6

opcode_d8: ;/* CLD */
  ClrD
  addq.l #cy_d8,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_d9: ;/* CMP abcd,y */
  addq.l #cy_d9,CD
  ABSOLUTE_Y_NCY cy_d9
  GETANYBYTE_d0
  bra COMPARE_A

opcode_dd: ;/* CMP abcd,x */
  addq.l #cy_dd,CD
  ABSOLUTE_X_NCY cy_dd
  GETANYBYTE_d0
  bra COMPARE_A

opcode_de: ;/* DEC abcd,x */
  addq.l #cy_de,CD
  ABSOLUTE_X
  GETANYBYTE ZFLAG
  subq.b #1,ZFLAG
  PUTANYBYTE_N cy_de

opcode_e0: ;/* CPX #ab */
  IMMEDIATE d0
  addq.l #cy_e0,CD
  move.b X,ZFLAG
  bra COMPARE

opcode_e1: ;/* SBC (ab,x) */
  INDIRECT_X
  addq.l #cy_e1,CD
  GETANYBYTE_d0
  bra sbc

opcode_e4: ;/* CPX ab */
  addq.l #cy_e4,CD
  ZPAGE
  GETZPBYTE d0
  move.b X,ZFLAG
  bra COMPARE

opcode_e5: ;/* SBC ab */
  addq.l #cy_e5,CD
  ZPAGE
  GETZPBYTE d0
  bra sbc

opcode_e6: ;/* INC ab */
  addq.l #cy_e6,CD
  ZPAGE
  GETZPBYTE ZFLAG
  addq.b #1,ZFLAG
  PUTZPBYTE_Z cy_e6

opcode_e8: ;/* INX */
  addq.l #cy_e8,CD
  addq.b #1,X
  NEXTCHANGE_REG X

opcode_e9: ;/* SBC #ab */
  IMMEDIATE d0
  addq.l #cy_e9,CD
  bra sbc

;opcode_ea: ;/* NOP */ ;official
instr_NOP:
  addq.l #cy_ea,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_ec: ;/* CPX abcd */
  ABSOLUTE
  addq.l #cy_ec,CD
  GETANYBYTE_d0
  move.b X,ZFLAG
  bra COMPARE

opcode_ed: ;/* SBC abcd */
  ABSOLUTE
  addq.l #cy_ed,CD
  GETANYBYTE_d0
  bra sbc

opcode_ee: ;/* INC abcd */
  addq.l #cy_ee,CD
  ABSOLUTE
  GETANYBYTE ZFLAG
  addq.b #1,ZFLAG
  PUTANYBYTE_N cy_ee

opcode_f0: ;/* BEQ */
  tst.b ZFLAG
  beq SOLVE
  DONT_BRA

opcode_f1: ;/* SBC (ab),y */
  addq.l #cy_f1,CD
  INDIRECT_Y_NCY cy_f1
  GETANYBYTE_d0
  bra sbc

opcode_f2:  ;/* ESC #ab (JAM) - on Atari is here instruction CIM
            ;[unofficial] !RS! */
  move.b (PC6502)+,d7
  move.l d7,-(a7)
  UPDATE_GLOBAL_REGS
  CPU_GetStatus d0
  jsr _AtariEscape
  addq.l #4,a7
  CPU_PutStatus d0
  UPDATE_LOCAL_REGS
  addq.l #cy_f2,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_f5: ;/* SBC ab,x */
  addq.l #cy_f5,CD
  ZPAGE_X
  GETZPBYTE d0
  bra sbc

opcode_f6: ;/* INC ab,x */
  addq.l #cy_f6,CD
  ZPAGE_X
  GETZPBYTE ZFLAG
  addq.b #1,ZFLAG
  PUTZPBYTE_Z cy_f6

opcode_f8: ;/* SED */
  SetD
  addq.l #cy_f8,CD
  bra.w  NEXTCHANGE_WITHOUT

opcode_f9: ;/* SBC abcd,y */
  addq.l #cy_f9,CD
  ABSOLUTE_Y_NCY cy_f9
  GETANYBYTE_d0
  bra sbc

opcode_fd: ;/* SBC abcd,x */
  addq.l #cy_fd,CD
  ABSOLUTE_X_NCY cy_fd
  GETANYBYTE_d0
  bra sbc

opcode_fe: ;/* INC abcd,x */
  addq.l #cy_fe,CD
  ABSOLUTE_X
  GETANYBYTE ZFLAG
  addq.b #1,ZFLAG
  PUTANYBYTE_N cy_fe

COMPARE_A:
  move.b A,ZFLAG
COMPARE:
  sub.b  d0,ZFLAG
  move.w CCR,CCR6502
  not.w  CCR6502
; bra.w  NEXTCHANGE_N

;MAIN LOOP , where we are counting cycles and working with other STUFF

NEXTCHANGE_N:
  ext.w  NFLAG
NEXTCHANGE_WITHOUT:
  cmp.l _xpos_limit,CD
  bge.s END_OF_CYCLE
****************************************
  ifd MONITOR_BREAK  ;following block of code allows you to enter
  move ccr,-(sp)     ;a break address in monitor
  move.l #rem_pc_steps,d7
  subq.l #1,d7
  lea _remember_PC,a0
.shift_rem_pc:
  move.w 2(a0),(a0)+
  dbra   d7,.shift_rem_pc
  move.l PC6502,d7
  sub.l memory_pointer,d7
  move.w d7,-(a0)  ; remember program counter
  cmp.w _break_addr,d7 ; break address reached ?
  beq.s .go_monitor
  tst.b _break_step ; step mode active ?
  beq.s .get_first
.go_monitor:
  move.b #$ff,d7
  move (sp)+,ccr
  bsr go_monitor  ;on break monitor is invoked
  move ccr,-(sp)
.get_first
  move (sp)+,ccr
  endc
****************************************
  clr.l  d7
  move.b (PC6502)+,d7
  ifd PROFILE
  lea _instruction_count,a0
  addq.l #1,(a0,d7.l*4)
  endc
  jmp ([OPMODE_TABLE,PC,d7.l*4])

END_OF_CYCLE:
  CPU_GetStatus d0
  UPDATE_GLOBAL_REGS
  move.l CD,_xpos ;returned value
  movem.l (a7)+,d2-d7/a2-a5
TERM_GO:
  rts

SOLVE:
  move.b (PC6502)+,d7
  extb.l d7
  move.l PC6502,d0
  add.l  d7,PC6502
  sub.l  memory_pointer,d0
  and.w  #255,d0                  ; !!!
  add.w  d7,d0
  and.w  #$ff00,d0
  bne.s  SOLVE_PB
  addq.l #cy_Bcc1,CD
  bra.w  NEXTCHANGE_WITHOUT
SOLVE_PB:
  addq.l #cy_Bcc2,CD
  bra.w  NEXTCHANGE_WITHOUT

adc:
  btst   #D_FLAGB,_regP
  bne.s  BCD_ADC
  move.w CCR6502,CCR
  addx.b d0,A
  bvs.s  .SET_V
  move.w CCR,CCR6502
  bclr   #31,VFLAG
  NEXTCHANGE_REG A
.SET_V:
  move.w CCR,CCR6502
  bset   #31,VFLAG
  NEXTCHANGE_REG A
; Version 1 : exact like Thor
;   Z from binary calc.
;   N + V after lower nibble decimal correction
;   C from decimal calc.
; a lot of code necessary to replicate a 6502 bug
BCD_ADC:
  move.w d0,-(a7)  ; needed first
  andi.b #$0f,d0   ; low nibble Add
  move.b A,d7
  andi.b #$0f,d7   ; low nibble A
  move.w CCR6502,CCR
  abcd   d0,d7     ; low nibble BCD add
  move.b A,d0
  andi.b #$f0,d0   ; high nibble Add
  add.b  d0,d7
  move.w (a7),d0
  andi.b #$f0,d0   ; high nibble Add
  add.b  d0,d7
  move.b d7,ZFLAG
  ext.w  NFLAG     ; NFLAG finished
  eor.b  A,d0      ; A eor data
  bmi.s  .CLR_V    ; highest bit ?
  eor.b  A,ZFLAG   ; A eor temp
  bpl.s  .CLR_V    ; not highest bit ?
  bset   #31,VFLAG
  bra.s  .V_DONE
 .CLR_V:
  bclr   #31,VFLAG
 .V_DONE:           ; VFLAG finished
  move.w (a7)+,d0  ; restore data
  move.w CCR6502,CCR
  move.b A,ZFLAG
  addx.b d0,ZFLAG  ; ZFLAG finished
  move.w CCR6502,CCR
  abcd   d0,A      ; A finished
  move.w CCR,CCR6502 ; CFLAG finished
  bra.w  NEXTCHANGE_WITHOUT
  nop                             ; !!! TEST !!!

sbc:
  btst   #D_FLAGB,_regP
  bne.s  BCD_SBC
  not.w  CCR6502
  move.w CCR6502,CCR
  subx.b d0,A
  bvs.s  .SET_V
  move.w CCR,CCR6502
  not.w  CCR6502
  bclr   #31,VFLAG
  NEXTCHANGE_REG A
.SET_V:
  move.w CCR,CCR6502
  not.w  CCR6502
  bset   #31,VFLAG
  NEXTCHANGE_REG A
; Version exact like Thor
;   C, Z, N, V from binary calc.
;   A from decimal calc.
BCD_SBC:
  move.b A,ZFLAG
  not.w  CCR6502
  move.w CCR6502,CCR
  sbcd   d0,A
  move.w CCR6502,CCR
  subx.b d0,ZFLAG
  bvs.s  .SET_VX
  move.w CCR,CCR6502
  not.w  CCR6502
  bclr   #31,VFLAG
  ext.w  NFLAG
  bra.w  NEXTCHANGE_WITHOUT
.SET_VX:
  move.w CCR,CCR6502
  not.w  CCR6502
  bset   #31,VFLAG
  ext.w  NFLAG
  bra.w  NEXTCHANGE_WITHOUT

Putbyte_SPCL macro
  move.l d1,-(a7)
  EXE_PUTBYTE_d0
  move.l (a7)+,d1
  endm

Putbyte_ADC:
  Putbyte_SPCL
  bra.w  adc

Putbyte_CMP:
  Putbyte_SPCL
  bra.w  COMPARE_A

Putbyte_SBC:
  Putbyte_SPCL
  bra.w  sbc

go_monitor:
  UPDATE_GLOBAL_REGS
  CPU_GetStatus d0
  move.l d7,-(a7)
  jsr    _AtariEscape
  addq.l #4,a7
  CPU_PutStatus d0
  UPDATE_LOCAL_REGS
  rts

OPMODE_TABLE:

  dc.l opcode_00,opcode_01,instr_CIM,opcode_03
  ;       brk       ora     illegal   illegal

  dc.l instr_SKB,opcode_05,opcode_06,opcode_07
  ;       nop       ora       asl       nop

  dc.l opcode_08,opcode_09,opcode_0a,opcode_29
  ;       php       ora       asla    illegal

  dc.l instr_SKW,opcode_0d,opcode_0e,opcode_0f
  ;      nop 3      ora       asl     illegal

  dc.l opcode_10,opcode_11,instr_CIM,opcode_13
  ;       bpl       ora     illegal   illegal

  dc.l opcode_14,opcode_15,opcode_16,opcode_17
  ;       nop       ora       asl     illegal

  dc.l opcode_18,opcode_19,instr_NOP,opcode_1b
  ;       clc       ora       nop     illegal

  dc.l instr_SKW,opcode_1d,opcode_1e,opcode_1f
  ;       nop       ora       asl     illegal

  dc.l opcode_20,opcode_21,instr_CIM,opcode_23
  ;       jsr       and     illegal   illegal

  dc.l opcode_24,opcode_25,opcode_26,opcode_27
  ;       bit       and       rol     illegal

  dc.l opcode_28,opcode_29,opcode_2a,opcode_29
  ;       plp       and       rolA    illegal

  dc.l opcode_2c,opcode_2d,opcode_2e,opcode_2f
  ;       bit       and       rol     illegal

  dc.l opcode_30,opcode_31,instr_CIM,opcode_33
  ;       bmi       and     illegal   illegal

  dc.l opcode_34,opcode_35,opcode_36,opcode_37
  ;       nop       and       rol     illegal

  dc.l opcode_38,opcode_39,instr_NOP,opcode_3b
  ;       sec       and       nop     illegal

  dc.l instr_SKW,opcode_3d,opcode_3e,opcode_3f
  ;       nop       and       rol     illegal

  dc.l opcode_40,opcode_41,instr_CIM,opcode_43
  ;       rti       eor     illegal   illegal

  dc.l instr_SKB,opcode_45,opcode_46,opcode_47
  ;       nop       eor       lsr     illegal

  dc.l opcode_48,opcode_49,opcode_4a,opcode_4b
  ;       pha       eor       lsrA    illegal

  dc.l opcode_4c,opcode_4d,opcode_4e,opcode_4f
  ;       jmp       eor       lsr     illegal

  dc.l opcode_50,opcode_51,instr_CIM,opcode_53
  ;       bvc       eor     illegal   illegal

  dc.l opcode_54,opcode_55,opcode_56,opcode_57
  ;       nop       eor       lsr     illegal

  dc.l opcode_58,opcode_59,instr_NOP,opcode_5b
  ;       cli       eor       nop     illegal

  dc.l instr_SKW,opcode_5d,opcode_5e,opcode_5f
  ;       nop       eor       lsr     illegal

  dc.l opcode_60,opcode_61,instr_CIM,opcode_63
  ;       rts       adc     illegal   illegal

  dc.l instr_SKB,opcode_65,opcode_66,opcode_67
  ;       nop       adc       ror     illegal

  dc.l opcode_68,opcode_69,opcode_6a,opcode_6b
  ;       pla       adc       rorA    illegal

  dc.l opcode_6c,opcode_6d,opcode_6e,opcode_6f
  ;    jmp (abcd)   adc       ror     illegal

  dc.l opcode_70,opcode_71,instr_CIM,opcode_73
  ;       bvs       adc     illegal   illegal

  dc.l opcode_74,opcode_75,opcode_76,opcode_77
  ;       nop       adc       ror     illegal

  dc.l opcode_78,opcode_79,instr_NOP,opcode_7b
  ;       SEI       adc     illegal   illegal

  dc.l instr_SKW,opcode_7d,opcode_7e,opcode_7f
  ;       nop       adc       ror     illegal

  dc.l opcode_80,opcode_81,opcode_82,opcode_83
  ;       nop       sta       nop     illegal

  dc.l opcode_84,opcode_85,opcode_86,opcode_87
  ;       sty       sta       stx     illegal

  dc.l opcode_88,opcode_89,opcode_8a,opcode_29
  ;       dey       nop       txa     illegal

  dc.l opcode_8c,opcode_8d,opcode_8e,opcode_8f
  ;       sty       sta       stx     illegal

  dc.l opcode_90,opcode_91,instr_CIM,opcode_93
  ;       bcc       sta     illegal   illegal

  dc.l opcode_94,opcode_95,opcode_96,opcode_97
  ;       sty       sta       stx     illegal

  dc.l opcode_98,opcode_99,opcode_9a,opcode_9b
  ;       tya       sta       txs     illegal

  dc.l opcode_9c,opcode_9d,opcode_9b,opcode_9b
  ;     illegal     sta     illegal   illegal

  dc.l opcode_a0,opcode_a1,opcode_a2,opcode_a3
  ;       ldy       lda       ldx       lax

  dc.l opcode_a4,opcode_a5,opcode_a6,opcode_a7
  ;       ldy       lda       ldx       lax

  dc.l opcode_a8,opcode_a9,opcode_aa,opcode_ab
  ;       tay       lda       tax       oal

  dc.l opcode_ac,opcode_ad,opcode_ae,opcode_af
  ;       ldy       lda       ldx       lax

  dc.l opcode_b0,opcode_b1,instr_CIM,opcode_b3
  ;       bcs       lda     illegal     lax

  dc.l opcode_b4,opcode_b5,opcode_b6,opcode_b7
  ;       ldy        lda        ldx       lax

  dc.l opcode_b8,opcode_b9,opcode_ba,opcode_bb
  ;       clv       lda       tsx     illegal

  dc.l opcode_bc,opcode_bd,opcode_be,opcode_bf
  ;       ldy       lda       ldx       lax

  dc.l opcode_c0,opcode_c1,opcode_c2,opcode_c3
  ;       cpy       cmp       nop       dcm

  dc.l opcode_c4,opcode_c5,opcode_c6,opcode_c7
  ;       cpy       cmp       dec     illegal

  dc.l opcode_c8,opcode_c9,opcode_ca,opcode_cb
  ;       iny       cmp       dex     illegal

  dc.l opcode_cc,opcode_cd,opcode_ce,opcode_cf
  ;       cpy       cmp       dec     illegal

  dc.l opcode_d0,opcode_d1,opcode_d2,opcode_d3
  ;       bne       cmp      escrts   illegal

  dc.l opcode_d4,opcode_d5,opcode_d6,opcode_d7
  ;       nop       cmp       dec     illegal

  dc.l opcode_d8,opcode_d9,instr_NOP,opcode_db
  ;       cld       cmp       nop     illegal

  dc.l instr_SKW,opcode_dd,opcode_de,opcode_df
  ;       nop       cmp       dec     illegal

  dc.l opcode_e0,opcode_e1,opcode_e2,opcode_e3
  ;       cpx       sbc       nop       nop

  dc.l opcode_e4,opcode_e5,opcode_e6,opcode_e7
  ;       cpx       sbc       inc     illegal

  dc.l opcode_e8,opcode_e9,instr_NOP,opcode_e9
  ;       inx       sbc       nop     illegal

  dc.l opcode_ec,opcode_ed,opcode_ee,opcode_ef
  ;       cpx       sbc       inc     illegal

  dc.l opcode_f0,opcode_f1,opcode_f2,opcode_f3
  ;       beq       sbc      esc#ab   illegal

  dc.l opcode_f4,opcode_f5,opcode_f6,opcode_f7
  ;       nop       sbc       inc     illegal

  dc.l opcode_f8,opcode_f9,instr_NOP,opcode_fb
  ;       sed       sbc       nop     illegal

  dc.l instr_SKW,opcode_fd,opcode_fe,opcode_ff
  ;       nop       sbc       inc      esc#ab

cycles:
_cycles:   ; dc.l for the world outside, equ for internal use

cy_CIM equ 2
cy_NOP equ 2
cy_SKB equ 3
cy_SKW equ 4
cy_Bcc equ 2
cy_Bcc1 equ 3
cy_Bcc2 equ 4
cy_00 equ 7
cy_01 equ 6
cy_02 equ cy_CIM
cy_03 equ 8
cy_04 equ cy_SKB
cy_05 equ 3
cy_06 equ 5
cy_07 equ 5
cy_08 equ 3
cy_09 equ 2
cy_0a equ 2
cy_0b equ 2
cy_0c equ cy_SKW
cy_0d equ 4
cy_0e equ 6
cy_0f equ 6
cy_10 equ cy_Bcc
cy_11 equ 5
cy_12 equ cy_CIM
cy_13 equ 8
cy_14 equ 4
cy_15 equ 4
cy_16 equ 6
cy_17 equ 6
cy_18 equ 2
cy_19 equ 4
cy_1a equ cy_NOP
cy_1b equ 7
cy_1c equ cy_SKW
cy_1d equ 4
cy_1e equ 7
cy_1f equ 7
cy_20 equ 6
cy_21 equ 6
cy_22 equ cy_CIM
cy_23 equ 8
cy_24 equ 3
cy_25 equ 3
cy_26 equ 5
cy_27 equ 5
cy_28 equ 4
cy_29 equ 2
cy_2a equ 2
cy_2b equ cy_29
cy_2c equ 4
cy_2d equ 4
cy_2e equ 6
cy_2f equ 6
cy_30 equ cy_Bcc
cy_31 equ 5
cy_32 equ cy_CIM
cy_33 equ 8
cy_34 equ 4
cy_35 equ 4
cy_36 equ 6
cy_37 equ 6
cy_38 equ 2
cy_39 equ 4
cy_3a equ cy_NOP
cy_3b equ 7
cy_3c equ cy_SKW
cy_3d equ 4
cy_3e equ 7
cy_3f equ 7
cy_40 equ 6
cy_41 equ 6
cy_42 equ cy_CIM
cy_43 equ 8
cy_44 equ cy_SKB
cy_45 equ 3
cy_46 equ 5
cy_47 equ 5
cy_48 equ 3
cy_49 equ 2
cy_4a equ 2
cy_4b equ 2
cy_4c equ 3
cy_4d equ 4
cy_4e equ 6
cy_4f equ 6
cy_50 equ cy_Bcc
cy_51 equ 5
cy_52 equ cy_CIM
cy_53 equ 8
cy_54 equ 4
cy_55 equ 4
cy_56 equ 6
cy_57 equ 6
cy_58 equ 2
cy_59 equ 4
cy_5a equ cy_NOP
cy_5b equ 7
cy_5c equ cy_SKW
cy_5d equ 4
cy_5e equ 7
cy_5f equ 7
cy_60 equ 6
cy_61 equ 6
cy_62 equ cy_CIM
cy_63 equ 8
cy_64 equ cy_SKB
cy_65 equ 3
cy_66 equ 5
cy_67 equ 5
cy_68 equ 4
cy_69 equ 2
cy_6a equ 2
cy_6b equ 2
cy_6c equ 5
cy_6d equ 4
cy_6e equ 6
cy_6f equ 6
cy_70 equ cy_Bcc
cy_71 equ 5
cy_72 equ cy_CIM
cy_73 equ 8
cy_74 equ 4
cy_75 equ 4
cy_76 equ 6
cy_77 equ 6
cy_78 equ 2
cy_79 equ 4
cy_7a equ cy_NOP
cy_7b equ 7
cy_7c equ cy_SKW
cy_7d equ 4
cy_7e equ 7
cy_7f equ 7
cy_80 equ 2
cy_81 equ 6
cy_82 equ 2
cy_83 equ 6
cy_84 equ 3
cy_85 equ 3
cy_86 equ 3
cy_87 equ 3
cy_88 equ 2
cy_89 equ 2
cy_8a equ 2
cy_8b equ cy_29
cy_8c equ 4
cy_8d equ 4
cy_8e equ 4
cy_8f equ 4
cy_90 equ cy_Bcc
cy_91 equ 6
cy_92 equ cy_CIM
cy_93 equ 6
cy_94 equ 4
cy_95 equ 4
cy_96 equ 4
cy_97 equ 4
cy_98 equ 2
cy_99 equ 5
cy_9a equ 2
cy_9b equ 5
cy_9c equ 5
cy_9d equ 5
cy_9e equ cy_9b
cy_9f equ cy_9b
cy_a0 equ 2
cy_a1 equ 6
cy_a2 equ 2
cy_a3 equ 6
cy_a4 equ 3
cy_a5 equ 3
cy_a6 equ 3
cy_a7 equ 3
cy_a8 equ 2
cy_a9 equ 2
cy_aa equ 2
cy_ab equ 2
cy_ac equ 4
cy_ad equ 4
cy_ae equ 4
cy_af equ 4
cy_b0 equ cy_Bcc
cy_b1 equ 5
cy_b2 equ cy_CIM
cy_b3 equ 5
cy_b4 equ 4
cy_b5 equ 4
cy_b6 equ 4
cy_b7 equ 4
cy_b8 equ 2
cy_b9 equ 4
cy_ba equ 2
cy_bb equ 4
cy_bc equ 4
cy_bd equ 4
cy_be equ 4
cy_bf equ 4
cy_c0 equ 2
cy_c1 equ 6
cy_c2 equ 2
cy_c3 equ 8
cy_c4 equ 3
cy_c5 equ 3
cy_c6 equ 5
cy_c7 equ 5
cy_c8 equ 2
cy_c9 equ 2
cy_ca equ 2
cy_cb equ 2
cy_cc equ 4
cy_cd equ 4
cy_ce equ 6
cy_cf equ 6
cy_d0 equ cy_Bcc
cy_d1 equ 5
cy_d2 equ 2
cy_d3 equ 8
cy_d4 equ 4
cy_d5 equ 4
cy_d6 equ 6
cy_d7 equ 6
cy_d8 equ 2
cy_d9 equ 4
cy_da equ cy_NOP
cy_db equ 7
cy_dc equ cy_SKW
cy_dd equ 4
cy_de equ 7
cy_df equ 7
cy_e0 equ 2
cy_e1 equ 6
cy_e2 equ 2
cy_e3 equ 8
cy_e4 equ 3
cy_e5 equ 3
cy_e6 equ 5
cy_e7 equ 5
cy_e8 equ 2
cy_e9 equ 2
cy_ea equ cy_NOP
cy_eb equ cy_e9
cy_ec equ 4
cy_ed equ 4
cy_ee equ 6
cy_ef equ 6
cy_f0 equ cy_Bcc
cy_f1 equ 5
cy_f2 equ 2
cy_f3 equ 8
cy_f4 equ 4
cy_f5 equ 4
cy_f6 equ 6
cy_f7 equ 6
cy_f8 equ 2
cy_f9 equ 4
cy_fa equ cy_NOP
cy_fb equ 7
cy_fc equ cy_SKW
cy_fd equ 4
cy_fe equ 7
cy_ff equ 7
    dc.l cy_00,cy_01,cy_02,cy_03,cy_04,cy_05,cy_06,cy_07
    dc.l cy_08,cy_09,cy_0a,cy_0b,cy_0c,cy_0d,cy_0e,cy_0f
    dc.l cy_10,cy_11,cy_12,cy_13,cy_14,cy_15,cy_16,cy_17
    dc.l cy_18,cy_19,cy_1a,cy_1b,cy_1c,cy_1d,cy_1e,cy_1f
    dc.l cy_20,cy_21,cy_22,cy_23,cy_24,cy_25,cy_26,cy_27
    dc.l cy_28,cy_29,cy_2a,cy_2b,cy_2c,cy_2d,cy_2e,cy_2f
    dc.l cy_30,cy_31,cy_32,cy_33,cy_34,cy_35,cy_36,cy_37
    dc.l cy_38,cy_39,cy_3a,cy_3b,cy_3c,cy_3d,cy_3e,cy_3f
    dc.l cy_40,cy_41,cy_42,cy_43,cy_44,cy_45,cy_46,cy_47
    dc.l cy_48,cy_49,cy_4a,cy_4b,cy_4c,cy_4d,cy_4e,cy_4f
    dc.l cy_50,cy_51,cy_52,cy_53,cy_54,cy_55,cy_56,cy_57
    dc.l cy_58,cy_59,cy_5a,cy_5b,cy_5c,cy_5d,cy_5e,cy_5f
    dc.l cy_60,cy_61,cy_62,cy_63,cy_64,cy_65,cy_66,cy_67
    dc.l cy_68,cy_69,cy_6a,cy_6b,cy_6c,cy_6d,cy_6e,cy_6f
    dc.l cy_70,cy_71,cy_72,cy_73,cy_74,cy_75,cy_76,cy_77
    dc.l cy_78,cy_79,cy_7a,cy_7b,cy_7c,cy_7d,cy_7e,cy_7f
    dc.l cy_80,cy_81,cy_82,cy_83,cy_84,cy_85,cy_86,cy_87
    dc.l cy_88,cy_89,cy_8a,cy_8b,cy_8c,cy_8d,cy_8e,cy_8f
    dc.l cy_90,cy_91,cy_92,cy_93,cy_94,cy_95,cy_96,cy_97
    dc.l cy_98,cy_99,cy_9a,cy_9b,cy_9c,cy_9d,cy_9e,cy_9f
    dc.l cy_a0,cy_a1,cy_a2,cy_a3,cy_a4,cy_a5,cy_a6,cy_a7
    dc.l cy_a8,cy_a9,cy_aa,cy_ab,cy_ac,cy_ad,cy_ae,cy_af
    dc.l cy_b0,cy_b1,cy_b2,cy_b3,cy_b4,cy_b5,cy_b6,cy_b7
    dc.l cy_b8,cy_b9,cy_ba,cy_bb,cy_bc,cy_bd,cy_be,cy_bf
    dc.l cy_c0,cy_c1,cy_c2,cy_c3,cy_c4,cy_c5,cy_c6,cy_c7
    dc.l cy_c8,cy_c9,cy_ca,cy_cb,cy_cc,cy_cd,cy_ce,cy_cf
    dc.l cy_d0,cy_d1,cy_d2,cy_d3,cy_d4,cy_d5,cy_d6,cy_d7
    dc.l cy_d8,cy_d9,cy_da,cy_db,cy_dc,cy_dd,cy_de,cy_df
    dc.l cy_e0,cy_e1,cy_e2,cy_e3,cy_e4,cy_e5,cy_e6,cy_e7
    dc.l cy_e8,cy_e9,cy_ea,cy_eb,cy_ec,cy_ed,cy_ee,cy_ef
    dc.l cy_f0,cy_f1,cy_f2,cy_f3,cy_f4,cy_f5,cy_f6,cy_f7
    dc.l cy_f8,cy_f9,cy_fa,cy_fb,cy_fc,cy_fd,cy_fe,cy_ff

