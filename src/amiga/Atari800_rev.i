VERSION		EQU	2
REVISION	EQU	1

DATE	MACRO
		dc.b '12.9.2004'
		ENDM

VERS	MACRO
		dc.b 'Atari800 2.1'
		ENDM

VSTRING	MACRO
		dc.b 'Atari800 2.1 (12.9.2004)',13,10,0
		ENDM

VERSTAG	MACRO
		dc.b 0,'$VER: Atari800 2.1 (12.9.2004)',0
		ENDM
