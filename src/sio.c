/*

 * All Input is assumed to be going to RAM (no longer, ROM works, too.)
 * All Output is assumed to be coming from either RAM or ROM
 *
 */
#define Peek(a) (dGetByte((a)))
#define DPeek(a) ( dGetByte((a))+( dGetByte((a)+1)<<8 ) )
/* PM Notes:
   note the versions in Thors emu are able to deal with ROM better than
   these ones.  These are only used for the 'fake' fast SIO replacement
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	/* for toupper() */

#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#ifdef VMS
#include <unixio.h>
#include <file.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#endif

extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;

#include "atari.h"
#include "config.h"
#include "cpu.h"
#include "memory.h"
#include "sio.h"
#include "pokeysnd.h"
#include "platform.h"
#include "log.h"
#include "diskled.h"
#include "binload.h"

void CopyFromMem(int to, UBYTE *, int size);
void CopyToMem(UBYTE *, ATPtr from, int size);

/* Both ATR and XFD images can have 128- and 256-byte boot sectors */
static int boot_sectorsize[MAX_DRIVES];

/* Format is also size of header :-) */
typedef enum Format {
	XFD = 0, ATR = 16
} Format;

static Format format[MAX_DRIVES];
static int disk[MAX_DRIVES] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int sectorcount[MAX_DRIVES];
static int sectorsize[MAX_DRIVES];

UnitStatus drive_status[MAX_DRIVES];
char sio_filename[MAX_DRIVES][FILENAME_LEN];

static char	 tmp_filename[MAX_DRIVES][FILENAME_LEN];
static UBYTE istmpfile[MAX_DRIVES] = {0, 0, 0, 0, 0, 0, 0, 0};

/* Serial I/O emulation support */
UBYTE CommandFrame[6];
int CommandIndex = 0;
UBYTE DataBuffer[256 + 3];
char sio_status[256];
int DataIndex = 0;
int TransferStatus = 0;
int ExpectedBytes = 0;

extern int opendcm( int diskno, const char *infilename, char *outfilename );
#ifdef ZLIB_CAPABLE
extern int openzlib(int diskno, const char *infilename, char *outfilename );
#endif

void SIO_Initialise(int *argc, char *argv[])
{
	int i;

	for (i = 0; i < MAX_DRIVES; i++)
	{
		strcpy(sio_filename[i], "Empty");
		memset(tmp_filename[i], 0, FILENAME_LEN );
		istmpfile[i] = 0;
	}

	TransferStatus = SIO_NoFrame;
}

/* This is to clean temp files that were not caught with a dismount. 
   It should be called from Atari_Exit() */
void Clear_Temp_Files( void )
{
	int i;

	for( i = 0; i < MAX_DRIVES; i++ )
	{
		if( istmpfile[i] != 0 )
		{
			if( disk[i] != -1 )
			{
				close(disk[i]);
				disk[i] = -1;
			}
			remove( tmp_filename[i] );
			memset( tmp_filename[i], 0, FILENAME_LEN );
			istmpfile[i] = 0;
		}
	}
}

/* This will take an existing mounted file and set it as temp so it will be deleted 
   when dismounted. Primarily for mounted images that have been made on-the-fly to
   load an executable without having it be on an Atari disk */
void Set_Temp_File( int diskno )
{
	diskno--;
	if( disk[ diskno ] == -1 )
		return;
	istmpfile[ diskno ] = 1;
	strcpy( tmp_filename[ diskno ], sio_filename[ diskno ] );
}

int SIO_Mount(int diskno, const char *filename, int b_open_readonly)
{
	char	upperfile[ FILENAME_LEN ];
	struct ATR_Header header;
	int fd, i;

	fd = -1;
	memset( upperfile, 0, FILENAME_LEN );
	for( i=0; i < (int)strlen( filename ); i++ )
		upperfile[i] = toupper( filename[i] );

	/* If file is DCM, open it with opendcm to create a temp file */
	if( !strcmp( &upperfile[ strlen( upperfile )-3 ], "DCM" ) )
	{
		istmpfile[ diskno -1 ] = 1;
		drive_status[ diskno -1 ] = ReadOnly;
		fd = opendcm( diskno, filename, tmp_filename[diskno - 1] );
		if( fd == -1 )
			istmpfile[ diskno - 1] = 0;
	}
#ifdef ZLIB_CAPABLE
	else if( !strcmp( &upperfile[ strlen( upperfile )-3], "ATZ" ) || 
		     !strcmp( &upperfile[strlen( upperfile )-6], "ATR.GZ" ) ||
			 !strcmp( &upperfile[ strlen( upperfile )-3], "XFZ" ) || 
			 !strcmp( &upperfile[strlen( upperfile )-6], "XFD.GZ") )
	{
		istmpfile[ diskno -1 ] = 1;
		drive_status[ diskno -1 ] = ReadOnly;
		fd = openzlib( diskno, filename, tmp_filename[ diskno - 1] );
		if( fd == -1 )
		{
			istmpfile[ diskno - 1] = 0;
			drive_status[ diskno - 1] = NoDisk;
		}
	}	
#endif /* ZLIB_CAPABLE */
	else /* Normal ATR, XFD disk */
	{
		drive_status[diskno - 1] = ReadWrite;
		strcpy(sio_filename[diskno - 1], "Empty");

		if( b_open_readonly == FALSE )
		{
		fd = open(filename, O_RDWR | O_BINARY, 0777);
		}
		if ( b_open_readonly == TRUE || fd == -1) 
		{
			fd = open(filename, O_RDONLY | O_BINARY, 0755);
			drive_status[diskno - 1] = ReadOnly;
		}
	}

	if (fd >= 0) {
		int status;
		ULONG file_length = lseek(fd, 0L, SEEK_END);
		lseek(fd, 0L, SEEK_SET);

		status = read(fd, &header, sizeof(struct ATR_Header));
		if (status == -1) {
			close(fd);
			disk[diskno - 1] = -1;
			return FALSE;
		}

		strcpy(sio_filename[diskno - 1], filename);

		boot_sectorsize[diskno - 1] = 128;

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
					sectorcount[diskno - 1] += 3;		/* 128-byte boot sectors */
				else
					boot_sectorsize[diskno - 1] = 256;	/* 256-byte boot sectors */
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

			if (file_length <= (1040 * 128))
				sectorsize[diskno - 1] = 128;	/* single density */
			else {
				sectorsize[diskno - 1] = 256;	/* double density */
				if ((file_length & 0xff) == 0)
					boot_sectorsize[diskno - 1] = 256;
			}
			sectorcount[diskno - 1] = 3 + (file_length - 3 * boot_sectorsize[diskno - 1]) / sectorsize[diskno - 1];
		}
	}
	else {
		drive_status[diskno - 1] = NoDisk;
	}

	disk[diskno - 1] = fd;

	return (disk[diskno - 1] != -1) ? TRUE : FALSE;
}

void SIO_Dismount(int diskno)
{
	if (disk[diskno - 1] != -1) 
	{
		close(disk[diskno - 1]);
		disk[diskno - 1] = -1;
		drive_status[diskno - 1] = NoDisk;
		strcpy(sio_filename[diskno - 1], "Empty");
		if( istmpfile[ diskno - 1] )
		{
			remove( tmp_filename[ diskno - 1] );
			memset( tmp_filename[ diskno - 1], 0, FILENAME_LEN );
			istmpfile[ diskno - 1] = 0;
		}
	}
}

void SIO_DisableDrive(int diskno)
{
	drive_status[diskno - 1] = Off;
	strcpy(sio_filename[diskno - 1], "Off");
}

void SizeOfSector(UBYTE unit, int sector, int *sz, ULONG * ofs)
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
		offset = format[unit] + (sector - 1) * boot_sectorsize[unit];
	}
	else {
		size = sectorsize[unit];
		offset = format[unit] + 3 * boot_sectorsize[unit] + (sector - 4) * size;
	}

	if (sz)
		*sz = size;

	if (ofs)
		*ofs = offset;
}

int SeekSector(int unit, int sector)
{
	ULONG offset;
	int size;

	sprintf(sio_status, "%d: %d", unit + 1, sector);
	SizeOfSector((UBYTE)unit, sector, &size, (ULONG*)&offset);
	if (offset < 0 || offset > lseek(disk[unit], 0L, SEEK_END)) {
#ifdef DEBUG
		Aprint("SIO:SeekSector() - Wrong seek offset");
#endif
	}
	else
		lseek(disk[unit], offset, SEEK_SET);

	return size;
}

/* Unit counts from zero up */
int ReadSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	if (start_binloading)
		return(BIN_loade_start(buffer));

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (sector > 0 && sector <= sectorcount[unit]) {
				Set_LED_Read(unit);
				size = SeekSector(unit, sector);
				read(disk[unit], buffer, size);
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

int WriteSector(int unit, int sector, UBYTE * buffer)
{
	int size;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				Set_LED_Write(unit);
				if (sector > 0 && sector <= sectorcount[unit]) {
					size = SeekSector(unit, sector);
					write(disk[unit], buffer, size);
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

/* Before this function we were ruining ATR images every time they were
 * formatted, because they would be re-initialized as XFD, paying no
 * attention to the ATR header information, and in fact overwriting it!
 */

int ATRFormat( int unit, UBYTE * buffer )
{
	int i;

	if( drive_status[unit] != Off )
	{
		if( disk[unit] != -1 ) 
		{
			if( drive_status[unit] == ReadWrite )
			{
				SeekSector( unit, 1 );
				memset( buffer, 0, sectorsize[unit] );
				for( i = 1; i <= sectorcount[unit]; i++ )
					write( disk[unit], buffer, sectorsize[unit] );
				memset( buffer, 0xff, sectorsize[unit] );
				return 'C';
			}
			else
				return 'E';
		}
		else
			return 'N';
	}

	return 0;
}

/*
 * FormatSingle is used on the XF551 for formating SS/DD and DS/DD too
 * however, I have to check if they expect a 256 byte buffer or if 128
 * is ok either
 */
int FormatSingle(int unit, UBYTE * buffer)
{
	int i;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				if( format[unit] == ATR )
					return ATRFormat( unit, buffer );
				sectorcount[unit] = 720;
				sectorsize[unit] = 128;
				format[unit] = XFD;
				SeekSector(unit, 1);
				memset(buffer, 0, 128);
				for (i = 1; i <= 720; i++)
					write(disk[unit], buffer, 128);
				memset(buffer, 0xff, 128);
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

int FormatEnhanced(int unit, UBYTE * buffer)
{
	int i;

	if (drive_status[unit] != Off) {
		if (disk[unit] != -1) {
			if (drive_status[unit] == ReadWrite) {
				if( format[unit] == ATR )
					return ATRFormat( unit, buffer );
				sectorcount[unit] = 1040;
				sectorsize[unit] = 128;
				format[unit] = XFD;
				SeekSector(unit, 1);
				memset(buffer, 0, 128);
				for (i = 1; i <= 1040; i++)
					write(disk[unit], buffer, 128);
				memset(buffer, 0xff, 128);
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

int WriteStatusBlock(int unit, UBYTE * buffer)
{
	int size;

	if (drive_status[unit] != Off) {
		/*
		 * We only care about the density and the sector count
		 * here. Setting everything else right here seems to
		 * be non-sense
		 */
		if (format[unit] == ATR) {
			/* I'm not sure about this density settings, my XF551
			 * honnors only the sector size and ignore the density
			 */
			size = buffer[6] * 256 + buffer[7];
			if (size == 128 || size == 256)
				sectorsize[unit] = size;
			else if (buffer[5] == 8) {
				sectorsize[unit] = 256;
			}
			else {
				sectorsize[unit] = 128;
			}
			/* Note, that the number of heads are minus 1 */
			sectorcount[unit] = buffer[0] * (buffer[2] * 256 + buffer[3]) * (buffer[4] + 1);
			return 'C';
		}
		else
			return 'E';
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
int ReadStatusBlock(int unit, UBYTE * buffer)
{
	int size, spt, heads;

	if (drive_status[unit] != Off) {
		spt = sectorcount[unit] / 40;
		heads =  (spt > 26) ? 2 : 1;
		SizeOfSector((UBYTE)unit, 0x168, &size, NULL);

		buffer[0] = 40;			/* # of tracks */
		buffer[1] = 1;			/* step rate. No idea what this means */
		buffer[2] = spt >> 8;	/* sectors per track. HI byte */
		buffer[3] = spt & 0xFF;	/* sectors per track. LO byte */
		buffer[4] = heads-1;	/* # of heads */
		if (size == 128) {
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
   Bit 2 = 1 indicates that a PUT operation was unsuccessful
   Bit 3 = 1 indicates that the diskete is write protected
   Bit 4 = 1 indicates active/standby

   plus

   Bit 5 = 1 indicates double density
   Bit 7 = 1 indicates duel density disk (1050 format)
 */
int DriveStatus(int unit, UBYTE * buffer)
{
	if (start_binloading) {
		buffer[0] = 0;
		buffer[1] = 64;
		buffer[2] = 1;
		buffer[3] = 0 ;
		return 'C';
	}
		
	if (drive_status[unit] != Off) {
		if (drive_status[unit] == ReadWrite) {
			buffer[0] = (sectorsize[unit] == 256) ? (32 + 16) : (16);
			/* buffer[1] = (disk[unit] != -1) ? (128) : (0); */
			buffer[1] = (disk[unit] != -1) ? (255) : (0);	/* for StripPoker */
		}
		else {
			buffer[0] = (sectorsize[unit] == 256) ? (32 + 8) : (8);
			buffer[1] = (disk[unit] != -1) ? (192) : (64);
		}
		if (sectorcount[unit] == 1040)
			buffer[0] |= 128;
		buffer[2] = 1;
		buffer[3] = 0;
		return 'C';
	}
	else
		return 0;
}


void SIO(void)
{
	int sector = DPeek(0x30a);
	UBYTE unit = Peek(0x301) - 1;
	UBYTE result = 0x00;
	ATPtr data = DPeek(0x304);
	int length = DPeek(0x308);
	int realsize = 0;
	int cmd = Peek(0x302);
#ifndef NO_SECTOR_DELAY
	static int delay_counter = 1;	/* no delay on first read */
#endif

	if (Peek(0x300) == 0x31)
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
			SizeOfSector(unit, sector, &realsize, NULL);
			if (realsize == length) {
				CopyFromMem(data, DataBuffer, realsize);
				result = WriteSector(unit, sector, DataBuffer);
			}
			else
				result = 'E';
			break;
		case 0x52:				/* Read */
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
		case 0x21:				/* Single Density Format */
			realsize = 128;
			if (realsize == length) {
				result = FormatSingle(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x22:				/* Enhanced Density Format */
			realsize = 128;
			if (realsize == length) {
				result = FormatEnhanced(unit, DataBuffer);
				if (result == 'C')
					CopyToMem(DataBuffer, data, realsize);
			}
			else
				result = 'E';
			break;
		case 0x66:				/* US Doubler Format - I think! */
			if( format[unit] == ATR )
				result = ATRFormat( unit, DataBuffer );
			result = 'A';		/* Not yet supported... to be done later... */
			break;
		default:
			result = 'N';
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
	Poke(0x0303, regY);
	Poke(0x42,0);
	SetC;
	Set_LED_Off();

}

void SIO_Initialize(void)
{
	TransferStatus = SIO_NoFrame;
}


UBYTE ChkSum(UBYTE * buffer, UWORD length)
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

void Command_Frame(void)
{
	int unit;
	int result = 'A';
	int sector;
	int realsize;


	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
	if (unit > 8) {				/* UBYTE - range ! */
		Aprint("Unknown command frame: %02x %02x %02x %02x %02x",
			   CommandFrame[0], CommandFrame[1], CommandFrame[2],
			   CommandFrame[3], CommandFrame[4]);
		result = 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4e:				/* Read Status */
			DataBuffer[0] = ReadStatusBlock(unit, DataBuffer + 1);
			DataBuffer[13] = ChkSum(DataBuffer + 1, 12);
			DataIndex = 0;
			ExpectedBytes = 14;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			break;
		case 0x4f:
			ExpectedBytes = 13;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
			break;
		case 0x50:				/* Write */
		case 0x57:
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			ExpectedBytes = realsize + 1;
			DataIndex = 0;
			TransferStatus = SIO_WriteFrame;
			Set_LED_Write(unit);
			break;
		case 0x52:				/* Read */
			SizeOfSector((UBYTE)unit, sector, &realsize, NULL);
			DataBuffer[0] = ReadSector(unit, sector, DataBuffer + 1);
			DataBuffer[1 + realsize] = ChkSum(DataBuffer + 1, (UWORD)realsize);
			DataIndex = 0;
			ExpectedBytes = 2 + realsize;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
#ifndef NO_SECTOR_DELAY
			if (sector == 2)
				DELAYED_SERIN_IRQ += SECTOR_DELAY;
#endif
			Set_LED_Read(unit);
			break;
		case 0x53:				/* Status */
			DataBuffer[0] = DriveStatus(unit, DataBuffer + 1);
			DataBuffer[1 + 4] = ChkSum(DataBuffer + 1, 4);
			DataIndex = 0;
			ExpectedBytes = 6;
			TransferStatus = SIO_ReadFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			break;
		case 0x21:				/* Single Density Format */
			DataBuffer[0] = FormatSingle(unit, DataBuffer + 1);
			DataBuffer[1 + 128] = ChkSum(DataBuffer + 1, 128);
			DataIndex = 0;
			ExpectedBytes = 2 + 128;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			break;
		case 0x22:				/* Duel Density Format */
			DataBuffer[0] = FormatEnhanced(unit, DataBuffer + 1);
			DataBuffer[1 + 128] = ChkSum(DataBuffer + 1, 128);
			DataIndex = 0;
			ExpectedBytes = 2 + 128;
			TransferStatus = SIO_FormatFrame;
			DELAYED_SERIN_IRQ = SERIN_INTERVAL;
			break;
		case 0x66:				/* US Doubler Format - I think! */
			result = 'A';		/* Not yet supported... to be done later... */
			break;
		default:
			Aprint("Command frame: %02x %02x %02x %02x %02x",
				   CommandFrame[0], CommandFrame[1], CommandFrame[2],
				   CommandFrame[3], CommandFrame[4]);
			result = 0;
			break;
		}

	if (result == 0)
		TransferStatus = SIO_NoFrame;
}


/* Enable/disable the command frame */
void SwitchCommandFrame(int onoff)
{

	if (onoff) {				/* Enabled */
		if (TransferStatus != SIO_NoFrame)
			Aprint("Unexpected command frame %x.", TransferStatus);
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

UBYTE WriteSectorBack(void)
{
	UWORD sector;
	UBYTE unit;
	UBYTE result;

	sector = CommandFrame[2] | (((UWORD) (CommandFrame[3])) << 8);
	unit = CommandFrame[0] - '1';
	if (unit > 8) {				/* UBYTE range ! */
		result = 0;
	}
	else
		switch (CommandFrame[1]) {
		case 0x4f:				/* Write Status Block */
			result = WriteStatusBlock(unit, DataBuffer);
			break;
		case 0x50:				/* Write */
		case 0x57:
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
				sum = ChkSum(DataBuffer, (UWORD)(ExpectedBytes - 1));
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
		byte = 'A';				/* Command acknoledged */
		Command_Frame();		/* Handle now the command */
		break;
	case SIO_FormatFrame:
		TransferStatus = SIO_ReadFrame;
		DELAYED_SERIN_IRQ = SERIN_INTERVAL << 3;
	case SIO_ReadFrame:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = SIO_NoFrame;
				Set_LED_Off();
			}
			else {
				DELAYED_SERIN_IRQ = SERIN_INTERVAL;
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
				Set_LED_Off();
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
			Set_LED_Off();
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
	char	tmp_filenames[MAX_DRIVES][ FILENAME_LEN ];
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
