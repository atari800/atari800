#ifndef _AMIGA_ASM_H_
#define _AMIGA_ASM_H_

ASM void ScreenData28bit( register __a0 UBYTE *screen,
                          register __a1 UBYTE *tempscreendata,
                          register __a2 UBYTE *colortable8,
                          register __d0 ULONG width,
                          register __d1 ULONG height);

ASM void ScreenData215bit( register __a0 UBYTE *screen,
                           register __a1 UWORD *,
                           register __a2 UWORD *colortable15,
                           register __d0 ULONG width,
                           register __d1 ULONG height);

#endif /* _AMIGA_ASM_H_ */

