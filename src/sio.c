/*
 * sio.c - Serial I/O emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef DEBUG
#include <time.h>
#include <sys/time.h>
#endif

#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "memory.h"
#include "sio.h"
#include "pokey.h"
#include "pokeysnd.h"
#include "platform.h"
#include "log.h"
#include "binload.h"
#include "cassette.h"

#ifdef SHOW_DISK_LED
#include "diskled.h"
#endif

/* If ATR image is in double density (256 bytes per sector),
   then the boot sectors (sectors 1-3) can be:
   - logical (as seen by Atari) - 128 bytes in each sector
   - physical (as stored on the disk) - 256 bytes in each sector.
     Only the first half of sector is used for storing data, rest is zero.
   - SIO2PC (the type used by the SIO2PC program) - 3 * 128 bytes for data
     of boot sectors, then 3 * 128 unused bytes (zero)
   The XFD images in double density have either logical or physical boot sectors.
*/
#define BOOT_SECTORS_LOGICAL	0
#define BOOT_SECTORS_PHYSICAL	1
#define BOOT_SECTORS_SIO2PC		2
static int boot_sectors_type[MAX_DRIVES];

/* Format is also size of header :-) */
typedef enum Format {
	XFD = 0, ATR = 16
} Format;

static Format format[MAX_DRIVES];
static FILE *disk[MAX_DRIVES] = {0, 0, 0, 0, 0, 0, 0, 0};
static int sectorcount[MAX_DRIVES];
static int sectorsize[MAX_DRIVES];
static int format_sectorcount[MAX_DRIVES];
static int format_sectorsize[MAX_DRIVES];
static int io_success[MAX_DRIVES];
#ifdef DEBUG
static struct timeval tv;
#endif

UnitStatus drive_status[MAX_DRIVES];
char sio_filename[MAX_DRIVES][FILENAME_MAX];

static char	tmp_filename[MAX_DRIVES][FILENAME_MAX];
static UBYTE istmpfile[MAX_DRIVES] = {0, 0, 0, 0, 0, 0, 0, 0};

/* Serial I/O emulation support */
UBYTE CommandFrame[6];
int CommandIndex = 0;
UBYTE DataBuffer[256 + 3];
char sio_status[256];
int DataIndex = 0;
int TransferStatus = 0;
int ExpectedBytes = 0;

extern FILE *opendcm( int diskno, const char *infilename, char *outfilename );
#ifdef HAVE_LIBZ
extern FILE *openzlib(int diskno, const char *infilename, char *outfilename );
#endif

void SIO_Initialise(int *argc, char *argv[])
{
	int i;

	for (i = 0; i < MAX_DRIVES; i++) {
		strcpy(sio_filename[i], "Empty");
		memset(tmp_filename[i], 0, FILENAME_MAX );
		drive_status[i] = Off;
		istmpfile[i] = 0;
		format_sectorsize[i] = 128;
		format_sectorcount[i] = 720;
	}

	TransferStatus = SIO_NoFrame;
}

/* umount disks so temporary files are deleted */
void SIO_Exit(void)
{
	int i;

	for (i = 1; i <= MAX_DRIVES; i++)
		SIO_Dismount(i);
}

int SIO_Mount(int diskno, const char *filename, int b_open_readonly)
{
	char upperfile[FILENAME_MAX];
	struct ATR_Header header;
	int fname_len;
	int i;
	FILE *f = NULL;

	memset(upperfile, 0, FILENAME_MAX);
	fname_len = (int) strlen(filename);
	for (i = 0; i < fname_len; i++)
		upperfile[i] = toupper(filename[i]);

	/* If file is DCM, open it with opendcm to create a temp file */
	if (fname_len > 4 && !strcmp(&upperfile[fname_len - 4], ".DCM")) {
		istmpfile[diskno - 1] = 1;
		drive_status[diskno - 1] = b_open_readonly ? ReadOnly : ReadWrite;	/* this is a fake but some games need it */
		f = opendcm(diskno, filename, tmp_filename[diskno - 1]);
		if (!f)
			istmpfile[diskno - 1] = 0;
	}
#ifdef HAVE_LIBZ
	else if ((fname_len > 4 && !strcmp(&upperfile[fname_len - 4], ".ATZ")) ||
			 (fname_len > 7 && !strcmp(&upperfile[fname_len - 7], ".ATR.GZ")) ||
			 (fname_len > 4 && !strcmp(&upperfile[fname_len - 4], ".XFZ")) ||
			 (fname_len > 7 && !strcmp(&upperfile[fname_len - 7], ".XFD.GZ"))) {
		istmpfile[diskno - 1] = 1;
		drive_status[diskno - 1] = b_open_readonly ? ReadOnly : ReadWrite;	/* this is a fake but some games need it */
		f = openzlib(diskno, filename, tmp_filename[diskno - 1]);
		if (!f) {
			istmpfile[diskno - 1] = 0;
			drive_status[diskno - 1] = NoDisk;
		}
	}	
#endif /* HAVE_LIBZ */
	else { /* Normal ATR, XFD disk */
		drive_status[diskno - 1] = ReadWrite;
		strcpy(sio_filename[diskno - 1], "Empty");

		if (b_open_readonly == FALSE)
		{
		  f = fopen(filename, "rb+");
		}
		if (b_open_readonly == TRUE || !f)
		{
			f = fopen(filename, "rb");
			drive_status[diskno - 1] = ReadOnly;
		}
	}

	if (f) {
		ULONG file_length;

		fseek(f, 0L, SEEK_END);
		file_length = ftell(f);
		fseek(f, 0L, SEEK_SET);

		if (fread(&header, 1, sizeof(struct ATR_Header), f) < sizeof(struct ATR_Header)) {
			fclose(f);
			disk[diskno - 1] = 0;
			return FALSE;
		}

		strcpy(sio_filename[diskno - 1], filename);

		boot_sectors_type[diskno - 1] = BOOT_SECTORS_LOGICAL;

		if ((header.magic1 == MAGIC1) && (header.magic2 == MAGIC2)) {
			format[diskno - 1] = ATR;

			if (header.writeprotect)
				drive_status[diskno - 1] = ReadOnly;

			sectorsize[diskno - 1] = header.secsizehi << 8 |
				header.secsizelo;

			/* ATR header contains length in 16-byte chunks */
			/* First compute number of 128-byte chunks */
			sectorcount[diskno - 1] = (header.hiseccounthi << 24 |
				header.hiseccountlo << 16 |
				header.seccounthi << 8 |
				header.seccountlo) >> 3;

			/* Fix if double density */
			if (sectorsize[diskno - 1] == 256) {
				if (sectorcount[diskno - 1] & 1)
					sectorcount[diskno - 1] += 3;		/* logical (128-byte) boot sectors */
				else {	/* 256-byte boot sectors */
					/* check if physical or SIO2PC */
					/* Physical if there's a non-zero byte in bytes 0x190-0x30f of the ATR file */
					UBYTE buffer[0x180];
					fseek(f, 0x190L, SEEK_SET);
					fread(buffer, 1, 0x180, f);
					boot_sectors_type[diskno - 1] = BOOT_SECTORS_SIO2PC;
					for (i = 0; i < 0x180; i++)
						if (buffer[i] != 0) {
							boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
							break;
						}
				}
				sectorcount[diskno - 1] >>= 1;
			}
#ifdef DEBUG
			Aprint("ATR: sectorcount = %d, sectorsize = %d",
				   sectorcount[diskno - 1],
				   sectorsize[diskno - 1]);
#endif
		}
		else {
			format[diskno - 1] = XFD;

			if (file_length <= (1040 * 128)) {
				sectorsize[diskno - 1] = 128;	/* single density */
				sectorcount[diskno - 1] = file_length / 128;
			}
			else {
				sectorsize[diskno - 1] = 256;	/* double density */
				if ((file_length & 0xff) == 0) {
					boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
					sectorcount[diskno - 1] = file_length / 256;
				}
				else
					sectorcount[diskno - 1] = (file_length + 0x180) / 256;
			}
		}
		format_sectorsize[diskno - 1] = sectorsize[diskno - 1];
		format_sectorcount[diskno - 1] = sectorcount[diskno - 1];
	}
	else {
		drive_status[diskno - 1] = NoDisk;
	}

	disk[diskno - 1] = f;

	return disk[diskno - 1] ? TRUE : FALSE;
}

void SIO_Dismount(int diskno)
{
	if (disk[diskno - 1])
	{
		fclose(disk[diskno - 1]);
		disk[diskno - 1] = 0;
		drive_status[diskno - 1] = NoDisk;
		strcpy(sio_filename[diskno - 1], "Empty");
		if( istmpfile[diskno - 1])
		{
			remove(tmp_filename[diskno - 1] );
			memset(tmp_filename[diskno - 1], 0, FILENAME_MAX);
			istmpfile[diskno - 1] = 0;
		}
	}
}

void SIO_DisableDrive(int diskno)
{
	drive_status[diskno - 1] = Off;
	strcpy(sio_filename[diskno - 1], "Off");
}

static void SizeOfSector(UBYTE unit, int sector, int *sz, ULONG * ofs)
{
	int size;
	ULONG offset;

	if (start_binloading) {
		if (sz)
			*sz = 128;
		if (ofs)
			*ofs = 0;
		return;
	}

	if (sector < 4) {
		/* special case for first three sectors in ATR and XFD image */
		size = 128;
		offset = format[unit] + (sector - 1) * (boot_sectors_type[unit] == BOOT_SECTORS_PHYSICAL ? 256 : 128);
	}
	else {
		size = sectorsize[unit];
		offset = format[unit] + (boot_sectors_type[unit] == BOOT_SECTORS_LOGICAL ? 0x180 : 0x300) + (sector - 4) * size;
	}

	if (sz)
		*sz = size;

	if (ofs)
		*ofs = offset;
}

static int SeekSector(int unit, int sector)
{
	ULONG offset;
	int size;

	sprintf(sio_status, "%d: %d", unit + 1, sector);
	SizeOfSector((UBYTE)unit, sector, &size, (ULONG*)&offset);
	fseek(disk[unit], 0L, SEEK_END);
	if (offset < 0 || offset > ftell(disk[unit])) {
#ifdef DEBUG
		Aprint("SIO:SeekSector() - Wrong seek offset");
#endif
	}
	else
		fseek(disk[unit], offset, SEEK_SET);

	return size;
}

/* Unit counts from zero up */
static int ReadSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	if (start_binloading)
		return BIN_loade_start(buffer);

	io_success[unit]=-1;
	if (drive_status[unit] != Off) {
		if (disk[unit]) {
			if (sector > 0 && sector <= sectorcount[unit]) {
#ifdef SHOW_DISK_LED
				LED_SetRead(unit, 1);
#endif
				size = SeekSector(unit, sector);
				fread(buffer, 1, size, disk[unit]);
				io_success[unit]=0;
				return 'C';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

static int WriteSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	io_success[unit]=-1;
	if (drive_status[unit] != Off) {
		if (disk[unit]) {
			if (drive_status[unit] == ReadWrite) {
#ifdef SHOW_DISK_LED
				LED_SetWrite(unit, 1);
#endif
				if (sector > 0 && sector <= sectorcount[unit]) {
					size = SeekSector(unit, sector);
					fwrite(buffer, 1, size, disk[unit]);
					io_success[unit]=0;
					return 'C';
				}
				else
					return 'E';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}
	else
		return 0;
}

static int FormatDisk(int unit, UBYTE *buffer, int sectsize, int sectcount)
{
	io_success[unit]=-1;
	if (drive_status[unit] != Off) {
		if (disk[unit]) {
			if (drive_status[unit] == ReadWrite) {
				char fname[FILENAME_MAX];
				int is_atr;
				int save_boot_sectors_type;
				int bootsectsize;
				int bootsectcount;
				FILE *f;
				int i;
				/* Note formatting the disk can change size of the file.
				   There is no portable way to truncate the file at given position.
				   We have to close the "r+b" opened file and open it in "wb" mode.
				   First get the information about the disk image, because we are going
				   to umount it. */
				memcpy(fname, sio_filename[unit], FILENAME_MAX);
				is_atr = (format[unit] == ATR);
				save_boot_sectors_type = boot_sectors_type[unit];
				bootsectsize = 128;
				if (sectsize == 256 && save_boot_sectors_type != BOOT_SECTORS_LOGICAL)
					bootsectsize = 256;
				bootsectcount = sectcount < 3 ? sectcount : 3;
				/* Umount the file and open it in "wb" mode (it will truncate the file) */
				SIO_Dismount(unit + 1);
				f = fopen(fname, "wb");
				if (f == NULL) {
					Aprint("FormatDisk: failed to open %s for writing", fname);
					return 'E';
				}
				/* Write ATR header if necessary */
				if (is_atr) {
					struct ATR_Header header;
					ULONG disksize = (bootsectsize * bootsectcount + sectsize * (sectcount - bootsectcount)) >> 4;
					memset(&header, 0, sizeof(header));
					header.magic1 = MAGIC1;
					header.magic2 = MAGIC2;
					header.secsizelo = (UBYTE) sectsize;
					header.secsizehi = (UBYTE) (sectsize >> 8);
					header.seccountlo = (UBYTE) disksize;
					header.seccounthi = (UBYTE) (disksize >> 8);
					header.hiseccountlo = (UBYTE) (disksize >> 16);
					header.hiseccounthi = (UBYTE) (disksize >> 24);
					fwrite(&header, 1, sizeof(header), f);
				}
				/* Write boot sectors */
				memset(buffer, 0, sectsize);
				for (i = 1; i <= 3 && i <= sectcount; i++)
					fwrite(buffer, 1, bootsectsize, f);
				/* Write regular sectors */
				for ( ; i <= sectcount; i++)
					fwrite(buffer, 1, sectsize, f);
				/* Close file and mount the disk back */
				fclose(f);
				SIO_Mount(unit + 1, fname, FALSE);
				/* We want to keep the current PHYSICAL/SIO2PC boot sectors type
				   (since the image is blank it can't be figured out by SIO_Mount) */
				if (bootsectsize == 256)
					boot_sectors_type[unit] = save_boot_sectors_type;
				/* Return information for Atari (buffer filled with ff's - no bad sectors) */
				memset(buffer, 0xff, sectsize);
				io_success[unit]=0;
				return 'C';
			}
			else
				return 'E';		/* write protection */
		}
		else
			return 'N';
	}
	return 0;
}

/* Set density and number of sectors
   This function is used before the format (0x21) command
   to set how the disk will be formatted.
   Note this function does *not* affect the currently attached disk
   (previously sectorsize/sectorcount were used which could result in
   a corrupted image).
*/
static int WriteStatusBlock(int unit, UBYTE * buffer)
{
	int size;

#ifdef DEBUG
	Aprint("Write Status-Block: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5], buffer[6], buffer[7],
		buffer[8], buffer[9], buffer[10], buffer[11]);
#endif
	if (drive_status[unit] != Off) {
		/*
		 * We only care about the density and the sector count
		 * here. Setting everything else right here seems to
		 * be non-sense
		 */
		/* I'm not sure about this density settings, my XF551
		 * honors only the sector size and ignores the density
		 */
		size = buffer[6] * 256 + buffer[7];
		if (size == 128 || size == 256)
			format_sectorsize[unit] = size;
		else if (buffer[5] == 8) {
			format_sectorsize[unit] = 256;
		}
		else {
			format_sectorsize[unit] = 128;
		}
		/* Note, that the number of heads are minus 1 */
		format_sectorcount[unit] = buffer[0] * (buffer[2] * 256 + buffer[3]) * (buffer[4] + 1);
		if (format_sectorcount[unit] < 4 || format_sectorcount[unit] > 65536)
			format_sectorcount[unit] = 720;
		return 'C';
	}
	else
		return 0;
}

/*
 * My german "Atari Profi Buch" says, buffer[4] holds the number of
 * heads. However, BiboDos and my XF551 think that´s the number minus 1.
 *
 * ???
 */
static int ReadStatusBlock(int unit, UBYTE * buffer)
{
	int spt, heads;

	if (drive_status[unit] != Off) {
		spt = sectorcount[unit] / 40;
		if (spt > 26) {
			heads = 2;
			spt /= 2;
		}
		else
			heads = 1;

		buffer[0] = 40;			/* # of tracks */
		buffer[1] = 1;			/* step rate. No idea what this means */
		buffer[2] = spt >> 8;	/* sectors per track. HI byte */
		buffer[3] = spt & 0xFF;	/* sectors per track. LO byte */
		buffer[4] = heads - 1;	/* # of heads */
		if (sectorsize[unit] == 128) {
			buffer[5] = 4;		/* density */
			buffer[6] = 0;		/* HI bytes per sector */
			buffer[7] = 128;	/* LO bytes per sector */
		}
		else {
			buffer[5] = 8;		/* double density */
			buffer[6] = 1;		/* HI bytes per sector */
			buffer[7] = 0;		/* LO bytes per sector */
		}
		buffer[8] = 1;			/* drive is online */
		buffer[9] = 192;		/* transfer speed. Whatever this means */
		return 'C';
	}
	else
		return 0;
}

/*
   Status Request from Atari 400/800 Technical Reference Notes

   DVSTAT + 0   Command Status
   DVSTAT + 1   Hardware Status
   DVSTAT + 2   Timeout
   DVSTAT + 3   Unused

   Command Status Bits

   Bit 0 = 1 indicates an invalid command frame was received
   Bit 1 = 1 indicates an invalid data frame was received
   Bit 2 = 1 indicates that last read/write operation was unsuccessful
   Bit 3 = 1 indicates that the diskette is write protected
   Bit 4 = 1 indicates active/standby

   plus

   Bit 5 = 1 indicates double density
   Bit 7 = 1 indicates dual density disk (1050 format)
 */
static int DriveStatus(int unit, UBYTE * buffer)
{
	if (start_binloading) {
		buffer[0] = 16 + 8;
		buffer[1] = 255;
		buffer[2] = 1;
		buffer[3] = 0 ;
		return 'C';
	}
		
	if (drive_status[unit] != Off) {
		buffer[0] = 16;			/* drive active */
		buffer[1] = 255;		/* WD 177x OK */
		if (io_success[unit]!=0)
			buffer[0] |= 4;         /* failed RW-operation */
		if (drive_status[unit] == ReadOnly)
			buffer[0] |= 8;		/* write protection */
		if (sectorsize[unit] == 256)
			buffer[0] |= 32;	/* double density */
		if (sectorcount[unit] == 1040)
			buffer[0] |= 128;	/* 1050 enhanced density */
		if (!disk[unit])
			buffer[1] &= (UBYTE) ~128;	/* no disk */
		buffer[2] = 1;
		buffer[3] = 0;
		return 'C';
	}
	else
		return 0;
}


void SIO(void)
{
	int sector = dGetWordAligned(0x30a);
	UBYTE unit = dGetByte(0x301) - 1;
	UBYTE result = 0x00;
	ATPtr data = dGetWordAligned(0x304);
	int length = dGetWordAligned(0x308);
	int realsize = 0;
	int cmd = dGetByte(0x302);
#ifndef NO_SECTOR_DELAY
	static int delay_counter = 1;	/* no delay on first read */
#endif

	if (dGetByte(0x300) == 0x31 && unit < MAX_DRIVES) {	/* UBYTE range ! */
#ifdef DEBUG
		gettimeofday(&tv,NULL);
		Aprint("%04lu.%03lu SIO disk command is %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x",
			tv.tv_sec%10000,tv.tv_usec/1000,
			cmd,dGetByte(0x303),dGetByte(0x304),dGetByte(0x305),dGetByte(0x306),
			dGetByte(0x308),dGetByte(0x309),dGetByte(0x30A),dGetByte(0x30B),
			dGetByte(0x30C),dGetByte(0x30D));
#endif
		switch (cmd) {
		case 0x4e:				/* Read Status Block */
			if (12 == length) {
				result = ReadStatusBlock(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, 12);
			}
			else
				result = 'E';
			break;
		case 0x4f:				/* Write Status Block */
			if (12 == length) {
				CopyFromMem(data, DataBuffer, 12);
				result = WriteStatusBlock(unit, DataBuffer);
			}
			else
				result = 'E';
			break;
		case 0x50:				/* Write */
		case 0x57:
		case 0xD0:				/* xf551 hispeed */
		case 0xD7:
			SizeOfSector(unit, sector, &realsize, NULL);
			if (realsize == length) {
				CopyFromMem(data, DataBuffer, realsize);
				result = WriteSector(unit, sector, DataBuffer);
			}
			else
				result = 'E';
			break;
		case 0x52:				/* Read */
		case 0xD2:				/* xf551 hispeed */
#ifndef NO_SECTOR_DELAY
			if (sector == 2) {
				if ((xpos = xpos_limit) == LINE_C)
					delay_counter--;
				if (delay_counter) {
					regPC = 0xe459;	/* stay in SIO */
					return;
				}
				else
					delay_counter = SECTOR_DELAY;
			}
#endif
			SizeOfSector(unit, sector, &realsize, NULL);
			if (realsize == length) {
				result = ReadSector(unit, sector, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x53:				/* Status */
			if (4 == length) {
				result = DriveStatus(unit, DataBuffer);
				CopyToMem(DataBuffer, data, 4);
			}
			else
				result = 'E';
			break;
		/*case 0x66:*/			/* US Doubler Format - I think! */
		case 0x21:				/* Format Disk */
		case 0xA1:				/* xf551 hispeed */
			realsize = format_sectorsize[unit];
			if (realsize == length) {
				result = FormatDisk(unit, DataBuffer, realsize, format_sectorcount[unit]);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else {
				//there are programs which send the format-command but dont wait for the result (eg xf-tools)
				FormatDisk(unit, DataBuffer, realsize, format_sectorcount[unit]);
				result = 'E';
			}
			break;
		case 0x22:				/* Enhanced Density Format */
		case 0xA2:				/* xf551 hispeed */
			realsize = 128;
			if (realsize == length) {
				result = FormatDisk(unit, DataBuffer, 128, 1040);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else {
				FormatDisk(unit, DataBuffer, 128, 1040);
				result = 'E';
			}
			break;
		default:
			result = 'N';
		}
	}
	else if (dGetByte(0x300) == 0x60) {
		result = CASSETTE_Sio();
		if (result == 'C')
			CopyToMem(cassette_buffer, data, length);
	}

	switch (result) {
	case 0x00:					/* Device disabled, generate timeout */
		regY = 138;
		SetN;
		break;
	case 'A':					/* Device acknoledge */
	case 'C':					/* Operation complete */
		regY = 1;
		ClrN;
		break;
	case 'N':					/* Device NAK */
		regY = 144;
		SetN;
		break;
	case 'E':					/* Device error */
	default:
		regY = 146;
		SetN;
		break;
	}
	regA = 0;	/* MMM */
	dPutByte(0x0303, regY);
	dPutByte(0x42,0);
	SetC;
}

UBYTE SIO_ChkSum(UBYTE * buffer, UWORD length)
{
	int i;
	int checksum = 0;

	for (i = 0; i < length; i++, buffer++) {
		checksum += *buffer;
		while (checksum > 255)
			checksum -= 255;
	}

	return checksum;
}

static UBYTE Command_Frame(void)
{
	int unit;
	int sector;
	int realsize;

	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
#ifdef DEBUG
	gettimeofday(&tv,NULL);
#endif
	if (unit < 0 || unit >= MAX_DRIVES) {
		/* Unknown device */
		Aprint("Unknown command frame: %02x %02x %02x %02x %02x",
			   CommandFrame[0], CommandFrame[1], CommandFrame[2],
			   CommandFrame[3], CommandFrame[4]);
		TransferStatus = SIO_NoFrame;
		return 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4e:				/* Read Status */
#ifdef DEBUG
			Aprint("%04lu.%03lu Read-status frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			DataBuffer[0] = ReadStatusBlock(unit, DataBuffer + 1);
			DataBuffer[13] = SIO_ChkSum(DataBuffer + 1, 12);
			DataIndex = 0;
			ExpectedBytes = 14;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			return 'A';
		case 0x4f:				/* Write status */
#ifdef DEBUG
			Aprint("%04lu.%03lu Write-status frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			ExpectedBytes = 13;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
			return 'A';
		case 0x50:				/* Write */
		case 0x57:
		case 0xD0:				/* xf551 hispeed */
		case 0xD7:
#ifdef DEBUG
			Aprint("%04lu.%03lu Write-sector frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			ExpectedBytes = realsize + 1;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
#ifdef SHOW_DISK_LED
			LED_SetWrite(unit, 10);
#endif
			return 'A';
		case 0x52:				/* Read */
		case 0xD2:				/* xf551 hispeed */
#ifdef DEBUG
			Aprint("%04lu.%03lu Read-sector frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			DataBuffer[0] = ReadSector(unit, sector, DataBuffer + 1);
			DataBuffer[1 + realsize] = SIO_ChkSum(DataBuffer + 1, (UWORD)realsize);
			DataIndex = 0;
			ExpectedBytes = 2 + realsize;
			TransferStatus = SIO_ReadFrame;
			//wait longer before confirmation because bytes could be lost before the buffer was set (see $E9FB & $EA37 in XL-OS)
			DELAYED_SERIN_IRQ = SERIN_INTERVAL << 2;
#ifndef NO_SECTOR_DELAY
			if (sector == 2)
				DELAYED_SERIN_IRQ += SECTOR_DELAY;
#endif
#ifdef SHOW_DISK_LED
			LED_SetRead(unit, 10);
#endif
			return 'A';
		case 0x53:				/* Status */
#ifdef DEBUG
			Aprint("%04lu.%03lu Status frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			DataBuffer[0] = DriveStatus(unit, DataBuffer + 1);
			DataBuffer[1 + 4] = SIO_ChkSum(DataBuffer + 1, 4);
			DataIndex = 0;
			ExpectedBytes = 6;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			return 'A';
		/*case 0x66:*/			/* US Doubler Format - I think! */
		case 0x21:				/* Format Disk */
		case 0xa1:				/* xf551 hispeed */
#ifdef DEBUG
			Aprint("%04lu.%03lu Format-disk frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			realsize = format_sectorsize[unit];
			DataBuffer[0] = FormatDisk(unit, DataBuffer + 1, realsize, format_sectorcount[unit]);
			DataBuffer[1 + realsize] = SIO_ChkSum(DataBuffer + 1, realsize);
			DataIndex = 0;
			ExpectedBytes = 2 + realsize;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			return 'A';
		case 0x22:				/* Dual Density Format */
		case 0xa2:				/* xf551 hispeed */
#ifdef DEBUG
			Aprint("%04lu.%03lu Format-Medium frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			DataBuffer[0] = FormatDisk(unit, DataBuffer + 1, 128, 1040);
			DataBuffer[1 + 128] = SIO_ChkSum(DataBuffer + 1, 128);
			DataIndex = 0;
			ExpectedBytes = 2 + 128;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			return 'A';
		default:
			/* Unknown command for a disk drive */
#ifdef DEBUG
			Aprint("%04lu.%03lu Command frame: %02x %02x %02x %02x %02x",
				tv.tv_sec%10000, tv.tv_usec/1000,
				CommandFrame[0], CommandFrame[1], CommandFrame[2],
				CommandFrame[3], CommandFrame[4]);
#endif
			TransferStatus = SIO_NoFrame;
			return 'E';
		}
}

/* Enable/disable the command frame */
void SwitchCommandFrame(int onoff)
{

	if (onoff) {				/* Enabled */
		if (TransferStatus != SIO_NoFrame)
			Aprint("Unexpected command frame at state %x.", TransferStatus);
		CommandIndex = 0;
		DataIndex = 0;
		ExpectedBytes = 5;
		TransferStatus = SIO_CommandFrame;
	}
	else {
		if (TransferStatus != SIO_StatusRead && TransferStatus != SIO_NoFrame &&
			TransferStatus != SIO_ReadFrame) {
			if (!(TransferStatus == SIO_CommandFrame && CommandIndex == 0))
				Aprint("Command frame %02x unfinished.", TransferStatus);
			TransferStatus = SIO_NoFrame;
		}
		CommandIndex = 0;
	}
}

static UBYTE WriteSectorBack(void)
{
	UWORD sector;
	UBYTE unit;
	UBYTE result;

	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
	if (unit >= MAX_DRIVES) {				/* UBYTE range ! */
		result = 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4f:				/* Write Status Block */
			result = WriteStatusBlock(unit, DataBuffer);
			break;
		case 0x50:				/* Write */
		case 0x57:
		case 0xD0:				/* xf551 hispeed */
		case 0xD7:
			result = WriteSector(unit, sector, DataBuffer);
			break;
		default:
			result = 'E';
		}

	return result;
}

/* Put a byte that comes out of POKEY. So get it here... */
void SIO_PutByte(int byte)
{
	UBYTE sum, result;

	switch (TransferStatus) {
	case SIO_CommandFrame:
		if (CommandIndex < ExpectedBytes) {
			CommandFrame[CommandIndex++] = byte;
			if (CommandIndex >= ExpectedBytes) {
				if (((CommandFrame[0] >= 0x31) && (CommandFrame[0] <= 0x38))) {
					TransferStatus = SIO_StatusRead;
					DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
				}
				else
					TransferStatus = SIO_NoFrame;
			}
		}
		else {
			Aprint("Invalid command frame!");
			TransferStatus = SIO_NoFrame;
		}
		break;
	case SIO_WriteFrame:		/* Expect data */
		if (DataIndex < ExpectedBytes) {
			DataBuffer[DataIndex++] = byte;
			if (DataIndex >= ExpectedBytes) {
				sum = SIO_ChkSum(DataBuffer, (UWORD)(ExpectedBytes - 1));
				if (sum == DataBuffer[ExpectedBytes - 1]) {
					result = WriteSectorBack();
					if (result) {
						DataBuffer[0] = 'A';
						DataBuffer[1] = result;
						DataIndex = 0;
						ExpectedBytes = 2;
						DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
						TransferStatus = SIO_FinalStatus;
					}
					else
						TransferStatus = SIO_NoFrame;
				}
				else {
					DataBuffer[0] = 'E';
					DataIndex = 0;
					ExpectedBytes = 1;
					DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
					TransferStatus = SIO_FinalStatus;
				}
			}
		}
		else {
			Aprint("Invalid data frame!");
		}
		break;
	}
	DELAYED_SEROUT_IRQ = SEROUT_INTERVAL;
}

/* Get a byte from the floppy to the pokey. */

int SIO_GetByte(void)
{
	int byte = 0;

	switch (TransferStatus) {
	case SIO_StatusRead:
		byte = Command_Frame();		/* Handle now the command */
		break;
	case SIO_FormatFrame:
		TransferStatus = SIO_ReadFrame;
		DELAYED_SERIN_IRQ = SERIN_INTERVAL << 3;
	case SIO_ReadFrame:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
			}
			else {
				//set delay using the expected transfer speed
				DELAYED_SERIN_IRQ = (DataIndex==1) ? SERIN_INTERVAL : ((SERIN_INTERVAL*AUDF[CHAN3]-1)/0x28+1);
			}
		}
		else {
			Aprint("Invalid read frame!");
			TransferStatus = SIO_NoFrame;
		}
		break;
	case SIO_FinalStatus:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
			}
			else {
				if (DataIndex == 0)
					DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
				else
					DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			}
		}
		else {
			Aprint("Invalid read frame!");
			TransferStatus = SIO_NoFrame;
		}
		break;
	default:
		break;
	}
	return byte;
}

int Rotate_Disks( void )
{
	char	tmp_filenames[MAX_DRIVES][ FILENAME_MAX ];
	int		i;
	int		bSuccess = TRUE;

	for( i=0; i < MAX_DRIVES; i++ )
	{
		strcpy( tmp_filenames[i], sio_filename[i] );
	}

	for( i=0; i < MAX_DRIVES; i++ )
	{
		SIO_Dismount( i + 1 );
	}

	for( i=1; i < MAX_DRIVES; i++ )
	{
		if( strcmp( tmp_filenames[i], "None" ) && strcmp( tmp_filenames[i], "Off" ) && strcmp( tmp_filenames[i], "Empty" ) )
		{
			if( SIO_Mount( i, tmp_filenames[i], FALSE ) == FALSE ) /* Note that this is NOT i-1 because SIO_Mount is 1 indexed */
				bSuccess = FALSE;
		}
	}

	i = MAX_DRIVES-1;
	while( (i > -1) && (!strcmp( tmp_filenames[i], "None" ) || !strcmp( tmp_filenames[i], "Off" ) || !strcmp( tmp_filenames[i], "Empty")) )
	{
		i--;
	}

	if( i > -1 )
	{
		if( SIO_Mount( i+1, tmp_filenames[0], FALSE ) == FALSE )
			bSuccess = FALSE;
	}

	return bSuccess;
}

/*
$Log$
Revision 1.17  2003/08/05 11:29:04  joy
XF551 highspeed transfer

Revision 1.16  2003/03/03 09:57:33  joy
Ed improved autoconf again plus fixed some little things

Revision 1.15  2003/02/24 09:33:10  joy
header cleanup

Revision 1.14  2001/10/03 16:46:09  fox
SET_LED and Atari_Set_LED are no longer used

Revision 1.13  2001/10/01 17:21:11  fox
updated for new macros in memory-d.h

Revision 1.12  2001/09/08 07:54:48  knik
used FILENAME_MAX instead of FILENAME_LEN

Revision 1.11  2001/09/03 11:32:58  fox
Disk drive answers 'E' to an unknown command.

Revision 1.10  2001/08/03 12:27:52  fox
cassette support

Revision 1.9  2001/07/25 18:07:07  fox
Format Disk rewritten. Now it can resize both ATR and XFD images.
The ATR header is being updated. Double density formatting works.

Revision 1.8  2001/07/25 12:57:07  fox
removed unused functions, added SIO_Exit(), corrected coding style

Revision 1.7  2001/07/23 09:11:30  fox
corrected and added checks if drive number is in range 1-8

Revision 1.6  2001/07/21 20:00:44  fox
made double density ATR images compatible with SIO2PC

Revision 1.5  2001/04/15 09:14:33  knik
zlib_capable -> have_libz (autoconf compatibility)

Revision 1.4  2001/03/25 06:57:36  knik
open() replaced by fopen()

Revision 1.3  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
