.include equates.m65
*=  $0600
PHA 
TXA 
PHA 
TYA 
PHA 
LDA 1536
STA WSYNC
;
STA WSYNC
STA WSYNC
STA WSYNC
STA WSYNC
STA WSYNC
STA WSYNC
STA WSYNC
LDY RANDOM ;sta $D20B ;POTGO
STA WSYNC
;NOP
;NOP
;NOP
;NOP
;NOP
;NOP
;NOP
;LDX 203
;
;STA COLPF0+2
.byte 99 ;change this to -1 in the data statements 
LDA RANDOM;LDA $D207 ;POT7
STA 207
;STA 204
LDA VCOUNT
STA 205
STY 206 ;save old random
LDA 208 ;disable dli flag
CMP #$0
BEQ SKIP
LDA #64
STA NMIEN
SKIP sta wsync
sta wsync
lda 1537
sta HPOSP0
lda 1538
sta SIZEP0
lda 1539
sta GRAFP0
PLA
TAY 
PLA 
TAX 
PLA 
RTI 
;jmp SKIP2
;REG
;.dbyte $0001
;SKIP2
;LDA REG
;ror a
;STA REG



