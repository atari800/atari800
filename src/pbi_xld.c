/*
 * pbi_xld.c - 1450XLD and 1400XL emulation
 *
 * Copyright (C) 2007-2008 Perry McFarlane
 * Copyright (C) 2002-2008 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "votrax.h"
#include "pbi_xld.h"
#include "pbi.h"
#include "util.h"
#include "sio.h"
#include "log.h"
#include "pokey.h"
#include "cpu.h"
#include "memory.h"
#include "stdlib.h"

#define DISK_PBI_NUM 0
#define MODEM_PBI_NUM 1
#define VOICE_PBI_NUM 7

#define DISK_MASK (1 << DISK_PBI_NUM)
#define MODEM_MASK (1 << MODEM_PBI_NUM)
#define VOICE_MASK (1 << VOICE_PBI_NUM)

static char *voicerom;
static char *diskrom;
static char xld_d_rom_filename[FILENAME_MAX] = FILENAME_NOT_SET;
static char xld_v_rom_filename[FILENAME_MAX] = FILENAME_NOT_SET;

static UBYTE votrax_latch = 0;
static UBYTE modem_latch = 0;

int PBI_XLD_enabled = FALSE;
static int xld_v_enabled = FALSE;
static int xld_d_enabled = FALSE;

/* Parallel Disk I/O emulation support */
#define PIO_NoFrame         (0x00)
#define PIO_CommandFrame    (0x01)
#define PIO_StatusRead      (0x02)
#define PIO_ReadFrame       (0x03)
#define PIO_WriteFrame      (0x04)
#define PIO_FinalStatus     (0x05)
#define PIO_FormatFrame     (0x06)
static UBYTE CommandFrame[6];
static int CommandIndex = 0;
static UBYTE DataBuffer[256 + 3];
static int DataIndex = 0;
static int TransferStatus = PIO_CommandFrame;
static int ExpectedBytes = 5;

static void PIO_PutByte(int byte);
static int PIO_GetByte(void);
static UBYTE PIO_Command_Frame(void);

static double ratio;
static int bit16;
#define VOTRAX_BLOCK_SIZE 1024 
void *temp_votrax_buffer;
void *votrax_buffer;

#ifdef PBI_DEBUG
#define D(a) a
#else
#define D(a) do{}while(0)
#endif

#define VOTRAX_RATE 24500

void PBI_XLD_Initialise(int *argc, char *argv[])
{
	int i, j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-1400") == 0) {
			xld_v_enabled = TRUE;
			PBI_XLD_enabled = TRUE;
		}else if (strcmp(argv[i], "-xld") == 0){
			xld_v_enabled = TRUE;
			xld_d_enabled = TRUE;
			PBI_XLD_enabled = TRUE;
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-1400            Emulate the Atari 1400XL");
				Aprint("\t-xld             Emulate the Atari 1450XLD");
			}

			argv[j++] = argv[i];
		}

	}
	*argc = j;

	if (xld_v_enabled) {
		voicerom = Util_malloc(0x1000);
		Atari800_LoadImage(xld_v_rom_filename, voicerom, 0x1000);
		printf("loaded XLD voice rom image\n");
		PBI_D6D7ram = TRUE;
	}

	/* If you set the drive to empty in the UI, the message is displayed */
	/* If you press select, I believe it tries to slow the I/O down */
	/* in order to increase compatibility. */
	/* dskcnt6 works. dskcnt10 does not */
	if (xld_d_enabled) {
		diskrom = Util_malloc(0x800);
		Atari800_LoadImage(xld_d_rom_filename, diskrom, 0x800);
		D(printf("loaded 1450XLD D: device driver rom image\n"));
		PBI_D6D7ram = TRUE;
	}
}

int PBI_XLD_ReadConfig(char *string, char *ptr) 
{
	if (strcmp(string, "XLD_D_ROM") == 0)
		Util_strlcpy(xld_d_rom_filename, ptr, sizeof(xld_d_rom_filename));
	else if (strcmp(string, "XLD_V_ROM") == 0)
		Util_strlcpy(xld_v_rom_filename, ptr, sizeof(xld_v_rom_filename));
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void PBI_XLD_WriteConfig(FILE *fp)
{
	fprintf(fp, "XLD_D_ROM=%s\n", xld_d_rom_filename);
	fprintf(fp, "XLD_V_ROM=%s\n", xld_v_rom_filename);
}

void PBI_XLD_Reset(void)
{
	votrax_latch = 0;
}

int PBI_XLD_D1_GetByte(UWORD addr)
{
	int result = PBI_NOT_HANDLED;
	if (xld_d_enabled && addr == 0xd114) {
	/* XLD input from disk to atari byte latch */
		result = (int)PIO_GetByte();
		D(printf("d114: disk read byte:%2x\n",result));
	}
	return result;
}

/* D1FF: each bit indicates IRQ status of a device */
UBYTE PBI_XLD_D1FF_GetByte()
{
	UBYTE result = 0;
	/* VOTRAX BUSY IRQ bit */
	if (!votraxsc01_status_r(0)) {
		result |= VOICE_MASK;
	}
	return result;
}

volatile static int votrax_written = FALSE;
volatile static int votrax_written_byte = 0x3f;
static int votrax_sync_samples;
static int votrax_busy = FALSE;

static void votrax_busy_callback(int busy_status);

void PBI_XLD_D1_PutByte(UWORD addr, UBYTE byte)
{
	if ((addr & ~3) == 0xd104)  {
		/* XLD disk strobe line */
		D(printf("votrax write:%4x\n",addr));
		votrax_sync_samples = (int)((1.0/ratio)*(double)votraxsc01_samples(votrax_written_byte, votrax_latch & 0x3f, votrax_sync_samples));
		/* write to both the sync and async */
		votrax_written = TRUE;
		votrax_written_byte = votrax_latch & 0x3f;
		if (!votrax_busy) {
			votrax_busy = TRUE;
			votrax_busy_callback(TRUE); /* idle -> busy */
		}

	}
	else if ((addr & ~3) == 0xd100 )  {
		/* votrax phoneme+irq-enable latch */
		if ( !(votrax_latch & 0x80) && (byte & 0x80) && (!votraxsc01_status_r(0))) {
			/* IRQ disabled -> enabled, and votrax idle: generate IRQ */
			D(printf("votrax IRQ generated: IRQ enable changed and idle\n"));
			GenerateIRQ();
			PBI_IRQ |= VOICE_MASK;
		} else if ((votrax_latch & 0x80) && !(byte & 0x80) ){
			/* IRQ enabled -> disabled : stop IRQ */
			PBI_IRQ &= ~VOICE_MASK;
			/* update pokey IRQ status */
			POKEY_PutByte(_IRQEN, IRQEN);
		}
		votrax_latch = byte;
	}
	else if (addr == 0xd108) {
	/* modem latch and XLD 8040 T1 input */
		D(printf("XLD 8040 T1:%d loop-back:%d modem+phone:%d offhook(modem relay):%d phaudio:%d DTMF:%d O/!A(originate/answer):%d SQT(squelch transmitter):%d\n",!!(byte&0x80),!!(byte&0x40),!!(byte&0x20),!!(byte&0x10),!!(byte&0x08),!!(byte&0x04),!!(byte&0x02),!!(byte&0x01)));
		modem_latch = byte;
	}
	else if (xld_d_enabled && addr == 0xd110) {
	/* XLD byte output from atari to disk latch */ 
		D(printf("d110: disk output byte:%2x\n",byte));
		if (modem_latch & 0x80){
			/* 8040 T1=1 */
			CommandIndex = 0;
			DataIndex = 0;
			TransferStatus = PIO_CommandFrame;
			ExpectedBytes = 5;
			D(printf("command frame expected\n"));
		}
		else if (TransferStatus == PIO_StatusRead || TransferStatus == PIO_ReadFrame) {
			D(printf("read ack strobe\n"));
		}
		else {
			PIO_PutByte(byte);
		}
	}
}

int PBI_XLD_D1FF_PutByte(UBYTE byte)
{
	int result = 0; /* handled */
	if (xld_d_enabled && byte == DISK_MASK) {
		memcpy(memory + 0xd800, diskrom, 0x800);
		D(printf("DISK rom activated\n"));
	} 
	else if (byte == MODEM_MASK) {
		memcpy(memory + 0xd800, voicerom + 0x800, 0x800);
		D(printf("MODEM rom activated\n"));
	} 
	else if (byte == VOICE_MASK) { 
		memcpy(memory + 0xd800, voicerom, 0x800);
		D(printf("VOICE rom activated\n"));
	}
	else result = PBI_NOT_HANDLED;
	return result;
}

static void votrax_busy_callback(int busy_status)
{
	if (!busy_status && (votrax_latch & 0x80)){
		/* busy->idle and IRQ enabled */
		D(printf("votrax IRQ generated\n"));
		GenerateIRQ();		
		PBI_IRQ |= VOICE_MASK;
	}
	else if (busy_status && (PBI_IRQ & VOICE_MASK)) {
		/* idle->busy and PBI_IRQ set */
		PBI_IRQ &= ~VOICE_MASK;
		/* update pokey IRQ status */
		POKEY_PutByte(_IRQEN, IRQEN);
	}
}

static void votrax_busy_callback_async(int busy_status)
{
	return;
	/* do nothing */
}

/* from sio.c */
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
		return SIO_WriteStatusBlock(unit, DataBuffer);
	case 0x50:				/* Write */
	case 0x57:
	case 0xD0:				/* xf551 hispeed */
	case 0xD7:
		return SIO_WriteSector(unit, sector, DataBuffer);
	default:
		return 'E';
	}
}

/* Put a byte that comes from the parallel bus */
static void PIO_PutByte(int byte)
{
	D(printf("TransferStatus:%d\n",TransferStatus));
	switch (TransferStatus) {
	case PIO_CommandFrame:
		D(printf("CommandIndex:%d ExpectedBytes:%d\n",CommandIndex,ExpectedBytes));
		if (CommandIndex < ExpectedBytes) {
			CommandFrame[CommandIndex++] = byte;
			if (CommandIndex >= ExpectedBytes) {
				if (CommandFrame[0] >= 0x31 && CommandFrame[0] <= 0x38) {
					TransferStatus = PIO_StatusRead;
					/*DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;*/
					D(printf("TransferStatus = PIO_StatusRead\n"));
				}
				else{
					TransferStatus = PIO_NoFrame;
					D(printf("TransferStatus = PIO_NoFrame\n"));
				}
			}
		}
		else {
			Aprint("Invalid command frame!");
			TransferStatus = PIO_NoFrame;
		}
		break;
	case PIO_WriteFrame:		/* Expect data */
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
						/*DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;*/
						TransferStatus = PIO_FinalStatus;
					}
					else
						TransferStatus = PIO_NoFrame;
				}
				else {
					DataBuffer[0] = 'E';
					DataIndex = 0;
					ExpectedBytes = 1;
					/*DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;*/
					TransferStatus = PIO_FinalStatus;
				}
			}
		}
		else {
			Aprint("Invalid data frame!");
		}
		break;
	}
	/*DELAYED_SEROUT_IRQ = SEROUT_INTERVAL;*/
}

/* Get a byte from the floppy to the parallel bus. */
static int PIO_GetByte(void)
{
	int byte = 0;
	D(printf("PIO_GetByte TransferStatus:%d\n",TransferStatus));

	switch (TransferStatus) {
	case PIO_StatusRead:
		byte = PIO_Command_Frame();		/* Handle now the command */
		break;
	case PIO_FormatFrame:
		TransferStatus = PIO_ReadFrame;
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL << 3;*/
		/* FALL THROUGH */
	case PIO_ReadFrame:
		D(printf("ReadFrame: DataIndex:%d ExpectedBytes:%d\n",DataIndex,ExpectedBytes));
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = PIO_NoFrame;
			}
			else {
				/* set delay using the expected transfer speed */
				/*DELAYED_SERIN_IRQ = (DataIndex == 1) ? SERIN_INTERVAL*/
					/*: ((SERIN_INTERVAL * AUDF[CHAN3] - 1) / 0x28 + 1);*/
			}
		}
		else {
			Aprint("Invalid read frame!");
			TransferStatus = PIO_NoFrame;
		}
		break;
	case PIO_FinalStatus:
		if (DataIndex < ExpectedBytes) {
			byte = DataBuffer[DataIndex++];
			if (DataIndex >= ExpectedBytes) {
				TransferStatus = PIO_NoFrame;
			}
			else {
				if (DataIndex == 0)
				;	/*DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;*/
				else
				;	/*DELAYED_SERIN_IRQ = SERIN_INTERVAL;*/
			}
		}
		else {
			Aprint("Invalid read frame!");
			TransferStatus = PIO_NoFrame;
		}
		break;
	default:
		break;
	}
	return byte;
}

static UBYTE PIO_Command_Frame(void)
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
		TransferStatus = PIO_NoFrame;
		return 0;
	}
	switch (CommandFrame[1]) {
	case 0x01:
		Aprint("PIO DISK: Set large mode (unimplemented)");
		return 'E';
	case 0x02:
		Aprint("PIO DISK: Set small mode (unimplemented)");
		return 'E';
	case 0x23:
		Aprint("PIO DISK: Drive Diagnostic In (unimplemented)");
		return 'E';
	case 0x24:
		Aprint("PIO DISK: Drive Diagnostic Out (unimplemented)");
		return 'E';
	case 0x4e:				/* Read Status */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Read-status frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		DataBuffer[0] = SIO_ReadStatusBlock(unit, DataBuffer + 1);
		DataBuffer[13] = SIO_ChkSum(DataBuffer + 1, 12);
		DataIndex = 0;
		ExpectedBytes = 14;
		TransferStatus = PIO_ReadFrame;
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL;*/
		return 'A';
	case 0x4f:				/* Write status */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Write-status frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		ExpectedBytes = 13;
		DataIndex = 0;
		TransferStatus = PIO_WriteFrame;
		return 'A';
	case 0x50:				/* Write */
	case 0x57:
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Write-sector frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		SIO_SizeOfSector((UBYTE) unit, sector, &realsize, NULL);
		ExpectedBytes = realsize + 1;
		DataIndex = 0;
		TransferStatus = PIO_WriteFrame;
		sio_last_op = SIO_LAST_WRITE;
		sio_last_op_time = 10;
		sio_last_drive = unit + 1;
		return 'A';
	case 0x52:				/* Read */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Read-sector frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		SIO_SizeOfSector((UBYTE) unit, sector, &realsize, NULL);
		DataBuffer[0] = SIO_ReadSector(unit, sector, DataBuffer + 1);
		DataBuffer[1 + realsize] = SIO_ChkSum(DataBuffer + 1, realsize);
		DataIndex = 0;
		ExpectedBytes = 2 + realsize;
		TransferStatus = PIO_ReadFrame;
		/* wait longer before confirmation because bytes could be lost */
		/* before the buffer was set (see $E9FB & $EA37 in XL-OS) */
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL << 2;*/
		/*
#ifndef NO_SECTOR_DELAY
		if (sector == 1) {
			//DELAYED_SERIN_IRQ += delay_counter;
			delay_counter = SECTOR_DELAY;
		}
		else {
			delay_counter = 0;
		}
#endif*/
		sio_last_op = SIO_LAST_READ;
		sio_last_op_time = 10;
		sio_last_drive = unit + 1;
		return 'A';
	case 0x53:				/* Status */
		/*
		from spec doc:
		BYTE 1 - DISK STATUS

		BIT 0 = 1 indicates an invalid
		command frame was receiv-
		ed.
		BIT 1 = 1 indicates an invalid
		data frame was received.
		BIT 2 = 1 indicates an opera-
		tion was unsuccessful.
		BIT 3 = 1 indicates the disk-
		ette is write protected.
		BIT 4 = 1 indicates drive is
		active.
		BITS 5-7 = 100 indicates single
		density format.
		BITS 5-7 = 101 indicates double
		density format.

		BYTE 2 - DISK CONTROLLER HARDWARE
		STATUS

		This byte shall contain the in-
		verted value of the disk con-
		troller hardware status regis-
		ter as of the last operation.
		The hardware status value for
		no errors shall be $FF. A zero
		in any bit position shall indi-
		cate an error. The definition
		of the bit positions shall be:

		BIT 0 = 0 indicates device busy
		BIT 1 = 0 indicates data re-
		quest is full on a read
		operation.
		BIT 2 = 0 indicates data lost
		BIT 3 = 0 indicates CRC error
		BIT 4 = 0 indicates desired
		track and sector not found
		BIT 5 = 0 indicates record
		type/write fault
		BIT 6 NOT USED
		*BIT 7 = 0 indicates device not
		ready (door open)

		BYTES 3 & 4 - TIMEOUT

		These bytes shall contain a
		disk controller provided maxi-
		mum timeout value, in seconds,
		for the worst case command. The
		worst case operation is for a
		disk format command (time TBD
		seconds). Byte 4 is not used,
		currently.*/
		/*****Compare with:******/
/*
   Status Request from Atari 400/800 Technical Reference Notes

   DVSTAT + 0   Command Status
   DVSTAT + 1   Hardware Status
   DVSTAT + 2   Timeout
   DVSTAT + 3   Unused

   Command Status Bits

   Bit 0 = 1 indicates an invalid command frame was received(same)
   Bit 1 = 1 indicates an invalid data frame was received(same)
   Bit 2 = 1 indicates that last read/write operation was unsuccessful(same)
   Bit 3 = 1 indicates that the diskette is write protected(same)
   Bit 4 = 1 indicates active/standby(same)

   plus

   Bit 5 = 1 indicates double density
   Bit 7 = 1 indicates dual density disk (1050 format)
 */

#ifdef PBI_DEBUG
		Aprint("PIO DISK: Status frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		/*if (drive_status[unit]==Off) drive_status[unit]=NoDisk;*/
		/*need to modify the line below also for  Off==NoDisk*/
		DataBuffer[0] = SIO_DriveStatus(unit, DataBuffer + 1);
		DataBuffer[2] = 0xff;/*/1;//SIO_DriveStatus(unit, DataBuffer + 1);*/
		if (drive_status[unit]==NoDisk || drive_status[unit]==Off){
		/*Can't turn 1450XLD drives off, so make Off==NoDisk*/
			DataBuffer[2]=0x7f;
		}
		DataBuffer[1 + 4] = SIO_ChkSum(DataBuffer + 1, 4);
		DataIndex = 0;
		ExpectedBytes = 6;
		TransferStatus = PIO_ReadFrame;
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL;*/
		return 'A';
	case 0x21:				/* Format Disk */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Format-disk frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		realsize = SIO_format_sectorsize[unit];
		DataBuffer[0] = SIO_FormatDisk(unit, DataBuffer + 1, realsize, SIO_format_sectorcount[unit]);
		DataBuffer[1 + realsize] = SIO_ChkSum(DataBuffer + 1, realsize);
		DataIndex = 0;
		ExpectedBytes = 2 + realsize;
		TransferStatus = PIO_FormatFrame;
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL;*/
		return 'A';
	case 0x22:				/* Dual Density Format */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Format-Medium frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		DataBuffer[0] = SIO_FormatDisk(unit, DataBuffer + 1, 128, 1040);
		DataBuffer[1 + 128] = SIO_ChkSum(DataBuffer + 1, 128);
		DataIndex = 0;
		ExpectedBytes = 2 + 128;
		TransferStatus = PIO_FormatFrame;
		/*DELAYED_SERIN_IRQ = SERIN_INTERVAL;*/
		return 'A';

		/*
		The Integral Disk Drive uses COMMAND BYTE $B1 and
			$B2 for internal use. These COMMAND BYTES may not
			be used by any other drivers.*/
	case 0xb1:
		Aprint("PIO DISK: Internal Command 0xb1 (unimplemented)");
		return 'E';
	case 0xb2:
		Aprint("PIO DISK: Internal Command 0xb2 (unimplemented)");
		return 'E';


	default:
		/* Unknown command for a disk drive */
#ifdef PBI_DEBUG
		Aprint("PIO DISK: Unknown Command frame: %02x %02x %02x %02x %02x",
			CommandFrame[0], CommandFrame[1], CommandFrame[2],
			CommandFrame[3], CommandFrame[4]);
#endif
		TransferStatus = PIO_NoFrame;
		return 'E';
	}
}

static int dsprate;
static int stereo;
static int samples_per_frame;

/* called from Pokey_sound_init */
void PBI_XLD_V_Init(int playback_freq, int num_pokeys, int b16)
{
	static struct VOTRAXSC01interface vi;
	int temp_votrax_buffer_size;
	if (!xld_v_enabled) return;
	bit16 = b16;
	dsprate = playback_freq;
	stereo = (num_pokeys == 2);
	if (num_pokeys != 1 && num_pokeys != 2) {
		Aprint("PBI_XLD_V_Init: cannot handle num_pokeys=%d", num_pokeys);
	}
	vi.num = 1;
	vi.BusyCallback[0] = votrax_busy_callback_async;
	VOTRAXSC01_sh_start((void *)&vi);
	samples_per_frame = dsprate/(tv_mode == TV_PAL ? 50 : 60);
	ratio = (double)VOTRAX_RATE/(double)dsprate;
	temp_votrax_buffer_size = (int)(VOTRAX_BLOCK_SIZE*ratio + 10); /* +10 .. little extra? */
	temp_votrax_buffer = Util_malloc(temp_votrax_buffer_size*sizeof(SWORD));
	votrax_buffer = Util_malloc(VOTRAX_BLOCK_SIZE*sizeof(SWORD));
}

/* process votrax and interpolate samples */
void votrax_process(SWORD *votrax_buffer, int len, SWORD *temp_votrax_buffer)
{
	static SWORD last_sample;
	static SWORD last_sample2;
	static double startpos;
	static int have;
	int max_left_sample_index = (int)(startpos + (double)(len - 1)*ratio);
	int pos = 0;
	double fraction = 0;
	int i;
	int floor_next_pos;

	if (have == 2) {
	    temp_votrax_buffer[0] = last_sample;
		temp_votrax_buffer[1] = last_sample2;
		Votrax_Update(0, temp_votrax_buffer + 2, (max_left_sample_index + 1 + 1) - 2);  
	}
	else if (have == 1) {
	    temp_votrax_buffer[0] = last_sample;
		Votrax_Update(0, temp_votrax_buffer + 1, (max_left_sample_index + 1 + 1) - 1);
	}
	else if (have == 0) {
		Votrax_Update(0, temp_votrax_buffer, max_left_sample_index + 1 + 1);
	}
	else if (have < 0) {
		Votrax_Update(0, temp_votrax_buffer, -have);
		Votrax_Update(0, temp_votrax_buffer, max_left_sample_index + 1 + 1);
	}

	for (i = 0; i < len; i++) {
		SWORD left_sample;
		SWORD right_sample;
		SWORD interp_sample;
		pos = (int)(startpos + (double)i*ratio);
		fraction = startpos + (double)i*ratio - (double)pos;
		left_sample = temp_votrax_buffer[pos];
		right_sample = temp_votrax_buffer[pos+1];
		interp_sample = (int)(left_sample + fraction*(double)(right_sample-left_sample));
		votrax_buffer[i] = interp_sample;
	}
	floor_next_pos = (int)(startpos + (double)len*ratio);
	startpos = (startpos + (double)len*ratio) - (double)floor_next_pos;
	if (floor_next_pos == max_left_sample_index)
	{ 
		have = 2;
		last_sample = temp_votrax_buffer[floor_next_pos];
		last_sample2 = temp_votrax_buffer[floor_next_pos+1];
	}
	else if (floor_next_pos == max_left_sample_index + 1) {
		have = 1;
		last_sample = temp_votrax_buffer[floor_next_pos];
	}
	else {
		have = (floor_next_pos - (max_left_sample_index + 2));
	}
}

/* 16 bit mixing */
static void mix(SWORD *dst, SWORD *src, int sndn, int volume)
{
	SWORD s1, s2;
	int val;
	int channel = 0;

	while (sndn--) {
		s1 = *src;
		s1 = s1*volume/128;
		s2 = *dst;
		src++;
		val = s1 + s2;
		if (val > 32767) val = 32767;
		if (val < -32768) val = -32768;
		*dst++ = val;
		if (stereo) {
			if (!channel) {
				channel = !channel;
				sndn++;
				src--;
			}
		}
	}
}

/* 8 bit mixing */
static void mix8(UBYTE *dst, SWORD *src, int sndn, int volume)
{
	SWORD s1, s2;
	int val;
	int channel = 0;

	while (sndn--) {
		s1 = *src;
		s1 = s1*volume/128;
		s2 = ((int)(*dst) - 0x80)*256;
		src++;
		val = s1 + s2;
		if (val > 32767) val = 32767;
		if (val < -32768) val = -32768;
		*dst++ = (UBYTE)((val/256) + 0x80);
		if (stereo) {
			if (!channel) {
				channel = !channel;
				sndn++;
				src--;
			}
		}
	}
}

void PBI_XLD_V_Frame(void)
{
	if (!xld_v_enabled) return;
	votrax_sync_samples -= samples_per_frame;
	if (votrax_sync_samples <= 0 ) {
		votrax_sync_samples = 0;
		votrax_busy = FALSE;
		votrax_busy_callback(FALSE); /* busy -> idle */
	}
}

void PBI_XLD_V_Process(void *sndbuffer, int sndn)
{
	if (!xld_v_enabled) return;

	if(votrax_written) {
		votrax_written = FALSE;
		votraxsc01_w(votrax_written_byte);
	}
	while (sndn > 0) {
		int amount = ((sndn > VOTRAX_BLOCK_SIZE) ? VOTRAX_BLOCK_SIZE : sndn);
		votrax_process(votrax_buffer, amount, temp_votrax_buffer);
		if (bit16) mix(sndbuffer, votrax_buffer, amount, 128/4);
		else mix8(sndbuffer, votrax_buffer, amount, 128/4);
		sndbuffer = (char *) sndbuffer + VOTRAX_BLOCK_SIZE*(bit16 ? 2 : 1)*(stereo ? 2: 1);
		sndn -= VOTRAX_BLOCK_SIZE;
	}
}


/*
vim:ts=4:sw=4:
*/
