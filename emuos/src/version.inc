;	Altirra - Atari 800/800XL emulator
;	Kernel ROM replacement - version info
;	Copyright (C) 2008-2020 Avery Lee
;
;	Copying and distribution of this file, with or without modification,
;	are permitted in any medium without royalty provided the copyright
;	notice and this notice are preserved.  This file is offered as-is,
;	without any warranty.

.macro _KERNELSTR_VERSION
		dta		'3.40'
.endm

.macro _KERNELSTR_VERSION_INTERNAL
		dta		"3.40"
.endm
