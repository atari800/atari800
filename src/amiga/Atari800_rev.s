VERSION = 2
REVISION = 3

.macro DATE
.ascii "29.12.2004"
.endm

.macro VERS
.ascii "Atari800 2.3"
.endm

.macro VSTRING
.ascii "Atari800 2.3 (29.12.2004)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: Atari800 2.3 (29.12.2004)"
.byte 0
.endm
