/* Configure library by modifying this file */

#ifndef ATARI_NTSC_CONFIG_H
#define ATARI_NTSC_CONFIG_H

/* Atari change: remove NES-specific emphasis support */

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Bits per pixel of output. Can be 15, 16, 32, or 24 (same as 32). */
#define ATARI_NTSC_OUT_DEPTH 16

/* Type of input pixel values. You'll probably use unsigned short
if you enable emphasis above. */
#define ATARI_NTSC_IN_T unsigned char

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define ATARI_NTSC_ADJ_IN( in ) in

/* For each pixel, this is the basic operation:
output_color = color_palette [ATARI_NTSC_ADJ_IN( ATARI_NTSC_IN_T )] */

#endif
