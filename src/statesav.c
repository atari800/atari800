/* State saving module */

/* Include files */
#ifdef WIN32
#include <windows.h>
#include <io.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "atari.h"
#include "config.h"
#include "log.h"
#ifdef ZLIB_CAPABLE
#include "zlib.h"
#endif

#define SAVE_VERSION_NUMBER	3

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

extern void AnticStateSave( void );
extern void MainStateSave( void );
extern void CpuStateSave( UBYTE SaveVerbose );
extern void GTIAStateSave( void );
extern void PIAStateSave( void );
extern void POKEYStateSave( void );

extern void AnticStateRead( void );
extern void MainStateRead( void );
extern void CpuStateRead( UBYTE SaveVerbose );
extern void GTIAStateRead( void );
extern void PIAStateRead( void );
extern void POKEYStateRead( void );

extern int ReadDisabledROMs( void );
extern int mach_xlxe;

#ifdef ZLIB_CAPABLE
#ifdef WIN32
extern FARPROC	pgzread, pgzopen, pgzclose, pgzwrite, pgzerror;
#define GZOPEN( X, Y ) (gzFile)pgzopen( X, Y )
#define GZCLOSE( X ) (int)pgzclose( X )
#define GZREAD( X, Y, Z ) (int)pgzread( X, Y, Z )
#define GZWRITE( X, Y, Z ) (int)pgzwrite( X, Y, Z )
#define GZERROR( X, Y ) (const char *)pgzerror( X, Y )
#else
#define GZOPEN( X, Y )		gzopen( X, Y )
#define GZCLOSE( X )		gzclose( X )
#define GZREAD( X, Y, Z )	gzread( X, Y, Z )
#define GZWRITE( X, Y, Z )	gzwrite( X, Y, Z )
#define GZERROR( X, Y )		gzerror( X, Y )
#endif
#else	/* ZLIB_CAPABLE */
#define GZOPEN( X, Y )		fopen( X, Y )
#define GZCLOSE( X )		fclose( X )
#define GZREAD( X, Y, Z )	fread( Y, Z, 1, X )
#define GZWRITE( X, Y, Z )	fwrite( Y, Z, 1, X )
#undef GZERROR
#endif	/* ZLIB_CAPABLE */

#ifndef ZLIB_CAPABLE
#define gzFile	FILE
#define Z_OK	0
#endif

static gzFile	*StateFile = NULL;
static int		nFileError = Z_OK;

static void GetGZErrorText( void )
{
#ifdef ZLIB_CAPABLE
	const char *error = GZERROR( StateFile, &nFileError );
	if( nFileError == Z_ERRNO )
	{
		nFileError = errno;
		Aprint( "The following general file I/O error occured: " );
		Aprint( strerror( nFileError ) );
		return;
	}
	Aprint( "ZLIB returned the following error: %s", error);
#endif	/* ZLIB_CAPABLE */
 	Aprint( "State-save failed." );
}
	
/* Value is memory location of data, num is number of type to save */
void SaveUBYTE( UBYTE *data, int num )
{
	int	result;

	if( !StateFile || nFileError != Z_OK )
		return;

	/* Assumption is that UBYTE = 8bits and the pointer passed in refers
	   directly to the active bits if in a padded location. If not (unlikely)
	   you'll have to redefine this to save appropriately for cross-platform
	   compatibility */
	result = GZWRITE( StateFile, data, num );
	if( result == 0 )
	{
		GetGZErrorText();
	}
}

/* Value is memory location of data, num is number of type to save */
void ReadUBYTE( UBYTE *data, int num )
{
	int	result;

	if( !StateFile || nFileError != Z_OK )
		return;

	result = GZREAD( StateFile, data, num );
	if( result == 0 )
	{
		GetGZErrorText();
	}
}

/* Value is memory location of data, num is number of type to save */
void SaveUWORD( UWORD *data, int num )
{
	if( !StateFile || nFileError != Z_OK )
		return;

	/* UWORDS are saved as 16bits, regardless of the size on this particular
	   platform. Each byte of the UWORD will be pushed out individually in 
	   LSB order. The shifts here and in the read routines will work for both
	   LSB and MSB architectures. */
	while( num > 0 )
	{
		UWORD	temp;
		UBYTE	byte;
		int	result;

		temp = *data;
		byte = temp & 0xff;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		temp >>= 8;
		byte = temp & 0xff;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}
		num--;
		data++;
	}
}

/* Value is memory location of data, num is number of type to save */
void ReadUWORD( UWORD *data, int num )
{
	if( !StateFile || nFileError != Z_OK )
		return;

	while( num > 0 )
	{
		UBYTE	byte1, byte2;
		int	result;

		
		result = GZREAD( StateFile, &byte1, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		result = GZREAD( StateFile, &byte2, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}
		*data = (byte2 << 8) | byte1;

		num--;
		data++;
	}
}

void SaveINT( int *data, int num )
{
	/* INTs are always saved as 32bits (4 bytes) in the file. They can be any size
	   on the platform however. The sign bit is clobbered into the fourth byte saved
	   for each int; on read it will be extended out to its proper position for the
	   native INT size */
	while( num > 0 )
	{
		unsigned char signbit = 0;
		unsigned int	temp;
		UBYTE	byte;
		int result;
		int temp0;

		if( !StateFile || nFileError != Z_OK )
		  return;

		temp0 = *data;
		if (temp0 < 0)
		{
		  temp0 = -temp0;
		  signbit = 0x80;
		}
		temp = (unsigned int)temp0;

		byte = temp & 0xff;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;			
		} 

		temp >>= 8;
		byte = temp & 0xff;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		temp >>= 8;
		byte = temp & 0xff;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}
		temp >>= 8;

		byte = (temp & 0x7f) | signbit;
		result = GZWRITE( StateFile, &byte, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}
		num--;
		data++;
	}
}

void ReadINT( int *data, int num )
{
	unsigned char signbit = 0;

	if( !StateFile || nFileError != Z_OK )
		return;

	while( num > 0 )
	{
		int	temp;
		UBYTE	byte1, byte2, byte3, byte4;
		int result;

		result = GZREAD( StateFile, &byte1, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;			
		} 

		result = GZREAD( StateFile, &byte2, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		result = GZREAD( StateFile, &byte3, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		result = GZREAD( StateFile, &byte4, 1 );
		if( result == 0 )
		{
			GetGZErrorText();
			num = 0;
			continue;
		}

		signbit = byte4 & 0x80;
		byte4 &= 0x7f;

		temp = (byte4 << 24) | (byte3 << 16) | (byte2 << 8) | byte1;
		if( signbit )
			temp = -temp;
		*data = temp;

		num--;
		data++;
	}
}

int SaveAtariState( char *filename, const char *mode, UBYTE SaveVerbose )
{
	int result;

	if( StateFile )
		result = GZCLOSE( StateFile );
	StateFile = NULL;
	nFileError = Z_OK;

	StateFile = GZOPEN( filename, mode );
	if( !StateFile )
	{
		Aprint( "Could not open %s for state save." );
		GetGZErrorText();
		return FALSE;
	}
	result = GZWRITE( StateFile, "ATARI800", 8 );
	if( !result )
	{
		GetGZErrorText();
	}
	else
	{
		UBYTE	StateVersion = SAVE_VERSION_NUMBER;
		
		SaveUBYTE( &StateVersion, 1 );
		SaveUBYTE( &SaveVerbose, 1 );
		/* The order here is important. Main must be first because it saves the machine type, and
		   decisions on what to save/not save are made based off that later in the process */
		MainStateSave( );
		AnticStateSave( );
		CpuStateSave( SaveVerbose );
		GTIAStateSave( );
		PIAStateSave( );
		POKEYStateSave( );
	}

	result = GZCLOSE( StateFile );
	StateFile = NULL;

	if( nFileError != Z_OK )
		return FALSE;

	return TRUE;
}

int ReadAtariState( char *filename, const char *mode )
{
	int result, result1;
	char	header_string[9];
	UBYTE	StateVersion  = 0;		/* The version of the save file */
	UBYTE	SaveVerbose	 = 0;		/* Verbose mode means save basic, OS if patched */

	if( StateFile )
		result = GZCLOSE( StateFile );
	StateFile = NULL;
	nFileError = Z_OK;

	StateFile = GZOPEN( filename, mode );
	if( !StateFile )
	{
		Aprint( "Could not open %s for state read." );
		GetGZErrorText();
		return FALSE;
	}

	result = GZREAD( StateFile, header_string, 8 );
	header_string[8] = 0;
	if( strcmp( header_string, "ATARI800" ) )
	{
		Aprint( "This is not an Atari800 state save file." );
		result = GZCLOSE( StateFile );
		StateFile = NULL;
		return FALSE;
	}

	result = GZREAD( StateFile, &StateVersion, 1 );
	result1 = GZREAD( StateFile, &SaveVerbose, 1 );
	if( result == 0 || result1 == 0 )
	{
		GetGZErrorText();
		Aprint( "Failed read from Atari state file." );
		result = GZCLOSE( StateFile );
		StateFile = NULL;
		return FALSE;
	}

	if( StateVersion != SAVE_VERSION_NUMBER)
	{
		Aprint( "Cannot read this state file because it is an incompatible version." );
		result = GZCLOSE( StateFile );
		StateFile = NULL;
		return FALSE;
	}

	MainStateRead( );
	AnticStateRead( );
	CpuStateRead( SaveVerbose );
	GTIAStateRead( );
	PIAStateRead( );
	POKEYStateRead( );

	result = GZCLOSE( StateFile );
	StateFile = NULL;

	if( nFileError != Z_OK )
		return FALSE;

	if( !SaveVerbose && mach_xlxe )
	{
		/* ReadDisabledRoms is a port specific function that will read atari basic into 
		   atari_basic[] and the OS into atarixl_os for XL/XEs. It should return FALSE
		   for failure. This is for saved states that don't have these ROMs in the save
		   because they are not important (not patched or otherwise modified) */
		if( !ReadDisabledROMs )
			return FALSE;
	}

	return TRUE;
}
