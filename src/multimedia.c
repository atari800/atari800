/*
 * multimedia.c - high level interface for writing multimedia files
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include "multimedia.h"
#include "file_export.h"
#include "util.h"
#include "log.h"

#ifdef SOUND

/* sndoutput is just the file pointer for the current sound file */
static FILE *sndoutput = NULL;

#define DEFAULT_SOUND_FILENAME_FORMAT "atari%03d.wav"

static char sound_filename_format[FILENAME_MAX] = DEFAULT_SOUND_FILENAME_FORMAT;
static int sound_no_last = -1;
static int sound_no_max = 1000;

#endif

#ifdef AVI_VIDEO_RECORDING

/* avioutput is just the file pointer for the current video file */
static FILE *avioutput = NULL;

#define DEFAULT_VIDEO_FILENAME_FORMAT "atari%03d.avi"

static char video_filename_format[FILENAME_MAX] = DEFAULT_VIDEO_FILENAME_FORMAT;
static int video_no_last = -1;
static int video_no_max = 1000;

#endif /* AVI_VIDEO_RECORDING */


int Multimedia_Initialise(int *argc, char *argv[])
{

	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */

		if (0) {}
#ifdef SOUND
		else if (strcmp(argv[i], "-soundfilename") == 0) {
			if (i_a)
				sound_no_max = Util_filenamepattern(argv[++i], sound_filename_format, FILENAME_MAX, DEFAULT_SOUND_FILENAME_FORMAT);
			else a_m = TRUE;
		}
#endif
#ifdef AVI_VIDEO_RECORDING
		else if (strcmp(argv[i], "-videofilename") == 0) {
			if (i_a)
				video_no_max = Util_filenamepattern(argv[++i], video_filename_format, FILENAME_MAX, DEFAULT_VIDEO_FILENAME_FORMAT);
			else a_m = TRUE;
		}
#endif
		else {
			if (strcmp(argv[i], "-help") == 0) {
#ifdef SOUND
				Log_print("\t-soundfilename <p>   Set filename pattern for audio recording");
#endif /* !DREAMCAST */
#ifdef AVI_VIDEO_RECORDING
				Log_print("\t-videofilename <p>   Set filename pattern for video recording");
#endif /* !DREAMCAST */
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		}
	}
	*argc = j;

	return TRUE;
}


/* Multimedia_IsFileOpen simply returns true if any multimedia file is currently
   open and able to receive writes.

   Recording both video and audio at the same time is not allowed. This is not
   a common use case, and not worth the additional code and user interface changes
   necessary to support this.

   RETURNS: TRUE is file is open, FALSE if it is not */
int Multimedia_IsFileOpen(void)
{
	return FALSE
#ifdef SOUND
		|| sndoutput != NULL
#endif
#ifdef AVI_VIDEO_RECORDING
		|| avioutput != NULL
#endif
	;
}

/* Multimedia_CloseFile should be called when the program is exiting, or
   when all data required has been written to the file.
   Multimedia_CloseFile will also be called automatically when a call is
   made to Multimedia_OpenSoundFile/Multimedia_OpenVideoFile, or an error is made in
   Multimedia_WriteToSoundFile. Note that both types of media files have to update
   the file headers with information regarding the length of the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
int Multimedia_CloseFile(void)
{
	int bSuccess = TRUE;

#ifdef SOUND
	if (sndoutput != NULL) {
		bSuccess = WAV_CloseFile(sndoutput);
		sndoutput = NULL;
	}
#endif
#ifdef AVI_VIDEO_RECORDING
	if (avioutput != NULL) {
		bSuccess = AVI_CloseFile(avioutput);
		avioutput = NULL;
	}
#endif

	return bSuccess;
}

#ifdef SOUND
/* Get the next filename in the sound_file_format pattern.
   RETURNS: True if filename is available, false if no filenames left in the pattern. */
int Multimedia_GetNextSoundFile(char *buffer, int bufsize) {
	return Util_findnextfilename(sound_filename_format, &sound_no_last, sound_no_max, buffer, bufsize, FALSE);
}

/* Multimedia_OpenSoundFile will start a new sound file and write out the
   header. If an existing sound file is already open it will be closed first,
   and the new file opened in its place.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
int Multimedia_OpenSoundFile(const char *szFileName)
{
	Multimedia_CloseFile();

	sndoutput = WAV_OpenFile(szFileName);

	return sndoutput != NULL;
}

/* Multimedia_WriteAudio will dump PCM data to the WAV file. The best way to do
   this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size).

   If using video, a call to Multimedia_WriteAudio must be called before calling
   Multimedia_WriteAudio again, but the audio and video functions may be called
   in either order.

   RETURNS: Non-zero if no error; zero if error */
int Multimedia_WriteAudio(const unsigned char *ucBuffer, unsigned int uiSize)
{
	int result = 0;

	if (ucBuffer && uiSize) {
		if (sndoutput) {
			result = WAV_WriteSamples(ucBuffer, uiSize, sndoutput);
			if (result == 0) {
				Multimedia_CloseFile();
			}
		}
#ifdef AVI_VIDEO_RECORDING
		else if (avioutput) {
			result = AVI_AddAudioSamples(ucBuffer, uiSize, avioutput);
			if (result == 0) {
				Multimedia_CloseFile();
			}
		}
#endif
	}

	return result;
}
#endif /* SOUND */

#ifdef AVI_VIDEO_RECORDING
/* Get the next filename in the video_file_format pattern.
   RETURNS: True if filename is available, false if no filenames left in the pattern. */
int Multimedia_GetNextVideoFile(char *buffer, int bufsize) {
	return Util_findnextfilename(video_filename_format, &video_no_last, video_no_max, buffer, bufsize, FALSE);
}

/* Multimedia_OpenVideoFile will start a new video file and write out the
   header. If an existing video file is already open it will be closed first,
   and the new file opened in its place.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
int Multimedia_OpenVideoFile(const char *szFileName)
{
	Multimedia_CloseFile();

	avioutput = AVI_OpenFile(szFileName);

	return avioutput != NULL;
}

/* Multimedia_WriteVideo will dump the Atari screen to the AVI file. A call to
   Multimedia_WriteAudio must be called before calling Multimedia_WriteVideo
   again, but the audio and video functions may be called in either order.

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input uiSize parm) */
int Multimedia_WriteVideo()
{
	int result = 0;

	if (avioutput) {
		result = AVI_AddVideoFrame(avioutput);
		if (result == 0) {
			Multimedia_CloseFile();
		}
	}

	return result;
}

#endif /* AVI_VIDEO_RECORDING */
