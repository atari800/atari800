	xdef	_timer_A
	xref	_timer_A_v_C

_timer_A:
	tst.b	semafor
	bne.s	konec
	st	semafor

	movem.l	d0-d7/a0-a6,zaloha_registru
	jsr	_timer_A_v_C
	movem.l	zaloha_registru,d0-d7/a0-a6

	sf	semafor
konec	rte

	section	bss
zaloha_registru:
	ds.l	15
zaloha_SR:
	ds.w	1
semafor	ds.b	1
