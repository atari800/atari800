#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>				/* for free() */
#include <unistd.h>				/* for open() */
#ifdef WIN32
#include "winatari.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include "rt-config.h"
#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "platform.h"
#include "prompts.h"
#include "gtia.h"
#include "sio.h"
#include "list.h"
#include "ui.h"
#include "log.h"
#include "statesav.h"
#include "config.h"
#include "antic.h"
#include "ataripcx.h"
#include "binload.h"
#include "sndsave.h"

extern int refresh_rate;

int alt_function = -1;		/* alt function init */

#define FILENAME_SIZE	32

#ifdef __DJGPP__
#include <dos.h>
#endif
#ifdef linux
#include <time.h>
#endif

int ui_is_active = FALSE;

static int current_disk_directory = 0;
static char curr_disk_dir[MAX_FILENAME_LEN] = "";
static char curr_cart_dir[MAX_FILENAME_LEN] = "";
static char curr_exe_dir[MAX_FILENAME_LEN] = "";
static char curr_state_dir[MAX_FILENAME_LEN] = "";
static char charset[1024];

#ifdef WIN32
extern unsigned long ulAtariState;
extern void SafeShowScreen(void);
extern HWND MainhWnd;
unsigned long hThread = 0L;
#endif	/* WIN32 */

#ifdef STEREO
extern int stereo_enabled;
#endif

#ifdef CRASH_MENU
int crash_code=-1;
UWORD crash_address;
UWORD crash_afterCIM;
int CrashMenu(UBYTE *screen);
#endif

unsigned char key_to_ascii[256] =
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

unsigned char ascii_to_screen[128] =
{
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};


#define KB_DELAY		20
#define KB_AUTOREPEAT		4

int GetKeyPress(UBYTE * screen)
{
	int keycode;

#ifndef BASIC
#ifdef SVGA_SPEEDUP
	int i;
		for(i=0;i<refresh_rate;i++)
#endif
			Atari_DisplayScreen(screen);
#endif

	while(1)
	{
		static int rep = KB_DELAY;
		if (Atari_Keyboard() == AKEY_NONE) {
			rep = KB_DELAY;
			atari_sync();
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
		keycode = Atari_Keyboard();
	} while (keycode == AKEY_NONE);

	return key_to_ascii[keycode];
}

void Plot(UBYTE * screen, int fg, int bg, int ch, int x, int y)
{
#ifndef CURSES
	int offset = ascii_to_screen[(ch & 0x07f)] * 8;
	int i;
	int j;

	char *ptr;

	ptr = screen + 24 * ATARI_WIDTH + 32 + y * (8 * ATARI_WIDTH) + (x << 3);

	for (i = 0; i < 8; i++) {
		UBYTE data;

		data = charset[offset++];

		for (j = 0; j < 8; j++) {
#ifdef USE_COLOUR_TRANSLATION_TABLE
			*ptr++ = colour_translation_table[data & 0x80 ? fg : bg];
#else
			*ptr++ = data & 0x80 ? fg : bg;
#endif
			data <<= 1;
		}

		ptr += ATARI_WIDTH - 8;
	}
#else
	UWORD screenaddr;
      	screenaddr = (memory[89] << 8) | memory[88];

        /* handle line drawiong chars */
        switch (ch) {
        case 18:  ch='-'; break;
        case 17:
        case 3:   ch='/'; break;
        case 26:
        case 5:   ch='\\'; break;
        case 124: ch='|';
        no32:
          memory[screenaddr + y * 40 + x] = (ch ) + (fg == 0x94 ? 0x80 : 0);
          return;
          break;
        }

        if (ch >= 'a' && ch <='z') goto no32;

        memory[screenaddr + y * 40 + x] = (ch - 32) + (fg == 0x94 ? 0x80 : 0);
#endif
}

void Print(UBYTE * screen, int fg, int bg, char *string, int x, int y)
{
	while (*string) {
		Plot(screen, fg, bg, *string++, x, y);
		x++;
	}
}

void CenterPrint(UBYTE * screen, int fg, int bg, char *string, int y)
{
	Print(screen, fg, bg, string, (40 - strlen(string)) / 2, y);
}

int EditString(UBYTE * screen, int fg, int bg,
				int len, char *string,
				int x, int y)
{
	int offset = 0;

	Print(screen, fg, bg, string, x, y);

	for(;;) {
		int ascii;

		Plot(screen, bg, fg, string[offset], x + offset, y);

		ascii = GetKeyPress(screen);
		switch (ascii) {
		case 0x1e:				/* Cursor Left */
			Plot(screen, fg, bg, string[offset], x + offset, y);
			if (offset > 0)
				offset--;
			break;
		case 0x1f:				/* Cursor Right */
			Plot(screen, fg, bg, string[offset], x + offset, y);
			if ((offset + 1) < len)
				offset++;
			break;
		case 0x7e:				/* Backspace */
			Plot(screen, fg, bg, string[offset], x + offset, y);
			if (offset > 0) {
				offset--;
				string[offset] = ' ';
			}
			break;
		case 0x9b:				/* Return */
			return TRUE;
		case 0x1b:				/* Esc */
			return FALSE;
		default:
			string[offset] = (char) ascii;
			Plot(screen, fg, bg, string[offset], x + offset, y);
			if ((offset + 1) < len)
				offset++;
			break;
		}
	}
}

void Box(UBYTE * screen, int fg, int bg, int x1, int y1, int x2, int y2)
{
	int x;
	int y;

	for (x = x1 + 1; x < x2; x++) {
		Plot(screen, fg, bg, 18, x, y1);
		Plot(screen, fg, bg, 18, x, y2);
	}

	for (y = y1 + 1; y < y2; y++) {
		Plot(screen, fg, bg, 124, x1, y);
		Plot(screen, fg, bg, 124, x2, y);
	}

	Plot(screen, fg, bg, 17, x1, y1);
	Plot(screen, fg, bg, 5, x2, y1);
	Plot(screen, fg, bg, 3, x2, y2);
	Plot(screen, fg, bg, 26, x1, y2);
}

void ClearScreen(UBYTE * screen)
{
	UBYTE *ptr;
#ifdef USE_COLOUR_TRANSLATION_TABLE
	memset(screen, colour_translation_table[0x00], ATARI_HEIGHT * ATARI_WIDTH);
	for (ptr = screen + ATARI_WIDTH * 24 + 32; ptr < screen + ATARI_WIDTH * (24 + 192); ptr += ATARI_WIDTH)
		memset(ptr, colour_translation_table[0x94], 320);
#else
	memset(screen, 0x00, ATARI_HEIGHT * ATARI_WIDTH);
	for (ptr = screen + ATARI_WIDTH * 24 + 32; ptr < screen + ATARI_WIDTH * (24 + 192); ptr += ATARI_WIDTH)
		memset(ptr, 0x94, 320);
#endif
}

void TitleScreen(UBYTE * screen, char *title)
{
	Box(screen, 0x9a, 0x94, 0, 0, 39, 2);
	CenterPrint(screen, 0x9a, 0x94, title, 1);
}

void ShortenItem(char *source, char *destination, int iMaxXsize)
{
	if (strlen(source) > iMaxXsize) {

		int iFirstLen = (iMaxXsize - 3) / 2;
		int iLastStart = strlen(source) - (iMaxXsize - 3 - iFirstLen);
		strncpy(destination, source, iFirstLen);
		destination[iFirstLen] = '\0';
		strcat(destination, "...");
		strcat(destination, source + iLastStart);

	}
	else
		strcpy(destination, source);
}

void SelectItem(UBYTE * screen,
				int fg, int bg,
				int index, char *items[],
				int nrows, int ncolumns,
				int xoffset, int yoffset,
				int active)
{
	int x;
	int y;
	int iMaxXsize = ((40 - xoffset) / ncolumns) - 1;
	char szOrig[MAX_FILENAME_LEN];
	char szString[41];

	x = index / nrows;
	y = index - (x * nrows);

	x = x * (40 / ncolumns);

	x += xoffset;
	y += yoffset;

	strcpy(szOrig, items[index]);

	if (strlen(szOrig) > 3) {
		int iKnownExt = FALSE;

		if (!strcasecmp(szOrig + strlen(szOrig) - 3, "ATR"))
			iKnownExt = TRUE;

		if (!strcasecmp(szOrig + strlen(szOrig) - 3, "XFD"))
			iKnownExt = TRUE;

		if (iKnownExt) {
			szOrig[strlen(szOrig) - 4] = '\0';
		}
	}

	ShortenItem(szOrig, szString, iMaxXsize);

	Print(screen, fg, bg, szString, x, y);

	if (active) {
		char empty[41];
		int ln;

		memset(empty, ' ', 38);
		empty[38] = '\0';
		Print(screen, bg, fg, empty, 1, 22);

		ShortenItem(szOrig, szString, 38);
		ln = strlen(szString);

		if (ln > iMaxXsize)
			CenterPrint(screen, fg, bg, szString, 22);	/*the selected item was shortened */
	}
}

int Select(UBYTE * screen,
		   int default_item,
		   int nitems, char *items[],
		   int nrows, int ncolumns,
		   int xoffset, int yoffset,
		   int scrollable,
		   int *ascii)
{
	int index = 0;

	for (index = 0; index < nitems; index++)
		SelectItem(screen, 0x9a, 0x94, index, items, nrows, ncolumns, xoffset, yoffset, FALSE);

	index = default_item;
	SelectItem(screen, 0x94, 0x9a, index, items, nrows, ncolumns, xoffset, yoffset, TRUE);

	for (;;) {
		int row;
		int column;
		int new_index;

		column = index / nrows;
		row = index - (column * nrows);

		*ascii = GetKeyPress(screen);
		switch (*ascii) {
		case 0x1c:				/* Up */
			if (row > 0)
				row--;
			else
				/*GOLDA CHANGED */ if (column > 0) {
				column--;
				row = nrows - 1;
			}
			else if (scrollable)
				return index + nitems + (nrows - 1);
			break;
		case 0x1d:				/* Down */
			if (row < (nrows - 1))
				row++;
			else
				/*GOLDA CHANGED */ if (column < (ncolumns - 1)) {
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
			return -2;			/*GOLDA CHANGED */
		case 0x20:				/* Space */
		case 0x7e:				/* Backspace */
		case 0x9b:				/* Select */
			return index;
		case 0x1b:				/* Cancel */
			return -1;
		default:
			break;
		}

		new_index = (column * nrows) + row;
		if ((new_index >= 0) && (new_index < nitems)) {
			SelectItem(screen, 0x9a, 0x94, index, items, nrows, ncolumns, xoffset, yoffset, FALSE);

			index = new_index;
			SelectItem(screen, 0x94, 0x9a, index, items, nrows, ncolumns, xoffset, yoffset, TRUE);
		}
	}
}

/* returns TRUE if valid filename */
int EditFilename(UBYTE *screen, char *fname)
{
	memset(fname, ' ', FILENAME_SIZE);
	fname[FILENAME_SIZE] = '\0';
	Box(screen, 0x9a, 0x94, 3, 9, 36, 11);
	Print(screen, 0x94, 0x9a, "Filename", 4, 9);
	if (!EditString(screen, 0x9a, 0x94, FILENAME_SIZE, fname, 4, 10))
		return FALSE;
	RemoveSpaces(fname);
	return fname[0] != '\0';
}

void SelectSystem(UBYTE * screen)
{
	int system;
	int ascii;
	int status;

	char *menu[7] =
	{
		"Atari OS/A",
		"Atari OS/B",
		"Atari 800XL",
		"Atari 130XE",
		"Atari 320XE (RAMBO)",
		"Atari 320XE (COMPY SHOP)",
		"Atari 5200"
	};

	ClearScreen(screen);
	TitleScreen(screen, "Select System");
	Box(screen, 0x9a, 0x94, 0, 3, 39, 23);

	system = Select(screen, 0, 7, menu, 7, 1, 1, 4, FALSE, &ascii);
	if (system < 0)
		return;

	switch (system) {
	case 0:
		Ram256 = 0;
		status = Initialise_AtariOSA();
		break;
	case 1:
		Ram256 = 0;
		status = Initialise_AtariOSB();
		break;
	case 2:
		Ram256 = 0;
		status = Initialise_AtariXL();
		break;
	case 3:
		Ram256 = 0;
		status = Initialise_AtariXE();
		break;
	case 4:
		Ram256 = 1;
		status = Initialise_Atari320XE();
		break;
	case 5:
		Ram256 = 2;
		status = Initialise_Atari320XE();
		break;
	case 6:
		Ram256 = 0;
		status = Initialise_Atari5200();
		break;
	default:
		status = 1;	/* don't init EmuOS */
		break;
	}
	if (!status) {
		Aprint("Operating System not available - using Emulated OS\n");
		status = Initialise_EmuOS();
	}
}

int FilenameSort(char *filename1, char *filename2)
{
	if (*filename1 == '[' && *filename2 != '[')
		return -1;
	if (*filename1 != '[' && *filename2 == '[')
		return 1;
	if (*filename1 == '[' && *filename2 == '[') {
		if (filename1[1] == '.')
			return -1;
		else if (filename2[1] == '.')
			return 1;
	}

	return strcmp(filename1, filename2);
}

List *GetDirectory(char *directory)
{
#ifdef WIN32
	List *list = NULL;
	char filesearch[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	HANDLE hSearch;

	strcpy(filesearch, directory);
	strcat(filesearch, "\\*.*");

	hSearch = FindFirstFile(filesearch, &FindFileData);

	if (hSearch) {
		list = ListCreate();
		if (!list) {
			MessageBox(NULL, "Out of memory", "Atari800Win", MB_OK);
			return NULL;
		}
		ListAddTail(list, FindFileData.cFileName);

		while (FindNextFile(hSearch, &FindFileData)) {
			char *filename = _strdup(FindFileData.cFileName);
			if (!filename)
				MessageBox(NULL, "Out of memory", "Atari800Win", MB_OK);
			else
				ListAddTail(list, filename);
		}

		FindClose(hSearch);

		ListSort(list, FilenameSort);
	}
#else
	DIR *dp = NULL;
	List *list = NULL;
	struct stat st;
	char fullfilename[MAX_FILENAME_LEN];
	char *filepart;
#ifdef DOS_DRIVES
	char *letter = "C:";
	char *letter2 = "[C:]";
#ifdef __DJGPP__
	unsigned short s_backup = _djstat_flags;
	_djstat_flags = _STAT_INODE | _STAT_EXEC_EXT | _STAT_EXEC_MAGIC | _STAT_DIRSIZE |
		_STAT_ROOT_TIME | _STAT_WRITEBIT;
	/*we do not need any of those 'hard-to-get' informations */
#endif	/* DJGPP */
#endif	/* DOS_DRIVES */
	strcpy(fullfilename, directory);
	filepart = fullfilename + strlen(fullfilename);
#ifdef BACK_SLASH
	if ((filepart == fullfilename) || *(filepart - 1) != '\\')
		*filepart++ = '\\';
#else
	if ((filepart == fullfilename) || *(filepart - 1) != '/')
		*filepart++ = '/';
#endif

	dp = opendir(directory);
	if (dp) {
		struct dirent *entry;

		list = ListCreate();
		if (!list) {
			Aprint("ListCreate(): Failed\n");
			return NULL;
		}
		while ((entry = readdir(dp))) {
			char *filename;

			if (strcmp(entry->d_name, ".") == 0)
				continue;

			strcpy(filepart, entry->d_name);	/*create full filename */
			stat(fullfilename, &st);
			if (st.st_mode & S_IFDIR) {		/*directories add as  [dir] */
				int len;

				len = strlen(entry->d_name);
				if ( (filename = (char *) malloc(len + 3)) ) {
					strcpy(filename + 1, entry->d_name);
					filename[0] = '[';
					filename[len + 1] = ']';
					filename[len + 2] = '\0';
				}
			}
			else
				filename = (char *) strdup(entry->d_name);

			if (!filename) {
				perror("strdup");
				return NULL;
			}
			ListAddTail(list, filename);
		}

		closedir(dp);

		ListSort(list, (void *) FilenameSort);
	}
#ifdef DOS_DRIVES
	/*in DOS, add all existing disk letters */
	ListAddTail(list, strdup("[A:]"));	/*do not check A: - it's slow */
	letter[0] = 'C';
	while (letter[0] <= 'Z') {
#ifdef __DJGPP__
		stat(letter, &st);
		if (st.st_mode & S_IXUSR)
#endif
		{
			letter2[1] = letter[0];
			ListAddTail(list, strdup(letter2));
		}
		(letter[0])++;
	}
#ifdef __DJGPP__
	_djstat_flags = s_backup;	/*return the original state */
#endif
#endif

#endif
	return list;
}

int FileSelector(UBYTE * screen, char *directory, char *full_filename)
{
	List *list;
	int flag = FALSE;
	int next_dir;
#ifndef WIN32
	do {
#ifdef __DJGPP__
		char helpdir[MAX_FILENAME_LEN];
		_fixpath(directory, helpdir);
		strcpy(directory, helpdir);
#else
		if (strcmp(directory, ".") == 0)
			getcwd(directory, MAX_FILENAME_LEN);
#endif
		next_dir = FALSE;
		list = GetDirectory(directory);
		if (list) {
			char *filename;
			int nitems = 0;
			int item = 0;
			int done = FALSE;
			int offset = 0;
			int nfiles = 0;

#define NROWS 18
#define NCOLUMNS 2
#define MAX_FILES (NROWS * NCOLUMNS)

			char *files[MAX_FILES];

			ListReset(list);
			while (ListTraverse(list, (void *) &filename))
				nfiles++;

			while (!done) {
				int ascii;

				ListReset(list);
				for (nitems = 0; nitems < offset; nitems++)
					ListTraverse(list, (void *) &filename);

				for (nitems = 0; nitems < MAX_FILES; nitems++) {
					if (ListTraverse(list, (void *) &filename)) {
						files[nitems] = filename;
					}
					else
						break;
				}

				ClearScreen(screen);
#if 1
				TitleScreen(screen, "Select File");
#else
				TitleScreen(screen, directory);
#endif
				Box(screen, 0x9a, 0x94, 0, 3, 39, 23);

				if (item < 0)
					item = 0;
				else if (item >= nitems)
					item = nitems - 1;
				item = Select(screen, item, nitems, files, NROWS, NCOLUMNS, 1, 4, TRUE, &ascii);
				if (item >= (nitems * 2 + NROWS)) {		/* Scroll Right */
					if ((offset + NROWS + NROWS) < nfiles)
						offset += NROWS;
					item = item % nitems;
				}
				else if (item >= nitems) {	/* Scroll Left */
					if ((offset - NROWS) >= 0)
						offset -= NROWS;
					item = item % nitems;
				}
				else if (item == -2) {	/* Next directory */
					DIR *dr;
					do {
						current_disk_directory = (current_disk_directory + 1) % disk_directories;
						strcpy(directory, atari_disk_dirs[current_disk_directory]);
						dr = opendir(directory);
					} while (dr == NULL);
					closedir(dr);
/*                  directory = atari_disk_dirs[current_disk_directory]; */
					next_dir = TRUE;
					break;
				}
				else if (item != -1) {
					if (files[item][0] == '[') {	/*directory selected */
						DIR *dr;
						char help[MAX_FILENAME_LEN];	/*new directory */

						if (strcmp(files[item], "[..]") == 0) {		/*go up */
							char *pos, *pos2;

							strcpy(help, directory);
							pos = strrchr(help, '/');
							if (!pos)
								pos = strrchr(help, '\\');
							if (pos) {
								*pos = '\0';
								/*if there is no slash in directory, add one at the end */
								pos2 = strrchr(help, '/');
								if (!pos2)
									pos2 = strrchr(help, '\\');
								if (!pos2) {
#ifdef BACK_SLASH
									*pos++ = '\\';
#else
									*pos++ = '/';
#endif
									*pos++ = '\0';
								}
							}

						}
#ifdef DOS_DRIVES
						else if (files[item][2] == ':' && files[item][3] == ']') {	/*disk selected */
							strcpy(help, files[item] + 1);
							help[2] = '\\';
							help[3] = '\0';
						}
#endif
						else {	/*directory selected */
							char lastchar = directory[strlen(directory) - 1];

							*strchr(files[item], ']') = '\0';	/*cut ']' */
							if (lastchar == '/' || lastchar == '\\')
								sprintf(help, "%s%s", directory, files[item] + 1);	/*directory already ends with slash */
							else
#ifndef BACK_SLASH
								sprintf(help, "%s/%s", directory, files[item] + 1);
#else
								sprintf(help, "%s\\%s", directory, files[item] + 1);
#endif
						}
						dr = opendir(help);		/*check, if new directory is valid */
						if (dr) {
							strcpy(directory, help);
							closedir(dr);
							next_dir = TRUE;
							break;
						}
					}
					else {		/*normal filename selected */
						char lastchar = directory[strlen(directory) - 1];
						if (lastchar == '/' || lastchar == '\\')
							sprintf(full_filename, "%s%s", directory, files[item]);		/*directory already ends with slash */
						else
#ifndef BACK_SLASH
							sprintf(full_filename, "%s/%s", directory, files[item]);
#else							/* DOS, TOS fs */
							sprintf(full_filename, "%s\\%s", directory, files[item]);
#endif
						flag = TRUE;
						break;
					}
				}
				else
					break;
			}

			ListFree(list, (void *) free);
		}
	} while (next_dir);
#endif /* WIN32 */
	return flag;
}

void DiskManagement(UBYTE * screen)
{
	char *menu[8];
	int rwflags[8];
	int done = FALSE;
	int dsknum = 0;
	int i;
#ifndef WIN32
	for (i = 0; i < 8; ++i) {
		menu[i] = sio_filename[i];
		rwflags[i] = (drive_status[i] == ReadOnly ? TRUE : FALSE);
	}

	while (!done) {
		char filename[1024];
		int ascii;

		ClearScreen(screen);
		TitleScreen(screen, "Disk Management");
		Box(screen, 0x9a, 0x94, 0, 3, 39, 23);

/*
		Print(screen, 0x9a, 0x94, "D1:", 1, 4);
		Print(screen, 0x9a, 0x94, "D2:", 1, 5);
		Print(screen, 0x9a, 0x94, "D3:", 1, 6);
		Print(screen, 0x9a, 0x94, "D4:", 1, 7);
		Print(screen, 0x9a, 0x94, "D5:", 1, 8);
		Print(screen, 0x9a, 0x94, "D6:", 1, 9);
		Print(screen, 0x9a, 0x94, "D7:", 1, 10);
		Print(screen, 0x9a, 0x94, "D8:", 1, 11);
*/
		for(i = 0; i < 8; i++) {
			char diskName[80];
			sprintf(diskName, "<%c>D%d:", rwflags[i] ? 'R' : 'W', i+1);
			Print(screen, 0x9a, 0x94, diskName, 1, 4+i);
		}

		dsknum = Select(screen, dsknum, 8, menu, 8, 1, 7, 4, FALSE, &ascii);
		if (dsknum > -1) {
			if (ascii == 0x9b) {	/* User pressed "Enter" to select a disk image */
				char *pathname;

/*              pathname=atari_disk_dirs[current_disk_directory]; */
				if (curr_disk_dir[0] == '\0')
					strcpy(curr_disk_dir, atari_disk_dirs[current_disk_directory]);
				while (FileSelector(screen, curr_disk_dir, filename)) {
					DIR *subdir;

					subdir = opendir(filename);
					if (!subdir) {	/* A file was selected */
						SIO_Dismount(dsknum + 1);
						SIO_Mount(dsknum + 1, filename, rwflags[dsknum]);
						break;
					}
					else {		/* A directory was selected */
						closedir(subdir);
						pathname = filename;
					}
				}
			}
			else if (ascii == 0x20) {	/* User pressed "SpaceBar" to change read/write flag of this drive */
				char *flag;
				rwflags[dsknum] = !rwflags[dsknum];
				/* now the drive should probably be remounted
				   and the rwflag should be read from the drive_status again */
				/* TODO remount drive */
				flag = rwflags[dsknum] ? "R" : "W";
				Print(screen, 0x9a, 0x94, flag, 2, 4+i);
			}
			else {
				if (strcmp(sio_filename[dsknum], "Empty") == 0)
					SIO_DisableDrive(dsknum + 1);
				else
					SIO_Dismount(dsknum + 1);
			}
		}
		else
			done = TRUE;
	}
#endif
}

void CartManagement(UBYTE * screen)
{
	typedef struct {
		UBYTE id[4];
		UBYTE type[4];
		UBYTE checksum[4];
		UBYTE gash[4];
	} Header;

	const int CART_UNKNOWN = 0;
	const int CART_STD_8K = 1;
	const int CART_STD_16K = 2;
	const int CART_OSS = 3;
	const int CART_AGS = 4;

	const int nitems = 5;

	static char *menu[5] =
	{
		"Create Cartridge from ROM image",
		"Extract ROM image from Cartridge",
		"Insert Cartridge",
		"Remove Cartridge",
		"Enable PILL Mode"
	};

	int done = FALSE;
	int option = 2;

	if (!curr_cart_dir[0])
	  strcpy(curr_cart_dir, atari_rom_dir);

	while (!done) {
		char filename[256];
		int ascii;

		ClearScreen(screen);
		TitleScreen(screen, "Cartridge Management");
		Box(screen, 0x9a, 0x94, 0, 3, 39, 23);

		option = Select(screen, option, nitems, menu, nitems, 1, 1, 4, FALSE, &ascii);
		switch (option) {
		case 0:
			if (FileSelector(screen, curr_cart_dir, filename)) {
				UBYTE image[32769];
				int type = CART_UNKNOWN;
				int nbytes;
				int fd;

				fd = open(filename, O_RDONLY | O_BINARY, 0777);
				if (fd == -1) {
					perror(filename);
					exit(1);
				}
				nbytes = read(fd, image, sizeof(image));
				switch (nbytes) {
				case 8192:
					type = CART_STD_8K;
					break;
				case 16384:
					{
						const int nitems = 2;
						static char *menu[2] =
						{
							"Standard 16K Cartridge",
							"OSS Super Cartridge"
						};

						int option = 0;

						Box(screen, 0x9a, 0x94, 8, 10, 31, 13);

						option = Select(screen, option,
										nitems, menu,
										nitems, 1,
										9, 11, FALSE, &ascii);
						switch (option) {
						case 0:
							type = CART_STD_16K;
							break;
						case 1:
							type = CART_OSS;
							break;
						default:
							continue;
						}
					}
					break;
				case 32768:
					type = CART_AGS;
				}

				close(fd);

				if (type != CART_UNKNOWN) {
					Header header;

					int checksum = 0;
					int i;

					char fname[FILENAME_SIZE+1];

					if (!EditFilename(screen, fname))
						break;

					for (i = 0; i < nbytes; i++)
						checksum += image[i];

					header.id[0] = 'C';
					header.id[1] = 'A';
					header.id[2] = 'R';
					header.id[3] = 'T';
					header.type[0] = (type >> 24) & 0xff;
					header.type[1] = (type >> 16) & 0xff;
					header.type[2] = (type >> 8) & 0xff;
					header.type[3] = type & 0xff;
					header.checksum[0] = (checksum >> 24) & 0xff;
					header.checksum[1] = (checksum >> 16) & 0xff;
					header.checksum[2] = (checksum >> 8) & 0xff;
					header.checksum[3] = checksum & 0xff;
					header.gash[0] = '\0';
					header.gash[1] = '\0';
					header.gash[2] = '\0';
					header.gash[3] = '\0';

					sprintf(filename, "%s/%s", atari_rom_dir, fname);
					fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0777);
					if (fd != -1) {
						write(fd, &header, sizeof(header));
						write(fd, image, nbytes);
						close(fd);
					}
				}
			}
			break;
		case 1:
			if (FileSelector(screen, curr_cart_dir, filename)) {
				int fd;

				fd = open(filename, O_RDONLY | O_BINARY, 0777);
				if (fd != -1) {
					Header header;
					UBYTE image[32769];
					char fname[FILENAME_SIZE+1];
					int nbytes;

					read(fd, &header, sizeof(header));
					nbytes = read(fd, image, sizeof(image));

					close(fd);

					if (!EditFilename(screen, fname))
						break;

					sprintf(filename, "%s/%s", atari_rom_dir, fname);

					fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0777);
					if (fd != -1) {
						write(fd, image, nbytes);
						close(fd);
					}
				}
			}
			break;
		case 2:
			if (FileSelector(screen, curr_cart_dir, filename)) {
				if (!Insert_Cartridge(filename)) {
					const int nitems = 4;
					static char *menu[4] =
					{
						"Standard 8K Cartridge",
						"Standard 16K Cartridge",
						"OSS Super Cartridge",
						"Atari 5200 Cartridge"
					};

					int option = 0;

					Box(screen, 0x9a, 0x94, 8, 10, 31, 15);

					option = Select(screen, option,
									nitems, menu,
									nitems, 1,
									9, 11, FALSE, &ascii);
					switch (option) {
					case 0:
						Insert_8K_ROM(filename);
						break;
					case 1:
						Insert_16K_ROM(filename);
						break;
					case 2:
						Insert_OSS_ROM(filename);
						break;
					case 3:
						Insert_32K_5200ROM(filename);
						break;
					}
				}
				Coldstart();
			}
			break;
		case 3:
			Remove_ROM();
			Coldstart();
			break;
		case 4:
			EnablePILL();
			Coldstart();
			break;
		default:
			done = TRUE;
			break;
		}
	}
}

void SoundRecording(UBYTE *screen)
{
	static int record_num=0;
	char buf[128];
	char msg[256];

	if (! IsSoundFileOpen())
	{	sprintf(buf,"%d.raw",record_num);
		if (OpenSoundFile(buf))
			sprintf(msg, "Recording sound to file \"%s\"",buf);
		else
			sprintf(msg, "Can't write to file \"%s\"",buf);
	}
	else
	{	CloseSoundFile();
		sprintf(msg, "Recording is stoped");
		record_num++;
	}
	if (screen != NULL) {
		CenterPrint(screen, 0x94, 0x9a, msg, 22);
		GetKeyPress(screen);
	}
}

void AboutEmulator(UBYTE * screen)
{
	ClearScreen(screen);

	Box(screen, 0x9a, 0x94, 0, 0, 39, 8);
	CenterPrint(screen, 0x9a, 0x94, ATARI_TITLE, 1);
	CenterPrint(screen, 0x9a, 0x94, "Copyright (c) 1995-1998 David Firth", 2);
	CenterPrint(screen, 0x9a, 0x94, "E-Mail: david@signus.demon.co.uk", 3);
	CenterPrint(screen, 0x9a, 0x94, "http://www.signus.demon.co.uk/", 4);
	CenterPrint(screen, 0x9a, 0x94, "Atari PokeySound 2.4", 6);
	CenterPrint(screen, 0x9a, 0x94, "Copyright (c) 1996-1998 Ron Fries", 7);

	Box(screen, 0x9a, 0x94, 0, 9, 39, 23);
	CenterPrint(screen, 0x9a, 0x94, "This program is free software; you can", 10);
	CenterPrint(screen, 0x9a, 0x94, "redistribute it and/or modify it under", 11);
	CenterPrint(screen, 0x9a, 0x94, "the terms of the GNU General Public", 12);
	CenterPrint(screen, 0x9a, 0x94, "License as published by the Free", 13);
	CenterPrint(screen, 0x9a, 0x94, "Software Foundation; either version 1,", 14);
	CenterPrint(screen, 0x9a, 0x94, "or (at your option) any later version.", 15);

	CenterPrint(screen, 0x94, 0x9a, "Press any Key to Continue", 22);
	GetKeyPress(screen);
}

int RunExe(UBYTE *screen)
{
	char exename[MAX_FILENAME_LEN];
	int ret = FALSE;

	if (!curr_exe_dir[0])
	  strcpy(curr_exe_dir, atari_exe_dir);
	if (FileSelector(screen, curr_exe_dir, exename)) {
		ret = BIN_loader(exename);
		if (! ret) {
			/* display log to a window */
		}
	}

	return ret;
}

int SaveState(UBYTE *screen)
{
	char statename[MAX_FILENAME_LEN];
	char fname[FILENAME_SIZE+1];

	if (!EditFilename(screen, fname))
		return 0;

	strcpy(statename, atari_state_dir);
	if (*statename) {
		char last = statename[strlen(statename)-1];
		if (last != '/' && last != '\\')
#ifdef BACK_SLASH
			strcat(statename, "\\");
#else
			strcat(statename, "/");
#endif
	}
	strcat(statename, fname);

	return SaveAtariState(statename, "wb", TRUE);
}

int LoadState(UBYTE *screen)
{
	char statename[MAX_FILENAME_LEN];
	int ret = FALSE;

	if (!curr_state_dir[0])
	  strcpy(curr_state_dir, atari_state_dir);
	if (FileSelector(screen, curr_state_dir, statename))
		ret = ReadAtariState(statename, "rb");

	return ret;
}

void ui(UBYTE *screen)
{
	static int initialised = FALSE;
	int option = 0;
	int done = FALSE;

	char *menu[] =
	{
		"Disk Management                  Alt+D",
		"Cartridge Management             Alt+C",
		"Run BIN file directly            Alt+R",
		"Select System                    Alt+Y",
		"Sound Mono/Stereo                Alt+O",
		"Sound Recording start/stop       Alt+W",
		"Artifacting mode                      ",
		"Save State                       Alt+S",
		"Load State                       Alt+L",
		"PCX screenshot                     F10",
		"PCX interlaced screenshot    Shift+F10",
		"Back to emulated Atari             Esc",
		"Reset (Warm Start)                  F5",
		"Reboot (Cold Start)           Shift+F5",
		"Enter monitor                       F8",
		"About the Emulator               Alt+A",
		"Exit Emulator                       F9"
	};
	const int nitems = sizeof(menu) / sizeof(menu[0]);
#ifdef CURSES
        char *screenbackup = malloc(40*24);
        if (screenbackup) memcpy(screenbackup,&memory[(memory[89] << 8) | memory[88]],40*24);  /* backup of textmode screen */
#endif

	ui_is_active = TRUE;
#ifdef WIN32
	ulAtariState |= ATARI_UI_ACTIVE;
#endif
	/* Sound_Active(FALSE); */
	if (!initialised) {
		get_charset(charset);
		initialised = TRUE;
	}

#ifdef CRASH_MENU
	if (crash_code >= 0) 
	{
		done = CrashMenu(screen);
		crash_code = -1;
	}
#endif	
	
	while (!done) {
		int ascii;

		ClearScreen(screen);
		TitleScreen(screen, ATARI_TITLE);
		Box(screen, 0x9a, 0x94, 0, 3, 39, 23);

		if (alt_function<0)
		{
			option = Select(screen, option, nitems, menu,
							nitems, 1, 1, 4, FALSE, &ascii);
		}
		else
		{
			option = alt_function;
			alt_function = -1;
			done = TRUE;
		}

		switch (option) {
		case -2:
		case -1:		/* ESC key */
			done = TRUE;
			break;
		case MENU_DISK:
			DiskManagement(screen);
			break;
		case MENU_CARTRIDGE:
			CartManagement(screen);
			break;
		case MENU_RUN:
			if (RunExe(screen))
				done = TRUE;	/* reboot immediately */
			break;
		case MENU_SYSTEM:
			SelectSystem(screen);
			break;
		case MENU_ARTIF:
			{
				const int nitems = 5;
				static char *menu[5] =
				{
					"none        ",
					"blue/brown 1",
					"blue/brown 2",
					"GTIA        ",
					"CTIA        "
				};

				int option = global_artif_mode;

				Box(screen, 0x9a, 0x94, 18, 10, 31, 16);

				option = Select(screen, option,
								nitems, menu,
								nitems, 1,
								19, 11, FALSE, &ascii);

				if (option >= 0)
					global_artif_mode = option;
			}
			artif_init();
			break;
		case MENU_SOUND:
			{
				char *msg;
#ifdef STEREO
				stereo_enabled = !stereo_enabled;
				if (stereo_enabled)
					msg = "Stereo sound output";
				else
					msg = " Mono sound output ";
#else
				msg = "Stereo sound was not compiled in";
#endif
				CenterPrint(screen, 0x94, 0x9a, msg, 22);
				GetKeyPress(screen);
			}
			break;
		case MENU_SOUND_RECORDING:
			SoundRecording(screen);
			break;
		case MENU_SAVESTATE:
			SaveState(screen);
			break;
		case MENU_LOADSTATE:
			LoadState(screen);
			break;
		case MENU_PCX:
			{
				char fname[FILENAME_SIZE + 1];
				if (EditFilename(screen, fname)) {
					ANTIC_RunDisplayList();
					Save_PCX_file(0, fname);
				}
			}
			break;
		case MENU_PCXI:
			{
				char fname[FILENAME_SIZE + 1];
				if (EditFilename(screen, fname)) {
					ANTIC_RunDisplayList();
					Save_PCX_file(1, fname);
				}
			}
			break;
		case MENU_BACK:
			done = TRUE;	/* back to emulator */
			break;
		case MENU_RESETW:
			Warmstart();
			done = TRUE;	/* reboot immediately */
			break;
		case MENU_RESETC:
			Coldstart();
			done = TRUE;	/* reboot immediately */
			break;
		case MENU_ABOUT:
			AboutEmulator(screen);
			break;
		case MENU_MONITOR:
			if (Atari_Exit(1)) {
				done = TRUE;
				break;
			}
			/* if 'quit' typed in monitor, exit emulator */
		case MENU_EXIT:
			Atari800_Exit(0);
			exit(0);
		}
	}
#ifdef CURSES
        if (screenbackup) {
          memcpy(&memory[(memory[89] << 8) | memory[88]],screenbackup,40*24);  /* restore textmode screen */
          free(screenbackup);
        }
#endif
	/* Sound_Active(TRUE); */
	ui_is_active = FALSE;
	while (Atari_Keyboard() != AKEY_NONE);	/* flush keypresses */
#ifdef WIN32
	ulAtariState &= ~ATARI_UI_ACTIVE;
	hThread = 0L;
	/* ExitThread(0); */
#endif
}


#ifdef CRASH_MENU

int CrashMenu(UBYTE *screen)
{
	int option = 0;
	char bf[40];	/* CIM info */
	
	char *menu[] =
	{
		"Reset (Warm Start)                  F5",
		"Reboot (Cold Start)           Shift+F5",
		"Menu                                F1",
		"Enter monitor                       F8",
		"Continue after CIM                 Esc",
		"Exit Emulator                       F9"
	};
	const int nitems = 6;

	while (1) {
		int ascii;

		ClearScreen(screen);
		TitleScreen(screen, "!!! The Atari computer has crashed !!!");
		Box(screen, 0x9a, 0x94, 0, 6, 39, 23);

		sprintf(bf,"Code $%02X (CIM) at address $%04X", crash_code, crash_address);
		CenterPrint(screen, 0x9a, 0x94, bf, 4);

		option = Select(screen, option, nitems, menu,
							nitems, 1, 1, 7, FALSE, &ascii);

		switch (option) {
		case 0:			/* Power On Reset */
			alt_function=MENU_RESETW;
			return FALSE;
		case 1:			/* Power Off Reset */
			alt_function=MENU_RESETC;
			return FALSE;
		case 2:			/* Menu */
			return FALSE;
		case 3:			/* Monitor */
			alt_function=MENU_MONITOR;
			return FALSE;
		case -2:
		case -1:		/* ESC key */
		case 4:			/* Continue after CIM */
			regPC = crash_afterCIM;
			return TRUE;
		case 5:			/* Exit */
			alt_function=MENU_EXIT;
			return FALSE;
		}
	}
	return FALSE;
}
#endif

void ReadCharacterSet( void )
{
	get_charset(charset);
}
