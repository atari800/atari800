/*
 * atari_wince.c - WinCE port specific code
 *
 * Copyright (C) 2001 Vasyl Tsvirkunov
 * Copyright (C) 2001-2006 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <windows.h>
#include "gx.h"

extern "C"
{
#include "input.h"
#include "main.h"
#include "keyboard.h"
#include "screen_wince.h"
#include "ui.h"

int virtual_joystick = 0;
int smkeyhack = 0;
extern HWND hWndMain;
};

int currentKeyboardMode = 4;
int currentKeyboardColor = 0;

int ui_clearkey = FALSE;
int kbui_timerset = FALSE;

int stylus_down = 0;

unsigned long* kbd_image;

/* Assumed to be 240x80 packed bitmap with buttons in 5 rows */
unsigned long kbd_image_800[] =
{
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 
	0x10004001, 0x31000400, 0x07100040, 0x40010004, 0x04001000, 0x00400100, 0x00040010, 0x00008000, 
	0xd00c403d, 0xf903340c, 0x0d903340, 0x407900c4, 0x64039038, 0x03401d06, 0x033440d3, 0x0000801c, 
	0xd00c400d, 0x0d07fc0c, 0x07101b40, 0x40cd00c4, 0x9407101c, 0x2558c964, 0xf3344d52, 0x00008f98, 
	0xd00c403d, 0x7903340c, 0x03900c40, 0x40ed00c4, 0x1406100c, 0x554549a4, 0x9bf554d2, 0x00009999, 
	0x100c400d, 0xc1033400, 0x1ed00640, 0x40ed0004, 0x1406100c, 0x35494924, 0xfb34c552, 0x00009999, 
	0x100040fd, 0x7d07fc00, 0x0cd03340, 0x400d0004, 0x9407101c, 0x15514924, 0x1b354552, 0x00008f98, 
	0x100c4031, 0x31033400, 0x1b903140, 0x40f90004, 0x64039038, 0x634d5d2e, 0xf33644d7, 0x000081bc, 
	0x100040f1, 0x01000400, 0x00100040, 0x40010004, 0x04001000, 0x00400d03, 0x00040010, 0x00008180, 
	0x900c4001, 0x6103f407, 0x07903f40, 0x407903f4, 0x8407901e, 0x00401901, 0x00040010, 0x00008000, 
	0xd00e4001, 0x7101840c, 0x00d00340, 0x40cd0304, 0xc40cd033, 0x00403100, 0x00040010, 0x00008000, 
	0x100c4001, 0x7900c406, 0x07d01f40, 0x40790184, 0x640ed03e, 0x00406100, 0x00040010, 0x00008000, 
	0x100c4001, 0x6d018403, 0x0cd03040, 0x40cd00c4, 0xc40dd030, 0x00403100, 0x00040010, 0x00008000, 
	0x900c4001, 0xfd033401, 0x0cd03340, 0x40cd0064, 0x840cd018, 0x00401901, 0x00040010, 0x00008000, 
	0xd03f4001, 0x6101e40f, 0x07901e40, 0x40790064, 0x0407900e, 0x00400d03, 0x00040010, 0x00008000, 
	0x10004001, 0x01000400, 0x00100040, 0x40010004, 0x04001000, 0x00400100, 0x00040010, 0x00008000, 
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00040000, 0x00008000, 
	0x0f20c07d, 0x81fa0c68, 0x681fa03e, 0x7e819a06, 0x03e80f20, 0x20188062, 0x61e40000, 0x00008c00, 
	0x19a0cf31, 0x801a0c68, 0x68062066, 0x18819a06, 0x066819a0, 0x201880f2, 0xf8340000, 0x0000bf7d, 
	0x19a7d831, 0x80fa0d68, 0xc8062066, 0x18819a03, 0x066819a0, 0x201881fa, 0x61e43000, 0x00008c4c, 
	0x19acdf31, 0x801a0fe8, 0x8806203e, 0x18819a01, 0x03e819a0, 0x207e8062, 0x63043000, 0x00008c0c, 
	0x0dacd9b1, 0x801a0ee8, 0x88062036, 0x18819a01, 0x006819a0, 0x203c8062, 0x63043000, 0x00008c0c, 
	0x1b27df31, 0x81fa0c68, 0x88062066, 0x7e81fa01, 0x00680f20, 0x20188062, 0xc1e43000, 0x0000b80d, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00043030, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00043018, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x207e8002, 0x00043ffc, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x200081fa, 0x00040018, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00040030, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x207e8002, 0x00040000, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00040000, 0x00008000, 
	0x00200001, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x20008002, 0x00040000, 0x00008000, 
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x00040002, 0x00008000, 
	0x62006089, 0x07a03c80, 0x81f207e8, 0x68182066, 0x00801a06, 0x01880620, 0x39e40012, 0x00008300, 
	0xf2004dd5, 0x0da00680, 0x801a0068, 0x68182066, 0x18801a03, 0x03080320, 0x3034002a, 0x00008fde, 
	0x9a005485, 0x19a03c81, 0x801a03e8, 0xe818207e, 0x18801a01, 0x07e81fa0, 0x31e5998a, 0x00008303, 
	0x9a004485, 0x19a06081, 0x81da0068, 0xe8182066, 0x00801a01, 0x03080320, 0x33046a0a, 0x00008303, 
	0xfa004495, 0x0da06081, 0x819a0068, 0x6819a066, 0x18801a03, 0x01880620, 0x3304ab0a, 0x00008303, 
	0x9a004509, 0x07a03c81, 0x81f20068, 0x680f2066, 0x1881fa06, 0x00080020, 0x79e51aaa, 0x00008e1e, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x00800200, 0x00080020, 0x0004cb12, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x00800200, 0x0cc80620, 0x00040802, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x18800200, 0x07880620, 0x00040002, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x18800200, 0x1fe81fa0, 0x00040002, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x00800200, 0x07880620, 0x00040002, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x18800200, 0x0cc80620, 0x00040002, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x18800200, 0x00080020, 0x00040002, 0x00008000, 
	0x02000001, 0x00200080, 0x80020008, 0x08002000, 0x0c800200, 0x00080020, 0x00040002, 0x00008000, 
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004001, 0x00000400, 0x00040010, 0x00008000, 
	0x40024899, 0xe40cd03f, 0x1f40cd01, 0x06340cd0, 0x900f40f1, 0x09226407, 0x01e40fd0, 0x00008030, 
	0x40022085, 0x340cd018, 0x3340cd03, 0x07740dd0, 0xd00c4031, 0x0882140c, 0xfb340bd0, 0x000080fc, 
	0x40076999, 0x3407900c, 0x1f40cd00, 0x07f40fd0, 0x100c4031, 0x1da66406, 0x9b3409d0, 0x00008031, 
	0x40022aa1, 0x34079006, 0x3340cd00, 0x06b40fd0, 0x100c4031, 0x08aa8403, 0x9b3408d0, 0x00008031, 
	0x40022aa1, 0x340cd003, 0x33407903, 0x06340ed0, 0x100c4031, 0x08aa8400, 0xfb340850, 0x00008030, 
	0x40042a99, 0xe40cd03f, 0x1f403101, 0x06340cd0, 0x100f40f1, 0x10aa6403, 0x19e40fd0, 0x000080e0, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004001, 0x00000400, 0x18040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004001, 0x0000040c, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004001, 0x00000406, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004001, 0x00000403, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x90004001, 0x00000401, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0xd00c4031, 0x00000400, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x500c4031, 0x00000400, 0x00040010, 0x00008000, 
	0x40000001, 0x04001000, 0x00400100, 0x00040010, 0x10004019, 0x00000400, 0x00040010, 0x00008000, 
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000000, 0x00000000, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000100, 0x00000000, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0xffc00040, 0x7ff00181, 0x00003ff8, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00400040, 0x7ff001c1, 0x00002008, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00400040, 0x7ff0ffe1, 0x00002828, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x80400040, 0x7ff0c1c1, 0x00002448, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x08400040, 0x7ff0c181, 0x00002288, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x1c400040, 0x4010c101, 0x00002108, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x08400040, 0x4010c001, 0x00002288, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00400040, 0x4010c001, 0x00002448, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x08400040, 0x4010c001, 0x00002828, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x08400040, 0x4010ffe1, 0x00002008, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0xffc00040, 0x7ff0ffe1, 0x00003ff8, 
	0x00000000, 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000000, 0x00000000, 
	0x00000000, 0xfffffc00, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000007f, 0x00000000, 0x00000000, 
};

unsigned long kbd_image_5200[] =
{
	0xc0000000, 0xffffffff, 0xfc0001ff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000000f, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x843c1060, 0x0400011f, 0x6000030f, 0x0007c080, 0xf8200000, 0x06000000, 0x00000008, 
	0x40000000, 0x04661070, 0x8400010c, 0xf9f3cfc1, 0x33ccc081, 0x982079f3, 0x1f9e7c79, 0x00000008, 
	0x40000000, 0x04301060, 0x04000106, 0x6336030f, 0x360cc080, 0x9820cc1b, 0x063306cd, 0x00000008, 
	0x40000000, 0x04181060, 0x0400010c, 0x6037c318, 0x37c7c080, 0xf820fcf3, 0x063f3cfc, 0x00000008, 
	0x40000000, 0x840c1060, 0x04000119, 0x60366318, 0x3660c080, 0xd8200d83, 0x0603600c, 0x00000008, 
	0x40000000, 0x047e11f8, 0x0400010f, 0xc037ce0f, 0xe7c0c081, 0x982078fb, 0x1c1e3e79, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0x40000000, 0x04001000, 0x04000100, 0x00000000, 0x00000080, 0x00200000, 0x00000000, 0x00000008, 
	0xc0000000, 0xffffffff, 0xfc0001ff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000000f, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x047e10c0, 0x0000010f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x840610e0, 0x00000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x843e10f0, 0x0000010f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x846010d8, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x846611f8, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x043c10c0, 0x0000010f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0xc0000000, 0xffffffff, 0x000001ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x043c11f8, 0x0000010f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x84661180, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x043c10c0, 0x0000011f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04661060, 0x00000118, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04661030, 0x0000010c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x043c1030, 0x00000107, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0xc0000000, 0xffffffff, 0x000001ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x843c1198, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0xc46610f0, 0x0000013f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x847613fc, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x846e10f0, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0xc4661198, 0x0000013f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x843c1000, 0x00000119, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x40000000, 0x04001000, 0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0xc0000000, 0xffffffff, 0x000001ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xffc00000, 0x7ff00181, 0x00003ff8, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x7ff001c1, 0x00002008, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x7ff0ffe1, 0x00002828, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x80400000, 0x7ff0c1c1, 0x00002448, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08400000, 0x7ff0c181, 0x00002288, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1c400000, 0x4010c101, 0x00002108, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08400000, 0x4010c001, 0x00002288, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x4010c001, 0x00002448, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08400000, 0x4010c001, 0x00002828, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08400000, 0x4010ffe1, 0x00002008, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xffc00000, 0x7ff0ffe1, 0x00003ff8, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
};

struct sKeydata
{
	short offset;
	short key;
};

#define KBD_ROTATE -100
#define KBD_NEGATE -101
#define KBD_HIDE   -102

sKeydata* kbd_struct;
int keys;

sKeydata kbd_struct_800[] =
{
	/* first row */
	{0,AKEY_ESCAPE},{14,AKEY_1},{28,AKEY_2},{42,AKEY_3},
	{56,AKEY_4},{70,AKEY_5},{84,AKEY_6},{98,AKEY_7},
	{112,AKEY_8},{126,AKEY_9},{140,AKEY_0},{154,AKEY_LESS},
	{168,AKEY_GREATER},{182,AKEY_BACKSPACE},{196,AKEY_BREAK},{210,AKEY_HELP},
	/* second row */
	{240,AKEY_TAB},{261,AKEY_q},{275,AKEY_w},{289,AKEY_e},
	{303,AKEY_r},{317,AKEY_t},{331,AKEY_y},{345,AKEY_u},
	{359,AKEY_i},{373,AKEY_o},{387,AKEY_p},{401,AKEY_MINUS},
	{415,AKEY_EQUAL},{429,AKEY_RETURN},{450,AKEY_F4},
	/* third row */
	{480,AKEY_CTRL},{505,AKEY_a},{519,AKEY_s},{533,AKEY_d},
	{547,AKEY_f},{561,AKEY_g},{575,AKEY_h},{589,AKEY_j},
	{603,AKEY_k},{617,AKEY_l},{631,AKEY_SEMICOLON},{645,AKEY_PLUS},
	{659,AKEY_ASTERISK},{673,AKEY_CAPSTOGGLE},{690,AKEY_F3},
	/* fourth row */
	{720,AKEY_SHFT},{750,AKEY_z},{764,AKEY_x},{778,AKEY_c},
	{792,AKEY_v},{806,AKEY_b},{820,AKEY_n},{834,AKEY_m},
	{848,AKEY_COMMA},{862,AKEY_FULLSTOP},{876,AKEY_SLASH},{890,AKEY_SHFT},
	{916,AKEY_ATARI},{930,AKEY_F2},
	/* fifth row */
	{960,AKEY_NONE},{975,AKEY_NONE},{1002,AKEY_SPACE},{1126,AKEY_NONE},
	{1140,AKEY_UI},{1155,KBD_ROTATE},{1169,KBD_NEGATE},{1184,KBD_HIDE},
	{1200, AKEY_NONE}
};

sKeydata kbd_struct_5200[] =
{
	{0,AKEY_NONE},  {30,0x3f}, {44,0x3d}, {58,0x3b}, {72,AKEY_NONE},
	{90,0x39},{135,0x31},{181,0x29},{227,AKEY_NONE},
	{240,AKEY_NONE},{270,0x37},{284,0x35},{298,0x33},{312,AKEY_NONE},
	{480,AKEY_NONE},{510,0x2f},{524,0x2d},{538,0x2b},{552,AKEY_NONE},
	{720,AKEY_NONE},{750,0x27},{764,0x25},{778,0x23},{792,AKEY_NONE},
	{960,AKEY_NONE},{975,AKEY_NONE},{1140,AKEY_UI},{1155,KBD_ROTATE},
	{1169,KBD_NEGATE},{1184,KBD_HIDE},
	{1200,AKEY_NONE}
};

// Keyboard translation table
struct sKeyTranslation
{
	short winKey;
	short aKey;
};

sKeyTranslation kbd_translation[] =
{
// Mappable entries
	{0, AKEY_NONE}, // 5 joystick entries
	{0, AKEY_NONE},
	{0, AKEY_NONE},
	{0, AKEY_NONE},
	{0, AKEY_NONE},
	{0, AKEY_UI},
	{0, AKEY_F1},
	{0, AKEY_F2},
	{0, AKEY_F3},
	{0, AKEY_F4},
	{0, AKEY_HELP},
	{0, AKEY_WARMSTART},
	{0, AKEY_COLDSTART},
	{0, AKEY_BREAK},
	{VK_RETURN, AKEY_RETURN},
// Non-mappable entries
	{VK_SHIFT, AKEY_SHFT},
	{VK_CONTROL, AKEY_CTRL},
	{'0', AKEY_0},
	{'1', AKEY_1},
	{'2', AKEY_2},
	{'3', AKEY_3},
	{'4', AKEY_4},
	{'5', AKEY_5},
	{'6', AKEY_6},
	{'7', AKEY_7},
	{'8', AKEY_8},
	{'9', AKEY_9},
	{'A', AKEY_a},
	{'B', AKEY_b},
	{'C', AKEY_c},
	{'D', AKEY_d},
	{'E', AKEY_e},
	{'F', AKEY_f},
	{'G', AKEY_g},
	{'H', AKEY_h},
	{'I', AKEY_i},
	{'J', AKEY_j},
	{'K', AKEY_k},
	{'L', AKEY_l},
	{'M', AKEY_m},
	{'N', AKEY_n},
	{'O', AKEY_o},
	{'P', AKEY_p},
	{'Q', AKEY_q},
	{'R', AKEY_r},
	{'S', AKEY_s},
	{'T', AKEY_t},
	{'U', AKEY_u},
	{'V', AKEY_v},
	{'W', AKEY_w},
	{'X', AKEY_x},
	{'Y', AKEY_y},
	{'Z', AKEY_z},
	{VK_LWIN, AKEY_HELP},
	{VK_BACK, AKEY_BACKSPACE},
	{VK_ESCAPE, AKEY_ESCAPE},
	{VK_BACKQUOTE, AKEY_ATARI},
	{VK_CAPITAL, AKEY_CAPSTOGGLE},
	{VK_TAB, AKEY_TAB},
	{VK_SPACE, AKEY_SPACE},
	{VK_LBRACKET, AKEY_LESS},
	{VK_RBRACKET, AKEY_GREATER},
	{VK_EQUAL, AKEY_EQUAL},
	{VK_SUBTRACT, AKEY_MINUS},
	{VK_ADD, AKEY_PLUS},
	{VK_MULTIPLY, AKEY_ASTERISK},
	{VK_DIVIDE, AKEY_SLASH},
	{VK_SEMICOLON, AKEY_SEMICOLON},
	{VK_COMMA, AKEY_COMMA},
	{VK_PERIOD, AKEY_FULLSTOP},
	{VK_UP, AKEY_UP},
	{VK_DOWN, AKEY_DOWN},
	{VK_LEFT, AKEY_LEFT},
	{VK_RIGHT, AKEY_RIGHT},
	{VK_RETURN, AKEY_RETURN}		// fallthrough case
};

#define KBDT_TRIGGER	0
#define KBDT_JOYUP		1
#define KBDT_JOYDOWN	2
#define KBDT_JOYLEFT	3
#define KBDT_JOYRIGHT	4
#define KBDT_UI			5
#define KBDT_F1			6
#define KBDT_F2			7
#define KBDT_F3			8
#define KBDT_F4			9
#define KBDT_HELP		10
#define KBDT_WARMSTART	11
#define KBDT_COLDSTART	12
#define KBDT_BREAK		13
#define KBDT_RETURN		14


void reset_kbd()
{
	if(machine_type == MACHINE_5200)
	{
		kbd_image = kbd_image_5200;
		kbd_struct = kbd_struct_5200;
		keys = sizeof(kbd_struct_5200)/sizeof(kbd_struct_5200[0]);
	}
	else
	{
		kbd_image = kbd_image_800;
		kbd_struct = kbd_struct_800;
		keys = sizeof(kbd_struct_800)/sizeof(kbd_struct_800[0]);
	}
}

short get_keypress(short x, short y)
{
	int offset;
	int i;
	
	if(currentKeyboardMode == 4 && get_screen_mode())
		return AKEY_NONE;
	
	offset = x + (y/16)*240;
	if(offset < 0)
		return AKEY_NONE;
	
	for(i=1; i<keys; i++)
		if(offset < kbd_struct[i].offset)
			return kbd_struct[i-1].key;
		
		return AKEY_NONE;
}

static GXKeyList klist;

/* map joystick direction to expected key code by screen orientation */
short joykey_map[3][4];

int activeKey;
int activeMod;
int stick0;
int trig0;

void push_key(short akey);
void release_key(short akey);

void hitbutton(short code)
{
	int kbcode = AKEY_NONE;
	
	if(ui_is_active)
	{
		trig0 = 1;
		stick0 = 0xff;

		if(code == joykey_map[get_screen_mode()][0])
			kbcode = AKEY_UP;
		else if(code == joykey_map[get_screen_mode()][1])
			kbcode = AKEY_DOWN;
		else if(code == joykey_map[get_screen_mode()][2])
			kbcode = AKEY_LEFT;
		else if(code == joykey_map[get_screen_mode()][3])
			kbcode = AKEY_RIGHT;
		else if(code == klist.vkStart && !issmartphone)
			kbcode = AKEY_BACKSPACE;
		else if(code == klist.vkA)
			kbcode = AKEY_SPACE;
		else if(code == klist.vkB)
			kbcode = AKEY_RETURN;
		else if(code == klist.vkC)
			kbcode = AKEY_ESCAPE;
		else
			for(int i=0; i<sizeof(kbd_translation)/sizeof(kbd_translation[0]); i++)
				if(code == kbd_translation[i].winKey)
				{
					kbcode = kbd_translation[i].aKey;
					break;
				}
	}
	else
	{
		if(code == joykey_map[get_screen_mode()][0])
			stick0 &= ~1;
		else if(code == joykey_map[get_screen_mode()][1])
			stick0 &= ~2;
		else if(code == joykey_map[get_screen_mode()][2])
			stick0 &= ~4;
		else if(code == joykey_map[get_screen_mode()][3])
			stick0 &= ~8;
		else if(code == klist.vkA || code == klist.vkB || ((code == '4' || code == '6') && issmartphone))
			trig0 = 0;
		else if(code == klist.vkC)
		{
			if (!kbui_timerset)
			{
				SetTimer(hWndMain, 1, 1000, NULL);
				kbui_timerset = TRUE;
			}
		}
		else if ((code == VK_F3) && (issmartphone))
			set_screen_mode(get_screen_mode()+1); 
		else
			for(int i=0; i<sizeof(kbd_translation)/sizeof(kbd_translation[0]); i++)
				if(code == kbd_translation[i].winKey)
				{
					kbcode = kbd_translation[i].aKey;
					break;
				}
	}
	if(kbcode != AKEY_NONE)
		push_key(kbcode);
}

void releasebutton(short code)
{
	int kbcode = AKEY_NONE, temp_ui = -1;
	
	if(ui_is_active)
		release_key(kbcode);
	else
	{
		if(code == joykey_map[get_screen_mode()][0])
			stick0 |= 1;
		else if(code == joykey_map[get_screen_mode()][1])
			stick0 |= 2;
		else if(code == joykey_map[get_screen_mode()][2])
			stick0 |= 4;
		else if(code == joykey_map[get_screen_mode()][3])
			stick0 |= 8;
		else if(code == klist.vkA || code == klist.vkB || ((code == '4' || code == '6') && issmartphone))
			trig0 = 1;
		else if(code == klist.vkC)
			if (kbui_timerset)
			{
				KillTimer(hWndMain, 1);
				kbui_timerset = FALSE;
				set_screen_mode(0);
				kbcode = AKEY_UI;
				push_key(kbcode);
				ui_clearkey = TRUE;
				return;
			}
	}

	release_key(kbcode);		// always release or the ui gets stuck
}

void Start_KBUI(void)
{
	int kbcode, temp_ui;
	KillTimer(hWndMain, 1);
	kbui_timerset = FALSE;
	temp_ui = ui_is_active;
	ui_is_active = TRUE;
	kbcode = kb_ui("Select Atari key to inject once", machine_type);
	if (kbcode != AKEY_NONE)
		push_key(kbcode);
	else
		release_key(AKEY_NONE);
	ui_is_active = temp_ui;
	return;
}

void tapscreen(short x, short y)
{
	short kbcode;
	
	stylus_down = 1;

	/* On-screen joystick */
	if(virtual_joystick && !ui_is_active && currentKeyboardMode == 4 && y < 240)
	{
		stick0 = 0xff;
		trig0 = 1;
		if(y < 90) /* up */
			stick0 &= ~1;
		if(y >= 150) /* down */
			stick0 &= ~2;
		if(x < 120) /* left */
			stick0 &= ~4;
		if(x >= 200) /* right */
			stick0 &= ~8;
		if(x >= 60 && x < 260 && y >= 45 && y < 195)
			trig0 = 0;
		return;
	}

	translate_kbd(&x, &y);
	
	/* In landscape - show keyboard if clicked bottom right corner */
	if(get_screen_mode() && currentKeyboardMode == 4 && x > 300 && y > 220)
		return;
	
	kbcode = get_keypress(x, y);
	
	/* Special keys */
	switch(kbcode)
	{
	case KBD_ROTATE:
	case KBD_NEGATE:
	case KBD_HIDE:
		return;
	}
	
	/* The way current UI works, it is not compatible with keyboard implementation
	in landscape mode */
	if(kbcode == AKEY_UI)
		set_screen_mode(0);
	
	/* Special translation to make on-screen UI easier to use */
	if(ui_is_active)
	{
		if(machine_type == MACHINE_5200)
		{
			switch(kbcode)
			{
			case 0x3d: /* 2 */
				kbcode = AKEY_UP;
				break;
			case 0x2d: /* 8 */
				kbcode = AKEY_DOWN;
				break;
			case 0x37: /* 4 */
				kbcode = AKEY_LEFT;
				break;
			case 0x33: /* 6 */
				kbcode = AKEY_RIGHT;
				break;
			case 0x39: /* Start */
				kbcode = AKEY_RETURN;
				break;
			case 0x27: /* * */
				kbcode = AKEY_ESCAPE;
				break;
			case 0x31: /* Pause */
				kbcode = AKEY_TAB;
				break;
			case 0x23: /* # */
				kbcode = AKEY_SPACE;
				break;
			}
		}
		else
		{
			switch(kbcode)
			{
			case AKEY_MINUS:
				kbcode = AKEY_UP;
				break;
			case AKEY_EQUAL:
				kbcode = AKEY_DOWN;
				break;
			case AKEY_PLUS:
				kbcode = AKEY_LEFT;
				break;
			case AKEY_ASTERISK:
				kbcode = AKEY_RIGHT;
				break;
			}
		}
	}

	push_key(kbcode);
}

void untapscreen(short x, short y)
{
	int kbcode;

	stylus_down = 0;

	/* On-screen joystick */
	if(virtual_joystick && !ui_is_active && currentKeyboardMode == 4 && y < 240)
	{
		stick0 = 0xff;
		trig0 = 1;
		return;
	}
	
	translate_kbd(&x, &y);
	
	/* Special tricks */
	/* In landscape - show keyboard if clicked bottom right corner */
	if(get_screen_mode() && currentKeyboardMode == 4)
	{
		if(x > 300 && y > 220 && !ui_is_active)
			currentKeyboardMode = 3;
		else if(ui_is_active)
			set_screen_mode(0);
		return;
	}
	
	/* In landscape -- move keyboard if clicked outside of it */
	int newKeyboardMode = currentKeyboardMode;
	if(get_screen_mode() && currentKeyboardMode != 4)
	{
		if(x<0 || x>=240)
			newKeyboardMode = newKeyboardMode ^ 1;
		if(y<0 || y>=80)
			newKeyboardMode = newKeyboardMode ^ 2;
	}
	if(newKeyboardMode != currentKeyboardMode)
	{
		currentKeyboardMode = newKeyboardMode;
		return;
	}
	
	kbcode = get_keypress(x, y);
	
	/* Special keys */
	switch(kbcode)
	{
	case KBD_ROTATE:
		if(!ui_is_active)
			set_screen_mode(get_screen_mode()+1);
		return;
	case KBD_NEGATE:
		currentKeyboardColor = 1-currentKeyboardColor;
		return;
	case KBD_HIDE:
		if(get_screen_mode())
			currentKeyboardMode = 4;
		return;
	}
	
	/* Special translation to make on-screen UI easier to use */
	if(ui_is_active)
	{
		if(machine_type == MACHINE_5200)
		{
			switch(kbcode)
			{
			case 0x3d: /* 2 */
				kbcode = AKEY_UP;
				break;
			case 0x2d: /* 8 */
				kbcode = AKEY_DOWN;
				break;
			case 0x37: /* 4 */
				kbcode = AKEY_LEFT;
				break;
			case 0x33: /* 6 */
				kbcode = AKEY_RIGHT;
				break;
			case 0x39: /* Start */
				kbcode = AKEY_RETURN;
				break;
			case 0x27: /* * */
				kbcode = AKEY_ESCAPE;
				break;
			case 0x31: /* Pause */
				kbcode = AKEY_TAB;
				break;
			case 0x23: /* # */
				kbcode = AKEY_SPACE;
				break;
			}
		}
		else
		{
			switch(kbcode)
			{
			case AKEY_MINUS:
				kbcode = AKEY_UP;
				break;
			case AKEY_EQUAL:
				kbcode = AKEY_DOWN;
				break;
			case AKEY_PLUS:
				kbcode = AKEY_LEFT;
				break;
			case AKEY_ASTERISK:
				kbcode = AKEY_RIGHT;
				break;
			}
		}
	}

	release_key(kbcode);
}

void dragscreen(short x, short y)
{
	if(stylus_down && virtual_joystick && !ui_is_active && currentKeyboardMode == 4 && y < 240)
	{
		translate_kbd(&x, &y);
		
		/* On-screen joystick trigger (in portrait or in landscape if no keyboard */
		if(virtual_joystick && currentKeyboardMode == 4)
		{
			stick0 = 0xff;
			trig0 = 1;
			if(y < 90) /* up */
				stick0 &= ~1;
			if(y >= 150) /* down */
				stick0 &= ~2;
			if(x < 120) /* left */
				stick0 &= ~4;
			if(x >= 200) /* right */
				stick0 &= ~8;
			if(x >= 60 && x < 260 && y >= 45 && y < 195)
				trig0 = 0;
			return;
		}
	}
}

void push_key(short akey)
{
	switch(akey)
	{
	case AKEY_NONE:
	case AKEY_UI:
		activeKey = akey;
		activeMod = 0;
		break;
	case AKEY_SHFT:
		activeMod ^= 0x40;
		break;
	case AKEY_CTRL:
		activeMod ^= 0x80;
		break;
	case AKEY_F2:
	case AKEY_OPTION:
		key_consol &= ~CONSOL_OPTION;
		break;
	case AKEY_F3:
	case AKEY_SELECT:
		key_consol &= ~CONSOL_SELECT;
		break;
	case AKEY_F4:
	case AKEY_START:
		key_consol &= ~CONSOL_START;
		break;
	default:
		activeKey = akey|activeMod;
		activeMod = 0;
		key_shift = (activeKey & AKEY_SHFT) || (activeKey & AKEY_SHFTCTRL);
	}
}

void release_key(short akey)
{
	activeKey = AKEY_NONE;
	key_consol = CONSOL_NONE;
	key_shift = 0;
}

int get_last_key()
{
	if (ui_clearkey)
	{
		ui_clearkey = FALSE;
		release_key(AKEY_UI);
		return AKEY_UI;
	}
	else if(activeKey != AKEY_BREAK)
		return activeKey;
	else
	{
		/* There is special case */
		release_key(AKEY_BREAK);
		return AKEY_BREAK;
	}
}

int prockb(void)
{
	static int framectr = 0;
#ifndef MULTITHREADED
	MsgPump();
#endif
/* UI.C violates the rest of architecture */
	if(ui_is_active && framectr++ == 5)
	{
		reset_kbd();
		refresh_kbd();
		framectr = 0;
	}
	return 0;
}

void uninitinput(void)
{
	/* To prevent start key leak to OS */
	while(GetAsyncKeyState(klist.vkStart) & 0x8000)
#ifndef MULTITHREADED
		MsgPump();
#else
		Sleep(10);
#endif
	GXCloseInput();
}

int initinput(void)
{
	GXOpenInput();
	klist = GXGetDefaultKeys(GX_NORMALKEYS);
	
	joykey_map[0][0] = klist.vkUp;
	joykey_map[0][1] = klist.vkDown;
	joykey_map[0][2] = klist.vkLeft;
	joykey_map[0][3] = klist.vkRight;
	
	joykey_map[1][0] = klist.vkLeft;
	joykey_map[1][1] = klist.vkRight;
	joykey_map[1][2] = klist.vkDown;
	joykey_map[1][3] = klist.vkUp;
	
	joykey_map[2][0] = klist.vkRight;
	joykey_map[2][1] = klist.vkLeft;
	joykey_map[2][2] = klist.vkUp;
	joykey_map[2][3] = klist.vkDown;

	if (smkeyhack)
	{
		klist.vkB ^= klist.vkC;
		klist.vkC ^= klist.vkB;
		klist.vkB ^= klist.vkC;
	}

	if (issmartphone)
	{
		kbd_translation[KBDT_F3].winKey = '8';
		kbd_translation[KBDT_F2].winKey = '7';
		kbd_translation[KBDT_UI].winKey = klist.vkC;
		kbd_translation[KBDT_F4].winKey = '9';
		kbd_translation[KBDT_RETURN].winKey = '0';
 
	}
	else
	{
		kbd_translation[KBDT_F3].winKey = klist.vkA;
		kbd_translation[KBDT_F2].winKey = klist.vkB;
		kbd_translation[KBDT_UI].winKey = klist.vkC;
	}

	kbd_image = kbd_image_800;
	kbd_struct = kbd_struct_800;
	keys = sizeof(kbd_struct_800)/sizeof(kbd_struct_800[0]);

	clearkb();

	stylus_down = 0;

	return 0;
}

void clearkb(void)
{
	activeKey = AKEY_NONE;
	activeMod = 0;
	key_consol = CONSOL_NONE;
	stick0 = 0xff;
	trig0 = 1;
}
