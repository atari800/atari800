/*
 * Convert an Atari 400/800/... cartridge rom to a CART file for the
 * atari800 emulator (atari800.github.io)
 *
 * Copyright (C) 2001-2010 Piotr Fusik
 * Copyright (C) 2001-2010 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include "cartridge.h"

#define URL "https://github.com/atari800/atari800/blob/master/DOC/cart.txt"
#define MIN_SIZE 2048		/* Minimum cartridge size */

long checklen(FILE *fp, int carttype);
void convert(char *romfile, char *cartfile, int32_t type);
void list_cartridges(void);

typedef struct {
  uint8_t cart[4];
  uint8_t type[4];
  uint8_t csum[4];
  uint8_t zero[4];
} header_t;


long checklen(FILE *fp, int ctype)
{
	long l;
	
	fseek(fp, 0, SEEK_END);
	l = ftell(fp);
	rewind(fp);
	if (l < MIN_SIZE) {
		fprintf(stderr, "Romfile size is too small (<%d bytes)\n", MIN_SIZE);
		exit(1);
	}
	if (l > CARTRIDGES[ctype].kb) {
		fprintf(stderr,
			"Romfile size is too large for cartridge type %d - %s (>%d KB)\n",
			ctype, CARTRIDGES[ctype].description, CARTRIDGES[ctype].kb);
		exit(1);
	}    
	
	return l;
}

void convert(char *romfile, char *cartfile, int32_t type)
{
	FILE *fp;
	long len;
	uint8_t *rom;
	uint32_t csum;
	header_t h;

	printf("Converting %s to %s (type %d - %s)\n", romfile, cartfile, type,
	       CARTRIDGES[type].description);

	/* Read the original rom file */
	fp = fopen(romfile, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Error opening rom file '%s': %s\n",
			romfile, strerror(errno));
		exit(1);
	}
	len = checklen(fp, type);
	rom = malloc(len);
	if (rom == NULL) {
		fprintf(stderr, "Allocation error: %s\n", strerror(errno));
		exit(1);
	}
  
	if (fread(rom, 1, len, fp) < len) {
		fprintf(stderr, "Read error: %s\n", strerror(errno));
		exit(1);
	}
	fclose(fp);

	/* Create the CART header */
	csum = CARTRIDGE_Checksum(rom, len);
	memset(&h, 0, sizeof(h));
	h.cart[0] = 'C';
	h.cart[1] = 'A';
	h.cart[2] = 'R';
	h.cart[3] = 'T';

	h.type[0] = (type & 0xff000000) >> 24;
	h.type[1] = (type & 0xff0000) >> 16;
	h.type[2] = (type & 0xff00) >> 8;
	h.type[3] = type & 0xff;

	h.csum[0] = (csum & 0xff000000) >> 24;
	h.csum[1] = (csum & 0xff0000) >> 16;
	h.csum[2] = (csum & 0xff00) >> 8;
	h.csum[3] = csum & 0xff;

	/* Create the new file, merge the header and rom data */
	fp = fopen(cartfile, "w");
	if (fp == NULL) {
		fprintf(stderr, "Error creating cartridge file '%s': %s\n",
			cartfile, strerror(errno));
		exit(1);
	}

	if (fwrite(&h, sizeof(h), 1, fp) != 1) {
		fprintf(stderr, "Error writing CAR header: %s\n", strerror(errno));
		exit(1);
	}
	if (fwrite(rom, len, 1, fp) != 1) {
		fprintf(stderr, "Error writing ROM body: %s\n", strerror(errno));
		exit(1);
	}

	fclose(fp);
}

void list_cartridges(void)
{
	int i;

	printf("Supported cartridge types:\n");
	for (i = 1; i <= CARTRIDGE_LAST_SUPPORTED; i++)
		printf("  Type %d: %s\n", i, CARTRIDGES[i].description);
	
}

int main(int argc, char *argv[])
{
	int32_t ctype;

	if (argc == 2 && strcmp(argv[1], "-l") == 0) {
		list_cartridges();
		exit(0);
	}
  
	if (argc != 4) {
		printf("%s - Convert romfile to cartridge file\n", argv[0]);
		printf("\t%s <romfile> <cartfile> <carttype>\n", argv[0]);
		printf("\t%s -l -- List supported cartridge types\n", argv[0]);
		exit(1);
	}

	ctype = atol(argv[3]);
	if (ctype <= CARTRIDGE_NONE || ctype > CARTRIDGE_LAST_SUPPORTED) {
		fprintf(stderr, "Invalid cartridge type: %s\n", argv[3]);
		fprintf(stderr, "See valid cartridge types at:\n");
		fprintf(stderr, "%s\n", URL);
		exit(1);
	}
     
	convert(argv[1], argv[2], ctype);

	return 0;
}
