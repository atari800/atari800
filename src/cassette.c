/*
 * cassette.c - cassette emulation
 *
 * Copyright (C) 2001 Piotr Fusik
 * Copyright (C) 2001-2003 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <string.h>

#include "atari.h"
#include "cpu.h"
#include "cassette.h"
#include "log.h"
#include "memory.h"
#include "sio.h"

#define MAX_BLOCKS 2048

static FILE *cassette_file = NULL;
static int cassette_isCAS;
UBYTE cassette_buffer[4096];
static ULONG cassette_block_offset[MAX_BLOCKS];

char cassette_filename[FILENAME_MAX];
char cassette_description[CASSETTE_DESCRIPTION_MAX];
int cassette_current_block;
int cassette_max_block = 0;
int cassette_savefile = FALSE;
int cassette_gapdelay = 0;	//in ms, includes leader and all gaps

int hold_start = 0;
int press_space = 0;

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

void CASSETTE_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	strcpy(cassette_filename, "None");
	memset(cassette_description, 0, sizeof(cassette_description));

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-tape") == 0)
			CASSETTE_Insert(argv[++i]);
		else if (strcmp(argv[i], "-boottape") == 0) {
			CASSETTE_Insert(argv[++i]);
			hold_start = 1;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-tape <file>     Insert cassette image");
				Aprint("\t-boottape <file>\tInsert cassette image and boot it");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;
}

/* Gets information about the cassette image. Returns TRUE if ok.
   To get information without attaching file, use:
   char description[CASSETTE_DESCRIPTION_MAX];
   int last_block;
   int isCAS;
   CASSETTE_CheckFile(filename, NULL, description, &last_block, &isCAS);
*/
int CASSETTE_CheckFile(char *filename, FILE **fp, char *description, int *last_block, int *isCAS)
{
	FILE *f;
	CAS_Header header;
	int blocks;

//atm, no appending to existing cas files - too dangerous to data!
//	f = fopen(filename, "rb+");
//	if (f == NULL)
		f = fopen(filename, "rb");
	if (f == NULL)
		return FALSE;
	if (description)
		memset(description, 0, CASSETTE_DESCRIPTION_MAX);
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
			fread(description, 1, length - skip, f);
		}
		fseek(f, skip, SEEK_CUR);

		/* count number of blocks */
		blocks = 0;
		do {
			//chunk header is always 8 bytes
			if (fread(&header, 1, 8, f) != 8)
				break;
			if (header.identifier[0] == 'd'
				&& header.identifier[1] == 'a'
				&& header.identifier[2] == 't'
				&& header.identifier[3] == 'a') {
				blocks++;
				if (fp)
					cassette_block_offset[blocks] = ftell(f)-4;
			}
			length = header.length_lo + (header.length_hi << 8);
			/* skip possibly present data block */
			fseek(f, length, SEEK_CUR);
		} while (blocks < MAX_BLOCKS);
	}
	else {
		/* raw file */
		ULONG file_length;
		if (isCAS)
			*isCAS = FALSE;
		fseek(f, 0L, SEEK_END);
		file_length = ftell(f);
		blocks = (file_length + 127) / 128 + 1;
	}
	if (last_block)
		*last_block = blocks;
	if (fp)
		*fp = f;
	else
		fclose(f);
	return TRUE;
}

//Create CAS file header for saving data to tape
int CASSETTE_CreateFile(char *filename, FILE **fp, int *isCAS)
{
	CAS_Header header;
	memset(&header,0,sizeof(header));

	//if no file pointer location was given, this function is senseless
	if(fp==NULL)
		return FALSE;
	//close if open
	if(*fp!=NULL)
		fclose(*fp);
	//create new file
	*fp=fopen(filename,"wb");
	if(*fp==NULL)
		return FALSE;

	//write CAS-header
	if((fwrite("FUJI",1,4,*fp)==4) &&
			(fwrite(&header.length_lo,1,4,*fp)==4)){}
	else{
		fclose(*fp);
		*fp=NULL;
		return FALSE;
	};
	
	memset(&header,0,sizeof(header));
	header.aux_lo=0x58;	//provisional: 600 baud
	header.aux_hi=2;
	if((fwrite("baud",1,4,*fp)==4) &&
			(fwrite(&header.length_lo,1,4,*fp)==4)){}
	else{
		fclose(*fp);
		*fp=NULL;
		return FALSE;
	};

	//file is now a cassette file
	*isCAS=TRUE;
	return TRUE;
}

int CASSETTE_Insert(char *filename)
{
	strcpy(cassette_filename, filename);
	cassette_current_block = 1;
	cassette_savefile = FALSE;
	return CASSETTE_CheckFile(filename, &cassette_file, cassette_description,
		 &cassette_max_block, &cassette_isCAS);
}

void CASSETTE_Remove(void)
{
	if (cassette_file != NULL) {
		fclose(cassette_file);
		cassette_file = NULL;
	}
	cassette_max_block=0;
	strcpy(cassette_filename, "None");
	memset(cassette_description, 0, sizeof(cassette_description));
}

/* returns block length (with checksum) */
static UWORD ReadRecord(void)
{
	UWORD length = 0;
	if (cassette_isCAS) {
		CAS_Header header;
		//if waiting for gap was longer than gap of record, skip
		//atm there is no check if we start then inmidst a record
		int filegaptimes = 0;
		while(cassette_gapdelay>=filegaptimes){
       			if (cassette_current_block > cassette_max_block){
				length=-1;
				break;
			};
			fseek(cassette_file, cassette_block_offset[cassette_current_block], SEEK_SET);
			fread(&header.length_lo, 1, 4, cassette_file);
			length = header.length_lo + (header.length_hi << 8);
			//add gaplength
			filegaptimes+=header.aux_lo + (header.aux_hi << 8);
			//add time used by the data themselves
			//atm, assumes a baud rate of 600 (a byte is encoded
			//  into 10 bits -> 60 bytes per second)
			filegaptimes+=1000/60*length;
		
			fread(&cassette_buffer[0], 1, length, cassette_file);
			cassette_current_block++;
		}
		cassette_gapdelay=0;
	}
	else {
		length = 132;
		cassette_buffer[0] = 0x55;
		cassette_buffer[1] = 0x55;
		if (cassette_current_block == cassette_max_block) {
			/* EOF record */
			cassette_buffer[2] = 0xfe;
			memset(cassette_buffer + 3, 0, 128);
		}
		else {
			int bytes;
			fseek(cassette_file, (cassette_current_block - 1) * 128, SEEK_SET);
			bytes = fread(cassette_buffer + 3, 1, 128, cassette_file);
			if (bytes < 128) {
				cassette_buffer[2] = 0xfa;	/* non-full record */
				memset(cassette_buffer + 3 + bytes, 0, 127 - bytes);
				cassette_buffer[0x82] = bytes;
			}
			else
				cassette_buffer[2] = 0xfc;	/* full record */
		}
		cassette_buffer[0x83] = SIO_ChkSum(cassette_buffer, 0x83);
		cassette_current_block++;
	}
	return length;
}

static UWORD WriteRecord(int len)
{
	CAS_Header header;
	memset(&header,0,sizeof(header));

	//always append
	fseek(cassette_file,0,SEEK_END);
	//write recordheader
	strncpy(header.identifier,"data",4);
	header.length_lo=len&0xFF;
	header.length_hi=(len>>8)&0xFF;
	header.aux_lo=cassette_gapdelay&0xff;
	header.aux_hi=(cassette_gapdelay>>8)&0xff;
	cassette_gapdelay=0;
	fwrite(&header,1,8,cassette_file);
	//write record
	return fwrite(&cassette_buffer,1,len,cassette_file);
}

int CASSETTE_AddGap(int gaptime)
{
	cassette_gapdelay+=gaptime;
	if(cassette_gapdelay<0)
		cassette_gapdelay=0;
	return cassette_gapdelay;
};

//Indicates that a loading leader is expected by the OS
void CASSETTE_LeaderLoad()
{
	cassette_gapdelay=9600;
	//registers for SETVBV: third system timer, ~0.1 sec
	regA = 3;
	regX = 0;
	regY = 5;
}

//indicates that a save leader is written by the OS
void CASSETTE_LeaderSave()
{
	cassette_gapdelay=19200;
	//registers for SETVBV: third system timer, ~0.1 sec
	regA = 3;
	regX = 0;
	regY = 5;
}

//Read Cassette Record from Storage medium
//returns size of data in buffer (atm equal with record size, but there
//  are protections with variable baud rates imaginable where a record
//  must be split and a baud chunk inserted inbetween) or -1 for error
int CASSETTE_Read()
{
	//no file or blockpositions dont match anymore after saving
	if ((cassette_file == NULL) || (cassette_savefile == TRUE))
		return -1;
       	if (cassette_current_block > cassette_max_block)
               	return -1;     /* EOF */
       	return ReadRecord();
}

//Write Cassette Record to Storage medium
//length is size of the whole data with checksum(s)
//returns really written bytes, -1 for error
int CASSETTE_Write(int length)
{
	//there must be a filename given for saving
	if(strcmp(cassette_filename,"None")==0)
		return -1;
	//if file doesnt exist (or has no records), create the header
	if((cassette_file==NULL || ftell(cassette_file)==0) &&
			(CASSETTE_CreateFile(cassette_filename,
			&cassette_file,&cassette_isCAS)==FALSE))
		return -1;
	//on a raw file, saving is denied because it can hold
	//  only 1 file and could cause confusion
	if(cassette_isCAS==FALSE)
		return -1;
	//save record (always appends to the end of file)
	cassette_savefile=TRUE;
	return WriteRecord(length);
}
