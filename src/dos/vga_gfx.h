#ifndef _VGA_GFX_H_
#define _VGA_GFX_H_

#include "atari.h"

#ifndef AT_USE_ALLEGRO

/****************************** VESA2 **********************************/

/*find given VESA mode and get informations about it*/
extern UBYTE VESA_getmode(int width,int height,UWORD *videomode,ULONG *memaddress,ULONG *line,ULONG *memsize);
/*open given vesa mode*/
extern UBYTE VESA_open(UWORD mode,ULONG memaddress,ULONG memsize,ULONG *linear,int *selector);
/*close VESA mode*/
extern UBYTE VESA_close(ULONG *linear,int *selector);
/*draw atari screen in VESA mode*/
extern void VESA_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector);
/*draw atari screen interlaced with darker lines*/
extern void VESA_i_blit(void *mem,ULONG width,ULONG height,ULONG bitmapline,ULONG videoline,UWORD selector);

/****************************** XMODE **********************************/

/*open given x-mode*/
extern UBYTE x_open(UWORD mode);
/*draw normal or interlaced atari screen*/
extern void x_blit(void *mem,UBYTE lines,ULONG change,ULONG videoline);
/*draw atari screen interlaced with darker lines*/
extern void x_blit_i2(void *mem,UBYTE lines,ULONG change,ULONG videoline);

#else

/****************************** ALLEGRO ********************************/

/*map allegro's bitmap into given memory*/
extern void Map_bitmap(BITMAP *bitmap,void *memory,int width,int height);
/*map allegro's bitmap into given memory interlaced with memory2*/
extern void Map_i_bitmap(BITMAP *bitmap,void *memory,void *memory2,int width,int height);
/*make darker copy of buffer 'source'*/
extern void make_darker(void *target,void *source,int bytes);

#endif /* AT_USE_ALLEGRO */

/*vertical retrace control*/
extern void v_ret(void);

#endif /* _VGA_GFX_H_ */
