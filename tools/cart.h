/*
 * cart.h
 *
 * Copyright (C) 2001-2010 Piotr Fusik
 * Copyright (C) 2001-2020 Atari800 development team (see DOC/CREDITS)
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

#ifndef CART_H_
#define CART_H_

#define MIN_CARTTYPE 1
#define MAX_CARTTYPE 70
#define URL "https://github.com/atari800/atari800/blob/master/DOC/cart.txt"
#define MIN_SIZE 2048		/* Minimum cartridge size */

long checklen(FILE *fp, int carttype);
void convert(char *romfile, char *cartfile, int32_t type);

typedef struct {
  uint8_t cart[4];
  uint8_t type[4];
  uint8_t csum[4];
  uint8_t zero[4];
} header_t;

#endif	/* CART_H_ */
