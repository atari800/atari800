VERSION = 2
REVISION = 2

.macro DATE
.ascii "8.10.2004"
.endm

.macro VERS
.ascii "Atari800 2.2"
.endm

.macro VSTRING
.ascii "Atari800 2.2 (8.10.2004)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: Atari800 2.2 (8.10.2004)"
.byte 0
.endm
