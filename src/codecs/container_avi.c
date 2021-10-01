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

/* This file is only compiled when VIDEO_RECORDING is defined. */

#include <stdio.h>
#include <stdlib.h>
#include "file_export.h"
#include "colours.h"
#include "util.h"
#include "log.h"
#ifdef SOUND
#include "codecs/audio.h"
#endif
#include "codecs/image.h"
#include "codecs/video.h"
#include "codecs/container.h"
#include "codecs/container_avi.h"


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

#define FRAME_INDEX_ALLOC_SIZE 1000
static int num_frames_allocated;
static ULONG frames_written;
static ULONG *frame_indexes;
#define FRAME_SIZE_MASK  0x1fffffff
#define VIDEO_FRAME_FLAG 0x20000000
#define AUDIO_FRAME_FLAG 0x40000000
#define KEYFRAME_FLAG    0x80000000


static int num_streams;


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
		fputl(audio_out->bitrate / 8, fp); /* suggested buffer size */
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
		fputl(audio_out->bitrate / 8, fp); /* bytes_per_second */
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

/* AVI_Prepare will start a new video file and write out an initial copy of the
   header. Note that the file will not be valid until the it is closed with
   AVI_Finalize because the length information contained in the header must be
   updated with the number of samples in the file.

   RETURNS: file pointer if successful, NULL if failure during open
   */
static int AVI_Prepare(FILE *fp)
{
#ifdef SOUND
	if (audio_codec) {
		num_streams = 2;
	}
	else
#endif
	{
		num_streams = 1;
	}

	/* some variables must exist before the call to WriteHeader */
	size_riff = 0;
	size_movi = 0;
	if (!AVI_WriteHeader(fp)) {
		File_Export_SetErrorMessage("Failed writing AVI header");
		return 0;
	}

	/* set up video statistics */
	frames_written = 0;

	byteswritten = ftell(fp) + 8; /* current size + index header */

	/* allocate space for index which is written at the end of the file */
	num_frames_allocated = FRAME_INDEX_ALLOC_SIZE;
	frame_indexes = (ULONG *)Util_malloc(num_frames_allocated * sizeof(ULONG));
	memset(frame_indexes, 0, num_frames_allocated * sizeof(ULONG));

	return 1;
}

/* AVI_WriteFrame writes out a single frame of video or audio, and saves the
   index data for the end-of-file index chunk */
static int AVI_WriteFrame(FILE *fp, const UBYTE *buf, int size, int frame_type, int is_keyframe) {
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

	/* check expected file data written equals the calculated size */
	return frame_size == expected_frame_size;
}

/* AVI_VideoFrame adds a video frame to the stream and updates the video
   statistics. */
static int AVI_VideoFrame(FILE *fp, const UBYTE *buf, int bufsize, int is_keyframe) {
	return AVI_WriteFrame(fp, buf, bufsize, VIDEO_FRAME_FLAG, is_keyframe);
}

#ifdef SOUND
/* AVI_AudioFrame adds audio data to the stream and update the audio
   statistics. */
static int AVI_AudioFrame(FILE *fp, const UBYTE *buf, int bufsize) {
	return AVI_WriteFrame(fp, buf, bufsize, AUDIO_FRAME_FLAG, TRUE);
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
	byteswritten += chunk_size;
	return (chunk_size == 8 + index_size);
}

static int AVI_SizeCheck(int size) {
	return size < MAX_RIFF_FILE_SIZE;
}

/* AVI_Finalize must be called to create a valid AVI file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
static int AVI_Finalize(FILE *fp)
{
	int result = 1;

	size_movi = ftell(fp) - size_movi; /* movi payload ends here */
	result = AVI_WriteIndex(fp);
	if (result > 0) {
		size_riff = ftell(fp) - 8;
		result = AVI_WriteHeader(fp);

		if (!result) {
			Log_print("Failed writing AVI header; file will not be playable.");
		}
	}
	else {
		Log_print("Failed writing AVI index; file will not be playable.");
	}

	free(frame_indexes);
	frame_indexes = NULL;
	num_frames_allocated = 0;
	return result;
}


CONTAINER_t Container_AVI = {
	"avi",
	"AVI format multimedia",
	&AVI_Prepare,
#ifdef SOUND
	&AVI_AudioFrame,
#else
	NULL,
#endif
	&AVI_VideoFrame,
	&AVI_SizeCheck,
	&AVI_Finalize,
};
