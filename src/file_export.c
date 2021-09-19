/*
 * file_export.c - low level interface for saving to various file formats
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
#include "file_export.h"
#include "codecs/video_mrle.h"
#include "screen.h"
#include "colours.h"
#include "cfg.h"
#include "util.h"
#include "log.h"
#ifdef SOUND
#include "sound.h"
#include "pokeysnd.h"
#include "codecs/audio.h"
#endif
#ifdef SUPPORTS_CHANGE_VIDEOMODE
#include "videomode.h"
#endif

#ifdef AVI_VIDEO_RECORDING
#include "codecs/image.h"
#include "codecs/video.h"
#endif

#if defined(SOUND) || defined(AVI_VIDEO_RECORDING)
/* RIFF files (WAV, AVI) are limited to 4GB in size, so define a reasonable max
   that's lower than 4GB */
#define MAX_RECORDING_SIZE (0xfff00000)

/* number of bytes written to the currently open multimedia file */
static ULONG byteswritten;

/* These variables are needed for statistics and on-screen information display. */
static ULONG video_frame_count;
static float fps;
static char description[16];
#endif

#if !defined(BASIC) && !defined(CURSES_BASIC)
#include "codecs/image.h"
#endif

#ifdef AVI_VIDEO_RECORDING

/* AVI requires the header at the beginning of the file contains sizes of each
   chunk, so the header will be rewritten upon the close of the file to update
   the header values with the final totals. *;
*/
static ULONG size_riff;
static ULONG size_movi;

/* AVI files using the version 1.0 indexes ('idx1') have a 32 bit limit, which
   limits file size to 4GB. Some media players may fail to play videos greater
   than 2GB because of their incorrect use of signed rather than unsigned 32 bit
   values.

   The maximum recording duration depends mostly on the complexity of the video.
   The audio is saved as raw PCM samples which doesn't vary per frame. On NTSC
   at 60Hz using 16 bit samples, this will be just under 1500 bytes per frame.

   The size of each encoded video frame depends on the complexity the screen
   image. The RLE compression is based on scan lines, and performs best when
   neighboring pixels on the scan line are the same color. Due to overhead in
   the compression sceme itself, the best it can do is about 1500 bytes on a
   completely black screen. Complex screens where many neighboring pixels have
   different colors result in video frames of around 30k. This is still a
   significant savings over an uncompressed frame which would be 80k.

   For complex scenes, therefore, this results in about 8 minutes of video
   recording per GB, or about 36 minutes when using the full 4GB file size. Less
   complex video will provide more recording time. For example, recording the
   unchanging BASIC prompt screen would result in about 6 hours of video.

   The video will automatically be stopped should the recording length approach
   the file size limit. */
static ULONG total_video_size;
static ULONG smallest_video_frame;
static ULONG largest_video_frame;

#define FRAME_INDEX_ALLOC_SIZE 1000
static int num_frames_allocated;
static ULONG frames_written;
static ULONG *frame_indexes;
#define FRAME_SIZE_MASK  0x1fffffff
#define VIDEO_FRAME_FLAG 0x20000000
#define AUDIO_FRAME_FLAG 0x40000000
#define KEYFRAME_FLAG    0x80000000


/* Some codecs allow for keyframes (full frame compression) and inter-frames
   (only the differences from the previous frame) */
static int keyframe_count;

#ifdef SOUND
static ULONG samples_written;

#endif

static int num_streams;

#endif /* AVI_VIDEO_RECORDING */

#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
int FILE_EXPORT_compression_level = 6;
#endif


int File_Export_Initialise(int *argc, char *argv[])
{
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (0) {}
		else if (strcmp(argv[i], "-compression-level") == 0) {
			if (i_a) {
				FILE_EXPORT_compression_level = Util_sscandec(argv[++i]);
				if (FILE_EXPORT_compression_level < 0 || FILE_EXPORT_compression_level > 9) {
					Log_print("Invalid png/zlib compression level - must be between 0 and 9");
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-compression-level <n>");
				Log_print("\t                 Set zlib/PNG compression level 0-9 (default 6)");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
	}
	*argc = j;
#endif

	return TRUE
#ifdef AVI_VIDEO_RECORDING
		&& CODECS_VIDEO_Initialise(argc, argv)
#endif
#ifdef SOUND
		&& CODECS_AUDIO_Initialise(argc, argv)
#endif
	;
}

int File_Export_ReadConfig(char *string, char *ptr)
{
	if (0) {}
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
	else if (strcmp(string, "COMPRESSION_LEVEL") == 0) {
		int num = Util_sscandec(ptr);
		if (num >= 0 && num <= 9)
			FILE_EXPORT_compression_level = num;
		else return FALSE;
	}
#endif
#ifdef AVI_VIDEO_RECORDING
	else if (CODECS_VIDEO_ReadConfig(string, ptr)) {
	}
#endif
#ifdef SOUND
	else if (CODECS_AUDIO_ReadConfig(string, ptr)) {
	}
#endif
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void File_Export_WriteConfig(FILE *fp)
{
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
	fprintf(fp, "COMPRESSION_LEVEL=%d\n", FILE_EXPORT_compression_level);
#endif
#ifdef AVI_VIDEO_RECORDING
	CODECS_VIDEO_WriteConfig(fp);
#endif
#ifdef SOUND
	CODECS_AUDIO_WriteConfig(fp);
#endif
}

#if defined(SOUND) || defined(AVI_VIDEO_RECORDING)
/* File_Export_ElapsedTime returns the current duration of the multimedia file.
   */
int File_Export_ElapsedTime(void)
{
	return (int)(video_frame_count / fps);
}

/* File_Export_CurrentSize returns the current size of the multimedia file in
   bytes. This should be considered approximate and not used in calculations
   related to the actual position in the written file.
   */
int File_Export_CurrentSize(void)
{
	return byteswritten;
}

/* File_Export_CurrentSize returns the current size of the multimedia file in
   bytes. This should be considered approximate and not used in calculations
   related to the actual position in the written file.
   */
char *File_Export_Description(void)
{
	return description;
}
#endif

/* fputw, fputl, and fwritele are utility functions to write values as
   little-endian format regardless of the endianness of the platform. */

/* write 16-bit word as little endian */
void fputw(UWORD x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
}

/* write 32-bit long as little endian */
void fputl(ULONG x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
	fputc((x >> 16) & 0xff, fp);
	fputc((x >> 24) & 0xff, fp);
}

/* fwritele mimics fwrite but writes data in little endian format if operating
   on a big endian platform.

   On a little endian platform, this function calls fwrite with no alteration to
   the data.

   On big endian platforms, the only valid size parameters are 1 and 2; size of
   1 indicates byte data, which has no endianness and will be written unaltered.
   If the size parameter is 2 (indicating WORD or UWORD 16-bit data), this
   function will reverse bytes in the file output.

   Note that size parameters greater than 2 (e.g. 4 which would indicate LONG &
   ULONG data) are not currently supported on big endian platforms because
   nothing currently use arrays of that size.

   RETURNS: number of elements written, or zero if error on big endial platforms */
size_t fwritele(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
#ifdef WORDS_BIGENDIAN
	size_t count;
	UBYTE *source;
	UBYTE c;

	if (size == 2) {
		/* fputc doesn't return useful error info as a return value, so simulate
		   the fwrite error condition by checking ferror before and after, and
		   if no error, return the number of elements written. */
		if (ferror(fp)) {
			return 0;
		}
		source = (UBYTE *)ptr;

		/* Instead of using this simple loop over fputc, a faster algorithm
		   could be written by copying the data into a temporary array, swapping
		   bytes there, and using fwrite. However, fputc may be cached behind
		   the scenes and this is likely to be fast enough for most platforms. */
		for (count = 0; count < nmemb; count++) {
			c = *source++;
			fputc(*source++, fp);
			fputc(c, fp);
		}
		if (ferror(fp)) {
			return 0;
		}
		return count;
	}
#endif
	return fwrite(ptr, size, nmemb, fp);
}


#ifdef SOUND
/* WAV_OpenFile will start a new sound file and write out the header. Note that
   the file will not be valid until the it is closed with WAV_CloseFile because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
FILE *WAV_OpenFile(const char *szFileName)
{
	FILE *fp;

	if (!Sound_enabled || !CODECS_AUDIO_Init()) {
		return NULL;
	}
	strcpy(description, "WAV ");
	strcat(description, audio_codec->codec_id);

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;
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

	All chunks must be word-aligned.

	Good description of WAVE format: http://www.sonicspot.com/guide/wavefiles.html
	*/

	fputs("RIFF", fp);
	fputl(0, fp); /* length to be filled in upon file close */
	fputs("WAVE", fp);

	fputs("fmt ", fp);
	fputl(16 + audio_out->extra_data_size, fp);
	fputw(audio_codec->format_type, fp);
	fputw(audio_out->num_channels, fp);
	fputl(audio_out->sample_rate, fp);
	fputl(audio_out->sample_rate * audio_out->num_channels * audio_out->sample_size, fp);
	fputw(audio_out->block_align, fp);
	fputw(audio_out->bits_per_sample, fp);
	if (audio_out->extra_data_size > 0) {
		fwrite(audio_out->extra_data, audio_out->extra_data_size, 1, fp);
	}

	fputs("data", fp);
	fputl(0, fp); /* length to be filled in upon file close */

	if (ftell(fp) != 44 + audio_out->extra_data_size) {
		fclose(fp);
		return NULL;
	}

	fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
	video_frame_count = 0;
	byteswritten = 0;
	return fp;
}

/* WAV_WriteSamples will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input num_samples * sample size) */
int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, FILE *fp)
{
	int size;

	if (!fp) return 0;

	if (!buf) {
		/* This happens at file close time, checking if audio codec has samples
		   remaining */
		if (!audio_codec->another_frame || !audio_codec->another_frame(TRUE)) {
			/* If the codec doesn't support buffered frames or there's nothing
			   remaining, there's no need to try to write another frame */
			return 1;
		}
	}

	do {
		video_frame_count++;

		size = audio_codec->frame(buf, num_samples, audio_buffer, audio_buffer_size);
		if (size < 0) {
			/* failed creating video frame; force close of file */
			Log_print("audio codec %s failed encoding frame", audio_codec->codec_id);
			return 0;
		}

		/* If audio frame size is zero, that means the codec needs more samples
		   before it can create a frame. See audio_codec_adpcm.c for an example.
		   Only if there is some data do we write the frame to the file. */
		if (size > 0) {
			size = fwrite(audio_buffer, 1, size, fp);
			if (!size) {
				/* failed during write; force close of file */
				return 0;
			}
			byteswritten += size;

			/* for next loop, only output samples remaining from previous frame */
			num_samples = 0;
		}
		else {
			/* report success if there weren't enough samples to fill a frame. */
			return 1;
		}
	} while (audio_codec->another_frame && audio_codec->another_frame(FALSE));

	return 1;
}


/* WAV_CloseFile must be called to create a valid WAV file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
int WAV_CloseFile(FILE *fp)
{
	int result = TRUE;
	char aligned = 0;
	int seconds;

	if (fp != NULL) {
		/* Force audio codec to write out the last frame. This only occurs in codecs
		with fixed block alignments */
		result = WAV_WriteSamples(NULL, 0, fp);

		if (result) {
			/* A RIFF file's chunks must be word-aligned. So let's align. */
			if (byteswritten & 1) {
				if (putc(0, fp) == EOF)
					result = FALSE;
				else
					aligned = 1;
			}

			if (result) {
				CODECS_AUDIO_End((float)(video_frame_count / fps));

				/* Sound file is finished, so modify header and close it. */
				if (fseek(fp, 4, SEEK_SET) != 0)	/* Seek past RIFF */
					result = FALSE;
				else {
					/* RIFF header's size field must equal the size of all chunks
					* with alignment, so the alignment byte is added.
					*/
					fputl(byteswritten + 36 + aligned, fp);
					if (fseek(fp, 40 + audio_out->extra_data_size, SEEK_SET) != 0)
						result = FALSE;
					else {
						/* But in the "data" chunk size field, the alignment byte
						* should be ignored. */
						fputl(byteswritten, fp);
						seconds = (int)(video_frame_count / fps);
						Log_print("WAV stats: %d:%02d:%02d, %dMB, %d frames", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, byteswritten / 1024 / 1024, video_frame_count);
					}
				}
			}
		}
		fclose(fp);
	}

	return result;
}
#endif /* SOUND */

#ifdef AVI_VIDEO_RECORDING

/* AVI_WriteHeader creates and writes out the file header. Note that this
   function will have to be called again just prior to closing the file in order
   to re-write the header with updated size values that are only known after all
   data has been written.

   RETURNS: TRUE if header was written successfully, FALSE if not
   */
static int AVI_WriteHeader(FILE *fp) {
	int i;
	int list_size;

	fseek(fp, 0, SEEK_SET);

	/* RIFF AVI header */
	fputs("RIFF", fp);
	fputl(size_riff, fp); /* length of entire file minus 8 bytes */
	fputs("AVI ", fp);

	/* hdrl LIST. Payload size includes the 4 bytes of the 'hdrl' identifier. */
	fputs("LIST", fp);

	/* total header size includes hdrl identifier plus avih size PLUS the video stream
	   header which is (strl header LIST + (strh + strf + strn)) */
	list_size = 4 + 8 + 56 + (12 + (8 + 56 + 8 + 40 + 256*4 + 8 + 16));

#ifdef SOUND
	/* if audio is included, add size of audio stream strl header LIST + (strh + strf + strn) */
	if (num_streams == 2) list_size += 12 + (8 + 56 + 8 + 18 + audio_out->extra_data_size + 8 + 12);
#endif

	fputl(list_size, fp); /* length of header payload */
	fputs("hdrl", fp);

	/* Main header is documented at https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/Aviriff/ns-aviriff-avimainheader */

	/* 8 bytes */
	fputs("avih", fp);
	fputl(56, fp); /* length of avih payload: 14 x 4 byte words */

	/* 56 bytes */
	fputl((ULONG)(1000000 / fps), fp); /* microseconds per frame */
	fputl(image_codec_width * image_codec_height * 3, fp); /* approximate bytes per second of video + audio FIXME: should likely be (width * height * 3 + audio) * fps */
	fputl(0, fp); /* reserved */
	fputl(0x10, fp); /* flags; 0x10 indicates the index at the end of the file */
	fputl(video_frame_count, fp); /* number of frames in the video */
	fputl(0, fp); /* initial frames, always zero for us */
	fputl(num_streams, fp); /* 2 = video and audio, 1 = video only */
	fputl(image_codec_width * image_codec_height * 3, fp); /* suggested buffer size */
	fputl(image_codec_width, fp); /* video width */
	fputl(image_codec_height, fp); /* video height */
	fputl(0, fp); /* reserved */
	fputl(0, fp);
	fputl(0, fp);
	fputl(0, fp);

	/* video stream format */

	/* 12 bytes for video stream strl LIST chuck header; LIST payload size includes the
	   4 bytes of the 'strl' identifier plus the strh + strf + strn sizes */
	fputs("LIST", fp);
	fputl(4 + 8 + 56 + 8 + 40 + 256*4 + 8 + 16, fp);
	fputs("strl", fp);

	/* Stream header format is document at https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/avifmt/ns-avifmt-avistreamheader */

	/* 8 bytes for stream header indicator */
	fputs("strh", fp);
	fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

	/* 56 bytes for stream header data */
	fputs("vids", fp); /* video stream */
	fwrite(video_codec->fourcc, 4, 1, fp);
	fputl(0, fp); /* flags */
	fputw(0, fp); /* priority */
	fputw(0, fp); /* language */
	fputl(0, fp); /* initial_frames */
	fputl(1000000, fp); /* scale */
	fputl((ULONG)(fps * 1000000), fp); /* rate = frames per second / scale */
	fputl(0, fp); /* start */
	fputl(video_frame_count, fp); /* length (for video is number of frames) */
	fputl(image_codec_width * image_codec_height * 3, fp); /* suggested buffer size */
	fputl(0, fp); /* quality */
	fputl(0, fp); /* sample size (0 = variable sample size) */
	fputl(0, fp); /* rcRect, ignored */
	fputl(0, fp);

	/* 8 bytes for stream format indicator */
	fputs("strf", fp);
	fputl(40 + 256*4, fp); /* length of header + palette info */

	/* 40 bytes for stream format data */
	fputl(40, fp); /* header_size */
	fputl(image_codec_width, fp); /* width */
	fputl(image_codec_height, fp); /* height */
	fputw(1, fp); /* number of bitplanes */
	fputw(8, fp); /* bits per pixel: 8 = paletted */
	fwrite(video_codec->avi_compression, 4, 1, fp);
	fputl(image_codec_width * image_codec_height * 3, fp); /* image_size */
	fputl(0, fp); /* x pixels per meter (!) */
	fputl(0, fp); /* y pikels per meter */
	fputl(256, fp); /* colors_used */
	fputl(0, fp); /* colors_important (0 = all are important) */

	/* 256 * 4 = 1024 bytes of palette in ARGB little-endian order */
	for (i = 0; i < 256; i++) {
		fputc(Colours_GetB(i), fp);
		fputc(Colours_GetG(i), fp);
		fputc(Colours_GetR(i), fp);
		fputc(0, fp);
	}

	/* 8 bytes for stream name indicator */
	fputs("strn", fp);
	fputl(16, fp); /* length of name */

	/* 16 bytes for name, zero terminated and padded with a zero */

	/* Note: everything in RIFF files must be word-aligned, so padding with a
	   zero is necessary if the length of the name plus the null terminator is
	   an odd value */
	fputs("atari800 video", fp);
	fputc(0, fp); /* null terminator */
	fputc(0, fp); /* padding to get to 16 bytes */

#ifdef SOUND
	if (num_streams == 2) {
		/* audio stream format */

		/* 12 bytes for audio stream strl LIST chuck header; LIST payload size includes the
		4 bytes of the 'strl' identifier plus the strh + strf + strn sizes */
		fputs("LIST", fp);
		fputl(4 + 8 + 56 + 8 + 18 + audio_out->extra_data_size + 8 + 12, fp);
		fputs("strl", fp);

		/* stream header format is same as video above even when used for audio */

		/* 8 bytes for stream header indicator */
		fputs("strh", fp);
		fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

		/* 56 bytes for stream header data */
		fputs("auds", fp); /* video stream */
		fwrite(audio_codec->fourcc, 4, 1, fp);
		fputl(0, fp); /* flags */
		fputw(0, fp); /* priority */
		fputw(0, fp); /* language */
		fputl(0, fp); /* initial_frames */
		fputl(audio_out->scale, fp); /* scale */
		fputl(audio_out->rate, fp); /* rate, i.e. samples per second */
		fputl(0, fp); /* start time; zero = no delay */
		fputl(audio_out->length, fp); /* length (for audio is number of samples) */
		fputl(audio_out->sample_rate * audio_out->num_channels * audio_out->sample_size, fp); /* suggested buffer size */
		fputl(0, fp); /* quality (-1 = default quality?) */
		fputl(audio_out->block_align, fp); /* sample size */
		fputl(0, fp); /* rcRect, ignored */
		fputl(0, fp);

		/* 8 bytes for stream format indicator */
		fputs("strf", fp);
		fputl(18 + audio_out->extra_data_size, fp); /* length of header */

		/* 18 bytes for stream format data */
		fputw(audio_codec->format_type, fp); /* format_type */
		fputw(audio_out->num_channels, fp); /* channels */
		fputl(audio_out->sample_rate, fp); /* sample_rate */
		fputl(audio_out->sample_rate * audio_out->num_channels * audio_out->sample_size * audio_out->bits_per_sample / 8, fp); /* bytes_per_second */
		fputw(audio_out->block_align, fp); /* bytes per frame */
		fputw(audio_out->bits_per_sample, fp); /* bits_per_sample */
		fputw(audio_out->extra_data_size, fp); /* extra data size */
		if (audio_out->extra_data_size > 0) {
			fwrite(audio_out->extra_data, audio_out->extra_data_size, 1, fp);
		}

		/* 8 bytes for stream name indicator */
		fputs("strn", fp);
		fputl(12, fp); /* length of name */

		/* 12 bytes for name, zero terminated */
		fputs("POKEY audio", fp);
		fputc(0, fp); /* null terminator */
	}
#endif /* SOUND */

	/* audia/video data */

	/* 8 bytes for audio/video stream LIST chuck header; LIST payload is the
	  'movi' chunk which in turn contains 00dc and 01wb chunks representing a
	  frame of video and the corresponding audio. */
	fputs("LIST", fp);
	fputl(size_movi, fp); /* length of all video and audio chunks */
	size_movi = ftell(fp); /* start of movi payload, will finalize after all chunks written */
	fputs("movi", fp);

	return (ftell(fp) == 12 + 8 + list_size + 12);
}

/* AVI_OpenFile will start a new video file and write out an initial copy of the
   header. Note that the file will not be valid until the it is closed with
   AVI_CloseFile because the length information contained in the header must be
   updated with the number of samples in the file.

   RETURNS: file pointer if successful, NULL if failure during open
   */
FILE *AVI_OpenFile(const char *szFileName)
{
	FILE *fp;

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;

	if (!CODECS_VIDEO_Init()) {
		return NULL;
	}
	strcpy(description, "AVI ");
	strcat(description, video_codec->codec_id);
#ifdef SOUND
	if (Sound_enabled) {
		if (!CODECS_AUDIO_Init()) {
			CODECS_VIDEO_End();
			return NULL;
		}
		num_streams = 2;
		strcat(description, " ");
		strcat(description, audio_codec->codec_id);
	}
	else
#endif
	{
		num_streams = 1;
	}

	/* some variables must exist before the call to WriteHeader */
	size_riff = 0;
	size_movi = 0;
	video_frame_count = 0;
	if (!AVI_WriteHeader(fp)) {
		CODECS_VIDEO_End();
#ifdef SOUND
		if (num_streams == 2) {
			CODECS_AUDIO_End(0);
		}
#endif
		fclose(fp);
		return NULL;
	}

	/* set up video statistics */
	frames_written = 0;
	keyframe_count = 0; /* force first frame to be keyframe */

	byteswritten = ftell(fp) + 8; /* current size + index header */
	fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
	total_video_size = 0;
	smallest_video_frame = 0xffffffff;
	largest_video_frame = 0;

	/* allocate space for index which is written at the end of the file */
	num_frames_allocated = FRAME_INDEX_ALLOC_SIZE;
	frame_indexes = (ULONG *)Util_malloc(num_frames_allocated * sizeof(ULONG));
	memset(frame_indexes, 0, num_frames_allocated * sizeof(ULONG));

	return fp;
}

/* AVI_WriteFrame writes out a single frame of video or audio, and saves the
   index data for the end-of-file index chunk */
static int AVI_WriteFrame(FILE *fp, UBYTE *buf, int size, int frame_type, int is_keyframe) {
	int padding;
	int frame_size;
	int expected_frame_size;

	frame_size = ftell(fp);

	/* AVI chunks must be word-aligned, i.e. lengths must be multiples of 2 bytes.
	   If the size is an odd number, the data is padded with a zero but the length
	   value still reports the actual length, not the padded length */
	padding = size % 2;
	if (frame_type == VIDEO_FRAME_FLAG) {
		fputs("00dc", fp);
	}
	else {
		fputs("01wb", fp);
	}
	fputl(size, fp);
	fwrite(buf, 1, size, fp);
	if (padding) {
		fputc(0, fp);
	}
	expected_frame_size = 8 + size + padding;

	size |= frame_type;
	if (is_keyframe) size |= KEYFRAME_FLAG;
	frame_indexes[frames_written] = size;
	frames_written++;
	if (frames_written >= num_frames_allocated) {
		num_frames_allocated += FRAME_INDEX_ALLOC_SIZE;
		frame_indexes = (ULONG *)Util_realloc(frame_indexes, num_frames_allocated * sizeof(ULONG));
	}

	/* update size limit calculation including the 16 bytes needed for each index entry */
	frame_size = ftell(fp) - frame_size;
	byteswritten += frame_size + 16;
	if (byteswritten > MAX_RECORDING_SIZE) {
		/* force file close when at the limit */
		Log_print("AVI max file size reached; closing file");
		return 0;
	}

	/* check expected file data written equals the calculated size */
	return frame_size == expected_frame_size;
}

/* AVI_AddVideoFrame adds a video frame to the stream and updates the video
   statistics. */
int AVI_AddVideoFrame(FILE *fp) {
	int size;
	int result;
	int is_keyframe;

	/* When a codec uses interframes (deltas from the previous frame), a
	   keyframe is needed every keyframe interval. */
	if (video_codec->uses_interframes) {
		keyframe_count--;
		if (keyframe_count <= 0) {
			is_keyframe = TRUE;
			keyframe_count = video_codec_keyframe_interval;
		}
		else {
			is_keyframe = FALSE;
		}
	}
	else {
		is_keyframe = TRUE;
	}

	size = video_codec->frame((UBYTE *)Screen_atari, is_keyframe, video_buffer, video_buffer_size);
	if (size < 0) {
		/* failed creating video frame; force close of file */
		Log_print("video codec %s failed encoding frame", video_codec->codec_id);
		return 0;
	}
	result = AVI_WriteFrame(fp, video_buffer, size, VIDEO_FRAME_FLAG, is_keyframe);
	if (!result) {
		/* failed during write; force close of file */
		return 0;
	}

	/* update statistics */
	video_frame_count++;
	total_video_size += size;
	if (size < smallest_video_frame) {
		smallest_video_frame = size;
	}
	if (size > largest_video_frame) {
		largest_video_frame = size;
	}

	return 1;
}

#ifdef SOUND
/* AVI_AddAudioSamples adds audio data to the stream and update the audio
   statistics. */
int AVI_AddAudioSamples(const UBYTE *buf, int num_samples, FILE *fp) {
	int size;
	int result;

	if (!buf) {
		/* This happens at file close time, checking if audio codec has samples
		   remaining */
		if (!audio_codec->another_frame || !audio_codec->another_frame(TRUE)) {
			/* If the codec doesn't support buffered frames or there's nothing
			   remaining, there's no need to try to write another frame */
			return 1;
		}
	}

	do {
		size = audio_codec->frame(buf, num_samples, audio_buffer, audio_buffer_size);
		if (size < 0) {
			/* failed creating video frame; force close of file */
			Log_print("audio codec %s failed encoding frame", audio_codec->codec_id);
			return 0;
		}

		/* If audio frame size is zero, that means the codec needs more samples
		   before it can create a frame. See audio_codec_adpcm.c for an example.
		   Only if there is some data do we write the frame to the file. */
		if (size > 0) {
			result = AVI_WriteFrame(fp, audio_buffer, size, AUDIO_FRAME_FLAG, TRUE);
			if (!result) {
				/* failed during write; force close of file */
				return 0;
			}
			samples_written += num_samples;

			/* for next loop, only output samples remaining from previous frame */
			num_samples = 0;
		}
		else {
			/* report success if there weren't enough samples to fill a frame. */
			return 1;
		}
	} while (audio_codec->another_frame && audio_codec->another_frame(FALSE));

	return result;
}
#endif

static int AVI_WriteIndex(FILE *fp) {
	int i;
	int offset;
	int size;
	int index_size;
	int chunk_size;
	ULONG index;
	int is_keyframe;

	if (frames_written == 0) return 0;

	chunk_size = ftell(fp);
	offset = 4;
	index_size = frames_written * 16;

	/* The index format used here is tag 'idx1" (index version 1.0) & documented at
	https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/Aviriff/ns-aviriff-avioldindex
	*/

	fputs("idx1", fp);
	fputl(index_size, fp);

	for (i = 0; i < frames_written; i++) {
		index = frame_indexes[i];
		is_keyframe = index & KEYFRAME_FLAG ? 0x10 : 0;
		size = index & FRAME_SIZE_MASK;
		if (index & VIDEO_FRAME_FLAG)
			fputs("00dc", fp); /* stream 0, a compressed video frame */
		else
			fputs("01wb", fp); /* stream 1, audio data */
		fputl(is_keyframe, fp); /* flags: is a keyframe */
		fputl(offset, fp); /* offset in bytes from start of the 'movi' list */
		fputl(size, fp); /* size of frame */
		offset += size + 8 + (size % 2); /* make sure to word-align next offset */
	}

	chunk_size = ftell(fp) - chunk_size;
	return (chunk_size == 8 + index_size);
}

/* AVI_CloseFile must be called to create a valid AVI file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
int AVI_CloseFile(FILE *fp)
{
	int seconds;
	int result;

#ifdef SOUND
	/* Force audio codec to write out the last frame. This only occurs in codecs
	   with fixed block alignments */
	if (num_streams == 2) {
		result = AVI_AddAudioSamples(NULL, 0, fp);
	}
	else
#endif
	{
		result = 1;
	}

	if (result && video_frame_count > 0) {
		seconds = (int)(video_frame_count / fps);
		Log_print("AVI stats: %d:%02d:%02d, %dMB, %d frames; video codec avg frame size %.1fkB, min=%.1fkB, max=%.1fkB", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, byteswritten / 1024 / 1024, video_frame_count, total_video_size / video_frame_count / 1024.0, smallest_video_frame / 1024.0, largest_video_frame / 1024.0);
	}

	/* end codecs so they have a chance to update any final statistics needed for
	   the header. */
#ifdef SOUND
	if (num_streams == 2) {
		CODECS_AUDIO_End((float)(video_frame_count / fps));
	}
#endif
	CODECS_VIDEO_End();

	if (result) {
		size_movi = ftell(fp) - size_movi; /* movi payload ends here */
		result = AVI_WriteIndex(fp);
		if (result > 0) {
			size_riff = ftell(fp) - 8;
			result = AVI_WriteHeader(fp);
		}
	}
	fclose(fp);

	free(frame_indexes);
	frame_indexes = NULL;
	num_frames_allocated = 0;
	return result;
}
#endif /* AVI_VIDEO_RECORDING */
