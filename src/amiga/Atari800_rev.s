VERSION = 2
REVISION = 4

.macro DATE
.ascii "16.3.2005"
.endm

.macro VERS
.ascii "Atari800 2.4"
.endm

.macro VSTRING
.ascii "Atari800 2.4 (16.3.2005)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: Atari800 2.4 (16.3.2005)"
.byte 0
.endm
