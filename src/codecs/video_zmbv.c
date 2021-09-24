/*
 * video_zmbv.c - Video codec for Zip Motion Blocks Video
 *
 * This is a derivative work of code from the FFmpeg project. The FFmpeg code
 * used here is licensed under the LGPLv2.1; Provisions within that license
 * permit derivative works to be licensed under the GPLv2.
 *
 * Modifications Copyright (C) 2021 Rob McMullen
 * Original code Copyright (c) 2006 Konstantin Shishkov
 *
 * This file is part of the Atari800 emulator project which emulates the Atari
 * 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Atari800 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Atari800; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Notable differences from original FFmpeg source:

   - only 8 bit-per-pixel image format is supported
   - palette changes within video stream are not supported
   - if compression level is zero, raw uncompressed data is stored rather than
     the zlib stream with compression level 0 data
   - motion estimation range is fixed to a value suited to Atari graphics
*/  


#include <string.h>
#include <stdlib.h>
#include "codecs/video_zmbv.h"
#include "screen.h"
#include "colours.h"
#include "log.h"
#include "util.h"
#include "file_export.h"

/* When zlib is available, ZMBV is the most efficient video codec in atari800
   and is the default. Without zlib, the RLE codec becomes the default because
   uncompressed ZMBV produces much larger output than RLE except in certain
   cases like scrolling games.

   If compiled without zlib, the codec is given the name "uzmbv" to prevent
   confusion. This forces the user to request this uncompressed codec over the
   RLE codec that is the default in this case.
   */

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))

/* Motion block width/height (maximum allowed value is 255)
 * Note: histogram datatype in block_cmp() must be big enough to hold values
 * up to (4 * ZMBV_BLOCK * ZMBV_BLOCK)
 */
#define ZMBV_BLOCK 8

/* Keyframe header format values. Note only the paletted, 8 bits per pixel
   format is supported here. The original FFmpeg code supported many more types. */
#define ZMBV_FMT_8BPP 4

static int video_left_margin;
static int video_top_margin;
static int video_width;
static int video_height;

static int lrange, urange;
static UBYTE *work_buf;
static UBYTE pal[768];
static UBYTE *prev_buf, *prev_buf_start;
static int pstride;
#ifdef HAVE_LIBZ
static int zlib_init_ok;
static z_stream zstream;
#endif
static int score_tab[ZMBV_BLOCK * ZMBV_BLOCK * 4 + 1];


static int block_cmp(UBYTE *src, int stride, UBYTE *src2, int stride2, int bw, int bh, int *xored)
{
	int sum = 0;
	int i, j;
	UWORD histogram[256] = {0};

	/* Build frequency histogram of byte values for src[] ^ src2[] */
	for(j = 0; j < bh; j++){
		for(i = 0; i < bw; i++){
			int t = src[i] ^ src2[i];
			histogram[t]++;
		}
		src += stride;
		src2 += stride2;
	}

	/* If not all the xored values were 0, then the blocks are different */
	*xored = (histogram[0] < bw * bh);

	/* Exit early if blocks are equal */
	if (!*xored) return 0;

	/* Sum the entropy of all values */
	for(i = 0; i < 256; i++)
		sum += score_tab[histogram[i]];

	return sum;
}

static int motion_estimation(UBYTE *src, int sstride, UBYTE *prev, int pstride, int x, int y, int *mx, int *my, int *xored)
{
	int dx, dy, txored, tv, bv, bw, bh;
	int mx0, my0;

	mx0 = *mx;
	my0 = *my;
	bw = FFMIN(ZMBV_BLOCK, video_width - x);
	bh = FFMIN(ZMBV_BLOCK, video_height - y);

	/* Try (0,0) */
	bv = block_cmp(src, sstride, prev, pstride, bw, bh, xored);
	*mx = *my = 0;
	if(!bv) return 0;

	/* Try previous block's MV (if not 0,0) */
	if (mx0 || my0){
		tv = block_cmp(src, sstride, prev + mx0 + my0 * pstride, pstride, bw, bh, &txored);
		if(tv < bv){
			bv = tv;
			*mx = mx0;
			*my = my0;
			*xored = txored;
			if(!bv) return 0;
		}
	}

	/* Try other MVs from top-to-bottom, left-to-right */
	for(dy = -lrange; dy <= urange; dy++){
		for(dx = -lrange; dx <= urange; dx++){
			if(!dx && !dy) continue; /* we already tested this block */
			if(dx == mx0 && dy == my0) continue; /* this one too */
			tv = block_cmp(src, sstride, prev + dx + dy * pstride, pstride, bw, bh, &txored);
			if(tv < bv){
				 bv = tv;
				 *mx = dx;
				 *my = dy;
				 *xored = txored;
				 if(!bv) return 0;
			 }
		 }
	}
	return bv;
}

static int ZMBV_CreateFrame(UBYTE *source, int keyframe, UBYTE *buf, int bufsize)
{
	UBYTE *src;
	UBYTE *prev;
	UBYTE *work;
	int fl;
	int work_size = 0;
	int bw, bh;
	int i, j;
	int size;

	fl = (keyframe ? 1 : 0);
	*buf++ = fl;
	size = 1;
	if (keyframe) {
		*buf++ = 0; /* hi ver */
		*buf++ = 1; /* lo ver */
#ifdef HAVE_LIBZ
		*buf++ = zlib_init_ok; /* comp */
#else
		*buf++ = 0;
#endif
		*buf++ = ZMBV_FMT_8BPP; /* format */
		*buf++ = ZMBV_BLOCK; /* block width */
		*buf++ = ZMBV_BLOCK; /* block height */
		size += 6;
	}

#ifdef HAVE_LIBZ
	if (zlib_init_ok) {
		work = work_buf;
	}
	else
#endif
	{
		work = buf;
	}
	src = source + (Screen_WIDTH * video_top_margin) + video_left_margin;
	prev = prev_buf_start;
	if(keyframe){
		work_size = 768;
		memcpy(work, pal, work_size);
		for(i = 0; i < video_height; i++){
			memcpy(work + work_size, src, video_width);
			src += Screen_WIDTH;
			work_size += video_width;
		}
	}else{
		int x, y, bh2, bw2, xored;
		UBYTE *tsrc;
		UBYTE *tprev;
		UBYTE *mv;
		int mx = 0, my = 0;

		bw = (video_width + ZMBV_BLOCK - 1) / ZMBV_BLOCK;
		bh = (video_height + ZMBV_BLOCK - 1) / ZMBV_BLOCK;
		mv = work + work_size;
		memset(work + work_size, 0, (bw * bh * 2 + 3) & ~3);
		work_size += (bw * bh * 2 + 3) & ~3;
		/* for now just XOR'ing */
		for(y = 0; y < video_height; y += ZMBV_BLOCK) {
			bh2 = FFMIN(video_height - y, ZMBV_BLOCK);
			for(x = 0; x < video_width; x += ZMBV_BLOCK, mv += 2) {
				bw2 = FFMIN(video_width - x, ZMBV_BLOCK);

				tsrc = src + x;
				tprev = prev + x;

				motion_estimation(tsrc, Screen_WIDTH, tprev, pstride, x, y, &mx, &my, &xored);
				mv[0] = (mx * 2) | !!xored;
				mv[1] = my * 2;
				tprev += mx + my * pstride;
				if(xored){
					for(j = 0; j < bh2; j++){
						for(i = 0; i < bw2; i++)
							work[work_size++] = tsrc[i] ^ tprev[i];
						tsrc += Screen_WIDTH;
						tprev += pstride;
					}
				}
			}
			src += Screen_WIDTH * ZMBV_BLOCK;
			prev += pstride * ZMBV_BLOCK;
		}
	}
	/* save the previous frame */
	src = source + (Screen_WIDTH * video_top_margin) + video_left_margin;
	prev = prev_buf_start;
	for(i = 0; i < video_height; i++){
		memcpy(prev, src, video_width);
		prev += pstride;
		src += Screen_WIDTH;
	}

#ifdef HAVE_LIBZ
	if (zlib_init_ok) {
		if (keyframe)
			deflateReset(&zstream);

		zstream.next_in = work_buf;
		zstream.avail_in = work_size;
		zstream.total_in = 0;

		zstream.next_out = buf;
		zstream.avail_out = bufsize - size;
		zstream.total_out = 0;
		if(deflate(&zstream, Z_SYNC_FLUSH) != Z_OK){
			Log_print("Error compressing data\n");
			return -1;
		}
		size += zstream.total_out;
	}
	else
#endif
	{
		size += work_size;
	}

	return size;
}

static int ZMBV_End(void)
{
	free(prev_buf);
#ifdef HAVE_LIBZ
	if (zlib_init_ok) {
		free(work_buf);
		deflateEnd(&zstream);
	}
#endif
	return 0;
}

/* Initialization must be called before any calls to ZMBV_CreateFrame */
static int ZMBV_Init(int width, int height, int left_margin, int top_margin)
{
	int i;
	int prev_size, prev_offset;
	int work_size;
	int comp_size;
	UBYTE *ptr;

	video_width = width;
	video_height = height;
	video_left_margin = left_margin;
	video_top_margin = top_margin;

	/* palette doesn't change, so only have to init it once */
	ptr = pal;
	for (i = 0; i < 256; i++) {
		*ptr++ = Colours_GetR(i);
		*ptr++ = Colours_GetG(i);
		*ptr++ = Colours_GetB(i);
	}

	/* Entropy-based score tables for comparing blocks.
	 * Suitable for blocks up to (ZMBV_BLOCK * ZMBV_BLOCK) bytes.
	 * Scores are nonnegative, lower is better.
	 */
	for(i = 1; i <= ZMBV_BLOCK * ZMBV_BLOCK; i++)
		score_tab[i] = -i * log2(i / (double)(ZMBV_BLOCK * ZMBV_BLOCK)) * 256;

	/* Motion estimation range: maximum distance is -64..63 */
	lrange = urange = 2;

	work_size = video_width * video_height + 1024 +
		((video_width + ZMBV_BLOCK - 1) / ZMBV_BLOCK) * ((video_height + ZMBV_BLOCK - 1) / ZMBV_BLOCK) * 2 + 4;

	/* Conservative upper bound taken from zlib v1.2.1 source via lcl.c */
	comp_size = work_size + ((work_size + 7) >> 3) +
						   ((work_size + 63) >> 6) + 11;


	/* Allocate prev buffer - pad around the image to allow out-of-edge ME:
	 * - The image should be padded with `lrange` rows before and `urange` rows
	 *   after.
	 * - The stride should be padded with `lrange` pixels, then rounded up to a
	 *   multiple of 16 bytes.
	 * - The first row should also be padded with `lrange` pixels before, then
	 *   aligned up to a multiple of 16 bytes.
	 */
	pstride = FFALIGN((video_width + lrange), 16);
	prev_size = FFALIGN(lrange, 16) + pstride * (lrange + video_height + urange);
	prev_offset = FFALIGN(lrange, 16) + pstride * lrange;
	prev_buf = (UBYTE *)Util_malloc(prev_size);
	memset(prev_buf, 0, prev_size);
	prev_buf_start = prev_buf + prev_offset;

#ifdef HAVE_LIBZ
	if (FILE_EXPORT_compression_level > 0) {
		work_buf = (UBYTE *)Util_malloc(work_size);
		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		i = deflateInit(&zstream, FILE_EXPORT_compression_level);
		if (i != Z_OK) {
			Log_print("Inflate init error: %d\n", i);
			return -1;
		}
		zlib_init_ok = 1;
	}
	else {
		work_buf = NULL; /* not needed if there's no compression! */
		zlib_init_ok = 0;
	}
#endif

	return comp_size;
}

VIDEO_CODEC_t Video_Codec_ZMBV = {
#ifdef HAVE_LIBZ
	"zmbv",
	"Zip Motion Blocks Video",
#else
	"uzmbv",
	"Uncompressed Zip Motion Blocks Video",
#endif
	{'Z', 'M', 'B', 'V'},
	{'Z', 'M', 'B', 'V'},
	TRUE,
	&ZMBV_Init,
	&ZMBV_CreateFrame,
	&ZMBV_End,
};
