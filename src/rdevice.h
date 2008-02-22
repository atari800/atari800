/*
 * rdevice.h - Atari850 emulation header file
 *
 * Copyright (c) ???? Tom Hunt, Chris Martin
 * Copyright (c) 2003,2008 Atari800 development team (see DOC/CREDITS)
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

#ifndef _RDEVICE_H_
#define _RDEVICE_H_

extern void Device_ROPEN(void);
extern void Device_RCLOS(void);
extern void Device_RREAD(void);
extern void Device_RWRIT(void);
extern void Device_RSTAT(void);
extern void Device_RSPEC(void);
extern void Device_RINIT(void);

extern int r_serial;
extern char r_device[];

#endif /* _RDEVICE_H_ */
