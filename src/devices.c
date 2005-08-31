/*
 * devices.c - emulation of H:, P:, E: and K: Atari devices
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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNIXIO_H
/* VMS */
#include <unixio.h>
#endif
#ifdef HAVE_FILE_H
/* VMS */
#include <file.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRECT_H
/* WIN32 */
#include <direct.h> /* mkdir, rmdir */
#endif
/* XXX: <sys/dir.h>, <ndir.h>, <sys/ndir.h> */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "rt-config.h"
#include "log.h"
#include "binload.h"
#include "pia.h" /* atari_os */
#include "sio.h"
#ifdef R_IO_DEVICE
#include "rdevice.h"
#endif
#ifdef __PLUS
#include "misc_win.h"
#endif

#ifdef BACK_SLASH
#define DIR_SEP_CHAR '\\'
#define DIR_SEP_STR  "\\"
#else
#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR  "/"
#endif

/* Splits a filename into directory part and file part. */
/* dir_part or file_part may be NULL. */
static void split_path(const char *path, char *dir_part, char *file_part)
{
	const char *p;
	/* find the last DIR_SEP_CHAR */
	p = strrchr(path, DIR_SEP_CHAR);
	if (p == NULL) {
		/* no DIR_SEP_CHAR: current dir */
		if (file_part != NULL)
			strcpy(file_part, path);
		if (dir_part != NULL)
			strcpy(dir_part, "" /* "." */);
	}
	else {
		if (file_part != NULL)
			strcpy(file_part, p + 1);
		if (dir_part != NULL) {
			int len = p - path;
			if (p == path || (p == path + 2 && path[1] == ':'))
				/* root dir: include DIR_SEP_CHAR in dir_part */
				len++;
			memcpy(dir_part, path, len);
			dir_part[len] = '\0';
		}
	}
}

/* Concatenates file paths.
   Places directory separator char between paths, unless path1 is empty
   or path2 already starts with the separator char. */
static void cat_path(char *result, const char *path1, const char *path2)
{
#ifdef HAVE_SNPRINTF
	snprintf(result, FILENAME_MAX,
#else
	sprintf(result,
#endif
		(path1[0] == '\0' || path2[0] == DIR_SEP_CHAR)
			? "%s%s" : "%s" DIR_SEP_STR "%s", path1, path2);
}

#ifdef HAVE_OPENDIR

#undef toupper
#define toupper(c)  (((c) >= 'a' && (c) <= 'z') ? (c) - 'a' + 'A' : (c))

static int match(const char *pattern, const char *filename)
{
	if (strcmp(pattern, "*.*") == 0)
		return TRUE;

	while (*filename && *pattern) {
		switch (*pattern) {
		case '?':
			pattern++;
			filename++;
			break;
		case '*':
			if (toupper(*filename) == toupper(pattern[1]))
				pattern++;
			else
				filename++;
			break;
		default:
			if (toupper(*pattern) != toupper(*filename))
				return FALSE;
			pattern++;
			filename++;
			break;
		}
	}
	if ((*filename)
		|| ((*pattern)
			&& (((*pattern != '*') && (*pattern != '?'))
				|| pattern[1]))) {
		return FALSE;
	}
	return TRUE;
}

static char dir_path[FILENAME_MAX];
static char filename_pattern[FILENAME_MAX];
static DIR *dp = NULL;

static int Device_OpenDir(const char *searchspec)
{
	split_path(searchspec, dir_path, filename_pattern);
	if (dp != NULL)
		closedir(dp);
	dp = opendir(dir_path);
	return dp != NULL;
}

static int Device_ReadDir(char *fullfilename, char *filename)
{
	struct dirent *entry;
	for (;;) {
		entry = readdir(dp);
		if (entry == NULL) {
			closedir(dp);
			dp = NULL;
			return FALSE;
		}
		if (entry->d_name[0] == '.') {
			/* don't match Unix hidden files unless specifically requested */
			if (filename_pattern[0] != '.')
				continue;
			/* never match "." */
			if (entry->d_name[1] == '\0')
				continue;
			/* never match ".." */
			if (entry->d_name[1] == '.' && entry->d_name[2] == '\0')
				continue;
		}
		if (match(filename_pattern, entry->d_name))
			break;
	}
	if (filename != NULL)
		strcpy(filename, entry->d_name);
	if (fullfilename != NULL)
		cat_path(fullfilename, dir_path, entry->d_name);
	return TRUE;
}

#define DO_DIR

#elif defined(WIN32) || defined(__PLUS)

#include <windows.h>

static char dir_path[FILENAME_MAX];
static WIN32_FIND_DATA wfd;
static HANDLE dh = INVALID_HANDLE_VALUE;

static int Device_OpenDir(const char *searchspec)
{
	split_path(searchspec, dir_path, NULL);
	if (dh != INVALID_HANDLE_VALUE)
		FindClose(dh);
	dh = FindFirstFile(searchspec, &wfd);
	return dh != INVALID_HANDLE_VALUE;
}

static int Device_ReadDir(char *fullfilename, char *filename)
{
	if (dh == INVALID_HANDLE_VALUE)
		return FALSE;
	/* don't match "." nor ".."  */
	while (wfd.cFileName[0] == '.' &&
	       (wfd.cFileName[1] == '\0' || (wfd.cFileName[1] == '.' && wfd.cFileName[2] == '\0'))
	) {
		if (!FindNextFile(dh, &wfd)) {
			FindClose(dh);
			dh = INVALID_HANDLE_VALUE;
			return FALSE;
		}
	}
	if (filename != NULL)
		strcpy(filename, wfd.cFileName);
	if (fullfilename != NULL)
		cat_path(fullfilename, dir_path, wfd.cFileName);
	if (!FindNextFile(dh, &wfd)) {
		FindClose(dh);
		dh = INVALID_HANDLE_VALUE;
	}
	return TRUE;
}

#define DO_DIR

#endif /* defined(WIN32) || defined(__PLUS) */

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

/* Note: you can't use H0: via Atari OS, because it sets ICDNOZ=1 for it. */
/* Current directory is only available as H5: */
static char *H[5] = {
	".",
	atari_h1_dir,
	atari_h2_dir,
	atari_h3_dir,
	atari_h4_dir
};

char HPath[FILENAME_MAX] = DEFAULT_H_PATH;
static char *HPaths[20];
static int HPathDrive[20];
static int HPathCount = 0;

static char current_dir[5][FILENAME_MAX];
static char current_path[5][FILENAME_MAX];

static int devbug = FALSE;

static FILE *fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int h_textmode[8];
static int h_wascr[8];

static int fid;
static char filename[FILENAME_MAX];
#ifdef HAVE_RENAME
static char newfilename[FILENAME_MAX];
#endif

static char *strtoupper(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		if (*ptr >= 'a' && *ptr <= 'z')
			*ptr += 'A' - 'a';

	return str;
}

static char *strtolower(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		if (*ptr >= 'A' && *ptr <= 'Z')
			*ptr += 'a' - 'A';

	return str;
}

static void Device_GeneratePath(void)
{
	int i;

	HPathCount = 0;
	HPaths[0] = strtok(HPath, ";");
	if (HPaths[0] != NULL) {
		HPathCount = 1;
		while (HPathCount < 20) {
			if ((HPaths[HPathCount] = strtok(NULL, ";")) == NULL)
				break;
			HPathCount++;
		}
	}

	for (i = 0; i < HPathCount; i++) {
		if ((HPaths[i][0] == 'H') &&
			(HPaths[i][2] == ':') &&
			((HPaths[i][1] >= '0') && (HPaths[i][1] <= '4'))) {
			HPathDrive[i] = HPaths[i][1] - '0';
			HPaths[i] += 3;
		}
		else {
			HPathDrive[i] = -1;
		}
		strtolower(HPaths[i]);
	}
}

static void Device_HHINIT(void);

void Device_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-H1") == 0)
			H[1] = argv[++i];
		else if (strcmp(argv[i], "-H2") == 0)
			H[2] = argv[++i];
		else if (strcmp(argv[i], "-H3") == 0)
			H[3] = argv[++i];
		else if (strcmp(argv[i], "-H4") == 0)
			H[4] = argv[++i];
		else if (strcmp(argv[i], "-Hpath") == 0)
			strcpy(HPath, argv[++i]);
		else if (strcmp(argv[i], "-devbug") == 0)
			devbug = TRUE;
		else if (strcmp(argv[i], "-hdreadonly") == 0)
			hd_read_only = (argv[++i][0] != '0');
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-H1 <path>       Set path for H1: device");
				Aprint("\t-H2 <path>       Set path for H2: device");
				Aprint("\t-H3 <path>       Set path for H3: device");
				Aprint("\t-H4 <path>       Set path for H4: device");
				Aprint("\t-Hpath <path>    Set path for Atari executables on the H: device");
				Aprint("\t-hdreadonly <on> Enable (1) or disable (0) read-only mode for H: device");
				Aprint("\t-devbug          Debugging messages for H: and P: devices");
			}
			argv[j++] = argv[i];
		}
	}

	*argc = j;

	Device_HHINIT();

}

int Device_isvalid(UBYTE ch)
{
	if ((ch >= 'A' && ch <= 'Z')
	 || (ch >= 'a' && ch <= 'z')
	 || (ch >= '0' && ch <= '9'))
		return TRUE;
	switch (ch) {
	case ':':
	case '.':
	case '!':
	case '#':
	case '$':
	case '&':
	case '(':
	case ')':
	case '-':
	case '@':
	case '_':
	case '*':
	case '?':
	case '>':
		return TRUE;
	default:
		return FALSE;
	}
}

static void Device_GetFilename(void)
{
	UWORD bufadr;
	int offset = 0;
	int devnam = TRUE;

	bufadr = dGetWord(ICBALZ);

	for (;;) {
		UBYTE c = GetByte(bufadr);
		if (!Device_isvalid(c))
			break;

		if (!devnam) {
			if (c >= 'A' && c <= 'Z')
				c += 'a' - 'A';

			filename[offset++] = c;
		}
		else if (c == ':')
			devnam = FALSE;

		bufadr++;
	}

	filename[offset++] = '\0';
}

#ifdef HAVE_RENAME

static void Device_GetFilenames(void)
{
	UWORD bufadr;
	int offset = 0;
	int devnam = TRUE;

	bufadr = dGetWord(ICBALZ);

	for (;;) {
		UBYTE c = GetByte(bufadr);
		if (!Device_isvalid(c))
			break;

		if (!devnam) {
			if (c >= 'A' && c <= 'Z')
				c += 'a' - 'A';

			filename[offset++] = c;
		}
		else if (c == ':')
			devnam = FALSE;

		bufadr++;
	}
	filename[offset++] = '\0';

	for (;;) {
		UBYTE c = GetByte(bufadr);
		if (Device_isvalid(c))
			break;
		if ((c > 0x80) || (c == 0)) {
			newfilename[0] = 0;
			return;
		}
		bufadr++;
	}

	offset = 0;
	for (;;) {
		UBYTE c = GetByte(bufadr);
		if (!Device_isvalid(c))
			break;

		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';

		newfilename[offset++] = c;
		bufadr++;
	}
	newfilename[offset++] = '\0';
}

static void fillin(const char *pattern, char *filename)
{
	while (*pattern) {
		switch (*pattern) {
		case '?':
			pattern++;
			filename++;
			break;
		case '*':
			if (*filename == pattern[1])
				pattern++;
			else
				filename++;
			break;
		default:
			*filename++ = *pattern++;
			break;
		}
	}
	*filename = '\0';
}

#endif /* HAVE_RENAME */

static void apply_relative_path(char *rel_path, char *current)
{
	char *end_current = current + strlen(current);

	if (*rel_path == '>' || *rel_path == ':') {
		if (*(rel_path + 1) != '>' && *(rel_path + 1) != ':') {
			current[0] = 0;
			end_current = current;
			rel_path++;
		}
	}
	else if (current[0] != 0) {
		*end_current++ = DIR_SEP_CHAR;
	}

	while (*rel_path) {
		if (*rel_path == '>' || *rel_path == ':') {
			if (*(rel_path + 1) == '>' || *(rel_path + 1) == ':') {
				/* Move up a directory */
				while ((end_current > current) && (*end_current != DIR_SEP_CHAR))
					end_current--;
				rel_path += 2;
				if ((*rel_path != '>') && (*rel_path != ':')
					&& (*rel_path != 0))
					*end_current++ = DIR_SEP_CHAR;
				else
					*end_current = 0;
			}
			else {
				*end_current++ = DIR_SEP_CHAR;
				rel_path++;
			}
		}
		else {
			*end_current++ = *rel_path++;
		}
	}

	*end_current = 0;
}


static void Device_ApplyPathToFilename(int devnum)
{
	char path[FILENAME_MAX];

	strcpy(path, current_dir[devnum]);
	apply_relative_path(filename, path);
	strcpy(filename, path);
}

#define GET_FID if ((fid = regX >> 4) >= 8) { regY = 134; /* invalid IOCB number */ SetN; return; }

#define GET_DEVNUM	devnum = dGetByte(ICDNOZ); \
	if (devnum > 9) { \
		regY = 160; /* invalid unit/drive number */ \
		SetN; \
		return; \
	} \
	if (devnum >= 5) \
		devnum -= 5;

static void Device_HHOPEN(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int temp;
#ifdef DO_DIR
	int extended;
	char entryname[FILENAME_MAX];
#endif

	if (devbug)
		Aprint("HHOPEN");

	GET_FID;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	Device_GetFilename();
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		regY = 160; /* invalid unit/drive number */
		SetN;
		return;
	}
	if (devnum >= 5) {
		h_textmode[fid] = TRUE;
		devnum -= 5;
	}
	else {
		h_textmode[fid] = FALSE;
	}
	h_wascr[fid] = FALSE;
	Device_ApplyPathToFilename(devnum);

#ifdef VMS
/* Assumes H[devnum] is a directory _logical_, not an explicit directory
   specification! */
	/* XXX: Maybe this should go into cat_path() ? */
# ifdef HAVE_SNPRINTF
	snprintf(fname, FILENAME_MAX, filename[0] == ':' ? "%s%s" : "%s:%s", H[devnum], filename);
# else
	sprintf(fname, filename[0] == ':' ? "%s%s" : "%s:%s", H[devnum], filename);
# endif
#else
	cat_path(fname, H[devnum], filename);
#endif
	strcpy(filename, fname);
	temp = dGetByte(ICAX1Z);

	switch (temp) {
	case 4:
		/* don't bother using "r" for textmode:
		   we want to support LF, CR/LF and CR, not only native EOLs */
		fp[fid] = fopen(fname, "rb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170; /* file not found */
			SetN;
		}
		break;
#ifdef DO_DIR
	case 6:
	case 7:
		fp[fid] = tmpfile();
		if (fp[fid] == NULL) {
			regY = 144; /* device done error */
			SetN;
			break;
		}
		if (!Device_OpenDir(filename)) {
			fclose(fp[fid]);
			fp[fid] = NULL;
			regY = 144; /* device done error */
			SetN;
			break;
		}
		extended = dGetByte(ICAX2Z);
		if (extended >= 128) {
			fprintf(fp[fid], "\nVolume:    HDISK%d\nDirectory: ",
					devnum);
			if (strcmp(dir_path, H[devnum]) == 0)
				fprintf(fp[fid], "MAIN\n\n");
			else {
				char end_dir_str[FILENAME_MAX];
				split_path(dir_path, NULL, end_dir_str);
				fprintf(fp[fid], "%s\n\n",
						/* strtoupper */(end_dir_str));
			}
		}

		while (Device_ReadDir(fname, entryname)) {
			struct stat status;
			char *ext;
			stat(fname, &status);
			/* strtoupper(entryname); */
			ext = strrchr(entryname, '.');
			if (ext == NULL)
				ext = "";
			else
				/* replace the dot with NUL,
				   so entryname is without extension */
				*ext++ = '\0';
			if (extended >= 128) {
#ifdef HAVE_LOCALTIME
				struct tm *filetime;
#endif
				if (status.st_mode & S_IFDIR) {
					fprintf(fp[fid], "%-8s     <DIR>  ",
							entryname);
				}
				else {
					fprintf(fp[fid], "%-8s %-3s %6d ",
							entryname, ext,
							(int) status.st_size);
				}
#ifdef HAVE_LOCALTIME
				filetime = localtime(&status.st_mtime);
				if (filetime->tm_year >= 100)
					filetime->tm_year -= 100;
				fprintf(fp[fid],
						"%02d-%02d-%02d %02d:%02d\n",
						filetime->tm_mon + 1, filetime->tm_mday,
						filetime->tm_year, filetime->tm_hour,
						filetime->tm_min);
#else /* HAVE_LOCALTIME */
				fprintf(fp[fid], "01-01-01 00:00\n");
#endif /* HAVE_LOCALTIME */
			}
			else {
				int size = (status.st_size + 255) >> 8;
				/* if (status.st_size == 0) size = 1; */
				if (size > 999)
					size = 999;
				if (status.st_mode & S_IFDIR)
					ext = "\304\311\322"; /* "DIR" with bit 7 set */
				fprintf(fp[fid], "%c %-8s%-3s %03d\n",
						(status.st_mode & S_IWRITE) ? ' ' : '*',
						entryname, ext, size);
			}

		}

		if (extended >= 128)
			fprintf(fp[fid], "   999 FREE SECTORS\n");
		else
			fprintf(fp[fid], "999 FREE SECTORS\n");

#ifdef HAVE_REWIND
		rewind(fp[fid]);
#else
		fseek(fp[fid], 0, SEEK_SET);
#endif
		h_textmode[fid] = TRUE;
		regY = 1;
		ClrN;
		break;
#endif /* DO_DIR */
	case 8:
	case 9: /* write at the end of file (append) */
		if (hd_read_only) {
			regY = 163; /* disk write-protected */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, h_textmode[fid] ? (temp == 9 ? "a" : "w") : (temp == 9 ? "ab" : "wb"));
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170; /* file not found; XXX */
			SetN;
		}
		break;
	case 12: /* read and write (update) */
		if (hd_read_only) {
			regY = 163; /* disk write-protected */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, h_textmode[fid] ? "r+" : "rb+");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170; /* file not found; XXX */
			SetN;
		}
		break;
	default:
		regY = 168; /* invalid device command */
		SetN;
		break;
	}
}

static void Device_HHCLOS(void)
{
	if (devbug)
		Aprint("HHCLOS");

	GET_FID;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	regY = 1;
	ClrN;
}

static void Device_HHREAD(void)
{
	if (devbug)
		Aprint("HHREAD");

	GET_FID;

	if (fp[fid]) {
		int ch;

		ch = fgetc(fp[fid]);
		if (ch != EOF) {
			if (h_textmode[fid]) {
				switch (ch) {
				case 0x0d:
					h_wascr[fid] = TRUE;
					ch = 0x9b;
					break;
				case 0x0a:
					if (h_wascr[fid]) {
						/* ignore LF next to CR */
						ch = fgetc(fp[fid]);
						if (ch != EOF) {
							if (ch == 0x0d) {
								h_wascr[fid] = TRUE;
								ch = 0x9b;
							}
							else
								h_wascr[fid] = FALSE;
						}
						else {
							regY = 136; /* end of file */
							SetN;
							break;
						}
					}
					else
						ch = 0x9b;
					break;
				default:
					break;
				}
			}
			regA = ch;
			regY = 1;
			ClrN;
		}
		else {
			regY = 136; /* end of file */
			SetN;
		}
	}
	else {
		regY = 136; /* end of file; XXX: this seems to be what Atari DOSes return */
		SetN;
	}
}

static void Device_HHWRIT(void)
{
	if (devbug)
		Aprint("HHWRIT");

	GET_FID;

	if (fp[fid]) {
		int ch;

		ch = regA;
		if (h_textmode[fid]) {
			switch (ch) {
			case 0x9b:
				ch = '\n';
				break;
			default:
				break;
			}
		}
		fputc(ch, fp[fid]);
		regY = 1;
		ClrN;
	}
	else {
		regY = 135; /* attempted to write to a read-only device */
		            /* XXX: this seems to be what Atari DOSes return */
		SetN;
	}
}

static void Device_HHSTAT(void)
{
	if (devbug)
		Aprint("HHSTAT");

	regY = 146; /* function not implemented in handler; XXX: check file existence? */
	SetN;
}

static FILE *binf = NULL;
static int runBinFile;
static int initBinFile;

/* Read a word from file */
static SLONG Device_HSPEC_BIN_read_word(void)
{
	UBYTE buf[2];
	if (fread(buf, 1, 2, binf) != 2) {
		fclose(binf);
		binf = NULL;
		if (start_binloading) {
			start_binloading = 0;
			Aprint("binload: not valid BIN file");
			regY = 180; /* not a binary file */
			SetN;
			return -1;
		}
		if (runBinFile)
			regPC = dGetWord(0x2e0);
		regY = 1;
		ClrN;
		return -1;
	}
	return buf[0] + (buf[1] << 8);
}

static void Device_HSPEC_BIN_loader_cont(void)
{
	SLONG temp;
	UWORD from;
	UWORD to;
	UBYTE byte;

	if (binf == NULL)
		return;
	if (start_binloading) {
		dPutByte(0x244, 0);
		dPutByte(0x09, 1);
	}
	else
		regS += 2;				/* pop ESC code */

	dPutByte(0x2e3, 0xd7);
	do {
		do
			temp = Device_HSPEC_BIN_read_word();
		while (temp == 0xffff);
		if (temp < 0)
			return;
		from = (UWORD) temp;

		temp = Device_HSPEC_BIN_read_word();
		if (temp < 0)
			return;
		to = (UWORD) temp;

		if (devbug)
			Aprint("H: Load:From = %x, To = %x", from, to);

		if (start_binloading) {
			if (runBinFile)
				dPutWord(0x2e0, from);
			start_binloading = 0;
		}
		to++;
		do {
			if (fread(&byte, 1, 1, binf) == 0) {
				fclose(binf);
				binf = NULL;
				if (runBinFile)
					regPC = dGetWord(0x2e0);
				if (initBinFile && (dGetByte(0x2e3) != 0xd7)) {
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);	/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetWord(0x2e2);
				}
				return;
			}
			PutByte(from, byte);
			from++;
		} while (from != to);
	} while (!initBinFile || dGetByte(0x2e3) == 0xd7);

	regS--;
	Atari800_AddEsc((UWORD) (0x100 + regS), ESC_BINLOADER_CONT,
					Device_HSPEC_BIN_loader_cont);
	regS--;
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetWord(0x2e2);
	SetC;

	dPutByte(0x0300, 0x31);		/* for "Studio Dream" */
}

#ifdef HAVE_RENAME

static void Device_HHSPEC_Rename(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int num_failed = 0;
	int num_locked = 0;

	if (devbug)
		Aprint("RENAME Command");
	if (hd_read_only) {
		regY = 163; /* disk write-protected */
		SetN;
		return;
	}

	GET_DEVNUM;

	Device_GetFilenames();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);

#ifdef DO_DIR
	if (!Device_OpenDir(fname)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(fname, NULL))
#endif /* DO_DIR */
	{
#ifdef HAVE_STAT
		/* Check file write permission to mimic Atari
		   permission system: read-only ("locked") file
		   cannot be deleted. Modern systems have
		   a different permission for file deletion. */
		struct stat filestatus;
		stat(fname, &filestatus);
		if (!(filestatus.st_mode & S_IWRITE))
			num_locked++;
		else
#endif /* HAVE_STAT */
		{
			char newdirpart[FILENAME_MAX];
			char newfilepart[FILENAME_MAX];
			char newfname[FILENAME_MAX];
			split_path(fname, newdirpart, newfilepart);
			fillin(newfilename, newfilepart);
			cat_path(newfname, newdirpart, newfilepart);
			if (rename(fname, newfname) == 0)
				num_changed++;
			else
				num_failed++;
		}
	}

	if (devbug)
		Aprint("%d renamed, %d failed, %d locked",
		       num_changed, num_failed, num_locked);

	if (num_locked) {
		regY = 167; /* file locked */
		SetN;
	}
	else if (num_failed != 0 || num_changed == 0) {
		regY = 170; /* file not found */
		SetN;
	}
	else {
		regY = 1;
		ClrN;
	}
}

#endif /* HAVE_RENAME */

#ifdef HAVE_UNLINK

static void Device_HHSPEC_Delete(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int num_deleted = 0;
	int num_failed = 0;
	int num_locked = 0;

	if (devbug)
		Aprint("DELETE Command");
	if (hd_read_only) {
		regY = 163; /* disk write-protected */
		SetN;
		return;
	}

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);

#ifdef DO_DIR
	if (!Device_OpenDir(fname)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(fname, NULL))
#endif /* DO_DIR */
	{
#ifdef HAVE_STAT
		/* Check file write permission to mimic Atari
		   permission system: read-only ("locked") file
		   cannot be deleted. Modern systems have
		   a different permission for file deletion. */
		struct stat filestatus;
		stat(fname, &filestatus);
		if (!(filestatus.st_mode & S_IWRITE))
			num_locked++;
		else
#endif /* HAVE_STAT */
			if (unlink(fname) == 0)
				num_deleted++;
			else
				num_failed++;
	}

	if (devbug)
		Aprint("%d deleted, %d failed, %d locked",
		       num_deleted, num_failed, num_locked);

	if (num_locked) {
		regY = 167; /* file locked */
		SetN;
	}
	else if (num_failed != 0 || num_deleted == 0) {
		regY = 170; /* file not found */
		SetN;
	}
	else {
		regY = 1;
		ClrN;
	}
}

#endif /* HAVE_UNLINK */

#ifdef HAVE_CHMOD

static void Device_Chmod(/* XXX: mode_t */ int mode)
{
	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int num_failed = 0;

	if (hd_read_only) {
		regY = 163; /* disk write-protected */
		SetN;
		return;
	}

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);

#ifdef DO_DIR
	if (!Device_OpenDir(fname)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(fname, NULL))
#endif /* DO_DIR */
	{
		if (chmod(fname, mode) == 0)
			num_changed++;
		else
			num_failed++;
	}

	if (devbug)
		Aprint("%d changed, %d failed",
		       num_changed, num_failed);

	if (num_failed != 0 || num_changed == 0) {
		regY = 170; /* file not found */
		SetN;
	}
	else {
		regY = 1;
		ClrN;
	}
}

static void Device_HHSPEC_Lock(void)
{
	if (devbug)
		Aprint("LOCK Command");
	Device_Chmod(S_IREAD);
}

static void Device_HHSPEC_Unlock(void)
{
	if (devbug)
		Aprint("UNLOCK Command");
	Device_Chmod(S_IREAD | S_IWRITE);
}

#endif /* HAVE_CHMOD */

static void Device_HHSPEC_Note(void)
{
	if (devbug)
		Aprint("NOTE Command");
	if (fp[fid]) {
		long pos = ftell(fp[fid]);
		if (pos >= 0) {
			int iocb = IOCB0 + fid * 16;
			dPutByte(iocb + ICAX5, (UBYTE) pos);
			dPutByte(iocb + ICAX3, (UBYTE) (pos >> 8));
			dPutByte(iocb + ICAX4, (UBYTE) (pos >> 16));
			regY = 1;
			ClrN;
		}
		else {
			regY = 144; /* device done error */
			SetN;
		}
	}
	else {
		regY = 130; /* specified device does not exist; XXX: correct? */
		SetN;
	}
}

static void Device_HHSPEC_Point(void)
{
	if (devbug)
		Aprint("POINT Command");
	if (fp[fid]) {
		int iocb = IOCB0 + fid * 16;
		long pos = (dGetByte(iocb + ICAX4) << 16) +
			(dGetByte(iocb + ICAX3) << 8) + (dGetByte(iocb + ICAX5));
		if (fseek(fp[fid], pos, SEEK_SET) == 0) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 166; /* invalid POINT request */
			SetN;
		}
	}
	else {
		regY = 130; /* specified device does not exist; XXX: correct? */
		SetN;
	}
}

static void Device_HHSPEC_Load(int mydos)
{
	char fname[FILENAME_MAX];
	char loadpath[FILENAME_MAX];
	char *drive_root;
	int devnum;
	int i;
	UBYTE buf[2];

	if (devbug)
		Aprint("LOAD Command");

	GET_DEVNUM;

	Device_GetFilename();

	for (i = 0; i < HPathCount; i++) {
		char *p;
		if (HPathDrive[i] == -1)
			drive_root = H[devnum];
		else
			drive_root = H[HPathDrive[i]];
		loadpath[0] = 0;
		apply_relative_path(HPaths[i], loadpath);
		cat_path(fname, drive_root, loadpath);
		p = fname + strlen(fname);
		if (filename[0] != DIR_SEP_CHAR)
			*p++ = DIR_SEP_CHAR;
		strcpy(p, filename);
		binf = fopen(fname, "rb");
		if (binf != NULL)
			break;
	}
	if (binf == NULL) {
		Device_ApplyPathToFilename(devnum);
		cat_path(fname, H[devnum], filename);
		binf = fopen(fname, "rb");
		if (binf == NULL) {
			Aprint("H: load: can't open %s", filename);
			regY = 170;
			SetN;
			return;
		}
	}
	if (fread(buf, 1, 2, binf) != 2 || buf[0] != 0xff || buf[1] != 0xff) {	/* check header */
		fclose(binf);
		binf = NULL;
		Aprint("H: load: not valid BIN file");
		regY = 180;
		SetN;
		return;
	}
	/*printf("Mydos %d, AX1 %d, AX2 %d\n", mydos, dGetByte(ICAX1Z),
		   dGetByte(ICAX2Z));*/
	if (mydos) {
		switch (dGetByte(ICAX1Z)) {
		case 4:
			runBinFile = 1;
			initBinFile = 1;
			break;
		case 5:
			runBinFile = 1;
			initBinFile = 0;
			break;
		case 6:
			runBinFile = 0;
			initBinFile = 1;
			break;
		case 7:
		default:
			runBinFile = 0;
			initBinFile = 0;
			break;
		}
	}
	else {
		if (dGetByte(ICAX2Z) < 128)
			runBinFile = 1;
		else
			runBinFile = 0;
		initBinFile = 1;
	}

	start_binloading = 1;
	Device_HSPEC_BIN_loader_cont();
}

static void Device_HHSPEC_File_Length(void)
{
	if (devbug)
		Aprint("Get File Length Command");
	/* If IOCB is open, then assume it is a file length command.. */
	if (fp[fid]) {
#ifdef HAVE_FSTAT
		int iocb;
		int filesize;
		struct stat fstatus;
		iocb = IOCB0 + fid * 16;
		fstat(fileno(fp[fid]), &fstatus);
		filesize = fstatus.st_size;
		dPutByte(iocb + ICAX3, (UBYTE) filesize);
		dPutByte(iocb + ICAX4, (UBYTE) (filesize >> 8));
		dPutByte(iocb + ICAX5, (UBYTE) (filesize >> 16));
		regY = 1;
		ClrN;
#else
		regY = 168;
		SetN;
#endif
	}
	/* Otherwise, we are going to assume it is a MYDOS Load File
	   command, since they use a different XIO value */
	else
		Device_HHSPEC_Load(TRUE);
}

#ifdef HAVE_MKDIR
static void Device_HHSPEC_Mkdir(void)
{
	char fname[FILENAME_MAX];
	int devnum;

	if (devbug)
		Aprint("MKDIR Command");

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);

#if defined(WIN32) || defined(__PLUS)
	if (mkdir(fname) == 0)
#else
	umask(S_IWGRP | S_IWOTH);
	if (mkdir(fname,
		 S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH) == 0)
#endif
	{
		regY = 1;
		ClrN;
	}
	else {
		regY = 144; /* device done error */
		SetN;
	}
}
#endif

#ifdef HAVE_RMDIR
static void Device_HHSPEC_Deldir(void)
{
	char fname[FILENAME_MAX];
	int devnum;

	if (devbug)
		Aprint("DELDIR Command");

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);

	if (rmdir(fname) == 0) {
		regY = 1;
		ClrN;
	}
	else {
		regY = (errno == ENOTEMPTY)
		     ? 167 /* Sparta: Cannot delete directory */
		     : 150 /* Sparta: Directory not found */;
		SetN;
	}
}
#endif

static void Device_HHSPEC_Cd(void)
{
	char fname[FILENAME_MAX];
	int devnum;
#ifdef HAVE_STAT
	struct stat filestatus;
#endif
	char new_path[FILENAME_MAX];

	if (devbug)
		Aprint("CD Command");

	Device_GetFilename();
	GET_DEVNUM;

	strcpy(new_path, current_dir[devnum]);
	apply_relative_path(filename, new_path);
	if (new_path[0] != 0)
		cat_path(fname, H[devnum], new_path);
	else
		strcpy(fname, H[devnum]);

#ifdef HAVE_STAT
	if (stat(fname, &filestatus) != 0) {
		regY = 150;
		SetN;
		return;
	}
#endif
	if (new_path[0] == DIR_SEP_CHAR)
		strcpy(current_dir[devnum], &new_path[1]);
	else
		strcpy(current_dir[devnum], new_path);
	strcpy(current_path[devnum], H[devnum]);
	if (current_dir[devnum][0] != 0) {
		strcat(current_path[devnum], DIR_SEP_STR);
		strcat(current_path[devnum], current_dir[devnum]);
	}
	regY = 1;
	ClrN;
}

static void Device_HHSPEC_Disk_Info(void)
{
	static UBYTE info[17] = {
		3, 0, 231, 3, 231, 3,
		'H', 'D', 'I', 'S', 'K', '0' /* + devnum */, ' ', ' ',
		0 /* devnum */, 0 /* devnum */, 0 };
	int devnum;

	if (devbug)
		Aprint("Get Disk Information Command");

	GET_DEVNUM;

	info[11] = (UBYTE) ('0' + devnum);
	info[14] = (UBYTE) devnum;
	info[15] = (UBYTE) devnum;
	CopyToMem(info, dGetWord(ICBLLZ), 17);

	regY = 1;
	ClrN;
}

static void Device_HHSPEC_Current_Dir(void)
{
	int devnum;
	char new_path[FILENAME_MAX];
	int bufadr;
	int pathlen;
	int i;

	if (devbug)
		Aprint("Get Current Directory Command");

	GET_DEVNUM;

	bufadr = dGetWord(ICBLLZ);

	new_path[0] = '>';
	strcpy(&new_path[1], current_dir[devnum]);

	pathlen = strlen(new_path);

	for (i = 0; i < pathlen; i++) {
		if (new_path[i] == DIR_SEP_CHAR)
			new_path[i] = '>';
	}
	strtoupper(new_path);
	new_path[pathlen] = (char) 0x9b;

	for (i = 0; i < pathlen + 1; i++)
		PutByte((UWORD) (bufadr + i), (UBYTE) new_path[i]);

	regY = 1;
	ClrN;
}

static void Device_HHSPEC(void)
{
	if (devbug)
		Aprint("HHSPEC");

	GET_FID;

	switch (dGetByte(ICCOMZ)) {
#ifdef HAVE_RENAME
	case 0x20:
		Device_HHSPEC_Rename();
		return;
#endif
#ifdef HAVE_UNLINK
	case 0x21:
		Device_HHSPEC_Delete();
		return;
#endif
#ifdef HAVE_CHMOD
	case 0x23:
		Device_HHSPEC_Lock();
		return;
	case 0x24:
		Device_HHSPEC_Unlock();
		return;
#endif
	case 0x26:
		Device_HHSPEC_Note();
		return;
	case 0x25:
		Device_HHSPEC_Point();
		return;
	case 0x27:
		Device_HHSPEC_File_Length();
		return;
	case 0x28:
		Device_HHSPEC_Load(FALSE);
		return;
#ifdef HAVE_MKDIR
	case 0x2A:
		Device_HHSPEC_Mkdir();
		return;
#endif
#ifdef HAVE_RMDIR
	case 0x2B:
		Device_HHSPEC_Deldir();
		return;
#endif
	case 0x2C:
		Device_HHSPEC_Cd();
		return;
	case 0x2F:
		Device_HHSPEC_Disk_Info();
		return;
	case 0x30:
		Device_HHSPEC_Current_Dir();
		return;
	case 0xFE:
		if (devbug)
			Aprint("FORMAT Command");
		break;
	default:
		if (devbug)
			Aprint("UNKNOWN Command %02x", dGetByte(ICCOMZ));
		break;
	}

	regY = 168; /* invalid device command */
	SetN;
}

static void Device_HHINIT(void)
{
	int i;

	if (devbug)
		Aprint("HHINIT");

	Device_GeneratePath();
	for (i = 0; i < 5; i++) {
		strcpy(current_path[i], H[i]);
		current_dir[i][0] = 0;
	}
}

#ifdef HAVE_SYSTEM

static FILE *phf = NULL;
static void Device_PHCLOS(void);
static char spool_file[FILENAME_MAX];

static void Device_PHOPEN(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phf)
		Device_PHCLOS();

	phf = Atari_tmpfile(spool_file, "w");
	if (phf) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144; /* device done error */
		SetN;
	}
}

static void Device_PHCLOS(void)
{
	if (devbug)
		Aprint("PHCLOS");

	if (phf) {
		fclose(phf);
		phf = NULL;

#ifdef __PLUS
		if (!Misc_ExecutePrintCmd(spool_file))
#endif
		{
			char command[256 + FILENAME_MAX]; /* 256 for print_command + FILENAME_MAX for spool_file */
			sprintf(command, print_command, spool_file);
			system(command);
#if defined(HAVE_UNLINK) && !defined(VMS) && !defined(MACOSX)
			if (unlink(spool_file) == -1) {
				perror(spool_file);
			}
#endif
		}
	}
	regY = 1;
	ClrN;
}

static void Device_PHWRIT(void)
{
	UBYTE byte;
	int status;

	if (devbug)
		Aprint("PHWRIT");

	byte = regA;
	if (byte == 0x9b)
		byte = '\n';

	status = fwrite(&byte, 1, 1, phf);
	if (status == 1) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144; /* device done error */
		SetN;
	}
}

static void Device_PHSTAT(void)
{
	if (devbug)
		Aprint("PHSTAT");
}

static void Device_PHINIT(void)
{
	if (devbug)
		Aprint("PHINIT");

	if (phf) {
		fclose(phf);
		phf = NULL;
#ifdef HAVE_UNLINK
		unlink(spool_file);
#endif
	}
	regY = 1;
	ClrN;
}

#endif /* HAVE_SYSTEM */

/* K: and E: handlers for BASIC version, using getchar() and putchar() */

#ifdef BASIC

static void Device_EHREAD(void)
{
	int ch;

	ch = getchar();
	switch (ch) {
	case EOF:
		Atari800_Exit(FALSE);
		exit(0);
		break;
	case '\n':
		ch = 0x9b;
		break;
	default:
		break;
	}
	regA = (UBYTE) ch;
	regY = 1;
	ClrN;
}

static void Device_EHWRIT(void)
{
	UBYTE ch;

	ch = regA;
	switch (ch) {
	case 0x7d:
		putchar('*');
		break;
	case 0x9b:
		putchar('\n');
		break;
	default:
		if ((ch >= 0x20) && (ch <= 0x7e))	/* for DJGPP */
			putchar(ch);
		break;
	}
	regY = 1;
	ClrN;
}

#endif /* BASIC */

/* Atari BASIC loader */

static UWORD ehopen_addr = 0;
static UWORD ehclos_addr = 0;
static UWORD ehread_addr = 0;
static UWORD ehwrit_addr = 0;

static void Device_IgnoreReady(void);
static void Device_GetBasicCommand(void);
static void Device_OpenBasicFile(void);
static void Device_ReadBasicFile(void);
static void Device_CloseBasicFile(void);

static void Device_RestoreHandler(UWORD address, UBYTE esc_code)
{
	Atari800_RemoveEsc(esc_code);
	/* restore original OS code */
	dCopyToMem(machine_type == MACHINE_XLXE
	            ? atari_os + address - 0xc000
	            : atari_os + address - 0xd800,
	           address, 3);
}

static void Device_RestoreEHOPEN(void)
{
	Device_RestoreHandler(ehopen_addr, ESC_EHOPEN);
}

static void Device_RestoreEHCLOS(void)
{
	Device_RestoreHandler(ehclos_addr, ESC_EHCLOS);
}

#ifndef BASIC

static void Device_RestoreEHREAD(void)
{
	Device_RestoreHandler(ehread_addr, ESC_EHREAD);
}

static void Device_RestoreEHWRIT(void)
{
	Device_RestoreHandler(ehwrit_addr, ESC_EHWRIT);
}

static void Device_InstallIgnoreReady(void)
{
	Atari800_AddEscRts(ehwrit_addr, ESC_EHWRIT, Device_IgnoreReady);
}

#endif

/* Atari Basic loader step 1: ignore "READY" printed on E: after booting */
/* or step 6: ignore "READY" printed on E: after the "ENTER" command */

static const UBYTE * const ready_prompt = (const UBYTE *) "\x9bREADY\x9b";

static const UBYTE *ready_ptr = NULL;

static const UBYTE *basic_command_ptr = NULL;

static void Device_IgnoreReady(void)
{
	if (ready_ptr != NULL && regA == *ready_ptr) {
		ready_ptr++;
		if (*ready_ptr == '\0') {
			ready_ptr = NULL;
			/* uninstall patch */
#ifdef BASIC
			Atari800_AddEscRts(ehwrit_addr, ESC_EHWRIT, Device_EHWRIT);
#else
			rts_handler = Device_RestoreEHWRIT;
#endif
			if (loading_basic == LOADING_BASIC_SAVED) {
				basic_command_ptr = (const UBYTE *) "RUN \"E:\"\x9b";
				Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_GetBasicCommand);
			}
			else if (loading_basic == LOADING_BASIC_LISTED) {
				basic_command_ptr = (const UBYTE *) "ENTER \"E:\"\x9b";
				Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_GetBasicCommand);
			}
			else if (loading_basic == LOADING_BASIC_RUN) {
				basic_command_ptr = (const UBYTE *) "RUN\x9b";
				Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_GetBasicCommand);
			}
		}
		regY = 1;
		ClrN;
		return;
	}
	/* not "READY" (maybe "BOOT ERROR" or a DOS message) */
	if (loading_basic == LOADING_BASIC_RUN) {
		/* don't "RUN" if no "READY" (probably "ERROR") */
		loading_basic = 0;
		ready_ptr = NULL;
	}
	if (ready_ptr != NULL) {
		/* If ready_ptr != ready_prompt then we skipped some characters
		   from ready_prompt, which weren't part of full ready_prompt.
		   Well, they probably weren't that important. :-) */
		ready_ptr = ready_prompt;
	}
	/* call original handler */
#ifdef BASIC
	Device_EHWRIT();
#else
	rts_handler = Device_InstallIgnoreReady;
	Device_RestoreEHWRIT();
	regPC = ehwrit_addr;
#endif
}

/* Atari Basic loader step 2: type command to load file from E: */
/* or step 7: type "RUN" for ENTERed program */

static void Device_GetBasicCommand(void)
{
	if (basic_command_ptr != NULL) {
		regA = *basic_command_ptr++;
		regY = 1;
		ClrN;
		if (*basic_command_ptr != '\0')
			return;
		if (loading_basic == LOADING_BASIC_SAVED || loading_basic == LOADING_BASIC_LISTED)
			Atari800_AddEscRts(ehopen_addr, ESC_EHOPEN, Device_OpenBasicFile);
		basic_command_ptr = NULL;
	}
#ifdef BASIC
	Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_EHREAD);
#else
	rts_handler = Device_RestoreEHREAD;
#endif
}

/* Atari Basic loader step 3: open file */

static void Device_OpenBasicFile(void)
{
	if (bin_file != NULL) {
		fseek(bin_file, 0, SEEK_SET);
		Atari800_AddEscRts(ehclos_addr, ESC_EHCLOS, Device_CloseBasicFile);
		Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_ReadBasicFile);
		regY = 1;
		ClrN;
	}
	rts_handler = Device_RestoreEHOPEN;
}

/* Atari Basic loader step 4: read byte */

static void Device_ReadBasicFile(void)
{
	if (bin_file != NULL) {
		int ch = fgetc(bin_file);
		if (ch == EOF) {
			regY = 136;
			SetN;
			return;
		}
		switch (loading_basic) {
		case LOADING_BASIC_LISTED:
			switch (ch) {
			case 0x9b:
				loading_basic = LOADING_BASIC_LISTED_ATARI;
				break;
			case 0x0a:
				loading_basic = LOADING_BASIC_LISTED_LF;
				ch = 0x9b;
				break;
			case 0x0d:
				loading_basic = LOADING_BASIC_LISTED_CR_OR_CRLF;
				ch = 0x9b;
				break;
			default:
				break;
			}
			break;
		case LOADING_BASIC_LISTED_CR:
			if (ch == 0x0d)
				ch = 0x9b;
			break;
		case LOADING_BASIC_LISTED_LF:
			if (ch == 0x0a)
				ch = 0x9b;
			break;
		case LOADING_BASIC_LISTED_CRLF:
			if (ch == 0x0a) {
				ch = fgetc(bin_file);
				if (ch == EOF) {
					regY = 136;
					SetN;
					return;
				}
			}
			if (ch == 0x0d)
				ch = 0x9b;
			break;
		case LOADING_BASIC_LISTED_CR_OR_CRLF:
			if (ch == 0x0a) {
				loading_basic = LOADING_BASIC_LISTED_CRLF;
				ch = fgetc(bin_file);
				if (ch == EOF) {
					regY = 136;
					SetN;
					return;
				}
			}
			else
				loading_basic = LOADING_BASIC_LISTED_CR;
			if (ch == 0x0d)
				ch = 0x9b;
			break;
		case LOADING_BASIC_SAVED:
		case LOADING_BASIC_LISTED_ATARI:
		default:
			break;
		}
		regA = (UBYTE) ch;
		regY = 1;
		ClrN;
	}
}

/* Atari Basic loader step 5: close file */

static void Device_CloseBasicFile(void)
{
	if (bin_file != NULL) {
		fclose(bin_file);
		bin_file = NULL;
		/* "RUN" ENTERed program */
		if (loading_basic != 0 && loading_basic != LOADING_BASIC_SAVED) {
			ready_ptr = ready_prompt;
			Atari800_AddEscRts(ehwrit_addr, ESC_EHWRIT, Device_IgnoreReady);
			loading_basic = LOADING_BASIC_RUN;
		}
		else
			loading_basic = 0;
	}
#ifdef BASIC
	Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_EHREAD);
#else
	Device_RestoreEHREAD();
#endif
	rts_handler = Device_RestoreEHCLOS;
	regY = 1;
	ClrN;
}

/* Device_PatchOS is called by Atari800_PatchOS to modify standard device
   handlers in Atari OS. It puts escape codes at beginnings of OS routines,
   so the patches work even if they are called directly, without CIO.
   Returns TRUE if something has been patched.
   Currently we only patch P: and, in BASIC version, E: and K:.
   We don't replace C: with H: now, so the cassette works even
   if H: is enabled.
*/
int Device_PatchOS(void)
{
	UWORD addr;
	UWORD devtab;
	int i;
	int patched = FALSE;

	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		addr = 0xf0e3;
		break;
	case MACHINE_XLXE:
		addr = 0xc42e;
		break;
	default:
		Aprint("Fatal Error in Device_PatchOS(): Unknown machine");
		return patched;
	}

	for (i = 0; i < 5; i++) {
		devtab = dGetWord(addr + 1);
		switch (dGetByte(addr)) {
#ifdef HAVE_SYSTEM
		case 'P':
			if (enable_p_patch) {
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_OPEN) + 1),
				                   ESC_PHOPEN, Device_PHOPEN);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_CLOS) + 1),
				                   ESC_PHCLOS, Device_PHCLOS);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_WRIT) + 1),
				                   ESC_PHWRIT, Device_PHWRIT);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_STAT) + 1),
				                   ESC_PHSTAT, Device_PHSTAT);
				Atari800_AddEscRts2((UWORD) (devtab + DEVICE_TABLE_INIT), ESC_PHINIT,
				                    Device_PHINIT);
				patched = TRUE;
			}
			else {
				Atari800_RemoveEsc(ESC_PHOPEN);
				Atari800_RemoveEsc(ESC_PHCLOS);
				Atari800_RemoveEsc(ESC_PHWRIT);
				Atari800_RemoveEsc(ESC_PHSTAT);
				Atari800_RemoveEsc(ESC_PHINIT);
			}
			break;
#endif

		case 'E':
			if (loading_basic) {
				ehopen_addr = dGetWord(devtab + DEVICE_TABLE_OPEN) + 1;
				ehclos_addr = dGetWord(devtab + DEVICE_TABLE_CLOS) + 1;
				ehread_addr = dGetWord(devtab + DEVICE_TABLE_READ) + 1;
				ehwrit_addr = dGetWord(devtab + DEVICE_TABLE_WRIT) + 1;
				ready_ptr = ready_prompt;
				Atari800_AddEscRts(ehwrit_addr, ESC_EHWRIT, Device_IgnoreReady);
				patched = TRUE;
			}
#ifdef BASIC
			else
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_WRIT) + 1),
				                   ESC_EHWRIT, Device_EHWRIT);
			Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_READ) + 1),
			                   ESC_EHREAD, Device_EHREAD);
			patched = TRUE;
			break;
		case 'K':
			Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_READ) + 1),
			                   ESC_KHREAD, Device_EHREAD);
			patched = TRUE;
			break;
#endif
		default:
			break;
		}
		addr += 3;				/* Next Device in HATABS */
	}
	return patched;
}

/* New handling of H: device.
   Previously we simply replaced C: device in OS with our H:.
   Now we don't change ROM for H: patch, but add H: to HATABS in RAM
   and put the device table and patches in unused area of address space
   (0xd100-0xd1ff), which is meant for 'new devices' (like hard disk).
   We have to contiunously check if our H: is still in HATABS,
   because RESET routine in Atari OS clears HATABS and initializes it
   using a table in ROM (see Device_PatchOS).
   Before we put H: entry in HATABS, we must make sure that HATABS is there.
   For example a program, that doesn't use Atari OS, could use this memory
   area for its own data, and we shouldn't place 'H' there.
   We also allow an Atari program to change address of H: device table.
   So after we put H: entry in HATABS, we only check if 'H' is still where
   we put it (h_entry_address).
   Device_UpdateHATABSEntry and Device_RemoveHATABSEntry can be used to add
   other devices than H:.
*/

#define HATABS 0x31a

UWORD Device_UpdateHATABSEntry(char device, UWORD entry_address,
							   UWORD table_address)
{
	UWORD address;
	if (entry_address != 0 && dGetByte(entry_address) == device)
		return entry_address;
	if (dGetByte(HATABS) != 'P' || dGetByte(HATABS + 3) != 'C'
		|| dGetByte(HATABS + 6) != 'E' || dGetByte(HATABS + 9) != 'S'
		|| dGetByte(HATABS + 12) != 'K')
		return entry_address;
	for (address = HATABS + 15; address < HATABS + 33; address += 3) {
		if (dGetByte(address) == device)
			return address;
		if (dGetByte(address) == 0) {
			dPutByte(address, device);
			dPutWord(address + 1, table_address);
			return address;
		}
	}
	/* HATABS full */
	return entry_address;
}

void Device_RemoveHATABSEntry(char device, UWORD entry_address,
							  UWORD table_address)
{
	if (entry_address != 0 && dGetByte(entry_address) == device
		&& dGetWord(entry_address + 1) == table_address) {
		dPutByte(entry_address, 0);
		dPutWord(entry_address + 1, 0);
	}
}

static UWORD h_entry_address = 0;
#ifdef R_IO_DEVICE
static UWORD r_entry_address = 0;
#endif

#define H_DEVICE_BEGIN  0xd140
#define H_TABLE_ADDRESS 0xd140
#define H_PATCH_OPEN    0xd150
#define H_PATCH_CLOS    0xd153
#define H_PATCH_READ    0xd156
#define H_PATCH_WRIT    0xd159
#define H_PATCH_STAT    0xd15c
#define H_PATCH_SPEC    0xd15f
#define H_DEVICE_END    0xd161

#ifdef R_IO_DEVICE
#define R_DEVICE_BEGIN  0xd180
#define R_TABLE_ADDRESS 0xd180
#define R_PATCH_OPEN    0xd1a0
#define R_PATCH_CLOS    0xd1a3
#define R_PATCH_READ    0xd1a6
#define R_PATCH_WRIT    0xd1a9
#define R_PATCH_STAT    0xd1ac
#define R_PATCH_SPEC    0xd1af
#define R_PATCH_INIT    0xd1b3
#define R_DEVICE_END    0xd1b5
#endif

void Device_Frame(void)
{
	if (enable_h_patch)
		h_entry_address = Device_UpdateHATABSEntry('H', h_entry_address, H_TABLE_ADDRESS);

#ifdef R_IO_DEVICE
	if (enable_r_patch)
		r_entry_address = Device_UpdateHATABSEntry('R', r_entry_address, R_TABLE_ADDRESS);
#endif
}

/* this is called when enable_h_patch is toggled */
void Device_UpdatePatches(void)
{
	if (enable_h_patch) {		/* enable H: device */
		/* change memory attributes for the area, where we put
		   the H: handler table and patches */
		SetROM(H_DEVICE_BEGIN, H_DEVICE_END);
		/* set handler table */
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_OPEN, H_PATCH_OPEN - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_CLOS, H_PATCH_CLOS - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_READ, H_PATCH_READ - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_WRIT, H_PATCH_WRIT - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_STAT, H_PATCH_STAT - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_SPEC, H_PATCH_SPEC - 1);
		/* set patches */
		Atari800_AddEscRts(H_PATCH_OPEN, ESC_HHOPEN, Device_HHOPEN);
		Atari800_AddEscRts(H_PATCH_CLOS, ESC_HHCLOS, Device_HHCLOS);
		Atari800_AddEscRts(H_PATCH_READ, ESC_HHREAD, Device_HHREAD);
		Atari800_AddEscRts(H_PATCH_WRIT, ESC_HHWRIT, Device_HHWRIT);
		Atari800_AddEscRts(H_PATCH_STAT, ESC_HHSTAT, Device_HHSTAT);
		Atari800_AddEscRts(H_PATCH_SPEC, ESC_HHSPEC, Device_HHSPEC);
		/* H: in HATABS will be added next frame by Device_Frame */
	}
	else {						/* disable H: device */
		/* remove H: entry from HATABS */
		Device_RemoveHATABSEntry('H', h_entry_address, H_TABLE_ADDRESS);
		/* remove patches */
		Atari800_RemoveEsc(ESC_HHOPEN);
		Atari800_RemoveEsc(ESC_HHCLOS);
		Atari800_RemoveEsc(ESC_HHREAD);
		Atari800_RemoveEsc(ESC_HHWRIT);
		Atari800_RemoveEsc(ESC_HHSTAT);
		Atari800_RemoveEsc(ESC_HHSPEC);
		/* fill memory area used for table and patches with 0xff */
		dFillMem(H_DEVICE_BEGIN, 0xff, H_DEVICE_END - H_DEVICE_BEGIN + 1);
	}

#ifdef R_IO_DEVICE
	if (enable_r_patch) {		/* enable R: device */
		/* change memory attributes for the area, where we put
		   the R: handler table and patches */
		SetROM(R_DEVICE_BEGIN, R_DEVICE_END);
		/* set handler table */
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_OPEN, R_PATCH_OPEN - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_CLOS, R_PATCH_CLOS - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_READ, R_PATCH_READ - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_WRIT, R_PATCH_WRIT - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_STAT, R_PATCH_STAT - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_SPEC, R_PATCH_SPEC - 1);
		dPutWord(R_TABLE_ADDRESS + DEVICE_TABLE_INIT, R_PATCH_INIT - 1);
		/* set patches */
		Atari800_AddEscRts(R_PATCH_OPEN, ESC_ROPEN, Device_ROPEN);
		Atari800_AddEscRts(R_PATCH_CLOS, ESC_RCLOS, Device_RCLOS);
		Atari800_AddEscRts(R_PATCH_READ, ESC_RREAD, Device_RREAD);
		Atari800_AddEscRts(R_PATCH_WRIT, ESC_RWRIT, Device_RWRIT);
		Atari800_AddEscRts(R_PATCH_STAT, ESC_RSTAT, Device_RSTAT);
		Atari800_AddEscRts(R_PATCH_SPEC, ESC_RSPEC, Device_RSPEC);
		Atari800_AddEscRts(R_PATCH_INIT, ESC_RINIT, Device_RINIT);
		/* R: in HATABS will be added next frame by Device_Frame */
	}
	else {						/* disable R: device */
		/* remove R: entry from HATABS */
		Device_RemoveHATABSEntry('R', r_entry_address, R_TABLE_ADDRESS);
		/* remove patches */
		Atari800_RemoveEsc(ESC_ROPEN);
		Atari800_RemoveEsc(ESC_RCLOS);
		Atari800_RemoveEsc(ESC_RREAD);
		Atari800_RemoveEsc(ESC_RWRIT);
		Atari800_RemoveEsc(ESC_RSTAT);
		Atari800_RemoveEsc(ESC_RSPEC);
		/* fill memory area used for table and patches with 0xff */
		dFillMem(R_DEVICE_BEGIN, 0xff, R_DEVICE_END - R_DEVICE_BEGIN + 1);
	}
#endif /* defined(R_IO_DEVICE) */
}

/*
$Log$
Revision 1.38  2005/08/31 20:12:15  pfusik
created Device_OpenDir() and Device_ReadDir(); improved H5:-H9;
Device_HHSPEC_Disk_Info(), Device_HHSPEC_Current_Dir() no longer use dPutByte()

Revision 1.37  2005/08/27 10:37:07  pfusik
DEFAULT_H_PATH moved to devices.h

Revision 1.36  2005/08/24 21:15:57  pfusik
H: and R: work with PAGED_ATTRIB

Revision 1.35  2005/08/23 03:50:19  markgrebe
Fixed month on modification date on directory listing.  Should be 1-12, not 0-11

Revision 1.34  2005/08/22 20:48:13  pfusik
avoid <ctype.h>

Revision 1.33  2005/08/21 15:42:10  pfusik
Atari_tmpfile(); DO_DIR -> HAVE_OPENDIR;
#ifdef HAVE_ERRNO_H; #ifdef HAVE_REWIND

Revision 1.32  2005/08/17 22:32:34  pfusik
direct.h and io.h on WIN32; fixed VC6 warnings

Revision 1.31  2005/08/16 23:09:56  pfusik
#include "config.h" before system headers;
dirty workaround for lack of mktemp();
#ifdef HAVE_LOCALTIME; #ifdef HAVE_SYSTEM

Revision 1.30  2005/08/15 17:20:25  pfusik
Basic loader; more characters valid in H: filenames

Revision 1.29  2005/08/10 19:54:16  pfusik
patching E: open doesn't make sense; fixed some warnings

Revision 1.28  2005/08/07 13:43:32  pfusik
MSVC headers have no S_IRUSR nor S_IWUSR
empty Hx_DIR now refers to the current directory rather than the root

Revision 1.27  2005/08/06 18:13:34  pfusik
check for rename() and snprintf()
fixed error codes
fixed "unused" warnings
other minor fixes

Revision 1.26  2005/08/04 22:50:24  pfusik
fancy I/O functions may be unavailable; got rid of numerous #ifdef BACK_SLASH

Revision 1.25  2005/03/24 18:11:47  pfusik
added "-help"

Revision 1.24  2005/03/08 04:32:41  pfusik
killed gcc -W warnings

Revision 1.23  2004/07/02 11:28:27  sba
Don't remove DO_DIR define when compiling for Amiga.

Revision 1.22  2003/09/14 20:07:30  joy
O_BINARY defined

Revision 1.21  2003/09/14 19:30:32  joy
mkstemp emulated if unavailable

Revision 1.20  2003/08/31 21:57:44  joy
rdevice module compiled in conditionally

Revision 1.19  2003/05/28 19:54:58  joy
R: device support (networking?)

Revision 1.18  2003/03/03 09:57:33  joy
Ed improved autoconf again plus fixed some little things

Revision 1.17  2003/02/24 09:32:54  joy
header cleanup

Revision 1.16  2003/01/27 13:14:54  joy
Jason's changes: either PAGED_ATTRIB support (mostly), or just clean up.

Revision 1.15  2003/01/27 12:55:23  joy
harddisk emulation now fully supports subdirectories.

Revision 1.12  2001/10/03 16:40:54  fox
rewritten escape codes handling,
corrected Device_isvalid (isalnum((char) 0x9b) == 1 !)

Revision 1.11  2001/10/01 17:10:34  fox
#include "ui.h" for CRASH_MENU externs

Revision 1.10  2001/09/21 16:54:11  fox
removed unused variable

Revision 1.9  2001/07/20 00:30:08  fox
replaced K_Device with Device_KHREAD,
replaced E_Device with Device_EHOPEN, Device_EHREAD and Device_EHWRITE,
removed ESC_BREAK

Revision 1.6  2001/03/25 06:57:35  knik
open() replaced by fopen()

Revision 1.5  2001/03/22 06:16:58  knik
removed basic improvement

Revision 1.4  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
