/*
 * ui_basic.c - main user interface
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* free() */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* getcwd() */
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h> /* getcwd on MSVC*/
#endif
/* XXX: <sys/dir.h>, <ndir.h>, <sys/ndir.h> */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif

#include "antic.h"
#include "atari.h"
#include "input.h"
#include "log.h"
#include "memory.h"
#include "platform.h"
#include "screen.h" /* atari_screen */
#include "ui.h"
#include "util.h"

#ifdef USE_CURSES
void curses_clear_screen(void);
void curses_clear_rectangle(int x1, int y1, int x2, int y2);
void curses_putch(int x, int y, int ascii, UBYTE fg, UBYTE bg);
#endif

static int initialised = FALSE;
static UBYTE charset[1024];

static const unsigned char key_to_ascii[256] =
{
	0x6C, 0x6A, 0x3B, 0x00, 0x00, 0x6B, 0x2B, 0x2A, 0x6F, 0x00, 0x70, 0x75, 0x9B, 0x69, 0x2D, 0x3D,
	0x76, 0x00, 0x63, 0x00, 0x00, 0x62, 0x78, 0x7A, 0x34, 0x00, 0x33, 0x36, 0x1B, 0x35, 0x32, 0x31,
	0x2C, 0x20, 0x2E, 0x6E, 0x00, 0x6D, 0x2F, 0x00, 0x72, 0x00, 0x65, 0x79, 0x7F, 0x74, 0x77, 0x71,
	0x39, 0x00, 0x30, 0x37, 0x7E, 0x38, 0x3C, 0x3E, 0x66, 0x68, 0x64, 0x00, 0x00, 0x67, 0x73, 0x61,

	0x4C, 0x4A, 0x3A, 0x00, 0x00, 0x4B, 0x5C, 0x5E, 0x4F, 0x00, 0x50, 0x55, 0x9B, 0x49, 0x5F, 0x7C,
	0x56, 0x00, 0x43, 0x00, 0x00, 0x42, 0x58, 0x5A, 0x24, 0x00, 0x23, 0x26, 0x1B, 0x25, 0x22, 0x21,
	0x5B, 0x20, 0x5D, 0x4E, 0x00, 0x4D, 0x3F, 0x00, 0x52, 0x00, 0x45, 0x59, 0x9F, 0x54, 0x57, 0x51,
	0x28, 0x00, 0x29, 0x27, 0x9C, 0x40, 0x7D, 0x9D, 0x46, 0x48, 0x44, 0x00, 0x00, 0x47, 0x53, 0x41,

	0x0C, 0x0A, 0x7B, 0x00, 0x00, 0x0B, 0x1E, 0x1F, 0x0F, 0x00, 0x10, 0x15, 0x9B, 0x09, 0x1C, 0x1D,
	0x16, 0x00, 0x03, 0x00, 0x00, 0x02, 0x18, 0x1A, 0x00, 0x00, 0x9B, 0x00, 0x1B, 0x00, 0xFD, 0x00,
	0x00, 0x20, 0x60, 0x0E, 0x00, 0x0D, 0x00, 0x00, 0x12, 0x00, 0x05, 0x19, 0x9E, 0x14, 0x17, 0x11,
	0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x7D, 0xFF, 0x06, 0x08, 0x04, 0x00, 0x00, 0x07, 0x13, 0x01,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define KB_DELAY       20
#define KB_AUTOREPEAT  3

static int GetKeyPress(void)
{
	int keycode;

#ifdef SVGA_SPEEDUP
	int i;
	for (i = 0; i < refresh_rate; i++)
#endif
		Atari_DisplayScreen();

	for (;;) {
		static int rep = KB_DELAY;
		if (Atari_Keyboard() == AKEY_NONE) {
			rep = KB_DELAY;
			break;
		}
		if (rep == 0) {
			rep = KB_AUTOREPEAT;
			break;
		}
		rep--;
		atari_sync();
	}

	do {
		atari_sync();
		keycode = Atari_Keyboard();
		switch (keycode) {
		case AKEY_WARMSTART:
			alt_function = MENU_RESETW;
			return 0x1b; /* escape */
		case AKEY_COLDSTART:
			alt_function = MENU_RESETC;
			return 0x1b; /* escape */
		case AKEY_EXIT:
			alt_function = MENU_EXIT;
			return 0x1b; /* escape */
		case AKEY_UI:
			if (alt_function >= 0) /* Alt+letter, not F1 */
				return 0x1b; /* escape */
			break;
		case AKEY_SCREENSHOT:
			alt_function = MENU_PCX;
			return 0x1b; /* escape */
		case AKEY_SCREENSHOT_INTERLACE:
			alt_function = MENU_PCXI;
			return 0x1b; /* escape */
		default:
			break;
		}
	} while (keycode < 0);

	return key_to_ascii[keycode];
}

static void Plot(int fg, int bg, int ch, int x, int y)
{
#ifdef USE_CURSES
	curses_putch(x, y, ch, (UBYTE) fg, (UBYTE) bg);
#else /* USE_CURSES */
	const UBYTE *font_ptr = charset + (ch & 0x7f) * 8;
	UBYTE *ptr = (UBYTE *) atari_screen + 24 * ATARI_WIDTH + 32 + y * (8 * ATARI_WIDTH) + x * 8;
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		UBYTE data = *font_ptr++;
		for (j = 0; j < 8; j++) {
#ifdef USE_COLOUR_TRANSLATION_TABLE
			video_putbyte(ptr++, (UBYTE) colour_translation_table[data & 0x80 ? fg : bg]);
#else
			video_putbyte(ptr++, (UBYTE) (data & 0x80 ? fg : bg));
#endif
			data <<= 1;
		}
		ptr += ATARI_WIDTH - 8;
	}
#endif /* USE_CURSES */
}

static void Print(int fg, int bg, const char *string, int x, int y, int maxwidth)
{
	char tmpbuf[40];
	if ((int) strlen(string) > maxwidth) {
		int firstlen = (maxwidth - 3) >> 1;
		int laststart = strlen(string) - (maxwidth - 3 - firstlen);
		sprintf(tmpbuf, "%.*s...%s", firstlen, string, string + laststart);
		string = tmpbuf;
	}
	while (*string != '\0') {
		Plot(fg, bg, *string++, x, y);
		x++;
	}
}

static void CenterPrint(int fg, int bg, const char *string, int y)
{
	int length = strlen(string);
	Print(fg, bg, string, (length < 38) ? (40 - length) >> 1 : 1, y, 38);
}

static void Box(int fg, int bg, int x1, int y1, int x2, int y2)
{
	int x;
	int y;

	for (x = x1 + 1; x < x2; x++) {
		Plot(fg, bg, 18, x, y1);
		Plot(fg, bg, 18, x, y2);
	}

	for (y = y1 + 1; y < y2; y++) {
		Plot(fg, bg, 124, x1, y);
		Plot(fg, bg, 124, x2, y);
	}

	Plot(fg, bg, 17, x1, y1);
	Plot(fg, bg, 5, x2, y1);
	Plot(fg, bg, 3, x2, y2);
	Plot(fg, bg, 26, x1, y2);
}

static void ClearRectangle(int bg, int x1, int y1, int x2, int y2)
{
#ifdef USE_CURSES
	curses_clear_rectangle(x1, y1, x2, y2);
#else
	UBYTE *ptr = (UBYTE *) atari_screen + ATARI_WIDTH * 24 + 32 + x1 * 8 + y1 * (ATARI_WIDTH * 8);
	int bytesperline = (x2 - x1 + 1) << 3;
	UBYTE *end_ptr = (UBYTE *) atari_screen + ATARI_WIDTH * 32 + 32 + y2 * (ATARI_WIDTH * 8);
	while (ptr < end_ptr) {
#ifdef USE_COLOUR_TRANSLATION_TABLE
		video_memset(ptr, (UBYTE) colour_translation_table[bg], bytesperline);
#else
		video_memset(ptr, (UBYTE) bg, bytesperline);
#endif
		ptr += ATARI_WIDTH;
	}
#endif /* USE_CURSES */
}

static void ClearScreen(void)
{
#ifdef USE_CURSES
	curses_clear_screen();
#else
#ifdef USE_COLOUR_TRANSLATION_TABLE
	video_memset((UBYTE *) atari_screen, colour_translation_table[0x00], ATARI_HEIGHT * ATARI_WIDTH);
#else
	video_memset((UBYTE *) atari_screen, 0x00, ATARI_HEIGHT * ATARI_WIDTH);
#endif
	ClearRectangle(0x94, 0, 0, 39, 23);
#endif /* USE_CURSES */
}

static void TitleScreen(const char *title)
{
	CenterPrint(0x9a, 0x94, title, 0);
}

void BasicUIMessage(const char *msg)
{
	CenterPrint(0x94, 0x9a, msg, 22);
	GetKeyPress();
}

static void SelectItem(int fg, int bg, int index, const char *items[],
                       const char *prefix[], const char *suffix[],
                       int nrows, int ncolumns,
                       int xoffset, int yoffset,
                       int itemwidth, int active)
{
	int x;
	int y;
	int iMaxXsize = ((40 - xoffset) / ncolumns) - 1;
	char szbuf[FILENAME_MAX + 40]; /* allow for prefix and suffix */
	char *p = szbuf;

	x = index / nrows;
	y = index - (x * nrows);

	x *= (itemwidth + 1);

	x += xoffset;
	y += yoffset;

	if (prefix != NULL && prefix[index] != NULL)
		p = Util_stpcpy(szbuf, prefix[index]);
	p = Util_stpcpy(p, items[index]);
	if (suffix != NULL && suffix[index] != NULL) {
		char *q = szbuf + itemwidth - strlen(suffix[index]);
		while (p < q)
			*p++ = ' ';
		strcpy(p, suffix[index]);
	}
	else {
		while (p < szbuf + itemwidth)
			*p++ = ' ';
		*p = '\0';
	}

	Print(fg, bg, szbuf, x, y, iMaxXsize);

	if (active) {
		ClearRectangle(0x94, 1, 22, 38, 22);
		if (iMaxXsize < 38 && (int) strlen(szbuf) > iMaxXsize)
			/* the selected item was shortened */
			CenterPrint(fg, bg, szbuf, 22);
	}
}

static int Select(int default_item, int nitems, const char *items[],
                  const char *prefix[], const char *suffix[],
                  int nrows, int ncolumns,
                  int xoffset, int yoffset,
                  int itemwidth, int scrollable, int *ascii)
{
	int index;
	int localascii;

	if (ascii == NULL)
		ascii = &localascii;

	ClearRectangle(0x94, xoffset, yoffset, xoffset + ncolumns * (itemwidth + 1) - 2, yoffset + nrows - 1);

	for (index = 0; index < nitems; index++)
		SelectItem(0x9a, 0x94, index, items, prefix, suffix, nrows, ncolumns, xoffset, yoffset, itemwidth, FALSE);

	index = default_item;
	SelectItem(0x94, 0x9a, index, items, prefix, suffix, nrows, ncolumns, xoffset, yoffset, itemwidth, TRUE);

	for (;;) {
		int row;
		int column;
		int new_index;

		column = index / nrows;
		row = index - (column * nrows);

		*ascii = GetKeyPress();
		switch (*ascii) {
		case 0x1c:				/* Up */
			if (row > 0)
				row--;
			else if (column > 0) {
				column--;
				row = nrows - 1;
			}
			else if (scrollable)
				return index + nitems + (nrows - 1);
			break;
		case 0x1d:				/* Down */
			if (row < (nrows - 1))
				row++;
			else if (column < (ncolumns - 1)) {
				row = 0;
				column++;
			}
			else if (scrollable)
				return index + nitems * 2 - (nrows - 1);
			break;
		case 0x1e:				/* Left */
			if (column > 0)
				column--;
			else if (scrollable)
				return index + nitems;
			break;
		case 0x1f:				/* Right */
			if (column < (ncolumns - 1))
				column++;
			else if (scrollable)
				return index + nitems * 2;
			break;
		case 0x7f:				/* Tab (for exchanging disk directories) */
			return -2;			/* GOLDA CHANGED */
		case 0x20:				/* Space */
		case 0x7e:				/* Backspace */
		case 0x9b:				/* Return=Select */
			return index;
		case 0x1b:				/* Esc=Cancel */
			return -1;
		default:
			break;
		}

		new_index = (column * nrows) + row;
		if ((new_index >= 0) && (new_index < nitems)) {
			SelectItem(0x9a, 0x94, index, items, prefix, suffix, nrows, ncolumns, xoffset, yoffset, itemwidth, FALSE);

			index = new_index;
			SelectItem(0x94, 0x9a, index, items, prefix, suffix, nrows, ncolumns, xoffset, yoffset, itemwidth, TRUE);
		}
	}
}

int BasicUISelect(const char *title, int subitem, int default_item, tMenuItem *items, int *seltype)
{
	int nitems;
	int retval;
	const tMenuItem *pitem;
	const char *prefix[100];
	const char *root[100];
	const char *suffix[100];
	int w;
	int x1, y1, x2, y2;
	int scrollable;
	int ascii;

	nitems = 0;
	retval = 0;
	for (pitem = items; pitem->sig[0] != '\0'; pitem++) {
		if (pitem->flags & ITEM_ENABLED) {
			prefix[nitems] = pitem->prefix;
			root[nitems] = pitem->item;
			if (pitem->flags & ITEM_CHECK) {
				if (pitem->flags & ITEM_CHECKED)
					suffix[nitems] = "Yes";
				else
					suffix[nitems] = "No ";
			}
			else
				suffix[nitems] = pitem->suffix;
			if (pitem->retval == default_item)
				retval = nitems;
			nitems++;
		}
	}

	if (nitems == 0)
		return -1; /* cancel immediately */

	if (subitem) {
		int i;
		w = 0;
		for (i = 0; i < nitems; i++) {
			int ws = strlen(root[i]);
			if (prefix[i] != NULL)
				ws += strlen(prefix[i]);
			if (suffix[i] != NULL)
				ws += strlen(suffix[i]);
			if (ws > w)
				w = ws;
		}
		if (w > 38)
			w = 38;

		x1 = (40 - w) / 2 - 1;
		x2 = x1 + w + 1;
		y1 = (24 - nitems) / 2 - 1;
		y2 = y1 + nitems + 1;
	}
	else {
		ClearScreen();
		TitleScreen(title);
		w = 38;
		x1 = 0;
		y1 = 1;
		x2 = 39;
		y2 = 23;
	}

	Box(0x9a, 0x94, x1, y1, x2, y2);
	scrollable = (nitems > y2 - y1 - 1);
	retval = Select(retval, nitems, root, prefix, suffix, nitems, 1, x1 + 1, y1 + 1, w, scrollable, &ascii);
	if (retval < 0)
		return retval;
	for (pitem = items; pitem->sig[0] != '\0'; pitem++) {
		if (pitem->flags & ITEM_ENABLED) {
			if (retval == 0) {
				if ((pitem->flags & ITEM_MULTI) && seltype) {
					switch (ascii) {
					case 0x9b:
						*seltype = USER_SELECT;
						break;
					case 0x20:
						*seltype = USER_TOGGLE;
						break;
					default:
						*seltype = USER_OTHER;
						break;
					}
				}
				return pitem->retval;
			}
			retval--;
		}
	}
	return 0;
}

int BasicUISelectInt(int default_value, int min_value, int max_value)
{
	static char item_values[100][4];
	static char *items[100];
	int value;
	int nitems;
	int nrows;
	int ncolumns;
	int x1, y1, x2, y2;
	if (min_value < 0 || max_value > 99 || min_value > max_value)
		return default_value;
	nitems = 0;
	for (value = min_value; value <= max_value; value++) {
		items[nitems] = item_values[nitems];
		sprintf(item_values[nitems], "%2d", value);
		nitems++;
	}
	if (nitems <= 10) {
		nrows = nitems;
		ncolumns = 1;
	}
	else {
		nrows = 10;
		ncolumns = (nitems + 9) / 10;
	}
	x1 = (39 - 3 * ncolumns) >> 1;
	y1 = (22 - nrows) >> 1;
	x2 = x1 + 3 * ncolumns;
	y2 = y1 + nrows + 1;
	Box(0x9a, 0x94, x1, y1, x2, y2);
	value = Select(default_value >= min_value && default_value <= max_value ? default_value - min_value : 0,
		nitems, (const char **) items, NULL, NULL, nrows, ncolumns, x1 + 1, y1 + 1, 2, FALSE, NULL);
	return value >= 0 ? value + min_value : default_value;
}

#ifdef WIN32

static WIN32_FIND_DATA wfd;
static HANDLE dh = INVALID_HANDLE_VALUE;

#ifdef _WIN32_WCE
/* WinCE's FindFirstFile/FindNext file don't return "." or "..". */
/* We check if the parent folder exists and add ".." if necessary. */
static char parentdir[FILENAME_MAX];
#endif

static int BasicUIOpenDir(const char *dirname)
{
#ifdef UNICODE
	WCHAR wfilespec[FILENAME_MAX];
	if (MultiByteToWideChar(CP_ACP, 0, dirname, -1, wfilespec, FILENAME_MAX - 4) <= 0)
		return FALSE;
	wcscat(wfilespec, (dirname[0] != '\0' && dirname[strlen(dirname) - 1] != '\\')
		? L"\\*.*" : L"*.*");
	dh = FindFirstFile(wfilespec, &wfd);
#else /* UNICODE */
	char filespec[FILENAME_MAX];
	Util_strlcpy(filespec, dirname, FILENAME_MAX - 4);
	strcat(filespec, (dirname[0] != '\0' && dirname[strlen(dirname) - 1] != '\\')
		? "\\*.*" : "*.*");
	dh = FindFirstFile(filespec, &wfd);
#endif /* UNICODE */
#ifdef _WIN32_WCE
	Util_splitpath(dirname, parentdir, NULL);
#endif
	return dh != INVALID_HANDLE_VALUE;
}

static int BasicUIReadDir(char *filename, int *isdir)
{
	if (dh == INVALID_HANDLE_VALUE) {
#ifdef _WIN32_WCE
		if (parentdir[0] != '\0' && Util_direxists(parentdir)) {
			strcpy(filename, "..");
			*isdir = TRUE;
			parentdir[0] = '\0';
			return TRUE;
		}
#endif /* _WIN32_WCE */
		return FALSE;
	}
#ifdef UNICODE
	if (WideCharToMultiByte(CP_ACP, 0, wfd.cFileName, -1, filename, FILENAME_MAX, NULL, NULL) <= 0)
		filename[0] = '\0';
#else
	Util_strlcpy(filename, wfd.cFileName, FILENAME_MAX);
#endif /* UNICODE */
#ifdef _WIN32_WCE
	/* just in case they will implement it some day */
	if (strcmp(filename, "..") == 0)
		parentdir[0] = '\0';
#endif
	*isdir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
	if (!FindNextFile(dh, &wfd)) {
		FindClose(dh);
		dh = INVALID_HANDLE_VALUE;
	}
	return TRUE;
}

#define DO_DIR

#elif defined(HAVE_OPENDIR)

static char dir_path[FILENAME_MAX];
static DIR *dp = NULL;

static int BasicUIOpenDir(const char *dirname)
{
	Util_strlcpy(dir_path, dirname, FILENAME_MAX);
	dp = opendir(dir_path);
	return dp != NULL;
}

static int BasicUIReadDir(char *filename, int *isdir)
{
	struct dirent *entry;
	char fullfilename[FILENAME_MAX];
	struct stat st;
	entry = readdir(dp);
	if (entry == NULL) {
		closedir(dp);
		dp = NULL;
		return FALSE;
	}
	strcpy(filename, entry->d_name);
	Util_catpath(fullfilename, dir_path, entry->d_name);
	stat(fullfilename, &st);
	*isdir = (st.st_mode & S_IFDIR) ? TRUE : FALSE;
	return TRUE;
}

#define DO_DIR

#endif /* defined(HAVE_OPENDIR) */

#ifdef DO_DIR

static char **filenames;
#define FILENAMES_INITIAL_SIZE 256 /* preallocate 1 KB */
static int n_filenames;

/* filename must be malloc'ed or strdup'ed */
static void FilenamesAdd(char *filename)
{
	if (n_filenames >= FILENAMES_INITIAL_SIZE && (n_filenames & (n_filenames - 1)) == 0) {
		/* n_filenames is a power of two: allocate twice as much */
		filenames = (char **) Util_realloc(filenames, 2 * n_filenames * sizeof(char *));
	}
	filenames[n_filenames++] = filename;
}

static int FilenamesCmp(const char *filename1, const char *filename2)
{
	if (filename1[0] == '[') {
		if (filename2[0] != '[')
			return -1;
		if (filename1[1] == '.') {
			if (filename2[1] != '.')
				return -1;
			/* return Util_stricmp(filename1, filename2); */
		}
		else if (filename2[1] == '.')
			return 1;
		/* return Util_stricmp(filename1, filename2); */
	}
	else if (filename2[0] == '[')
		return 1;
	return Util_stricmp(filename1, filename2);
}

/* quicksort */
static void FilenamesSort(const char **start, const char **end)
{
	while (start + 1 < end) {
		const char **left = start + 1;
		const char **right = end;
		const char *pivot = *start;
		const char *tmp;
		while (left < right) {
			if (FilenamesCmp(*left, pivot) <= 0)
				left++;
			else {
				right--;
				tmp = *left;
				*left = *right;
				*right = tmp;
			}
		}
		left--;
		tmp = *left;
		*left = *start;
		*start = tmp;
		FilenamesSort(start, left);
		start = right;
	}
}

static void FilenamesFree(void)
{
	while (n_filenames > 0)
		free(filenames[--n_filenames]);
	free(filenames);
}

static void GetDirectory(const char *directory)
{
#ifdef __DJGPP__
	unsigned short s_backup = _djstat_flags;
	_djstat_flags = _STAT_INODE | _STAT_EXEC_EXT | _STAT_EXEC_MAGIC | _STAT_DIRSIZE |
		_STAT_ROOT_TIME | _STAT_WRITEBIT;
	/* we do not need any of those 'hard-to-get' informations */
#endif	/* DJGPP */

	filenames = (char **) Util_malloc(FILENAMES_INITIAL_SIZE * sizeof(char *));
	n_filenames = 0;

	if (BasicUIOpenDir(directory)) {
		char filename[FILENAME_MAX];
		int isdir;

		while (BasicUIReadDir(filename, &isdir)) {
			char *filename2;

			if (filename[0] == '\0' ||
				(filename[0] == '.' && filename[1] == '\0'))
				continue;

			if (isdir) {
				/* add directories as [dir] */
				size_t len = strlen(filename);
				filename2 = (char *) Util_malloc(len + 3);
				memcpy(filename2 + 1, filename, len);
				filename2[0] = '[';
				filename2[len + 1] = ']';
				filename2[len + 2] = '\0';
			}
			else
				filename2 = Util_strdup(filename);

			FilenamesAdd(filename2);
		}

		FilenamesSort((const char **) filenames, (const char **) filenames + n_filenames);
	}
	else {
		Aprint("Error opening '%s' directory", directory);
	}
#ifdef DOS_DRIVES
	/* in DOS/Windows, add all existing disk letters */
	{
		char letter;
		for (letter = 'A'; letter <= 'Z'; letter++) {
#ifdef __DJGPP__
			static char drive[3] = "C:";
			struct stat st;
			drive[0] = letter;
			/* don't check floppies - it's slow */
			if (letter < 'C' || (stat(drive, &st) == 0 && (st.st_mode & S_IXUSR) != 0))
#elif defined(WIN32)
#ifdef UNICODE
			static WCHAR rootpath[4] = L"C:\\";
#else
			static char rootpath[4] = "C:\\";
#endif
			rootpath[0] = letter;
			if (GetDriveType(rootpath) != DRIVE_NO_ROOT_DIR)
#endif /* defined(WIN32) */
			{
				static char drive2[5] = "[C:]";
				drive2[1] = letter;
				FilenamesAdd(Util_strdup(drive2));
			}
		}
	}
#endif /* DOS_DRIVES */
#ifdef __DJGPP__
	_djstat_flags = s_backup;	/* restore the original state */
#endif
}

/* Select file or directory.
   The result is returned in path and path is where selection begins (i.e. it must be initialized).
   pDirectories are "favourite" directories (there are nDirectories of them). */
static int FileSelector(char *path, int select_dir, char pDirectories[][FILENAME_MAX], int nDirectories)
{
	char current_dir[FILENAME_MAX];
	char highlighted_file[FILENAME_MAX];
	highlighted_file[0] = '\0';
	if (path[0] == '\0' && nDirectories > 0)
		strcpy(current_dir, pDirectories[0]);
	else if (select_dir)
		strcpy(current_dir, path);
	else
		Util_splitpath(path, current_dir, highlighted_file);
#ifdef __DJGPP__
	{
		char help_dir[FILENAME_MAX];
		_fixpath(current_dir, help_dir);
		strcpy(current_dir, help_dir);
	}
#elif defined(HAVE_GETCWD)
	if (current_dir[0] == '\0' || (current_dir[0] == '.' && current_dir[1] == '\0'))
		getcwd(current_dir, FILENAME_MAX);
#else
	if (directory[0] == '\0') {
		directory[0] = '.';
		directory[1] = '\0';
	}
#endif
	for (;;) {
		int offset = 0;
		int item = 0;
		int i;

#define NROWS 20
#define NCOLUMNS 2
#define MAX_FILES (NROWS * NCOLUMNS)

		/* The WinCE version may spend several seconds when there are many
		   files in the directory. */
		/* The extra spaces are needed to clear the previous window title. */
		TitleScreen("            Please wait...            ");
		Atari_DisplayScreen();

		GetDirectory(current_dir);

		if (n_filenames == 0) {
			/* XXX: shouldn't happen: there should be at least ".." or a drive letter */
			FilenamesFree();
			BasicUIMessage("No files inside directory");
			return FALSE;
		}

		if (highlighted_file[0] != '\0') {
			for (i = 0; i < n_filenames; i++) {
				if (strcmp(filenames[i], highlighted_file) == 0) {
					item = i % NROWS;
					offset = i - item;
					break;
				}
			}
		}

		for (;;) {
			const char **files = (const char **) filenames + offset;
			int nitems;
			int ascii;
			if (offset + MAX_FILES <= n_filenames)
				nitems = MAX_FILES;
			else
				nitems = n_filenames - offset;

			ClearScreen();
#if 0
			TitleScreen(select_dir ? "Space = select current directory" : "Select File");
#else
			TitleScreen(current_dir);
#endif
			Box(0x9a, 0x94, 0, 1, 39, 23);

			if (item < 0)
				item = 0;
			else if (item >= nitems)
				item = nitems - 1;
			item = Select(item, nitems, files, NULL, NULL, NROWS, NCOLUMNS, 1, 2, 37 / NCOLUMNS, TRUE, &ascii);

			if (item >= nitems * 2 + NROWS) {
				/* Scroll Right */
				if (offset + NROWS + NROWS < n_filenames) {
					offset += NROWS;
					item %= nitems;
				}
				else
					item = nitems - 1;
				continue;
			}
			if (item >= nitems) {
				/* Scroll Left */
				if (offset - NROWS >= 0) {
					offset -= NROWS;
					item %= nitems;
				}
				else
					item = 0;
				continue;
			}
			if (item == -2) {
				/* Tab = next favourite directory */
				if (nDirectories > 0) {
					/* default: pDirectories[0] */
					int current_index = nDirectories - 1;
					/* are we in one of pDirectories? */
					for (i = 0; i < nDirectories; i++)
						if (strcmp(pDirectories[i], current_dir) == 0) {
							current_index = i;
							break;
						}
					i = current_index;
					do {
						if (++i >= nDirectories)
							i = 0;
						if (Util_direxists(pDirectories[i])) {
							strcpy(current_dir, pDirectories[i]);
							break;
						}
					} while (i != current_index);
				}
				break;
			}
			if (item == -1) {
				/* Esc = cancel */
				FilenamesFree();
				return FALSE;
			}
			if (ascii == 0x7e) {
				/* Backspace = parent directory */
				char new_dir[FILENAME_MAX];
				Util_splitpath(current_dir, new_dir, NULL);
				if (Util_direxists(new_dir)) {
					strcpy(current_dir, new_dir);
					break;
				}
				continue;
			}
			if (ascii == ' ' && select_dir) {
				/* Space = select current directory */
				strcpy(path, current_dir);
				FilenamesFree();
				return TRUE;
			}
			if (files[item][0] == '[') {
				/* Change directory */
				char new_dir[FILENAME_MAX];

				if (strcmp(files[item], "[..]") == 0) {
					/* go up */
					Util_splitpath(current_dir, new_dir, NULL);
				}
#ifdef DOS_DRIVES
				else if (files[item][2] == ':' && files[item][3] == ']') {
					/* disk selected */
					new_dir[0] = files[item][1];
					new_dir[1] = ':';
					new_dir[2] = '\\';
					new_dir[3] = '\0';
				}
#endif
				else {
					/* directory selected */
					char *pbracket = strrchr(files[item], ']');
					if (pbracket != NULL)
						*pbracket = '\0';	/*cut ']' */
					Util_catpath(new_dir, current_dir, files[item] + 1);
				}
				/* check if new directory is valid */
				if (Util_direxists(new_dir)) {
					strcpy(current_dir, new_dir);
					break;
				}
				continue;
			}
			if (!select_dir) {
				/* normal filename selected */
				Util_catpath(path, current_dir, files[item]);
				FilenamesFree();
				return TRUE;
			}
		}

		FilenamesFree();
		highlighted_file[0] = '\0';
	}
}

#endif /* DO_DIR */

/* nDirectories >= 0 means we are editing a file name */
static int EditString(int fg, int bg, const char *title,
                      char *string, int size, int x, int y, int width,
                      char pDirectories[][FILENAME_MAX], int nDirectories)
{
	int caret = strlen(string);
	int offset = 0;
	for (;;) {
		int i;
		char *p;
		int ascii;
		Box(fg, bg, x, y, x + 1 + width, y + 2);
		Print(bg, fg, title, x + 1, y, width);
		if (caret - offset >= width)
			offset = caret - width + 1;
		else if (caret < offset)
			offset = caret;
		p = string + offset;
		for (i = 0; i < width; i++)
			if (offset + i == caret)
				Plot(bg, fg, *p != '\0' ? *p++ : ' ', x + 1 + i, y + 1);
			else
				Plot(fg, bg, *p != '\0' ? *p++ : ' ', x + 1 + i, y + 1);
		ascii = GetKeyPress();
		switch (ascii) {
		case 0x1e:				/* Cursor Left */
			if (caret > 0)
				caret--;
			break;
		case 0x1f:				/* Cursor Right */
			if (string[caret] != '\0')
				caret++;
			break;
		case 0x7e:				/* Backspace */
			if (caret > 0) {
				caret--;
				p = string + caret;
				do
					p[0] = p[1];
				while (*p++ != '\0');
			}
			break;
		case 0xfe:				/* Delete */
			if (string[caret] != '\0') {
				p = string + caret;
				do
					p[0] = p[1];
				while (*p++ != '\0');
			}
			break;
		case 0x7d:				/* Clear screen */
		case 0x9c:				/* Delete line */
			caret = 0;
			string[0] = '\0';
			break;
		case 0x9b:				/* Return */
			if (nDirectories >= 0) {
				/* check filename */
				char lastchar;
				if (string[0] == '\0')
					return FALSE;
				lastchar = string[strlen(string) - 1];
				return lastchar != '/' && lastchar != '\\';
			}
			return TRUE;
		case 0x1b:				/* Esc */
			return FALSE;
#ifdef DO_DIR
		case 0x7f:				/* Tab = select directory */
			if (nDirectories >= 0) {
				char temp_filename[FILENAME_MAX + 1];
				char temp_path[FILENAME_MAX];
				char temp_file[FILENAME_MAX];
				char *p;
				strcpy(Util_stpcpy(temp_filename, string), "*");
				Util_splitpath(temp_filename, temp_path, temp_file);
				p = temp_file + strlen(temp_file) - 1;
				if (*p == '*') { /* XXX: should be always... */
					*p = '\0';
					if (FileSelector(temp_path, TRUE, pDirectories, nDirectories)) {
						Util_catpath(string, temp_path, temp_file);
						caret = strlen(string);
						offset = 0;
					}
				}
			}
			break;
#endif
		default:
			/* Insert character */
			i = strlen(string);
			if (i < size - 1 && ascii >= ' ' && ascii < 0x7f) {
				do
					string[i + 1] = string[i];
				while (--i >= caret);
				string[caret++] = (char) ascii;
			}
			break;
		}
	}
}

/* returns TRUE if accepted filename */
static int EditFilename(const char *title, char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories)
{
	char edited_filename[FILENAME_MAX];
	strcpy(edited_filename, pFilename);
	if (edited_filename[0] == '\0') {
		if (nDirectories > 0)
			strcpy(edited_filename, pDirectories[0]);
#ifdef HAVE_GETCWD
		if (edited_filename[0] == '\0') {
			getcwd(edited_filename, FILENAME_MAX);
			if (edited_filename[0] != '\0' && strlen(edited_filename) < FILENAME_MAX - 1) {
				char *p = edited_filename + strlen(edited_filename) - 1;
				if (*p != '/' && *p != '\\') {
					p[1] = DIR_SEP_CHAR;
					p[2] = '\0';
				}
			}
		}
#endif
	}
	if (!EditString(0x9a, 0x94, title, edited_filename, FILENAME_MAX, 3, 11, 32, pDirectories, nDirectories))
		return FALSE;
	strcpy(pFilename, edited_filename);
	return TRUE;
}

int BasicUIEditString(const char *pTitle, char *pString, int nSize)
{
	return EditString(0x9a, 0x94, pTitle, pString, nSize, 3, 11, 32, NULL, -1);
}

int BasicUIGetSaveFilename(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories)
{
#ifdef DO_DIR
	return EditFilename("Save as (Tab=dir)", pFilename, pDirectories, nDirectories);
#else
	return EditFilename("Save as", pFilename, pDirectories, nDirectories);
#endif
}

int BasicUIGetLoadFilename(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories)
{
#ifdef DO_DIR
	return FileSelector(pFilename, FALSE, pDirectories, nDirectories);
#else
	return EditFilename("Filename", pFilename, pDirectories, nDirectories);
#endif
}

int BasicUIGetDirectoryPath(char *pDirectory)
{
#ifdef DO_DIR
	return FileSelector(pDirectory, TRUE, NULL, 0);
#else
	return EditFilename("Path", pDirectory, NULL, -1);
#endif
}

void BasicUIAboutBox(void)
{
	ClearScreen();

	Box(0x9a, 0x94, 0, 0, 39, 8);
	CenterPrint(0x9a, 0x94, ATARI_TITLE, 1);
	CenterPrint(0x9a, 0x94, "Copyright (c) 1995-1998 David Firth", 2);
	CenterPrint(0x9a, 0x94, "and", 3);
	CenterPrint(0x9a, 0x94, "(c)1998-2005 Atari800 Development Team", 4);
	CenterPrint(0x9a, 0x94, "See CREDITS file for details.", 5);
	CenterPrint(0x9a, 0x94, "http://atari800.atari.org/", 7);

	Box(0x9a, 0x94, 0, 9, 39, 23);
	CenterPrint(0x9a, 0x94, "This program is free software; you can", 10);
	CenterPrint(0x9a, 0x94, "redistribute it and/or modify it under", 11);
	CenterPrint(0x9a, 0x94, "the terms of the GNU General Public", 12);
	CenterPrint(0x9a, 0x94, "License as published by the Free", 13);
	CenterPrint(0x9a, 0x94, "Software Foundation; either version 1,", 14);
	CenterPrint(0x9a, 0x94, "or (at your option) any later version.", 15);

	CenterPrint(0x94, 0x9a, "Press any Key to Continue", 22);
	GetKeyPress();
}

void BasicUIInit(void)
{
	if (!initialised) {
		get_charset(charset);
		initialised = TRUE;
	}
}

tUIDriver basic_ui_driver =
{
	&BasicUISelect,
	&BasicUISelectInt,
	&BasicUIEditString,
	&BasicUIGetSaveFilename,
	&BasicUIGetLoadFilename,
	&BasicUIGetDirectoryPath,
	&BasicUIMessage,
	&BasicUIAboutBox,
	&BasicUIInit
};

/*
$Log$
Revision 1.39  2005/10/19 21:40:30  pfusik
tons of changes, see ChangeLog

Revision 1.38  2005/10/09 20:33:36  pfusik
silenced a warning

Revision 1.37  2005/09/27 21:41:08  pfusik
UI's charset is now in ATASCII order; curses_putch()

Revision 1.36  2005/09/18 15:08:03  pfusik
fixed file selector: last directory entry wasn't sorted;
saved a few bytes per tMenuItem

Revision 1.35  2005/09/14 20:32:18  pfusik
".." in Win32 API based file selector on WINCE;
include B: in DOS_DRIVES; detect floppies on WIN32

Revision 1.34  2005/09/11 20:38:43  pfusik
implemented file selector on MSVC

Revision 1.33  2005/09/11 07:19:22  pfusik
fixed file selector which I broke yesterday;
use Util_realloc() instead of realloc(); fixed a warning

Revision 1.32  2005/09/10 12:37:25  pfusik
char * -> const char *; Util_splitpath() and Util_catpath()

Revision 1.31  2005/09/07 22:00:29  pfusik
shorten the messages to fit on screen

Revision 1.30  2005/09/06 22:58:29  pfusik
improved file selector; fixed MSVC warnings

Revision 1.29  2005/09/04 18:16:18  pfusik
don't hide ATR/XFD file extensions in the file selector

Revision 1.28  2005/08/27 10:36:07  pfusik
MSVC declares getcwd() in <direct.h>

Revision 1.27  2005/08/24 21:03:41  pfusik
use stricmp() if there's no strcasecmp()

Revision 1.26  2005/08/21 17:40:53  pfusik
DO_DIR -> HAVE_OPENDIR

Revision 1.25  2005/08/18 23:34:00  pfusik
shortcut keys in UI

Revision 1.24  2005/08/17 22:49:15  pfusik
compile without <dirent.h>

Revision 1.23  2005/08/16 23:07:28  pfusik
#include "config.h" before system headers

Revision 1.22  2005/08/15 17:27:00  pfusik
char charset[] -> UBYTE charset[]

Revision 1.21  2005/08/14 08:44:23  pfusik
avoid negative array indexes with special keys pressed in UI;
fixed indentation

Revision 1.20  2005/08/13 08:53:42  pfusik
CURSES_BASIC; fixed indentation

Revision 1.19  2005/08/06 18:25:40  pfusik
changed () function signatures to (void)

Revision 1.18  2005/05/20 09:08:17  pfusik
fixed some warnings

Revision 1.17  2005/03/10 04:41:26  pfusik
fixed a memory leak

Revision 1.16  2005/03/08 04:32:46  pfusik
killed gcc -W warnings

Revision 1.15  2005/03/05 12:34:08  pfusik
fixed "Error opening '' directory"

Revision 1.14  2005/03/03 09:27:46  pfusik
moved atari_screen to screen.h

Revision 1.13  2004/09/24 15:28:40  sba
Fixed NULL pointer access in filedialog, which happened if no files are within the directory.

Revision 1.12  2004/08/08 08:41:47  joy
copyright year increased

Revision 1.11  2003/12/21 11:00:26  joy
problem with opening invalid folders in UI identified

Revision 1.10  2003/02/24 09:33:13  joy
header cleanup

Revision 1.9  2003/02/08 23:52:17  joy
little cleanup

Revision 1.8  2003/01/27 13:14:51  joy
Jason's changes: either PAGED_ATTRIB support (mostly), or just clean up.

Revision 1.7  2002/06/12 06:40:41  vasyl
Fixed odd behavior of Up button on the first item in file selector

Revision 1.6  2002/03/30 06:19:28  vasyl
Dirty rectangle scheme implementation part 2.
All video memory accesses everywhere are going through the same macros
in ANTIC.C. UI_BASIC does not require special handling anymore. Two new
functions are exposed in ANTIC.H for writing to video memory.

Revision 1.5  2001/11/29 12:36:42  joy
copyright notice updated

Revision 1.4  2001/10/16 17:11:27  knik
keyboard autorepeat rate changed

Revision 1.3  2001/10/11 17:27:22  knik
added atari_sync() call in keyboard loop--keyboard is sampled
at reasonable rate

*/
