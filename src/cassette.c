/*
 * cassette.c - cassette emulation
 *
 * Copyright (C) 2001 Piotr Fusik
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "cpu.h"
#include "cassette.h"
#include "esc.h"
#include "log.h"
#include "memory.h"
#include "sio.h"
#include "util.h"
#include "pokey.h"

#define MAX_BLOCKS 2048

static FILE *cassette_file = NULL;
static int cassette_isCAS;
static UBYTE *cassette_buffer = NULL;
static size_t buffer_size;
 /* Standard record length, needed by ReadRecord() when reading raw files */
enum { DEFAULT_BUFFER_SIZE = 132 };
static ULONG cassette_block_offset[MAX_BLOCKS];
static SLONG cassette_elapsedtime;  /* elapsed time since begin of file */
                                    /* in scanlines */
static SLONG cassette_savetime;	    /* helper for cas save */
static SLONG cassette_nextirqevent; /* timestamp of next irq in scanlines */

char CASSETTE_filename[FILENAME_MAX] = "";
CASSETTE_status_t CASSETTE_status = CASSETTE_STATUS_NONE;
int CASSETTE_record = FALSE;
static int cassette_writable = FALSE;
static int cassette_readable = FALSE;

char CASSETTE_description[CASSETTE_DESCRIPTION_MAX] = "";
static int cassette_current_blockbyte = 0;
int CASSETTE_current_block;
static int cassette_max_blockbytes = 0;
int CASSETTE_max_block = 0;
static int cassette_gapdelay = 0;	/* in ms, includes leader and all gaps */
static int cassette_motor = 0;
static int cassette_baudrate = 600;
static int cassette_baudblock[MAX_BLOCKS];

int CASSETTE_hold_start_on_reboot = 0;
int CASSETTE_hold_start = 0;
int CASSETTE_press_space = 0;
static int eof_of_tape = 0;

typedef struct {
	char identifier[4];
	UBYTE length_lo;
	UBYTE length_hi;
	UBYTE aux_lo;
	UBYTE aux_hi;
} CAS_Header;
/*
Just for remembering - CAS format in short:
It consists of chunks. Each chunk has a header, possibly followed by data.
If a header is unknown or unexpected it may be skipped by the length of the
header (8 bytes), and additionally the length given in the length-field(s).
There are (until now) 3 types of chunks:
-CAS file marker, has to be at begin of file - identifier "FUJI", length is
number of characters of an (optional) ascii-name (without trailing 0), aux
is always 0.
-baud rate selector - identifier "baud", length always 0, aux is rate in baud
(usually 600; one byte is 8 bits + startbit + stopbit, makes 60 bytes per
second).
-data record - identifier "data", length is length of the data block (usually
$84 as used by the OS), aux is length of mark tone (including leader and gaps)
just before the record data in milliseconds.
*/

/* Call this function after each change of
   cassette_motor, CASSETTE_status or eof_of_tape. */
static void UpdateFlags(void)
{
	cassette_readable = cassette_motor &&
	                    (CASSETTE_status == CASSETTE_STATUS_READ_WRITE ||
	                     CASSETTE_status == CASSETTE_STATUS_READ_ONLY) &&
	                     !eof_of_tape;
	cassette_writable = cassette_motor &&
	                    CASSETTE_status == CASSETTE_STATUS_READ_WRITE;
}

int CASSETTE_ReadConfig(char *string, char *ptr)
{
	if (strcmp(string, "CASSETTE_FILENAME") == 0)
		Util_strlcpy(CASSETTE_filename, ptr, sizeof(CASSETTE_filename));
	else if (strcmp(string, "CASSETTE_LOADED") == 0) {
		int value = Util_sscanbool(ptr);
		if (value == -1)
			return FALSE;
		CASSETTE_status = (value ? CASSETTE_STATUS_READ_WRITE : CASSETTE_STATUS_NONE);
	}
	else return FALSE;
	return TRUE;
}

void CASSETTE_WriteConfig(FILE *fp)
{
	fprintf(fp, "CASSETTE_FILENAME=%s\n", CASSETTE_filename);
	fprintf(fp, "CASSETTE_LOADED=%d\n", CASSETTE_status != CASSETTE_STATUS_NONE);
}

int CASSETTE_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc);		/* is argument available? */
		int a_m = FALSE;			/* error, argument missing! */

		if (strcmp(argv[i], "-tape") == 0) {
			if (i_a) {
				Util_strlcpy(CASSETTE_filename, argv[++i], sizeof(CASSETTE_filename));
				CASSETTE_status = CASSETTE_STATUS_READ_WRITE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-boottape") == 0) {
			if (i_a) {
				Util_strlcpy(CASSETTE_filename, argv[++i], sizeof(CASSETTE_filename));
				CASSETTE_status = CASSETTE_STATUS_READ_WRITE;
				CASSETTE_hold_start = 1;
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-tape <file>     Insert cassette image");
				Log_print("\t-boottape <file> Insert cassette image and boot it");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}

	*argc = j;

	/* If CASSETTE_status was set in this function or in CASSETTE_ReadConfig(),
	   then tape is to be mounted. */
	if (CASSETTE_status != CASSETTE_STATUS_NONE && CASSETTE_filename[0] != '\0')
		CASSETTE_Insert(CASSETTE_filename);

	return TRUE;
}

void CASSETTE_Exit(void)
{
	CASSETTE_Remove();
}

/* Gets information about the cassette image. Returns TRUE if ok.
   To get information without attaching file, use:
   char description[CASSETTE_DESCRIPTION_MAX];
   int last_block;
   int isCAS;
   int writable;
   CASSETTE_CheckFile(filename, NULL, description, &last_block, &isCAS, &writable);
*/
int CASSETTE_CheckFile(const char *filename, FILE **fp, char *description, int *last_block, int *isCAS, int *writable)
{
	FILE *f;
	CAS_Header header;
	int blocks;

	/* Check if the file is writable. If not, recording will be disabled. */
	f = fopen(filename, "rb+");
	if (writable != NULL)
		*writable = f != NULL;
	/* If opening for reading+writing failed, reopen it as read-only. */
	if (f == NULL)
		f = fopen(filename, "rb");
	if (f == NULL)
		return FALSE;
	if (description)
		description[0] = '\0';

	if (fread(&header, 1, 6, f) == 6
		&& header.identifier[0] == 'F'
		&& header.identifier[1] == 'U'
		&& header.identifier[2] == 'J'
		&& header.identifier[3] == 'I') {
		/* CAS file */
		UWORD length;
		UWORD skip;
		if (isCAS)
			*isCAS = TRUE;
		fseek(f, 2L, SEEK_CUR);	/* ignore the aux bytes */

		/* read or skip file description */
		skip = length = header.length_lo + (header.length_hi << 8);
		if (description) {
			if (length < CASSETTE_DESCRIPTION_MAX)
				skip = 0;
			else
				skip -= CASSETTE_DESCRIPTION_MAX - 1;
			if (fread(description, 1, length - skip, f) < (length - skip)) {
				Log_print("Error reading cassette file.\n");
			}
		}
		fseek(f, skip, SEEK_CUR);

		/* count number of blocks */
		blocks = 0;
		if (fp) {
			cassette_baudblock[0]=600;
			cassette_block_offset[0] = ftell(f);
		}
		for (;;) {
			/* chunk header is always 8 bytes */
			if (fread(&header, 1, 8, f) != 8)
				break;
			length = header.length_lo + (header.length_hi << 8);
			if (header.identifier[0] == 'b'
			 && header.identifier[1] == 'a'
			 && header.identifier[2] == 'u'
			 && header.identifier[3] == 'd') {
				cassette_baudrate=header.aux_lo + (header.aux_hi << 8);
				if (fp)
					cassette_block_offset[blocks] += length + 8;
			}
			else if (header.identifier[0] == 'd'
			 && header.identifier[1] == 'a'
			 && header.identifier[2] == 't'
			 && header.identifier[3] == 'a') {
				if (fp)
					cassette_baudblock[blocks] = cassette_baudrate;
				if (++blocks >= MAX_BLOCKS) {
					--blocks;
					break;
				}
				if (fp)
					cassette_block_offset[blocks] = cassette_block_offset[blocks - 1] + length + 8;
			}
			/* skip possibly present data block */
			fseek(f, length, SEEK_CUR);
		}
	}
	else {
		/* raw file */
		int file_length = Util_flen(f);
		blocks = ((file_length + 127) >> 7) + 1;
		if (isCAS)
			*isCAS = FALSE;
	}
	if (last_block)
		*last_block = blocks;
	if (fp)
		*fp = f;
	else
		fclose(f);
	return TRUE;
}

/* Write the initial FUJI and baud blocks of the CAS file. */
static int WriteInitialBlocks(FILE *fp, char const *description)
{
	CAS_Header header;
	size_t desc_len = strlen(description);
	memset(&header, 0, sizeof(header));
	/* write CAS-header */
	header.length_lo = (UBYTE) desc_len;
	header.length_hi = (UBYTE) (desc_len >> 8);
	if (fwrite("FUJI", 1, 4, fp) != 4
	    || fwrite(&header.length_lo, 1, 4, fp) != 4
	    || fwrite(description, 1, desc_len, fp) != desc_len)
		return FALSE;

	memset(&header, 0, sizeof(header));
	header.aux_lo = cassette_baudrate & 0xff; /* provisional: 600 baud */
	header.aux_hi = cassette_baudrate >> 8;
	if (fwrite("baud", 1, 4, fp) != 4
	    || fwrite(&header.length_lo, 1, 4, fp) != 4)
		return FALSE;
	return TRUE;
}

static UWORD WriteRecord(int len)
{
	CAS_Header header;

	/* on a raw file, saving is denied because it can hold
	    only 1 file and could cause confusion */
	if (!cassette_isCAS)
		return FALSE;
	/* always append */
	if (fseek(cassette_file, cassette_block_offset[CASSETTE_max_block], SEEK_SET) != 0)
		return FALSE;
	/* write recordheader */
	Util_strncpy(header.identifier, "data", 4);
	header.length_lo = len & 0xFF;
	header.length_hi = (len >> 8) & 0xFF;
	header.aux_lo = cassette_gapdelay & 0xff;
	header.aux_hi = (cassette_gapdelay >> 8) & 0xff;
	cassette_gapdelay = 0;
	if (fwrite(&header, 1, 8, cassette_file) != 8)
		return FALSE;
	/* Saving is supported only with standard baudrate. */
	cassette_baudblock[CASSETTE_max_block] = 600;
	CASSETTE_max_block++;
	cassette_block_offset[CASSETTE_max_block] = cassette_block_offset[CASSETTE_max_block - 1] + len + 8;
	CASSETTE_current_block = CASSETTE_max_block + 1;
	/* write record */
	return fwrite(cassette_buffer, 1, len, cassette_file);
}

int CASSETTE_CreateCAS(const char *filename, const char *description) {
	FILE *fp = NULL;
	/* create new file */
	fp = fopen(filename, "wb+");
	if (fp == NULL)
		return FALSE;

	if (!WriteInitialBlocks(fp, description)) {
		fclose(fp);
		return FALSE;
	}

	CASSETTE_Remove(); /* Unmount any previous tape image. */
	cassette_file = fp;
	Util_strlcpy(CASSETTE_filename, filename, sizeof(CASSETTE_filename));
	if (description != NULL)
		Util_strlcpy(CASSETTE_description, description, sizeof(CASSETTE_description));
	cassette_isCAS = TRUE;
	cassette_elapsedtime = 0;
	cassette_savetime = 0;
	cassette_nextirqevent = 0;
	cassette_current_blockbyte = 0;
	cassette_max_blockbytes = 0;
	CASSETTE_current_block = 1;
	CASSETTE_max_block = 0;
	cassette_block_offset[0] = strlen(description) + 16;
	cassette_gapdelay = 0;
	eof_of_tape = 0;
	cassette_baudrate = 600;
	CASSETTE_record = TRUE;
	CASSETTE_status = CASSETTE_STATUS_READ_WRITE;
	UpdateFlags();
	cassette_buffer = Util_malloc((buffer_size = DEFAULT_BUFFER_SIZE) * sizeof(UBYTE));

	return TRUE;
}

int CASSETTE_Insert(const char *filename)
{
	int writable;
	CASSETTE_Remove();
	/* Guard against providing CASSETTE_filename as parameter. */
	if (CASSETTE_filename != filename)
		strcpy(CASSETTE_filename, filename);
	cassette_elapsedtime = 0;
	cassette_savetime = 0;
	cassette_nextirqevent = 0;
	cassette_current_blockbyte = 0;
	cassette_max_blockbytes = 0;
	CASSETTE_current_block = 1;
	cassette_gapdelay = 0;
	eof_of_tape = 0;
	cassette_baudrate = 600;
	if (!CASSETTE_CheckFile(filename, &cassette_file, CASSETTE_description,
	                        &CASSETTE_max_block, &cassette_isCAS, &writable))
		return FALSE;
	if (cassette_isCAS)
		CASSETTE_status = (writable ? CASSETTE_STATUS_READ_WRITE : CASSETTE_STATUS_READ_ONLY);
	else /* No support for writing raw files */
		CASSETTE_status = CASSETTE_STATUS_READ_ONLY;
	CASSETTE_record = FALSE;
	UpdateFlags();
	cassette_buffer = Util_malloc((buffer_size = DEFAULT_BUFFER_SIZE) * sizeof(UBYTE));

	return TRUE;
}

void CASSETTE_Remove(void)
{
	if (cassette_file != NULL) {
		/* save last block, ignore trailing space tone */
		if ((cassette_current_blockbyte > 0) && cassette_writable && CASSETTE_record)
			WriteRecord(cassette_current_blockbyte);

		fclose(cassette_file);
		cassette_file = NULL;
		free(cassette_buffer);
	}
	CASSETTE_max_block = 0;
	CASSETTE_status = CASSETTE_STATUS_NONE;
	CASSETTE_description[0] = '\0';
	UpdateFlags();
}

/* Enlarge cassette_buffer to LENGTH if needed. */
static void EnlargeBuffer(size_t length)
{
	if (buffer_size < length) {
		/* Enlarge the buffer at least 2 times. */
		buffer_size *= 2;
		if (buffer_size < length)
			buffer_size = length;
		cassette_buffer = Util_realloc(cassette_buffer, buffer_size * sizeof(UBYTE));
	}
}

/* Read a record from the file. Returns block length (with checksum).
   Writes length of pre-record gap (in ms) into *gap. */
static int ReadRecord(int *gap)
{
	int length = 0;
	if (cassette_isCAS) {
		CAS_Header header;

		/* While reading a block, offset by 4 to skip its 4-byte name (assume it's "data") */
		fseek(cassette_file, cassette_block_offset[CASSETTE_current_block-1] + 4, SEEK_SET);
		if (fread(&header.length_lo, 1, 4, cassette_file) < 4) {
			Log_print("Error reading cassette file.\n");
		}
		length = header.length_lo + (header.length_hi << 8);
		*gap = header.aux_lo + (header.aux_hi << 8);
		/* read block into buffer */
		EnlargeBuffer(length);
		if (fread(cassette_buffer, 1, length, cassette_file) < length) {
			Log_print("Error reading cassette file.\n");
		}
	}
	else {
		length = 132;
		/* Don't enlarge buffer - its default size is at least 132. */
		*gap = (CASSETTE_current_block == 1 ? 19200 : 260);
		cassette_buffer[0] = 0x55;
		cassette_buffer[1] = 0x55;
		if (CASSETTE_current_block >= CASSETTE_max_block) {
			/* EOF record */
			cassette_buffer[2] = 0xfe;
			memset(cassette_buffer + 3, 0, 128);
		}
		else {
			int bytes;
			fseek(cassette_file, (CASSETTE_current_block - 1) * 128, SEEK_SET);
			bytes = fread(cassette_buffer + 3, 1, 128, cassette_file);
			if (bytes < 128) {
				cassette_buffer[2] = 0xfa; /* non-full record */
				memset(cassette_buffer + 3 + bytes, 0, 127 - bytes);
				cassette_buffer[0x82] = bytes;
			}
			else
				cassette_buffer[2] = 0xfc;	/* full record */
		}
		cassette_buffer[0x83] = SIO_ChkSum(cassette_buffer, 0x83);
	}
	return length;
}

/* Read a record by SIO-patch
   returns block length (with checksum) */
static int ReadRecord_SIO(void)
{
	int length = 0;
	/* if waiting for gap was longer than gap of record, skip
	   atm there is no check if we start then inmidst a record */
	int filegaptimes = 0;
	while (cassette_gapdelay >= filegaptimes) {
		int gap;
		if (CASSETTE_current_block > CASSETTE_max_block) {
			length = 0;
			eof_of_tape = 1;
			UpdateFlags();
			break;
		};
		length = ReadRecord(&gap);
		/* add gaplength */
		filegaptimes += gap;
		/* add time used by the data themselves
		   a byte is encoded into 10 bits */
		filegaptimes += length * 10 * 1000 / cassette_baudblock[CASSETTE_current_block-1];
		CASSETTE_current_block++;
	}
	cassette_gapdelay = 0;
	return length;
}

int CASSETTE_AddGap(int gaptime)
{
	cassette_gapdelay += gaptime;
	if (cassette_gapdelay < 0)
		cassette_gapdelay = 0;
	return cassette_gapdelay;
}

/* Indicates that a loading leader is expected by the OS */
void CASSETTE_LeaderLoad(void)
{
	if (CASSETTE_record)
	CASSETTE_ToggleRecord();
	CASSETTE_TapeMotor(TRUE);
	cassette_gapdelay = 9600;
	/* registers for SETVBV: third system timer, ~0.1 sec */
	CPU_regA = 3;
	CPU_regX = 0;
	CPU_regY = 5;
}

/* indicates that a save leader is written by the OS */
void CASSETTE_LeaderSave(void)
{
	if (!CASSETTE_record)
	CASSETTE_ToggleRecord();
	CASSETTE_TapeMotor(TRUE);
	cassette_gapdelay = 19200;
	/* registers for SETVBV: third system timer, ~0.1 sec */
	CPU_regA = 3;
	CPU_regX = 0;
	CPU_regY = 5;
	eof_of_tape = 0;
}

int CASSETTE_ReadToMemory(UWORD dest_addr, int length)
{
	int read_length;
	if (!cassette_readable)
		return FALSE;
	read_length = ReadRecord_SIO();
	/* Copy record to memory, excluding the checksum byte if it exists. */
	MEMORY_CopyToMem(cassette_buffer, dest_addr, read_length >= length ? length : read_length);
	return read_length == length + 1 &&
	       cassette_buffer[length] == SIO_ChkSum(cassette_buffer, length);
}

int CASSETTE_WriteFromMemory(UWORD src_addr, int length)
{
	/* File must be writable */
	if (!cassette_writable)
		return -1;
	EnlargeBuffer(length + 1);
	/* Put record into buffer. */
	MEMORY_CopyFromMem(src_addr, cassette_buffer, length);
	/* Eval checksum over buffer data. */
	cassette_buffer[length] = SIO_ChkSum(cassette_buffer, length);

	return WriteRecord(length + 1) == length + 1;
}

void CASSETTE_Seek(unsigned int position)
{
	if (CASSETTE_status != CASSETTE_STATUS_NONE) {
		/* save last block */
		if (cassette_current_blockbyte > 0 && cassette_writable && CASSETTE_record)
			WriteRecord(cassette_current_blockbyte);

		CASSETTE_current_block = (int)position;
		if (CASSETTE_current_block > CASSETTE_max_block + 1)
			CASSETTE_current_block = CASSETTE_max_block + 1;
		cassette_elapsedtime = 0;
		cassette_savetime = 0;
		cassette_nextirqevent = 0;
		cassette_current_blockbyte = 0;
		cassette_max_blockbytes = 0;
		cassette_gapdelay = 0;
		eof_of_tape = 0;
		cassette_baudrate = 600;
		CASSETTE_record = FALSE;
		UpdateFlags();
	}
}

/* convert milliseconds to scanlines */
static SLONG MSToScanLines(int ms)
{
	/* for PAL resolution, deviation in NTSC is negligible */
	return 312*50*ms/1000;
}

/* Support to read a record by POKEY-registers
   evals gap length
   returns block length (with checksum) */
static int ReadRecord_POKEY(void)
{
	int length = 0;
	int gap = 0;
	/* no file or blockpositions don't match anymore after saving */
	if (!cassette_readable) {
		cassette_nextirqevent = -1;
		length = -1;
		return length;
	}
	/* if end of CAS file */
	if (CASSETTE_current_block > CASSETTE_max_block){
		cassette_nextirqevent = -1;
		length = -1;
		eof_of_tape = 1;
		UpdateFlags();
		return length;
	}
	length = ReadRecord(&gap);
	/* eval total length as pregap + 1 byte */
	cassette_nextirqevent = cassette_elapsedtime +
	                        MSToScanLines(gap + 10 * 1000 /
	                                      (cassette_isCAS ? cassette_baudblock[CASSETTE_current_block-1] : 600)
	                                     );
	cassette_max_blockbytes = length;
	cassette_current_blockbyte = 0;
	return length;
}

/* sets the stamp of next irq and loads new record if necessary
   adjust is a value to correction of time of next irq*/
static int SetNextByteTime_POKEY(int adjust)
{
	int length = 0;
	cassette_current_blockbyte += 1;
	/* if there are still bytes in the buffer, take next byte */
	if (cassette_current_blockbyte < cassette_max_blockbytes) {
		cassette_nextirqevent = cassette_elapsedtime + adjust
			+ MSToScanLines(10 * 1000 / (cassette_isCAS?cassette_baudblock[
			CASSETTE_current_block-1]:600));
		return 0;
	}

	/* 0 indicates that there was no previous block being read and
	   CASSETTE_current_block should not be increased. */
	if (cassette_max_blockbytes != 0)
		CASSETTE_current_block++;
	/* if buffer is exhausted, load next record */
	length = ReadRecord_POKEY();
	/* if failed, return -1 */
	if (length == -1) {
		cassette_nextirqevent = -1;
		return -1;
	}
	cassette_nextirqevent += adjust;
	return 0;
}

/* Get the byte for which the serial data ready int has been triggered */
int CASSETTE_GetByte(void)
{
	if (cassette_readable)
		/* there are programs which load 2 blocks as one */
		return cassette_buffer[cassette_current_blockbyte];
	else
		return 0;
}

/* Check status of I/O-line
  Mark tone and stop bit gives 1, start bit 0
  $55 as data give 1,0,1,0,1,0,1,0 (sync to evaluate tape speed,
    least significant bit first)
  returns state of I/O-line as block.byte.bit */
int CASSETTE_IOLineStatus(void)
{
	int bit = 0;

	/* if motor off and EOF return always 1 (equivalent the mark tone) */
	if (!cassette_readable || CASSETTE_record) {
		return 1;
	}

	/* exam rate; if elapsed time > nextirq - duration of one byte */
	if (cassette_elapsedtime >
		(cassette_nextirqevent - 10 * 50 * 312 / 
		(cassette_isCAS?cassette_baudblock[CASSETTE_current_block-1]:
		600) + 1)) {
		bit = (cassette_nextirqevent - cassette_elapsedtime)
			/ (50 * 312 / (cassette_isCAS?cassette_baudblock[
			CASSETTE_current_block-1]:600));
	}
	else {
		bit = 0;
	}

	/* if stopbit or out of range, return mark tone */
	if ((bit <= 0) || (bit > 9))
		return 1;

	/* if start bit, return space tone */
	if (bit == 9)
		return 0;

	/* eval tone to return */
	return (CASSETTE_GetByte() >> (8 - bit)) & 1;
}

/* Get the delay to trigger the next interrupt
   remark: The I/O-Line-status is also evaluated on this basis */
int CASSETTE_GetInputIRQDelay(void)
{
	int timespan;

	/* if no file or eof or motor off, return zero */
	if (ESC_enable_sio_patch || !cassette_readable || CASSETTE_record || cassette_nextirqevent < 0)
		return 0;

	/* return time span */
	timespan = cassette_nextirqevent - cassette_elapsedtime;
	/* if timespan is negative, eval timespan to next byte */
	if (timespan <= 0) {
		if (SetNextByteTime_POKEY(0) == -1)
			return -1;
		/* return time span */
		timespan = cassette_nextirqevent - cassette_elapsedtime;
	}
	if (timespan < 40) {
		timespan += ((312 * 50 - 1) / (cassette_isCAS?cassette_baudblock[
			CASSETTE_current_block-1]:600)) * 10;
	}

	/* if still negative, return "failed" */
	if (timespan < 0) {
		timespan = -1;
	}
	return timespan;
}

/* put a byte into the cas file */
/* the block is being written at first putbyte of the subsequent block */
void CASSETTE_PutByte(int byte)
{
	if (!ESC_enable_sio_patch && cassette_writable && CASSETTE_record) {
		/* get time since last byte-put resp. motor on */
		int cassette_putdelay = 1000*(cassette_elapsedtime-cassette_savetime)/312/50; /* in ms, delay since last putbyte */
		/* subtract one byte-duration from put delay */
		cassette_putdelay -= 1000 * 10 * (POKEY_AUDF[POKEY_CHAN3] + POKEY_AUDF[POKEY_CHAN4]*0x100)/895000;
		/* if putdelay > 5 (ms) */
		if (cassette_putdelay > 05) {

			/* write previous block */
			if (cassette_current_blockbyte > 0) {
				WriteRecord(cassette_current_blockbyte);
				cassette_current_blockbyte = 0;
				cassette_gapdelay = 0;
			}
			/* set new gap-time */
			cassette_gapdelay += cassette_putdelay;
		}
		/* put byte into buffer */
		EnlargeBuffer(cassette_current_blockbyte + 1);
		cassette_buffer[cassette_current_blockbyte] = byte;
		cassette_current_blockbyte++;
		/* set new last byte-put time */
		cassette_savetime = cassette_elapsedtime;
	}
}

/* set motor status
 1 - on, 0 - off
 remark: the real evaluation is done in AddScanLine*/
void CASSETTE_TapeMotor(int onoff)
{
	if (cassette_motor != onoff) {
		if (!ESC_enable_sio_patch) {
			if (cassette_writable && CASSETTE_record) {
				if (onoff == 0) {
					/* get time since last byte-put resp. motor on */
					int cassette_putdelay = 1000*(cassette_elapsedtime-cassette_savetime)/312/50; /* in ms, delay since last putbyte */
					/* subtract one byte-duration from put delay */
					cassette_putdelay -= 1000*10 * (POKEY_AUDF[POKEY_CHAN3] + POKEY_AUDF[POKEY_CHAN4]*0x100)/895000;
					/* set putdelay non-negative */
					if (cassette_putdelay < 0) {
						cassette_putdelay = 0;
					}
					/* write block from buffer */
					if (cassette_current_blockbyte > 0) {
						WriteRecord(cassette_current_blockbyte);
						cassette_gapdelay = 0;
						cassette_current_blockbyte = 0;
					}
					/* set new gap-time */
					cassette_gapdelay += cassette_putdelay;
				}
				else {
					/* motor on if it was off - restart gap time */
					cassette_savetime = cassette_elapsedtime;
				}
			}
			else if (cassette_readable) {
				if (onoff == 1)
					cassette_savetime = cassette_elapsedtime;
			}
		}
		cassette_motor = onoff;
		UpdateFlags();
	}
}

void CASSETTE_ToggleRecord(void)
{
	if ((cassette_current_blockbyte > 0) && cassette_writable && CASSETTE_record)
		WriteRecord(cassette_current_blockbyte);
	CASSETTE_record = !CASSETTE_record;
	cassette_elapsedtime = 0;
	cassette_savetime = 0;
	cassette_nextirqevent = 0;
	cassette_current_blockbyte = 0;
	cassette_max_blockbytes = 0;
	cassette_gapdelay = 0;
	cassette_baudrate = 600;
	if (CASSETTE_record)
		eof_of_tape = 0;
	UpdateFlags();
}

void CASSETTE_AddScanLine(void)
{
	int tmp;
	/* if motor on and a cassette file is opened, and not eof */
	/* increment elapsed cassette time */
	if (cassette_readable || cassette_writable) {
		cassette_elapsedtime++;

		/* only for loading: set next byte interrupt time */
		/* valid cassette times are up to 870 baud, giving
		   a time span of 18 scanlines, so comparing with
		   -2 leaves a safe timespan for letting get the bit out
		   of the pokey */
		if (cassette_readable && !CASSETTE_record && cassette_nextirqevent - cassette_elapsedtime <= -2) {
			tmp = SetNextByteTime_POKEY(-2);
		}
	}
}

/*
vim:ts=4:sw=4:
*/
