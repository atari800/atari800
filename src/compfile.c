/*
 * compfile.c - File I/O and ZLIB compression
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
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_FCNTL_H
/* probably not needed here anymore */
#include <fcntl.h>
#endif

#include "atari.h"
#include "log.h"

/* Size of memory buffer ZLIB should use when decompressing files */
#define ZLIB_BUFFER_SIZE	32767

/* prototypes */
static int decode_C1(void);
static int decode_C3(void);
static int decode_C4(void);
static int decode_C6(void);
static int decode_C7(void);
static int decode_FA(void);
static int read_atari16(FILE *);
static int write_atari16(FILE *, int);
static int read_offset(FILE *);
static int read_sector(FILE *);
static int write_sector(FILE *);
static long soffset(void);

/* Global variables */
static unsigned int	secsize;
static unsigned short cursec, maxsec;
static unsigned char createdisk, working, last, density, buf[256], atr;
static FILE *fin = NULL, *fout = NULL;

/* This function was added because unlike DOS, the Windows version might visit this
   module many times, not a run-once occurence. Everything needs to be reset per file,
   so that is what init_globals does */
static void init_globals(FILE *input, FILE *output)
{
	secsize = 0;
	cursec = maxsec = 0;
	createdisk = working = last = density = 0;
	atr = 16;
	memset(buf, 0, 256);
	fin = input;
	fout = output;
}

static void show_file_error(FILE *stream)
{
	if (feof(stream))
		Aprint("Unexpected end of file during I/O operation, file is probably corrupt");
	else
		Aprint("I/O error reading or writing a character");
}

/* Opens a ZLIB compressed (gzip) file, creates a temporary filename, and decompresses
   the contents of the .gz file to the temporary file name. Note that *outfilename is
   actually blank coming in and is filled by Atari_tmpfile */
FILE *openzlib(int diskno, const char *infilename, char *outfilename)
{
#ifndef HAVE_LIBZ
	Aprint("This executable cannot decompress ZLIB files");
	return NULL;
#else
	gzFile gzSource;
	FILE *file = NULL, *outfile = NULL;
	char *zlib_buffer = NULL;

	zlib_buffer = malloc(ZLIB_BUFFER_SIZE + 1);
	if (!zlib_buffer) {
		Aprint("Could not obtain memory for zlib decompression");
		return NULL;
	}

	outfile = Atari_tmpfile(outfilename, "wb");
	if (outfile == NULL) {
		Aprint("Could not open temporary file");
		free(zlib_buffer);
		return NULL;
	}

	gzSource = gzopen(infilename, "rb");
	if (!gzSource) {
		Aprint("ZLIB could not open file %s", infilename);
		fclose(outfile);
	}
	else {
		/* Convert the gzip file to the temporary file */
		int	result, temp;

		Aprint("Converting %s to %s", infilename, outfilename);
		do {
			result = gzread(gzSource, zlib_buffer, ZLIB_BUFFER_SIZE);
			if (result > 0) {
				if ((int) fwrite(zlib_buffer, 1, result, outfile) != result) {
					Aprint("Error writing to temporary file %s, disk may be full", outfilename);
					result = -1;
				}
			}
		} while (result == ZLIB_BUFFER_SIZE);
		temp = gzclose(gzSource);
		fclose(outfile);
		if (result >= 0)
			file = fopen(outfilename, "rb");
		else {
			Aprint("Error while parsing gzip file");
			file = NULL;
		}
	}

	if (!file) {
		free(zlib_buffer);
		Aprint("Removing temporary file %s", outfilename);
		remove(outfilename);
	}

	return file;
#endif	/* HAVE_LIBZ */
}

int dcmtoatr(FILE *fin, FILE *fout, const char *input, char *output)
{
	int archivetype;	/* Block type for first block */
	int blocktype;		/* Current block type */
	int tmp;			/* Temporary for read without clobber on eof */

	init_globals(fin, fout);
	Aprint("Converting %s to %s", input, output);
	archivetype = blocktype = fgetc(fin);

	if (archivetype == EOF) {
		show_file_error(fin);
		return 0;
	}

	switch(blocktype) {
	case 0xF9:
	case 0xFA:
		break;
	default:
		Aprint("0x%02X is not a known header block at start of input file", blocktype);
		return 0;
	}

#ifdef HAVE_REWIND
	rewind(fin);
#else
	fseek(fin, 0, SEEK_SET);
#endif

	for (;;) {
		if (feof(fin)) {
			if ((!last) && (blocktype == 0x45) && (archivetype == 0xF9)) {
				Aprint("Multi-part archive error.");
				Aprint("To process these files, you must first combine the files into a single file.");
#if defined(WIN32) || defined(DJGPP)
				Aprint("COPY /B file1.dcm+file2.dcm+file3.dcm newfile.dcm from the DOS prompt");
#elif defined(linux) || defined(unix)
				Aprint("cat file1.dcm file2.dcm file3.dcm >newfile.dcm from the shell");
#endif
			}
			else {
				Aprint("EOF before end block, input file likely corrupt");
			}
			return 0;
		}
		
		if (working && soffset() != ftell(fout)) {
			Aprint("Output desynchronized, possibly corrupt dcm file. fin=%lu fout=%lu != %lu cursec=%u secsize=%u", 
				ftell(fin), ftell(fout), soffset(), cursec, secsize);
			return 0;
		}
		
		tmp = fgetc(fin); /* blocktype is needed on EOF error--don't corrupt it */
		if (tmp == EOF) {
			show_file_error(fin);
			return 0;
		}

		blocktype = tmp;
		switch (blocktype) {
		case 0xF9:
		case 0xFA:
			/* New block */
			if (decode_FA() == 0)
				return 0;
			break;
		case 0x45:
			/* End block */
			working = 0;
			if (last)
				return 1;	/* Normal exit */
			break;
		case 0x41:
		case 0xC1:
			if (decode_C1() == 0)
				return 0;
			break;
		case 0x43:
		case 0xC3:
			if (decode_C3() == 0)
				return 0;
			break;
		case 0x44:
		case 0xC4:
			if (decode_C4() == 0)
				return 0;
			break;
		case 0x46:
		case 0xC6:
			if (decode_C6() == 0)
				return 0;
			break;
		case 0x47:
		case 0xC7:
			if (decode_C7() == 0)
				return 0;
			break;
		default:
			Aprint("0x%02X is not a known block type.  File may be corrupt.", blocktype);
			return 0;
		} /* end case */

		if ((blocktype != 0x45) && (blocktype != 0xFA) && (blocktype != 0xF9)) {
			if (!(blocktype & 0x80)) {
				cursec = read_atari16(fin);
				if (fseek(fout, soffset(), SEEK_SET) != 0) {
					Aprint("Failed a seek in output file, cannot continue" );
					return 0;
				}
			} 
			else {
				cursec++;
				if (cursec == 4 && secsize != 128)
					fseek(fout, (secsize - 128) * 3 ,SEEK_CUR);
			}
		}
	}
	return 0; /* Should never be executed */
}

/* Opens a DCM file and decodes it to a temporary file, then returns the
   file handle for the temporary file and its name. */
FILE *opendcm(int diskno, const char *infilename, char *outfilename)
{
	FILE *infile, *outfile;
	FILE *file = NULL;

	outfile = Atari_tmpfile(outfilename, "wb");
	if (outfile == NULL) {
		Aprint("Cannot create temporary file\n");
		return NULL;
	}
	infile = fopen(infilename, "rb");
	if (infile == NULL) {
		fclose(outfile);
	}
	else if (dcmtoatr(infile, outfile, infilename, outfilename) != 0) {
		fclose(outfile);
		file = fopen(outfilename, "rb");
	}

	if (file == NULL) {
		Aprint("Removing temporary file %s", outfilename);
		remove(outfilename);
	}

	return file;
}

static int decode_C1(void)
{
	int secoff, tmpoff, c;

	tmpoff = fgetc(fin);
	if (tmpoff == EOF) {
		show_file_error(fin);
		return 0;
	}

	c = tmpoff;
	for (secoff = 0; secoff <= tmpoff; secoff++) {
		buf[c] = fgetc(fin);
		c--;
		if (feof(fin)) {
			show_file_error(fin);
			return 0;
		}
	}
	if (!write_sector(fout))
		return 0;
	return 1;
}

static int decode_C3(void)
{
	int secoff, tmpoff, c;

	secoff = 0;
	do {
		if (secoff)
			tmpoff = read_offset(fin);
		else
			tmpoff = fgetc(fin);

		if (tmpoff == EOF) {
			show_file_error(fin);
			return 0;
		}

		for (; secoff < tmpoff; secoff++) {
			buf[secoff] = fgetc(fin);
			if (feof(fin)) {
				show_file_error(fin);
				return 0;
			}
		}
		if (secoff == (int) secsize)
			break;

		tmpoff = read_offset(fin);
		c = fgetc(fin);
		if (tmpoff == EOF || c == EOF) {
			show_file_error(fin);
			return 0;
		}

		for (; secoff < tmpoff; secoff++) 
			buf[secoff] = c;
	} while (secoff < (int) secsize);

	if (!write_sector(fout))
		return 0;
	return 1;
}

static int decode_C4(void)
{
	int secoff,tmpoff;

	tmpoff = read_offset(fin);
	if (tmpoff == EOF) {
		show_file_error( fin );
		return 0;
	}

	for (secoff = tmpoff; secoff < (int) secsize; secoff++) {
		buf[secoff] = fgetc(fin);
		if (feof(fin)) {
			show_file_error( fin );
			return 0;
		}
	}
	if (!write_sector(fout))
		return 0;
	return 1;
}

static int decode_C6(void)
{
	if (!write_sector(fout))
		return 0;
	return 1;
}

static int decode_C7(void)
{
	if (!read_sector(fin))
		return 0;
	if (!write_sector(fout))
		return 0;

	return 1;
}

static int decode_FA(void)
{
	unsigned char c;

	if (working) {
		Aprint("Trying to start section but last section never had an end section block.");
		return 0;
	}
	c = fgetc(fin);
	if (feof(fin)) {
		show_file_error(fin);
		return 0;
	}
	density = ((c & 0x70) >> 4);
	last = ((c & 0x80) >> 7);
	switch(density) {
	case 0:
		maxsec = 720;
		secsize = 128;
		break;
	case 2:
		maxsec = 720;
		secsize = 256;
		break;
	case 4:
		maxsec = 1040;
		secsize = 128;
		break;
	default:
		Aprint("Density type is unknown, density type=%u", density);
		return 0;
	}

	if (createdisk == 0) {
		createdisk = 1;
		/* write out atr header */
		/* special code, 0x0296 */
		if (write_atari16(fout, 0x296) == 0)
			return 0;
		/* image size (low) */
		if (write_atari16(fout, (short) (((long) maxsec * secsize) >> 4)) == 0)
			return 0;
		/* sector size */
		if (write_atari16(fout, secsize) == 0)
			return 0;
		/* image size (high) */
		if (write_atari16(fout, (short) (((long) maxsec * secsize) >> 20)) == 0)
			return 0;
		/* 8 bytes unused */
		if (write_atari16(fout, 0) == 0)
			return 0;
		if (write_atari16(fout, 0) == 0)
			return 0;
		if (write_atari16(fout, 0) == 0)
			return 0;
		if (write_atari16(fout, 0) == 0)
			return 0;
		memset(buf, 0, 256);
		for (cursec = 0; cursec < maxsec; cursec++) {
			if (fwrite(buf, secsize, 1, fout) != 1) {
				Aprint("Error writing to output file");
				return 0;
			}
		}
	}
	cursec = read_atari16(fin);
	if (fseek(fout, soffset(), SEEK_SET) != 0) {
		Aprint("Failed a seek in output file, cannot continue");
		return 0;
	}
	working = 1;
	return 1;
}

/*
** read_atari16()
** Read a 16-bit integer with Atari byte-ordering.
** 1  Jun 95  crow@cs.dartmouth.edu (Preston Crow)
*/
static int read_atari16(FILE *fin)
{
	int ch_low, ch_high; /* fgetc() is type int, not char */

	ch_low = fgetc(fin);
	ch_high = fgetc(fin);
	if (ch_low == EOF || ch_high == EOF) {
		show_file_error(fin);
		return 0;
	}
	return (ch_low + 256 * ch_high);
}

static int write_atari16(FILE *fout, int n)
{
	UBYTE ch_low, ch_high;

	ch_low = (UBYTE) n;
	ch_high = (UBYTE) (n >> 8);
	if (fputc(ch_low, fout) == EOF || fputc(ch_high, fout) == EOF) {
		show_file_error(fout);
		return 0;
	}
	return 1;
}

/*
** read_offset()
** Simple routine that 'reads' the offset from an RLE encoded block, if the
**   offset is 0, then it returns it as 256.
** 5  Jun 95  cmwagner@gate.net (Chad Wagner)
*/
static int read_offset(FILE *fin)
{
	int ch; /* fgetc() is type int, not char */

	ch = fgetc(fin);
	if (ch == EOF) {
		show_file_error(fin);
		return EOF;
	}
	if (ch == 0)
		ch = 256;

	return ch;
}

/*
** read_sector()
** Simple routine that reads in a sector, based on it's location, and the
**  sector size.  Sectors 1-3, are 128 bytes, all other sectors are secsize.
** 5  Jun 95  cmwagner@gate.net (Chad Wagner)
*/
static int read_sector(FILE *fin)
{
	if (fread(buf, (cursec < 4 ? 128 : secsize), 1, fin) != 1) {
		Aprint("A sector read operation failed from the source file");
		return 0;
	}
	return 1;
}

/*
** write_sector()
** Simple routine that writes in a sector, based on it's location, and the
**  sector size.  Sectors 1-3, are 128 bytes, all other sectors are secsize.
** 5  Jun 95  cmwagner@gate.net (Chad Wagner)
*/
static int write_sector(FILE *fout)
{
	if (fwrite(buf, (cursec < 4 ? 128 : secsize), 1, fout) != 1) {
		Aprint("A sector write operation failed to the destination file");
		return 0;
	}
	return 1;
}

/*
** soffset()
** calculates offsets within ATR or XFD images, for seeking.
** 28 Sep 95  lovegrov@student.umass.edu (Mike White)
*/
static long soffset()
{
	/* XXX: does this work correctly for 256-byte sectors? */
	return (long) atr + (cursec < 4 ? ((long) cursec - 1) * 128 :
			 ((long) cursec - 1) * (long) secsize);
}

/*
$Log$
Revision 1.20  2005/08/21 15:40:13  pfusik
Atari_tmpfile(); #ifdef HAVE_REWIND; removed a redundant assertion

Revision 1.19  2005/08/17 22:30:49  pfusik
#include <io.h> on WIN32

Revision 1.18  2005/08/16 23:08:08  pfusik
#include "config.h" before system headers;
dirty workaround for lack of mktemp()

Revision 1.17  2005/08/15 17:17:06  pfusik
fixed indentation

Revision 1.16  2005/08/07 13:44:43  pfusik
fixed indenting; other minor improvements

Revision 1.15  2005/03/08 04:32:41  pfusik
killed gcc -W warnings

Revision 1.14  2003/12/17 07:01:18  markgrebe
Fixed serious bug in Type 41 decoding

Revision 1.13  2003/09/14 20:07:28  joy
O_BINARY defined

Revision 1.12  2003/09/14 19:30:31  joy
mkstemp emulated if unavailable

Revision 1.11  2003/06/15 07:32:35  joy
GZOPEN for non HAVE_LIBZ platforms

Revision 1.10  2003/03/03 09:57:32  joy
Ed improved autoconf again plus fixed some little things

Revision 1.9  2003/02/24 09:32:50  joy
header cleanup

Revision 1.8  2001/12/04 13:05:16  joy
include zlib.h from the system include path and not from local directory (suggested by Nathan)

Revision 1.7  2001/09/08 07:49:33  knik
unused definitions and inclusions removed

Revision 1.6  2001/07/25 13:03:35  fox
removed unused declarations

Revision 1.5  2001/04/15 09:14:33  knik
zlib_capable -> have_libz (autoconf compatibility)

Revision 1.4  2001/03/25 06:57:35  knik
open() replaced by fopen()

Revision 1.3  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
