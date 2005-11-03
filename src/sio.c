/*
 * sio.c - Serial I/O emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"  /* ypos */
#include "atari.h"
#include "binload.h"
#include "cassette.h"
#include "compfile.h"
#include "cpu.h"
#include "log.h"
#include "memory.h"
#include "platform.h"
#include "pokey.h"
#include "pokeysnd.h"
#include "sio.h"
#include "util.h"
#ifndef BASIC
#include "statesav.h"
#endif

/* If ATR image is in double density (256 bytes per sector),
   then the boot sectors (sectors 1-3) can be:
   - logical (as seen by Atari) - 128 bytes in each sector
   - physical (as stored on the disk) - 256 bytes in each sector.
     Only the first half of sector is used for storing data, the rest is zero.
   - SIO2PC (the type used by the SIO2PC program) - 3 * 128 bytes for data
     of boot sectors, then 3 * 128 unused bytes (zero)
   The XFD images in double density have either logical or physical
   boot sectors. */
#define BOOT_SECTORS_LOGICAL	0
#define BOOT_SECTORS_PHYSICAL	1
#define BOOT_SECTORS_SIO2PC		2
static int boot_sectors_type[MAX_DRIVES];

static int header_size[MAX_DRIVES];
static FILE *disk[MAX_DRIVES] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int sectorcount[MAX_DRIVES];
static int sectorsize[MAX_DRIVES];
static int format_sectorcount[MAX_DRIVES];
static int format_sectorsize[MAX_DRIVES];
static int io_success[MAX_DRIVES];

UnitStatus drive_status[MAX_DRIVES];
char sio_filename[MAX_DRIVES][FILENAME_MAX];

Util_tmpbufdef(static, sio_tmpbuf[MAX_DRIVES])

int sio_last_op;
int sio_last_op_time = 0;
int sio_last_drive;
int sio_last_sector;
char sio_status[256];

/* Serial I/O emulation support */
static UBYTE CommandFrame[6];
static int CommandIndex = 0;
static UBYTE DataBuffer[256 + 3];
static int DataIndex = 0;
static int TransferStatus = SIO_NoFrame;
static int ExpectedBytes = 0;

int ignore_header_writeprotect = FALSE;

void SIO_Initialise(int *argc, char *argv[])
{
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		strcpy(sio_filename[i], "Off");
		drive_status[i] = Off;
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
	FILE *f = NULL;
	UnitStatus status = ReadWrite;
	struct ATR_Header header;

	/* avoid overruns in sio_filename[] */
	if (strlen(filename) >= FILENAME_MAX)
		return FALSE;

	/* release previous disk */
	SIO_Dismount(diskno);

	/* open file */
	if (!b_open_readonly)
		f = Util_fopen(filename, "rb+", sio_tmpbuf[diskno - 1]);
	if (f == NULL) {
		f = Util_fopen(filename, "rb", sio_tmpbuf[diskno - 1]);
		if (f == NULL)
			return FALSE;
		status = ReadOnly;
	}

	/* read header */
	if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
		fclose(f);
		return FALSE;
	}

	/* detect compressed image and uncompress */
	switch (header.magic1) {
	case 0xf9:
	case 0xfa:
		/* DCM */
		{
			FILE *f2 = Util_tmpopen(sio_tmpbuf[diskno - 1]);
			if (f2 == NULL)
				return FALSE;
			Util_rewind(f);
			if (!CompressedFile_DCMtoATR(f, f2)) {
				Util_fclose(f2, sio_tmpbuf[diskno - 1]);
				fclose(f);
				return FALSE;
			}
			fclose(f);
			f = f2;
		}
		Util_rewind(f);
		if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
			Util_fclose(f, sio_tmpbuf[diskno - 1]);
			return FALSE;
		}
		status = ReadOnly;
		/* XXX: status = b_open_readonly ? ReadOnly : ReadWrite; */
		break;
	case 0x1f:
		if (header.magic2 == 0x8b) {
			/* ATZ/ATR.GZ, XFZ/XFD.GZ */
			fclose(f);
			f = Util_tmpopen(sio_tmpbuf[diskno - 1]);
			if (f == NULL)
				return FALSE;
			if (!CompressedFile_ExtractGZ(filename, f)) {
				Util_fclose(f, sio_tmpbuf[diskno - 1]);
				return FALSE;
			}
			Util_rewind(f);
			if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
				Util_fclose(f, sio_tmpbuf[diskno - 1]);
				return FALSE;
			}
			status = ReadOnly;
			/* XXX: status = b_open_readonly ? ReadOnly : ReadWrite; */
		}
		break;
	default:
		break;
	}

	boot_sectors_type[diskno - 1] = BOOT_SECTORS_LOGICAL;

	if (header.magic1 == MAGIC1 && header.magic2 == MAGIC2) {
		/* ATR (may be temporary from DCM or ATR/ATR.GZ) */
		header_size[diskno - 1] = 16;

		sectorsize[diskno - 1] = (header.secsizehi << 8) + header.secsizelo;
		if (sectorsize[diskno - 1] != 128 && sectorsize[diskno - 1] != 256) {
			Util_fclose(f, sio_tmpbuf[diskno - 1]);
			return FALSE;
		}

		if (header.writeprotect != 0 && !ignore_header_writeprotect)
			status = ReadOnly;

		/* ATR header contains length in 16-byte chunks. */
		/* First compute number of 128-byte chunks
		   - it's number of sectors on single density disk */
		sectorcount[diskno - 1] = ((header.hiseccounthi << 24)
			+ (header.hiseccountlo << 16)
			+ (header.seccounthi << 8)
			+ header.seccountlo) >> 3;

		/* Fix number of sectors if double density */
		if (sectorsize[diskno - 1] == 256) {
			if ((sectorcount[diskno - 1] & 1) != 0)
				/* logical (128-byte) boot sectors */
				sectorcount[diskno - 1] += 3;
			else {
				/* 256-byte boot sectors */
				/* check if physical or SIO2PC: physical if there's
				   a non-zero byte in bytes 0x190-0x30f of the ATR file */
				UBYTE buffer[0x180];
				int i;
				fseek(f, 0x190, SEEK_SET);
				if (fread(buffer, 1, 0x180, f) != 0x180) {
					Util_fclose(f, sio_tmpbuf[diskno - 1]);
					return FALSE;
				}
				boot_sectors_type[diskno - 1] = BOOT_SECTORS_SIO2PC;
				for (i = 0; i < 0x180; i++)
					if (buffer[i] != 0) {
						boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
						break;
					}
			}
			sectorcount[diskno - 1] >>= 1;
		}
	}
	else {
		/* XFD (may be temporary from XFZ/XFD.GZ) */
		int file_length = Util_flen(f);

		header_size[diskno - 1] = 0;

		if (file_length <= (1040 * 128)) {
			/* single density */
			sectorsize[diskno - 1] = 128;
			sectorcount[diskno - 1] = file_length >> 7;
		}
		else {
			/* double density */
			sectorsize[diskno - 1] = 256;
			if ((file_length & 0xff) == 0) {
				boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
				sectorcount[diskno - 1] = file_length >> 8;
			}
			else
				sectorcount[diskno - 1] = (file_length + 0x180) >> 8;
		}
	}

#ifdef DEBUG
	Aprint("sectorcount = %d, sectorsize = %d",
		   sectorcount[diskno - 1], sectorsize[diskno - 1]);
#endif
	format_sectorsize[diskno - 1] = sectorsize[diskno - 1];
	format_sectorcount[diskno - 1] = sectorcount[diskno - 1];
	strcpy(sio_filename[diskno - 1], filename);
	drive_status[diskno - 1] = status;
	disk[diskno - 1] = f;
	return TRUE;
}

void SIO_Dismount(int diskno)
{
	if (disk[diskno - 1] != NULL) {
		Util_fclose(disk[diskno - 1], sio_tmpbuf[diskno - 1]);
		disk[diskno - 1] = NULL;
		drive_status[diskno - 1] = NoDisk;
		strcpy(sio_filename[diskno - 1], "Empty");
	}
}

void SIO_DisableDrive(int diskno)
{
	SIO_Dismount(diskno);
	drive_status[diskno - 1] = Off;
	strcpy(sio_filename[diskno - 1], "Off");
}

static void SizeOfSector(UBYTE unit, int sector, int *sz, ULONG *ofs)
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
		offset = header_size[unit] + (sector - 1) * (boot_sectors_type[unit] == BOOT_SECTORS_PHYSICAL ? 256 : 128);
	}
	else {
		size = sectorsize[unit];
		offset = header_size[unit] + (boot_sectors_type[unit] == BOOT_SECTORS_LOGICAL ? 0x180 : 0x300) + (sector - 4) * size;
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

	sio_last_sector = sector;
	sprintf(sio_status, "%d: %d", unit + 1, sector);
	SizeOfSector((UBYTE) unit, sector, &size, &offset);
	fseek(disk[unit], offset, SEEK_SET);

	return size;
}

/* Unit counts from zero up */
static int ReadSector(int unit, int sector, UBYTE *buffer)
{
	int size;
	if (start_binloading)
		return BIN_loader_start(buffer);

	io_success[unit] = -1;
	if (drive_status[unit] == Off)
		return 0;
	if (disk[unit] == NULL)
		return 'N';
	if (sector <= 0 || sector > sectorcount[unit])
		return 'E';
	sio_last_op = SIO_LAST_READ;
	sio_last_op_time = 1;
	sio_last_drive = unit + 1;
	/* FIXME: what sector size did the user expect? */
	size = SeekSector(unit, sector);
	fread(buffer, 1, size, disk[unit]);
	io_success[unit] = 0;
	return 'C';
}

static int WriteSector(int unit, int sector, const UBYTE *buffer)
{
	int size;
	io_success[unit] = -1;
	if (drive_status[unit] == Off)
		return 0;
	if (disk[unit] == NULL)
		return 'N';
	if (drive_status[unit] != ReadWrite || sector <= 0 || sector > sectorcount[unit])
		return 'E';
	sio_last_op = SIO_LAST_WRITE;
	sio_last_op_time = 1;
	sio_last_drive = unit + 1;
	size = SeekSector(unit, sector);
	fwrite(buffer, 1, size, disk[unit]);
	io_success[unit] = 0;
	return 'C';
}

static int FormatDisk(int unit, UBYTE *buffer, int sectsize, int sectcount)
{
	char fname[FILENAME_MAX];
	int is_atr;
	int save_boot_sectors_type;
	int bootsectsize;
	int bootsectcount;
	FILE *f;
	int i;
	io_success[unit] = -1;
	if (drive_status[unit] == Off)
		return 0;
	if (disk[unit] == NULL)
		return 'N';
	if (drive_status[unit] != ReadWrite)
		return 'E';
	/* Note formatting the disk can change size of the file.
	   There is no portable way to truncate the file at given position.
	   We have to close the "rb+" open file and open it in "wb" mode.
	   First get the information about the disk image, because we are going
	   to umount it. */
	memcpy(fname, sio_filename[unit], FILENAME_MAX);
	is_atr = (header_size[unit] == 16);
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
	for (i = 1; i <= bootsectcount; i++)
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
	io_success[unit] = 0;
	return 'C';
}

/* Set density and number of sectors
   This function is used before the format (0x21) command
   to set how the disk will be formatted.
   Note this function does *not* affect the currently attached disk
   (previously sectorsize/sectorcount were used which could result in
   a corrupted image).
*/
static int WriteStatusBlock(int unit, const UBYTE *buffer)
{
	int size;
#ifdef DEBUG
	Aprint("Write Status-Block: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5], buffer[6], buffer[7],
		buffer[8], buffer[9], buffer[10], buffer[11]);
#endif
	if (drive_status[unit] == Off)
		return 0;
	/* We only care about the density and the sector count here.
	   Setting everything else right here seems to be non-sense.
	   I'm not sure about this density settings, my XF551
	   honors only the sector size and ignores the density */
	size = buffer[6] * 256 + buffer[7];
	if (size == 128 || size == 256)
		format_sectorsize[unit] = size;
	/* Note that the number of heads are minus 1 */
	format_sectorcount[unit] = buffer[0] * (buffer[2] * 256 + buffer[3]) * (buffer[4] + 1);
	if (format_sectorcount[unit] < 1 || format_sectorcount[unit] > 65535)
		format_sectorcount[unit] = 720;
	return 'C';
}

static int ReadStatusBlock(int unit, UBYTE *buffer)
{
	UBYTE tracks;
	UBYTE heads;
	int spt;
	if (drive_status[unit] == Off)
		return 0;
	/* default to 1 track, 1 side for non-standard images */
	tracks = 1;
	heads = 1;
	spt = sectorcount[unit];

	if (spt % 40 == 0) {
		/* standard disk */
		tracks = 40;
		spt /= 40;
		if (spt > 26 && spt % 2 == 0) {
			/* double-sided */
			heads = 2;
			spt >>= 1;
			if (spt > 26 && spt % 2 == 0) {
				/* double-sided, 80 tracks */
				tracks = 80;
				spt >>= 1;
			}
		}
	}

	buffer[0] = tracks;              /* # of tracks */
	buffer[1] = 1;                   /* step rate */
	buffer[2] = (UBYTE) (spt >> 8);  /* sectors per track. HI byte */
	buffer[3] = (UBYTE) spt;         /* sectors per track. LO byte */
	buffer[4] = (UBYTE) (heads - 1); /* # of heads minus 1 */
	/* FM for single density, MFM otherwise */
	buffer[5] = (sectorsize[unit] == 128 && sectorcount[unit] <= 720) ? 0 : 4;
	buffer[6] = (UBYTE) (sectorsize[unit] >> 8); /* bytes per sector. HI byte */
	buffer[7] = (UBYTE) sectorsize[unit];        /* bytes per sector. LO byte */
	buffer[8] = 1;                   /* drive is online */
	buffer[9] = 192;                 /* transfer speed, whatever this means */
	buffer[10] = 0;
	buffer[11] = 0;
	return 'C';
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
static int DriveStatus(int unit, UBYTE *buffer)
{
	if (start_binloading) {
		buffer[0] = 16 + 8;
		buffer[1] = 255;
		buffer[2] = 1;
		buffer[3] = 0 ;
		return 'C';
	}

	if (drive_status[unit] == Off)
		return 0;
	buffer[0] = 16;         /* drive active */
	buffer[1] = disk[unit] != NULL ? 255 /* WD 177x OK */ : 127 /* no disk */;
	if (io_success[unit] != 0)
		buffer[0] |= 4;     /* failed RW-operation */
	if (drive_status[unit] == ReadOnly)
		buffer[0] |= 8;     /* write protection */
	if (format_sectorsize[unit] == 256)
		buffer[0] |= 32;    /* double density */
	if (format_sectorcount[unit] == 1040)
		buffer[0] |= 128;   /* 1050 enhanced density */
	buffer[2] = 1;
	buffer[3] = 0;
	return 'C';
}

#ifndef NO_SECTOR_DELAY
/* A hack for the "Overmind" demo.  This demo verifies if sectors aren't read
   faster than with a typical disk drive.  We introduce a delay
   of SECTOR_DELAY scanlines between successive reads of sector 1. */
#define SECTOR_DELAY 3200
static int delay_counter = 0;
static int last_ypos = 0;
#endif

/* SIO patch emulation routine */
void SIO(void)
{
	int sector = dGetWordAligned(0x30a);
	UBYTE unit = dGetByte(0x301) - 1;
	UBYTE result = 0x00;
	UWORD data = dGetWordAligned(0x304);
	int length = dGetWordAligned(0x308);
	int realsize = 0;
	int cmd = dGetByte(0x302);

	/* FIXME: add dGetByte(0x300) and dGetByte(0x301) */
	if (dGetByte(0x300) == 0x31 && unit < MAX_DRIVES) {	/* UBYTE range ! */
#ifdef DEBUG
		Aprint("SIO disk command is %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x",
			cmd, dGetByte(0x303), dGetByte(0x304), dGetByte(0x305), dGetByte(0x306),
			dGetByte(0x308), dGetByte(0x309), dGetByte(0x30a), dGetByte(0x30b),
			dGetByte(0x30c), dGetByte(0x30d));
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
			if (sector == 1) {
				if (delay_counter > 0) {
					if (last_ypos != ypos) {
						last_ypos = ypos;
						delay_counter--;
					}
					regPC = 0xe459;	/* stay at SIO patch */
					return;
				}
				delay_counter = SECTOR_DELAY;
			}
			else {
				delay_counter = 0;
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
				/* there are programs which send the format-command but dont wait for the result (eg xf-tools) */
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
	/* cassette i/o */
	else if (dGetByte(0x300) == 0x60) {
		int storagelength = 0;
		UBYTE gaps = dGetByte(0x30b);
		switch (cmd){
		case 0x52:	/* read */
			/* set expected Gap */
			CASSETTE_AddGap(gaps == 0 ? 2000 : 160);
			/* get record from storage medium */
			storagelength = CASSETTE_Read();
			if (storagelength - 1 != length)	/* includes -1 as error */
				result = 'E';
			else
				result = 'C';
			/* check checksum */
			if (cassette_buffer[length] != SIO_ChkSum(cassette_buffer, length))
				result = 'E';
			/* if all went ok, copy to Atari */
			if (result == 'C')
				CopyToMem(cassette_buffer, data, length);
			break;
		case 0x57:	/* write */
			/* put record into buffer */
			CopyFromMem(data, cassette_buffer, length);
			/* eval checksum over buffer data */
			cassette_buffer[length] = SIO_ChkSum(cassette_buffer, length);
			/* add pregap length */
			CASSETTE_AddGap(gaps == 0 ? 3000 : 260);
			/* write full record to storage medium */
			storagelength = CASSETTE_Write(length + 1);
			if (storagelength - 1 != length)	/* includes -1 as error */
				result = 'E';
			else
				result = 'C';
			break;
		default:
			result = 'N';
		}
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

UBYTE SIO_ChkSum(const UBYTE *buffer, int length)
{
#if 0
	/* old, less efficient version */
	int i;
	int checksum = 0;
	for (i = 0; i < length; i++, buffer++) {
		checksum += *buffer;
		while (checksum > 255)
			checksum -= 255;
	}
#else
	int checksum = 0;
	while (--length >= 0)
		checksum += *buffer++;
	do
		checksum = (checksum & 0xff) + (checksum >> 8);
	while (checksum > 255);
#endif
	return checksum;
}

static UBYTE Command_Frame(void)
{
	int unit;
	int sector;
	int realsize;

	sector = CommandFrame[2] | (((UWORD) CommandFrame[3]) << 8);
	unit = CommandFrame[0] - '1';

	if (unit < 0 || unit >= MAX_DRIVES) {
		/* Unknown device */
		Aprint("Unknown command frame: %02x %02x %02x %02x %02x",
			   CommandFrame[0], CommandFrame[1], CommandFrame[2],
			   CommandFrame[3], CommandFrame[4]);
		TransferStatus = SIO_NoFrame;
		return 0;
	}
	switch (CommandFrame[1]) {
	case 0x4e:				/* Read Status */
#ifdef DEBUG
		Aprint("Read-status frame: %02x %02x %02x %02x %02x",
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
		Aprint("Write-status frame: %02x %02x %02x %02x %02x",
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
		Aprint("Write-sector frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		SizeOfSector((UBYTE) unit, sector, &realsize, NULL);
		ExpectedBytes = realsize + 1;
		DataIndex = 0;
		TransferStatus = SIO_WriteFrame;
		sio_last_op = SIO_LAST_WRITE;
		sio_last_op_time = 10;
		sio_last_drive = unit + 1;
		return 'A';
	case 0x52:				/* Read */
	case 0xD2:				/* xf551 hispeed */
#ifdef DEBUG
		Aprint("Read-sector frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		SizeOfSector((UBYTE) unit, sector, &realsize, NULL);
		DataBuffer[0] = ReadSector(unit, sector, DataBuffer + 1);
		DataBuffer[1 + realsize] = SIO_ChkSum(DataBuffer + 1, realsize);
		DataIndex = 0;
		ExpectedBytes = 2 + realsize;
		TransferStatus = SIO_ReadFrame;
		/* wait longer before confirmation because bytes could be lost */
		/* before the buffer was set (see $E9FB & $EA37 in XL-OS) */
		DELAYED_SERIN_IRQ = SERIN_INTERVAL << 2;
#ifndef NO_SECTOR_DELAY
		if (sector == 1) {
			DELAYED_SERIN_IRQ += delay_counter;
			delay_counter = SECTOR_DELAY;
		}
		else {
			delay_counter = 0;
		}
#endif
		sio_last_op = SIO_LAST_READ;
		sio_last_op_time = 10;
		sio_last_drive = unit + 1;
		return 'A';
	case 0x53:				/* Status */
#ifdef DEBUG
		Aprint("Status frame: %02x %02x %02x %02x %02x",
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
		Aprint("Format-disk frame: %02x %02x %02x %02x %02x",
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
		Aprint("Format-Medium frame: %02x %02x %02x %02x %02x",
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
		Aprint("Command frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		TransferStatus = SIO_NoFrame;
		return 'E';
	}
}

/* Enable/disable the Tape Motor */
void SIO_TapeMotor(int onoff)
{
	/* if sio is patched, do not do anything */
	if (enable_sio_patch)
		return;
	if (onoff) {
		/* set frame to cassette frame, if not */
		/* in a transfer with an intelligent peripheral */
		if (TransferStatus == SIO_NoFrame) {
			TransferStatus = SIO_CasRead;
			CASSETTE_TapeMotor(onoff);
			DELAYED_SERIN_IRQ = CASSETTE_GetInputIRQDelay();
		}
		else {
			CASSETTE_TapeMotor(onoff);
		}
	}
	else {
		/* set frame to none */
		if (TransferStatus == SIO_CasRead) {
			TransferStatus = SIO_NoFrame;
			CASSETTE_TapeMotor(onoff);
			DELAYED_SERIN_IRQ = 0; /* off */
		}
		else {
			CASSETTE_TapeMotor(onoff);
			DELAYED_SERIN_IRQ = 0; /* off */
		}
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

	sector = CommandFrame[2] + (CommandFrame[3] << 8);
	unit = CommandFrame[0] - '1';
	if (unit >= MAX_DRIVES)		/* UBYTE range ! */
		return 0;
	switch (CommandFrame[1]) {
	case 0x4f:				/* Write Status Block */
		return WriteStatusBlock(unit, DataBuffer);
	case 0x50:				/* Write */
	case 0x57:
	case 0xD0:				/* xf551 hispeed */
	case 0xD7:
		return WriteSector(unit, sector, DataBuffer);
	default:
		return 'E';
	}
}

/* Put a byte that comes out of POKEY. So get it here... */
void SIO_PutByte(int byte)
{
	switch (TransferStatus) {
	case SIO_CommandFrame:
		if (CommandIndex < ExpectedBytes) {
			CommandFrame[CommandIndex++] = byte;
			if (CommandIndex >= ExpectedBytes) {
				if (CommandFrame[0] >= 0x31 && CommandFrame[0] <= 0x38) {
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
				UBYTE sum = SIO_ChkSum(DataBuffer, ExpectedBytes - 1);
				if (sum == DataBuffer[ExpectedBytes - 1]) {
					UBYTE result = WriteSectorBack();
					if (result != 0) {
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
		/* FALL THROUGH */
	case SIO_ReadFrame:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
			}
			else {
				/* set delay using the expected transfer speed */
				DELAYED_SERIN_IRQ = (DataIndex == 1) ? SERIN_INTERVAL
					: ((SERIN_INTERVAL * AUDF[CHAN3] - 1) / 0x28 + 1);
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
	case SIO_CasRead:
		byte = CASSETTE_GetByte();
		DELAYED_SERIN_IRQ = CASSETTE_GetInputIRQDelay();
		break;
	default:
		break;
	}
	return byte;
}

#if !defined(BASIC) && !defined(__PLUS)
int Rotate_Disks(void)
{
	char tmp_filenames[MAX_DRIVES][FILENAME_MAX];
	int i;
	int bSuccess = TRUE;

	for (i = 0; i < MAX_DRIVES; i++) {
		strcpy(tmp_filenames[i], sio_filename[i]);
		SIO_Dismount(i + 1);
	}

	for (i = 1; i < MAX_DRIVES; i++) {
		if (strcmp(tmp_filenames[i], "None") && strcmp(tmp_filenames[i], "Off") && strcmp(tmp_filenames[i], "Empty") ) {
			if (!SIO_Mount(i, tmp_filenames[i], FALSE)) /* Note that this is NOT i-1 because SIO_Mount is 1 indexed */
				bSuccess = FALSE;
		}
	}

	i = MAX_DRIVES - 1;
	while (i > -1 && (!strcmp(tmp_filenames[i], "None") || !strcmp(tmp_filenames[i], "Off") || !strcmp(tmp_filenames[i], "Empty")) ) {
		i--;
	}

	if (i > -1)	{
		if (!SIO_Mount(i + 1, tmp_filenames[0], FALSE))
			bSuccess = FALSE;
	}

	return bSuccess;
}
#endif /* !defined(BASIC) && !defined(__PLUS) */

#ifndef BASIC

void SIOStateSave(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		SaveINT((int *) &drive_status[i], 1);
		SaveFNAME(sio_filename[i]);
	}
}

void SIOStateRead(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		int saved_drive_status;
		char filename[FILENAME_MAX];

		ReadINT(&saved_drive_status, 1);
		drive_status[i] = saved_drive_status;

		ReadFNAME(filename);
		if (filename[0] == 0)
			continue;

		/* If the disk drive wasn't empty or off when saved,
		   mount the disk */
		switch (saved_drive_status) {
		case ReadOnly:
			SIO_Mount(i + 1, filename, TRUE);
			break;
		case ReadWrite:
			SIO_Mount(i + 1, filename, FALSE);
			break;
		default:
			break;
		}
	}
}

#endif /* BASIC */
