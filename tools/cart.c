/*
 * Convert an Atari 400/800/... cartridge rom to a CART file for the
 * atari800 emulator (atari800.github.io)
 * 
 * See https://github.com/atari800/atari800/blob/master/DOC/cart.txt 
 *
 * 4 Jan 2020 - Walt Drummond <walt@drummond.us>
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

#define MIN_CARTTYPE 1
#define MAX_CARTTYPE 70
#define URL "https://github.com/atari800/atari800/blob/master/DOC/cart.txt"

uint32_t CARTRIDGE_Checksum(const uint8_t *image, int nbytes);
long flen(FILE *fp);
void convert(char *romfile, char *cartfile, int32_t type);

typedef struct {
  uint8_t cart[4];
  uint8_t type[4];
  uint8_t csum[4];
  uint8_t zero[4];
} header;

/* Taken directly from atari800-4.1.0/src/cartridge.c */
uint32_t CARTRIDGE_Checksum(const uint8_t *image, int nbytes)
{
	int checksum = 0;
	while (nbytes > 0) {
		checksum += *image++;
		nbytes--;
	}
	return checksum;
}

long flen(FILE *fp)
{
  long l;
  
  fseek(fp, 0, SEEK_END);
  l = ftell(fp);
  rewind(fp);
  return l;
}

void convert(char *romfile, char *cartfile, int32_t type)
{
  FILE *fp;
  long len;
  uint8_t *rom;
  uint32_t csum;
  header h;

  printf("Converting %s to %s (type %d)\n", romfile, cartfile, type);

  /* Read the original rom file */
  fp = fopen(romfile, "rb");
  if (fp == NULL) {
    printf("Error opening rom file '%s': %s\n", romfile, strerror(errno));
    exit(1);
  }
  len = flen(fp);
  rom = malloc(len);
  if (rom == NULL) {
      printf("Allocation error: %s\n", strerror(errno));
      exit(1);
  }
  
  if (fread(rom, 1, len, fp) < len) {
    printf("Read error: %s\n", strerror(errno));
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
    printf("Error creating cartridge file '%s': %s\n",
	   cartfile, strerror(errno));
    exit(1);
  }

  if (fwrite(&h, sizeof(h), 1, fp) != 1) {
    printf("Error writing CAR header: %s\n", strerror(errno));
    exit(1);
  }
  if (fwrite(rom, len, 1, fp) != 1) {
    printf("Error writing ROM body: %s\n", strerror(errno));
    exit(1);
  }

  fclose(fp);
}

int main(int argc, char *argv[]) {

  int32_t ctype;
  
  if (argc != 4) {
    printf("%s: romfile cartfile carttype\n", argv[0]);
    exit(1);
  }

  ctype = atol(argv[3]);
  if (ctype < MIN_CARTTYPE || ctype > MAX_CARTTYPE) {
    printf("Invalid cartridge type: %s\n", argv[3]);
    printf("See valid cartridge types at:\n");
    printf("%s\n", URL);
    exit(1);
  }
     
  convert(argv[1], argv[2], ctype);

  return 0;
}
