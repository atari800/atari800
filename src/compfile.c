/* dcmtoatr based on code written by Chad Wagner (cmwagner@gate.net),
   version 1.4 (also by Preston Crow) */

/* Mostly this code has just been ludicrously armored to detect any I/O 
   errors by me; and removed the main loop (which just became the function
   dcmtoatr) since Atari800 might call this repeatedly for a set of
   files rich@kesmai.com */

/* This also became the natural place to put zlib conversion functions */

/* Include files */
#ifdef WIN32
#include <windows.h>
#include <io.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>	/* for close(), write() */
#ifdef __MSDOS__
#include <io.h>
#endif
#include "atari.h"
#include "config.h"
#include "log.h"
#ifdef ZLIB_CAPABLE
#include "zlib.h"
#endif

#ifndef MAX_PATH
#define MAX_PATH 128
#endif

extern int SIO_Mount(int diskno, const char *filename, int b_open_readonly );
extern void Set_Temp_File( int diskno );
extern void SIO_Dismount(int diskno);

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
static int write_atari16(FILE *,int);
static int read_offset(FILE *);
static int read_sector(FILE *);
static int write_sector(FILE *);
static long soffset(void);

/* Global variables */
static unsigned int	secsize;
static unsigned short	cursec ,maxsec;
static unsigned char	createdisk ,working ,last ,density, buf[256], atr;
static FILE *fin = NULL, *fout = NULL;


#ifdef WIN32
extern FARPROC	pgzread, pgzopen, pgzclose, pgzwrite, pgzerror;
#define GZOPEN( X, Y ) (gzFile)pgzopen( X, Y )
#define GZCLOSE( X ) (int)pgzclose( X )
#define GZREAD( X, Y, Z ) (int)pgzread( X, Y, Z )
#else
#define GZOPEN( X, Y ) gzopen( X, Y )
#define GZCLOSE( X ) gzclose( X )
#define GZREAD( X, Y, Z ) gzread( X, Y, Z )
#endif

/* This is a port-specific function that should return -1 if the port is unable to
   use zlib, any other value if it can. For instance, Windows might return -1 if the
   zlib DLL cannot be loaded. If you are not zlib capable you should macro out the
   gz???? functions to avoid linkage problems */
extern int zlib_capable( void );

/* prepend_tmpfile_path is a port-specific function that should insert into the supplied
   buffer pointer any path name the port wants before the filename returned by tmpnam().
   Note that tmpnam() typically returns a slash as the first character, so you should
   make sure prepend_tmpfile_path doesn't include a trailing slash of its own. This
   function should return the number of bytes that the prepended path represents */
extern int prepend_tmpfile_path( char *buffer );

/* This function was added because unlike DOS, the Windows version might visit this
   module many times, not a run-once occurence. Everything needs to be reset per file,
   so that is what init_globals does */
static void init_globals( FILE *input, FILE *output )
{
	secsize = 0;
	cursec = maxsec = 0;
	createdisk = working = last = density = 0;
	atr = 16;
	memset( buf, 0, 256 );
	fin = input;
	fout = output;
}

static void show_file_error( FILE *stream )
{
	if( feof( stream ) )
		Aprint( "Unexpected end of file during I/O operation, file is probably corrupt" );
	else
		Aprint( "I/O error reading or writing a character" );
}

/* Opens a ZLIB compressed (gzip) file, creates a temporary filename, and decompresses
   the contents of the .gz file to the temporary file name. Note that *outfilename is
   actually blank coming in and is filled in with whatever tmpnam returns */
int openzlib(int diskno, const char *infilename, char *outfilename )
{
#ifndef ZLIB_CAPABLE
	Aprint( "This executable cannot decompress ZLIB files" );
	return -1;
#else
	gzFile	gzSource;
	int	fd = -1, outfile = -1;
	char	*curptr = outfilename;
	char	*zlib_buffer = NULL;

	if( zlib_capable() == -1 )
	{
		Aprint( "This executable cannot decompress ZLIB files" );
		return -1;
	}

	zlib_buffer = malloc( ZLIB_BUFFER_SIZE + 1 );
	if( !zlib_buffer )
	{
		Aprint( "Could not obtain memory for zlib decompression" );
		return -1;
	}

	curptr += prepend_tmpfile_path( outfilename );
	if( tmpnam( curptr )== NULL )
	{
		Aprint( "Could not obtain temporary filename" );
		free( zlib_buffer );
		return -1;
	}

	outfile = open(outfilename, O_CREAT | O_WRONLY | O_BINARY, 0777);
	if( outfile == -1 )
	{
		Aprint( "Could not open temporary file" );
		free( zlib_buffer );
		return -1;
	}

	gzSource = GZOPEN( infilename, "rb" );
	if( !gzSource )
	{
		Aprint( "ZLIB could not open file %s", infilename );
		close( outfile );
	}
	else	/* Convert the gzip file to the temporary file */
	{
		int	result, temp;

		Aprint( "Converting %s to %s", infilename, outfilename );
		do
		{
			result = GZREAD( gzSource, &zlib_buffer[0], ZLIB_BUFFER_SIZE );
			if( result > 0 )
			{
				if( write( outfile, zlib_buffer, result ) != result )
				{
					Aprint( "Error writing to temporary file %s, disk may be full", outfilename );
					result = -1;
				}
			}
		} while( result == ZLIB_BUFFER_SIZE );
		temp = GZCLOSE( gzSource );
#ifdef WIN32
		_commit( outfile );
#endif
		close( outfile );
		if( result > -1 )
			fd = open(outfilename, O_RDONLY | O_BINARY, 0777);
		else
		{
			Aprint( "Error while parsing gzip file" );
			fd = -1;
		}
	}

	if( fd == -1 )
	{
		if( zlib_buffer )
			free( zlib_buffer );
		Aprint( "Removing temporary file %s", outfilename );
		remove( outfilename );
	}

	return fd;
#endif	/* ZLIB_CAPABLE */
}

int dcmtoatr(FILE *fin, FILE *fout, const char *input, char *output )
{
	int archivetype;	/* Block type for first block */
	int blocktype;		/* Current block type */
	int tmp;			/* Temporary for read without clobber on eof */

	init_globals( fin, fout );
	Aprint( "Converting %s to %s", input, output );
	if( !fin || !fout )
	{
		Aprint( "Programming error - NULL file specified for conversion" );
		return 0;
	}
	archivetype = blocktype = fgetc(fin);

	if( archivetype == EOF )
	{
		show_file_error( fin );
		return 0;
	}

	switch(blocktype) 
	{
		case 0xF9:
		case 0xFA:
			break;
		default:
			Aprint("0x%02X is not a known header block at start of input file",blocktype);
			return 0;
	}
	
	rewind(fin);
	
	while( 1 )
	{
		if (feof(fin)) 
		{
			fflush(stdout); /* Possible buffered I/O confusion fix */
			if ((!last) && (blocktype == 0x45) && (archivetype == 0xF9)) {
				Aprint("Multi-part archive error.");
				Aprint("To process these files, you must first combine the files into a single file.");
				Aprint("COPY /B file1.dcm+file2.dcm+file3.dcm newfile.dcm from the DOS prompt");
			}
			else 
			{
				Aprint("EOF before end block, input file likely corrupt");
			}
			return 0;
		}
		
		if (working) {
			if (soffset() != ftell(fout)) 
			{
				Aprint("Output desyncronized, possibly corrupt dcm file. fin=%lu fout=%lu != %lu cursec=%u secsize=%u", 
					ftell(fin),ftell(fout),soffset(),cursec,secsize);
				return 0;
			}
		}
		
		tmp = fgetc(fin); /* blocktype is needed on EOF error--don't corrupt it */
		if( tmp == EOF )
		{
			show_file_error( fin );
			return 0;
		}

		blocktype = tmp;
		switch(blocktype) 
		{
		      case 0xF9:
			  case 0xFA:
				  /* New block */
				  if( decode_FA() == 0 )
					  return 0;
				  break;
			  case 0x45:
				  /* End block */
				  working=0;
				  if (last)
					  return 1;	/* Normal exit */
				  break;
			  case 0x41:
			  case 0xC1:
				  if( decode_C1() == 0 )
					  return 0;
				  break;
			  case 0x43:
			  case 0xC3:
				  if( decode_C3() == 0 )
					  return 0;
				  break;
			  case 0x44:
			  case 0xC4:
				  if( decode_C4() == 0 )
					  return 0;
				  break;
			  case 0x46:
			  case 0xC6:
				  if( decode_C6() == 0 )
					  return 0;
				  break;
			  case 0x47:
			  case 0xC7:
				  if( decode_C7() == 0 )
					  return 0;
				  break;
			  default:
				  Aprint("0x%02X is not a known block type.  File may be corrupt.",blocktype);
				  return 0;
		} /* end case */
		
		if ((blocktype != 0x45) && (blocktype != 0xFA) && (blocktype != 0xF9)) 
		{
			if (!(blocktype & 0x80)) 
			{
				cursec=read_atari16(fin);
				if( fseek(fout, soffset(), SEEK_SET) != 0 )
				{
					Aprint( "Failed a seek in output file, cannot continue" );
					return 0;
				}
			} 
			else 
			{
				cursec++;
				if(cursec==4 && secsize!=128)
					fseek(fout,(secsize-128)*3,SEEK_CUR);
			}
		}
	} 	
	return 0; /* Should never be executed */
}

/* Opens a DCM file and decodes it to a temporary file, then returns the
   file handle for the temporary file and its name. */
int opendcm( int diskno, const char *infilename, char *outfilename )
{
	FILE	*infile, *outfile;
	int	fd = -1;
	char	*curptr = outfilename;

#ifdef WIN32
	curptr += GetTempPath( MAX_PATH, outfilename) - 1;
	if( (curptr == outfilename - 1) || *curptr != '\\' )
		curptr++;
#endif

	if( tmpnam( curptr )== NULL )
		return -1;
	outfile = fopen( outfilename, "wb" );
	if( !outfile )
		return -1;
	
	infile = fopen( infilename, "rb" );
	if( !infile )
	{
		fclose( outfile );
	}
	else
	{
		if( dcmtoatr( infile, outfile, infilename, outfilename ) != 0 )
		{
			fflush( outfile );
			fclose( outfile );
			fd = open(outfilename, O_RDONLY | O_BINARY, 0777);
		}
	}

	if( fd == -1 )
	{
		Aprint( "Removing temporary file %s", outfilename );
		remove( outfilename );
	}

	return fd;
}

static int decode_C1(void)
{
	int	secoff,tmpoff,c;

	tmpoff = fgetc(fin);
	c = fgetc(fin);
	if( tmpoff == EOF || c == EOF )
	{
		show_file_error( fin );
		return 0;
	}

	for (secoff=0; secoff<(int)secsize; secoff++) 
		buf[secoff]=c;

	c=tmpoff;
	for (secoff=0; secoff<tmpoff; secoff++) 
	{
		c--;
		buf[c]=fgetc(fin);
		if( feof(fin) )
		{
			show_file_error( fin );
			return 0;
		}
	}
	if( !write_sector(fout) )
		return 0;
	return 1;
}

static int decode_C3(void)
{
	int	secoff,tmpoff,c;
	
	secoff=0;
	do 
	{
		if (secoff)
			tmpoff = read_offset(fin);
		else
			tmpoff = fgetc(fin);

		if( tmpoff == EOF )
		{
			show_file_error( fin );
			return 0;
		}

		for (; secoff<tmpoff; secoff++) 
		{
			buf[secoff] = fgetc(fin);
			if( feof(fin) )
			{
				show_file_error( fin );
				return 0;
			}

		}
		if (secoff == (int)secsize)
			break;

		tmpoff = read_offset(fin);
		c = fgetc(fin);
		if( tmpoff == EOF || c == EOF )
		{
			show_file_error( fin );
			return 0;
		}

		for (; secoff<tmpoff; secoff++) 
		{
			buf[secoff] = c;
		}
	} while(secoff < (int)secsize);

	if( !write_sector(fout) )
		return 0;
	return 1;
}

static int decode_C4(void)
{
	int	secoff,tmpoff;

	tmpoff = read_offset(fin);
	if( tmpoff == EOF )
	{
		show_file_error( fin );
		return 0;
	}

	for (secoff=tmpoff; secoff<(int)secsize; secoff++) {
		buf[secoff]=fgetc(fin);
		if( feof(fin) )
		{
			show_file_error( fin );
			return 0;
		}
	}
	if( !write_sector(fout) )
		return 0;
	return 1;
}

static int decode_C6(void)
{
	if( !write_sector(fout) )
		return 0;
	return 1;
}

static int decode_C7(void)
{
	if( !read_sector(fin) )
		return 0;
	if( !write_sector(fout) )
		return 0;
	
	return 1;
}

static int decode_FA(void)
{
	unsigned char c;

	if (working) 
	{
		Aprint("Trying to start section but last section never had an end section block.");
		return 0;
	}
	c=fgetc(fin);
	if( feof(fin) )
	{
		show_file_error( fin );
		return 0;
	}
	density=((c & 0x70) >> 4);
	last=((c & 0x80) >> 7);
	switch(density) 
	{
		case 0:
			maxsec=720;
			secsize=128;
			break;
		case 2:
			maxsec=720;
			secsize=256;
			break;
		case 4:
			maxsec=1040;
			secsize=128;
			break;
		default:
			Aprint( "Density type is unknown, density type=%u",density);
			return 0;
	}

	if (createdisk == 0) {
		createdisk = 1;
		/* write out atr header */
		/* special code, 0x0296 */
		if( write_atari16(fout,0x296) == 0 )
			return 0;
		/* image size (low) */
		if( write_atari16(fout,(short)(((long)maxsec * secsize) >> 4)) == 0 )
			return 0;
		/* sector size */
		if( write_atari16(fout,secsize) == 0 )
			return 0;
		/* image size (high) */
		if( write_atari16(fout,(short)(((long)maxsec * secsize) >> 20)) == 0 )
			return 0;
		/* 8 bytes unused */
		if( write_atari16(fout,0) == 0 )
			return 0;
		if( write_atari16(fout,0) == 0 )
			return 0;
		if( write_atari16(fout,0) == 0 )
			return 0;
		if( write_atari16(fout,0) == 0 )
			return 0;
		memset(buf,0,256);
		for (cursec=0; cursec<maxsec; cursec++) {
			if( fwrite(buf,secsize,1,fout) != 1 )
			{
				Aprint( "Error writing to output file" );
				return 0;
			}
		}
	}
	cursec=read_atari16(fin);
	if( fseek( fout, soffset(), SEEK_SET) != 0 )
	{
		Aprint( "Failed a seek in output file, cannot continue" );
		return 0;
	}
	working=1;
	return 1;
}

/*
** read_atari16()
** Read a 16-bit integer with Atari byte-ordering.
** 1  Jun 95  crow@cs.dartmouth.edu (Preston Crow)
*/
static int read_atari16(FILE *fin)
{
	int ch_low,ch_high; /* fgetc() is type int, not char */

	ch_low = fgetc(fin);
	ch_high = fgetc(fin);
	if( ch_low == EOF || ch_high == EOF )
	{
		show_file_error( fin );
		return 0;
	}
	return(ch_low + 256*ch_high);
}

static int write_atari16(FILE *fout,int n)
{
	unsigned char ch_low,ch_high;

	ch_low = (unsigned char)(n&0xff);
	ch_high = (unsigned char)(n/256);
	if( fputc(ch_low,fout) == EOF || fputc(ch_high,fout) == EOF )
	{
		show_file_error( fout );
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
	if( ch == EOF )
	{
		show_file_error( fin );
		return EOF;
	}
	if (ch == 0)
		ch = 256;

	return(ch);
}

/*
** read_sector()
** Simple routine that reads in a sector, based on it's location, and the
**  sector size.  Sectors 1-3, are 128 bytes, all other sectors are secsize.
** 5  Jun 95  cmwagner@gate.net (Chad Wagner)
*/
static int read_sector(FILE *fin)
{
	if( fread(buf,(cursec < 4 ? 128 : secsize),1,fin) != 1 )
	{
		Aprint( "A sector read operation failed from the source file" );
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
	if( fwrite(buf,(cursec < 4 ? 128 : secsize),1,fout) != 1 )
	{
		Aprint( "A sector write operation failed to the destination file" );
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
	return (long)atr + (cursec < 4 ? ((long)cursec - 1) * 128 :
			 ((long)cursec - 1) * secsize);
}
