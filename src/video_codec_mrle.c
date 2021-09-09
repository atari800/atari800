/*
 * video_codec_mrle.c - Video codec for Microsoft Run-Length Encoding format
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

#include <string.h>
#include "video_codec_mrle.h"
#include "screen.h"


/* This file implements the Microsoft Run Length Encoding video codec, fourcc
   code of 'mrle'.

   This is the simplest codec that I could find that is supported on multiple
   platforms. It compresses *much* better than raw video, and is supported by
   ffmpeg and ffmpeg-based players (like vlc and mpv), as well as proprietary
   applications like Windows Media Player. See:

   https://wiki.multimedia.cx/index.php?title=Microsoft_RLE
   
   for a description of the format. */

static int MRLE_CompressLine(UBYTE *buf, const UBYTE *ptr, int width) {
	int extra;
	int count;
	UBYTE last;
	UBYTE *buf_start;
	const UBYTE *run_start;
	const UBYTE *ptr_end;

	buf_start = buf;
	ptr_end = ptr + width;
	do {
		last = *ptr;
		run_start = ptr;
		do {
			ptr++;
		} while (last == *ptr && ptr < ptr_end);
		count = ptr - run_start;
		if (count > 1) {
			/* Run of same color pixels */
			while (count > 0) {
				if (count > 254) {
					*buf++ = 254;
					count -= 254;
				}
				else {
					*buf++ = count;
					count = 0;
				}
				*buf++ = last;
			}
			/* try to match another run of a color */
			continue;
		}
		else {
			/* run of different pixels, stopping at next pair of matching pixels */
			while (ptr < ptr_end - 1 && *ptr != *(ptr+1) && *(ptr+1) != *(ptr+2)) {
				ptr++;
			}
			while (run_start < ptr) {
				count = ptr - run_start;
				if (count > 254) {
					/* stop at 254 to avoid use of padding byte for this chunk */
					count = 254;
				}

				if (count < 3) {
					/* can't do a data copy length of 1 or 2 directly */
					*buf++ = 1;
					*buf++ = *run_start++;
					if (count == 2) {
						*buf++ = 1;
						*buf++ = *run_start++;
					}
				}
				else {
					/* data copy of length 3 to 255 can be done in one encoding */
					*buf++ = 0;
					*buf++ = count;
					extra = count & 1;
					while (count-- > 0) {
						*buf++ = *run_start++;
					}
					if (extra) *buf++ = 0;
				}
			}
		}
	} while (ptr < ptr_end);

	/* mark end of line */
	*buf++ = 0;
	*buf++ = 0;
	return buf - buf_start;
}

/* MRLE_CreateKeyframe fills the output buffer with the fourcc type 'mrle' run
   length encoding data using the paletted data of the Atari screen.

   RETURNS: number of encoded bytes */
static int MRLE_CreateKeyframe(UBYTE *buf, int bufsize, const UBYTE *source, int width, int height, int left_margin, int top_margin) {
	UBYTE *buf_start;
	const UBYTE *ptr;
	int y;
	int size;

	buf_start = buf;

	/* MRLE codec requires image origin at bottom left, so start saving at last scan
	   line and work back to the zeroth scan line. */

	for (y = (top_margin + height)-1; y >= top_margin; y--) {
		ptr = source + (y * Screen_WIDTH) + left_margin;

		size = MRLE_CompressLine(buf, ptr, width);
		buf += size;
	}

	/* mark end of bitmap */
	*buf++ = 0;
	*buf++ = 1;

	return buf - buf_start;
}

static int MRLE_CompressDelta(UBYTE *buf, const UBYTE *ptr, UBYTE *ref, int width, int *dy) {
	int extra;
	int count;
	UBYTE last;
	UBYTE *buf_start;
	const UBYTE *run_start;
	const UBYTE *ptr_end;
	UBYTE *ref_end;

	buf_start = buf;
	ptr_end = ptr + width;
	ref_end = ref + width;

	/* right margin won't change, so find it outside the main loop */
	while (*(ptr_end - 1) == *(ref_end - 1) && ptr < ptr_end) {
		ptr_end--;
		ref_end--;
	}

	while (ptr < ptr_end) {
		run_start = ptr;

		/* check for next change from reference screen */
		while (*ptr == *ref && ptr < ptr_end) {
			ptr++;
			ref++;
		}
		if (ptr == ptr_end) break; /* no more differences in rest of scan line! */

		/* skipping pixels that are the same as the reference image */
		count = ptr - run_start;

		/* it takes 4 bytes to encode a skip, so it's possible that the skip
		   takes more space than just encoding. Force the skip if the pixel
		   count is big enough, otherwise reset the pointer and just encode as
		   normal. */
		if (count > 4 || *dy) {
			do {
				*buf++ = 0;
				*buf++ = 2;
				*buf++ = (UBYTE)(count > 255 ? 255 : count);
				*buf++ = (UBYTE)*dy;

				count -= 255;
				*dy = 0; /* dy will never be more than 255 because there are only 240 lines in Screen_atari! */
			} while (count > 0);
		}
		else {
			ptr -= count;
			ref -= count;
		}

		/* encode the differences */
		last = *ptr;
		run_start = ptr;
		do {
			ptr++;
			ref++;
		} while (last == *ptr && ptr < ptr_end);
		count = ptr - run_start;
		if (count > 1) {
			/* Run of same color pixels */
			while (count > 0) {
				if (count > 254) {
					*buf++ = 254;
					count -= 254;
				}
				else {
					*buf++ = count;
					count = 0;
				}
				*buf++ = last;
			}
			/* try to match another run of a color */
			continue;
		}
		else {
			/* run of different pixels, stopping at next pair of matching pixels */
			while (ptr < ptr_end - 1 && *ptr != *(ptr+1) && *(ptr+1) != *(ptr+2)) {
				ptr++;
				ref++;
			}
			while (run_start < ptr) {
				count = ptr - run_start;
				if (count > 254) {
					/* stop at 254 to avoid use of padding byte for this chunk */
					count = 254;
				}

				if (count < 3) {
					/* can't do a data copy length of 1 or 2 directly */
					*buf++ = 1;
					*buf++ = *run_start++;
					if (count == 2) {
						*buf++ = 1;
						*buf++ = *run_start++;
					}
				}
				else {
					/* data copy of length 3 to 255 can be done in one encoding */
					*buf++ = 0;
					*buf++ = count;
					extra = count & 1;
					while (count-- > 0) {
						*buf++ = *run_start++;
					}
					if (extra) *buf++ = 0;
				}
			}
		}
	}

	if (buf > buf_start) {
		/* mark end of line */
		*buf++ = 0;
		*buf++ = 0;
	}
	else {
		/* no end of line marker when a blank line is encountered */
		*dy = *dy + 1;
	}
	return buf - buf_start;
}

/* MRLE_CreateInterframe compares the current screen to the reference screen and
   fills the output buffer with the MRLE encoded differences. It also updates
   the reference screen to hold the current screen.

   RETURNS: number of encoded bytes, which may be zero if identical to previous
   frame */
static int MRLE_CreateInterframe(UBYTE *buf, int bufsize, const UBYTE *source, UBYTE *reference, int width, int height, int left_margin, int top_margin) {
	UBYTE *buf_start;
	const UBYTE *ptr;
	UBYTE *ref;
	int y;
	int dy;
	int size;

	buf_start = buf;

	/* dy is used to keep track of consecutive blank lines encoutered in the line
	   encoder */
	dy = 0;

	for (y = (top_margin + height)-1; y >= top_margin; y--) {
		ptr = source + (y * Screen_WIDTH) + left_margin;
		ref = reference + (y * Screen_WIDTH) + left_margin;

		size = MRLE_CompressDelta(buf, ptr, ref, width, &dy);
		buf += size;
	}

	if (buf > buf_start) {
		/* mark end of bitmap */
		*buf++ = 0;
		*buf++ = 1;
	}

	return buf - buf_start;
}

int MRLE_CreateFrame(const UBYTE *source, int keyframe, UBYTE *buf, int bufsize, UBYTE *reference_screen, int width, int height, int left_margin, int top_margin) {
	int size;

	if (keyframe) {
		size = MRLE_CreateKeyframe(buf, bufsize, source, width, height, left_margin, top_margin);
	}
	else {
		size = MRLE_CreateInterframe(buf, bufsize, source, reference_screen, width, height, left_margin, top_margin);
	}

	/* save new reference screen */
	memcpy(reference_screen, source, Screen_HEIGHT * Screen_WIDTH);

	return size;
}
