
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "atari.h"
#include "config.h"
#include "log.h"
#include "cpu.h"
#include "memory-d.h"
#include "pia.h"

int start_binloading = 0;
static int bin_fd = -1;

/* Read a word from file */
static int BIN_read_word(void)
{
	UBYTE buf[2];
	if (read(bin_fd, buf, 2) != 2) {
		close(bin_fd);
		bin_fd = -1;
		if (start_binloading) {
			start_binloading = 0;
			Aprint("binload: not valid BIN file");
			return -1;
		}
		regPC = dGetByte(0x2e0) | (dGetByte(0x2e1) << 8);
		return -1;
	}
	return buf[0] | (buf[1] << 8);
}

/* Start or continue loading */
void BIN_loader_cont(void)
{
	int from;
	int to;
	UBYTE byte;

	if (bin_fd < 0)
		return;
	if (start_binloading)
	{
		dPutByte(0x244, 0);
		dPutByte(0x09, 1);
	}
	else
		regS += 2;	/* pop ESC code */

	dPutByte(0x2e3, 0xd7);
	do {
		do
			from = BIN_read_word();
		while (from == 0xffff);
		if (from < 0)
			return;

		to = BIN_read_word();
		if (to < 0)
			return;

		if (start_binloading) {
			dPutByte(0x2e0, from & 0xff);
			dPutByte(0x2e1, from >> 8);
			start_binloading = 0;
		}

		to++;
		to &= 0xffff;
		do {
			if (read(bin_fd, &byte, 1) == 0) {
				close(bin_fd);
				bin_fd = -1;
				regPC = dGetByte(0x2e0) | (dGetByte(0x2e1) << 8);
				if (dGetByte(0x2e3) != 0xd7) {
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);		/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetByte(0x2e2) | (dGetByte(0x2e3) << 8);
				}
				return;
			}
			PutByte(from, byte);
			from++;
			from &= 0xffff;
		} while (from != to);
	} while (dGetByte(0x2e3) == 0xd7);

	dPutByte(0x0100 + regS--, ESC_BINLOADER_CONT);
	dPutByte(0x0100 + regS--, 0xf2);	/* ESC */
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetByte(0x2e2) | (dGetByte(0x2e3) << 8);
	SetC;
}

/* Fake boot sector to call BIN_loader_cont at boot time */
int BIN_loade_start(UBYTE *buffer)
{
	buffer[0] = 0x00;	/* ignored */
	buffer[1] = 0x01;	/* one boot sector */
	buffer[2] = 0x00;	/* start at memory location 0x0700 */
	buffer[3] = 0x07;
	buffer[4] = 0x77;	/* reset reboots (0xe477 = Atari OS Coldstart) */
	buffer[5] = 0xe4;
	buffer[6] = 0xf2;	/* ESC */
	buffer[7] = ESC_BINLOADER_CONT;
	return 'C';
}

/* Load BIN file, returns TRUE if ok */
int BIN_loader(char *filename)
{
	UBYTE buf[2];
	if (bin_fd >= 0)		/* close previously open file */
		close(bin_fd);
	if ((bin_fd = open(filename, O_RDONLY | O_BINARY)) < 0) {	/* open */
		Aprint("binload: can't open %s", filename);
		return FALSE;
	}
	if (read(bin_fd, buf, 2) != 2 || buf[0] != 0xff || buf[1] != 0xff) {	/* check header */
		close(bin_fd);
		bin_fd = -1;
		Aprint("binload: not valid BIN file");
		return FALSE;
	}

	Coldstart();			/* reboot */
	start_binloading = 1;	/* force SIO to call BIN_loade_start at boot */
	return TRUE;
}
