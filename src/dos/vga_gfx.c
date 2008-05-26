/*
 * vga_gfx.c - DOS VGA graphics routines
 *
 * Copyright (c) 2001 Robert Golias
 * Copyright (c) 2001-2003 Atari800 development team (see DOC/CREDITS)
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

#include <go32.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <string.h>
#include "atari.h"
#include "config.h"
#include "vga_gfx.h"

#ifdef AT_USE_ALLEGRO
#include <allegro.h>
#endif


#define TRUE 1
#define FALSE 0

#ifndef AT_USE_ALLEGRO

/***************************************************************************
 * VESA2 support                                                           *
 ***************************************************************************/

/*structure that is returned by VESA function 4f00  (get vesa info)  */
struct VESAinfo
{
  char sig[4];
  UWORD version;
  ULONG OEM; /*dos pointer*/
  ULONG capabilities;
  ULONG videomodes; /*dos pointer*/
  UWORD memory;
  UWORD oemRevision;
  ULONG oemVendorName; /*dos pointer*/
  ULONG oemProductName; /*dos pointer*/
  ULONG oemProductRev; /*dos pointer*/
  UBYTE reserved[222+256];
}__attribute__((packed));
/*structure that is returned by VESA function 4f01 (get videomode info) */
struct modeInfo
{
  UWORD attributes;
  UBYTE winAAttributes;
  UBYTE winBAttributes;
  UWORD winGranularity;
  UWORD winSize;
  UWORD winASegment;
  UWORD winBSegment;
  ULONG winfunc; /*dos pointer*/
  UWORD bytesPerScanline;

  UWORD XResolution;
  UWORD YResolution;
  UBYTE XCharSize;
  UBYTE YCharSize;
  UBYTE numOfPlanes;
  UBYTE bitsPerPixel;
  UBYTE numOfBanks;
  UBYTE memoryModel;
  UBYTE bankSize;
  UBYTE numOfImagePages;
  UBYTE reserved;

  UBYTE redMaskSize;
  UBYTE redFieldPosition;
  UBYTE greenMaskSize;
  UBYTE greenFieldPosition;
  UBYTE blueMaskSize;
  UBYTE blueFieldPosition;
  UBYTE reservedMaskSize;
  UBYTE reservedPosition;
  UBYTE directAttributes;

  ULONG basePtr;
  ULONG offScreenMemoryOffset;
  ULONG offScreenMemorySize;
  UBYTE reserved2[216];
}__attribute__((packed));


/*functions for mapping physical memory and allocating the corresponding selector */
static UBYTE mapPhysicalMemory(ULONG addr,ULONG length,ULONG *linear)
{
  __dpmi_meminfo meminfo;

  if (addr>=0x100000)
  {
    meminfo.size=length;
    meminfo.address=addr;
    *linear=0;
    if (__dpmi_physical_address_mapping(&meminfo)!=0)
      return FALSE;
    *linear=meminfo.address;
    __dpmi_lock_linear_region(&meminfo);
  }
  else
    *linear=addr;  /* there is no way for mapping memory under 1MB with dpmi0.9
                      so we suppose that the address remains the same*/
  return TRUE;
}
static UBYTE unmapPhysicalMemory(ULONG *linear)
{
  __dpmi_meminfo meminfo;

  if (*linear>=0x100000)
  {
    meminfo.address=*linear;
    __dpmi_free_physical_address_mapping(&meminfo);
  }  /* when <1MB, suppose that it lies in regular DOS memory and does not
        requires freeing...  */
  *linear=0;
  return TRUE;
}
static UBYTE createSelector(ULONG addr,ULONG length,int *selector)
{
  *selector=__dpmi_allocate_ldt_descriptors(1);
  if (*selector<0)
  {
    *selector=0;
    return FALSE;
  }
  if (__dpmi_set_segment_base_address(*selector,addr)<0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    return FALSE;
  }
  if (__dpmi_set_segment_limit(*selector,length-1)<0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    return FALSE;
  }
  return TRUE;
}
#if 0
/* unused by our code, warning suppressed */
static UBYTE freeSelector(int *selector)
{
  if (*selector>0)
  {
    __dpmi_free_ldt_descriptor(*selector);
    *selector=0;
  }
  return TRUE;
}
#endif

static UBYTE getPhysicalMemory(ULONG addr,ULONG length,ULONG *linear,int *selector)
{
  if (!mapPhysicalMemory(addr,length,linear))
  {
    selector=0;
    return FALSE;
  }
  if (!createSelector(*linear,length,selector))
  {
    unmapPhysicalMemory(linear);
    return FALSE;
  }
  return TRUE;
}
static UBYTE freePhysicalMemory(ULONG *linear,int *selector)
{
  unmapPhysicalMemory(linear);
  linear=0;
  selector=0;
  return TRUE;
}

/* VESA getmode - finds the desired videomode*/
UBYTE VESA_getmode(int width,int height,UWORD *videomode,ULONG *memaddress,ULONG *line,ULONG *memsize)
{
  __dpmi_regs rg;
  struct VESAinfo vInfo;
  struct modeInfo mInfo;
  UWORD modesAddr;
  UWORD mode;
  UWORD modes[0x1000]; /* modes list needs to be saved (K.N.) */
  int i;

  *videomode=0;

  if (_go32_info_block.size_of_transfer_buffer < 512)
    return FALSE;  /*transfer buffer too small*/

  memset(&vInfo,0,sizeof(vInfo));
  strncpy(vInfo.sig,"VBE2",4);
  dosmemput(&vInfo,sizeof(vInfo),__tb&0xfffff);
  rg.x.ax=0x4f00;
  rg.x.di=__tb & 0xf;
  rg.x.es=(__tb >>4 )&0xffff;
  __dpmi_int(0x10,&rg);
  dosmemget(__tb&0xfffff,sizeof(vInfo),&vInfo);
  if (rg.x.ax!=0x004f || strncmp(vInfo.sig,"VESA",4) || (vInfo.version<0x200) )
    return FALSE;  /*vesa 2.0 not found*/

  /*now we must search the videomode list for desired screen resolutin*/
  modesAddr=( (vInfo.videomodes>>12)&0xffff0)+(vInfo.videomodes&0xffff);
  dosmemget(__tb&0xfffff, sizeof(modes), modes); /* save modes list (K.N.) */
  for (i = 0; (mode = modes[i]) != 0xffff; i++)
  {
    modesAddr+=2;

    rg.x.ax=0x4f01;
    rg.x.cx=mode;
    rg.x.es=(__tb>>4) & 0xffff;
    rg.x.di=__tb & 0xf;
    __dpmi_int(0x10,&rg);   /*get vesa-mode info*/
    if (rg.h.al!=0x4f) return FALSE;  /*function not supported, but we need it*/


    if (rg.h.ah!=0) continue; /*mode not supported*/
    dosmemget(__tb&0xfffff,sizeof(mInfo)-216,&mInfo); /*note: we don't need the reserved bytes*/
    if ((mInfo.attributes&0x99) != 0x99) continue;
    /*0x99 = available, color, graphics mode, LFB available*/
    if (mInfo.XResolution!=width || mInfo.YResolution!=height) continue;
    if (mInfo.numOfPlanes!=1 || mInfo.bitsPerPixel!=8 || mInfo.memoryModel!=4) continue;
    break;
  }

  if (mode==0xffff) return FALSE;

  *line=mInfo.bytesPerScanline;
  *memsize=*line * height;
  *videomode=(mode&0x03ff)|0x4000;  /*LFB version of video mode*/
  *memaddress=mInfo.basePtr;
  return TRUE;
}

/* VESA_open - opens videomode and allocates the corresponding selector */
UBYTE VESA_open(UWORD mode,ULONG memaddress,ULONG memsize,ULONG *linear,int *selector)
{
  __dpmi_regs rg;

  rg.x.ax=0x4f02;
  rg.x.bx=mode;
  __dpmi_int(0x10,&rg);
  if (rg.x.ax!=0x004f) return FALSE;

  if (!getPhysicalMemory(memaddress,memsize,linear,selector))
    return FALSE;
  return TRUE;
}
UBYTE VESA_close(ULONG *linear,int *selector)
{
  __dpmi_regs rg;

  freePhysicalMemory(linear,selector);
  rg.x.ax=0x0003;
  __dpmi_int(0x10,&rg);
  return TRUE;
}



/***************************************************************************
 * XMODE support                                                           *
 ***************************************************************************/

/* Procedure for initializing X-mode. Possible modes:
 *              0       320x175                 6       360x175
 *              1       320x200                 7       360x200
 *              2       320x240                 8       360x240
 *              3       320x350                 9       360x350
 *              4       320x400                 a       360x400
 *              5       320x480                 b       360x480
 *
 * This procedure is based on Guru mode by Per Ole Klemetsrud
 */
UWORD x_regtab[]={0x0e11,0x0014,0xe317,0x6b00,0x5901,
                  0x5a02,0x8e03,0x5e04,0x8a05,0x2d13};
UWORD x_vert480[]={0x4009,0x3e07,0x0b06,0xea10,0x8c11,
                   0xe715,0x0416,0xdf12,0xdf12};
UWORD x_vert350[]={0x4009,0x1f07,0xbf06,0x8310,0x8511,
                   0x6315,0xba16,0x5d12,0x5d12};
UBYTE x_clocktab[]={0xa3,0x0,0xe3,0xa3,0x0,0xe3,0xa7,0x67,
                    0xe7,0xa7,0x67,0xe7};
UWORD *x_verttab[]={x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480,
                   x_vert350+1,(void*)0,x_vert480+1,x_vert350,(void*)1,x_vert480};

void x_open_asm(int mode);	/* in vga_asm.s */

UBYTE x_open(UWORD mode)
{
    __dpmi_regs rg;

    if (mode>0xb) return 0;
    rg.x.ax = 0x1a00;
    __dpmi_int(0x10,&rg);
    if (rg.h.al != 0x1a) return 0;

    rg.x.ax=0x0013;
    __dpmi_int(0x10,&rg);

	x_open_asm(mode);
	return 0;
}

#else



/***************************************************************************
 * ALLEGRO support                                                         *
 ***************************************************************************/


/*this routine maps allegro's bitmap to given memory */
void Map_bitmap(BITMAP *bitmap,void *memory,int width,int height)
{
  int i;

  bitmap->dat=memory;
  bitmap->line[0]=memory;
  for (i=1;i<height;i++)
    bitmap->line[i]=bitmap->line[i-1]+width;
}
/*this routine maps allegro's bitmap to given memory interlaced with memory2*/
void Map_i_bitmap(BITMAP *bitmap,void *memory,void *memory2,int width,int height)
{
  int i;

  bitmap->dat=memory;
  bitmap->line[0]=memory;
  bitmap->line[1]=memory2;
  for (i=2;i<height;i+=2)
  {
    bitmap->line[i]=bitmap->line[i-2]+width;
    bitmap->line[i+1]=bitmap->line[i-1]+width;
  }
}

#endif
