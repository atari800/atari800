#include "sndsave.h"

#define WAVE_CHUNK_HEADER	"RIFF\000\000\000\000WAVEfmt\040\020\000\000\000\001\000\001\000"
#define WAVE_CHUNK_HEADER_LENGTH	24
#define DATA_CHUNK_HEADER	"\001\000\010\000data\000\000\000\000"
#define DATA_CHUNK_HEADER_LENGTH	8

/* sndoutput is just the file pointer for the current sound file */
static FILE *sndoutput = NULL;

/* iSoundRate is the cycle rate in hertz of the WAV being recorded. In
   Atari800 this is equivalent to the value passed into Pokey_sound_init
   as the second parameter (i.e. Pokey_sound_init( XXX, iSoundRate, YYY) */
int	iSoundRate = 0;


/* IsSoundFileOpen simply returns true if the sound file is currently open and able to receive writes
   RETURNS: TRUE is file is open, FALSE if it is not */
BOOL IsSoundFileOpen( void )
{
	return( sndoutput != 0 );
}


/* CloseSoundFile should be called when the program is exiting, or when all data required has been
   written to the file. CloseSoundFile will also be called automatically when a call is made to
   OpenSoundFile, or an error is made in WriteToSoundFile. Note that CloseSoundFile has to back track
   to the header written out in OpenSoundFile and update it with the length of samples written 
  
   RETURNS: TRUE if file closed with no problems, FALSE if failure during close */

BOOL CloseSoundFile( void )
{
	BOOL	bSuccess = TRUE;

	if( sndoutput )
	{
		int	iPos = 0;

		/* Sound file is finished, so modify header and close it. */
		if( fseek( sndoutput, 0, SEEK_END ) != 0 )
			bSuccess = FALSE;

		iPos = ftell( sndoutput );

		/* iPos must be greater than the data headers plus one data element for this file to make sense */
		if( iPos > ( WAVE_CHUNK_HEADER_LENGTH + DATA_CHUNK_HEADER_LENGTH + 1 ) )
		{

			if( fseek( sndoutput, 4, SEEK_SET )!= 0 )	/* Seek past RIFF */
				bSuccess = FALSE;

			iPos -= 8;
			if( fwrite( &iPos, 1, 4, sndoutput ) != 4 ) /* Write out file size - 8 */
				bSuccess = FALSE;

			if( fseek( sndoutput, 40, SEEK_SET ) != 0)
				bSuccess = FALSE;

			iPos -= 36;						
			if( fwrite( &iPos, 1, 4, sndoutput ) != 4)	/* Write out size of just sample data */
				bSuccess = FALSE;
		}
		else
		{
			bSuccess = FALSE;
		}
		
		fclose( sndoutput );
	}
	sndoutput = NULL;

	return bSuccess;
}


/* OpenSoundFile will start a new sound file and write out the header. If an existing sound file is
   already open it will be closed first, and the new file opened in it's place 

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open */

BOOL OpenSoundFile( const char *szFileName )
{
	if( sndoutput )
	{
		CloseSoundFile();
	}

	sndoutput = fopen( szFileName, "wb" );

	if( sndoutput == NULL )
	{
		return FALSE;
	}
	/*
	The RIFF header: 

	  Offset  Length   Contents
	  0       4 bytes  'RIFF'
	  4       4 bytes  <file length - 8>
	  8       4 bytes  'WAVE'
	
	The fmt chunk: 

	  12      4 bytes  'fmt '
	  16      4 bytes  0x00000010     // Length of the fmt data (16 bytes)
	  20      2 bytes  0x0001         // Format tag: 1 = PCM
	  22      2 bytes  <channels>     // Channels: 1 = mono, 2 = stereo
	  24      4 bytes  <sample rate>  // Samples per second: e.g., 44100
	  28      4 bytes  <bytes/second> // sample rate * block align
	  32      2 bytes  <block align>  // channels * bits/sample / 8
	  34      2 bytes  <bits/sample>  // 8 or 16

	The data chunk: 

	  36      4 bytes  'data'
	  40      4 bytes  <length of the data block>
	  44        bytes  <sample data>
	*/

	if( fwrite( WAVE_CHUNK_HEADER, 1, WAVE_CHUNK_HEADER_LENGTH, sndoutput ) != WAVE_CHUNK_HEADER_LENGTH )
	{
		fclose( sndoutput );
		return FALSE;
	}

	if( fwrite( &iSoundRate, 1, 4, sndoutput ) != 4 )
	{
		fclose( sndoutput );
		return FALSE;
	}

	if( fwrite( &iSoundRate, 1, 4, sndoutput ) != 4 )
	{
		fclose( sndoutput );
		return FALSE;
	}

	if( fwrite( DATA_CHUNK_HEADER, 1, DATA_CHUNK_HEADER_LENGTH, sndoutput ) != DATA_CHUNK_HEADER_LENGTH )
	{
		fclose( sndoutput );
		return FALSE;
	}

	return TRUE;
}

/* WriteToSoundFile will dump PCM data to the WAV file. The best way to do this for Atari800 is
   probably to call it directly after Pokey_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the input uiSize parm) */
   
int WriteToSoundFile( const unsigned char *ucBuffer, const unsigned int uiSize )
{
	unsigned int uiWriteLength = 0;

	if( sndoutput && ucBuffer && uiSize )
	{
		uiWriteLength = fwrite( ucBuffer, 1, uiSize, sndoutput );
	}

	return( uiWriteLength );
}
