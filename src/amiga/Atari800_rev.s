VERSION = 2
REVISION = 1

.macro DATE
.ascii "12.9.2004"
.endm

.macro VERS
.ascii "Atari800 2.1"
.endm

.macro VSTRING
.ascii "Atari800 2.1 (12.9.2004)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: Atari800 2.1 (12.9.2004)"
.byte 0
.endm
