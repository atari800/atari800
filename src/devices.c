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
#include "binload.h"
#include "cpu.h"
#include "devices.h"
#include "log.h"
#include "memory.h"
#include "pia.h" /* atari_os */
#include "sio.h"
#include "util.h"
#ifdef R_IO_DEVICE
#include "rdevice.h"
#endif
#ifdef __PLUS
#include "misc_win.h"
#endif

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

#ifdef WIN32

#include <windows.h>

#undef FILENAME_CONV
#undef FILENAME

#ifdef UNICODE
#define FILENAME_CONV \
	WCHAR wfilename[FILENAME_MAX]; \
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0) \
		return FALSE;
#define FILENAME wfilename
#else /* UNICODE */
#define FILENAME_CONV
#define FILENAME filename
#endif /* UNICODE */

#endif /* WIN32 */


/* Read Directory abstraction layer -------------------------------------- */

#ifdef WIN32

static char dir_path[FILENAME_MAX];
static WIN32_FIND_DATA wfd;
static HANDLE dh = INVALID_HANDLE_VALUE;

static int Device_OpenDir(const char *filename)
{
	FILENAME_CONV;
	Util_splitpath(filename, dir_path, NULL);
	if (dh != INVALID_HANDLE_VALUE)
		FindClose(dh);
	dh = FindFirstFile(FILENAME, &wfd);
	if (dh == INVALID_HANDLE_VALUE) {
		/* don't raise error if the path is ok but no file matches:
		   Win98 returns ERROR_FILE_NOT_FOUND,
		   WinCE returns ERROR_NO_MORE_FILES */
		DWORD err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
			return FALSE;
	}
	return TRUE;
}

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
#ifdef UNICODE
	char afilename[MAX_PATH];
#endif
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
#ifdef UNICODE
	if (WideCharToMultiByte(CP_ACP, 0, wfd.cFileName, -1, afilename, MAX_PATH, NULL, NULL) <= 0)
		strcpy(afilename, "?ERROR");
#define FOUND_FILENAME afilename
#else
#define FOUND_FILENAME wfd.cFileName
#endif /* UNICODE */
	if (filename != NULL)
		strcpy(filename, FOUND_FILENAME);
	if (fullpath != NULL)
		Util_catpath(fullpath, dir_path, FOUND_FILENAME);
	if (isdir != NULL)
		*isdir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
	if (readonly != NULL)
		*readonly = (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? TRUE : FALSE;
	if (size != NULL)
		*size = (int) wfd.nFileSizeLow;
	if (timetext != NULL) {
		FILETIME lt;
		SYSTEMTIME st;
		if (FileTimeToLocalFileTime(&wfd.ftLastWriteTime, &lt) != 0
		 && FileTimeToSystemTime(&lt, &st) != 0) {
			int hour = st.wHour;
			char ampm = 'a';
			if (hour >= 12) {
				hour -= 12;
				ampm = 'p';
			}
			if (hour == 0)
				hour = 12;
			sprintf(timetext, "%2d-%02d-%02d %2d:%02d%c",
				st.wMonth, st.wDay, st.wYear % 100, hour, st.wMinute, ampm);
		}
		else
			strcpy(timetext, " 1-01-01 12:00p");
	}

	if (!FindNextFile(dh, &wfd)) {
		FindClose(dh);
		dh = INVALID_HANDLE_VALUE;
	}
	return TRUE;
}

#define DO_DIR

#elif defined(HAVE_OPENDIR)

static int match(const char *pattern, const char *filename)
{
	if (strcmp(pattern, "*.*") == 0)
		return TRUE;

	for (;;) {
		switch (*pattern) {
		case '\0':
			return (*filename == '\0');
		case '?':
			if (*filename == '\0' || *filename == '.')
				return FALSE;
			pattern++;
			filename++;
			break;
		case '*':
			if (Util_chrieq(*filename, pattern[1]))
				pattern++;
			else if (*filename == '\0')
				return FALSE; /* because pattern[1] != '\0' */
			else
				filename++;
			break;
		default:
			if (!Util_chrieq(*pattern, *filename))
				return FALSE;
			pattern++;
			filename++;
			break;
		}
	}
}

static char dir_path[FILENAME_MAX];
static char filename_pattern[FILENAME_MAX];
static DIR *dp = NULL;

static int Device_OpenDir(const char *filename)
{
	Util_splitpath(filename, dir_path, filename_pattern);
	if (dp != NULL)
		closedir(dp);
	dp = opendir(dir_path);
	return dp != NULL;
}

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
	struct dirent *entry;
	char temppath[FILENAME_MAX];
#ifdef HAVE_STAT
	struct stat status;
#endif
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
	Util_catpath(temppath, dir_path, entry->d_name);
	if (fullpath != NULL)
		strcpy(fullpath, temppath);
#ifdef HAVE_STAT
	if (stat(temppath, &status) == 0) {
		if (isdir != NULL)
			*isdir = (status.st_mode & S_IFDIR) ? TRUE : FALSE;
		if (readonly != NULL)
			*readonly = (status.st_mode & S_IWRITE) ? FALSE : TRUE;
		if (size != NULL)
			*size = (int) status.st_size;
		if (timetext != NULL) {
#ifdef HAVE_LOCALTIME
			struct tm *ft;
			int hour;
			char ampm = 'a';
			ft = localtime(&status.st_mtime);
			hour = ft->tm_hour;
			if (hour >= 12) {
				hour -= 12;
				ampm = 'p';
			}
			if (hour == 0)
				hour = 12;
			sprintf(timetext, "%2d-%02d-%02d %2d:%02d%c",
				ft->tm_mon + 1, ft->tm_mday, ft->tm_year % 100,
				hour, ft->tm_min, ampm);
#else
			strcpy(timetext, " 1-01-01 12:00p");
#endif /* HAVE_LOCALTIME */
		}
	}
	else
#endif /* HAVE_STAT */
	{
		if (isdir != NULL)
			*isdir = FALSE;
		if (readonly != NULL)
			*readonly = FALSE;
		if (size != NULL)
			*size = 0;
		if (timetext != NULL)
			strcpy(timetext, " 1-01-01 12:00p");
	}
	return TRUE;
}

#define DO_DIR

#elif defined(PS2)

extern char dir_path[FILENAME_MAX];

int Atari_OpenDir(const char *filename);

#define Device_OpenDir Atari_OpenDir

int Atari_ReadDir(char *fullpath, char *filename, int *isdir,
                  int *readonly, int *size, char *timetext);

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
	char tmp_filename[FILENAME_MAX];
	if (filename == NULL)
		filename = tmp_filename;
	do {
		if (!Atari_ReadDir(fullpath, filename, isdir, readonly, size, timetext))
			return FALSE;
		/* reject "." and ".." */
	} while (filename[0] == '.' &&
	         (filename[1] == '\0' || (filename[1] == '.' && filename[2] == '\0')));
	return TRUE;
}

#define DO_DIR

#endif /* defined(PS2) */


/* Rename File/Directory abstraction layer ------------------------------- */

#ifdef WIN32

static int Device_Rename(const char *oldname, const char *newname)
{
#ifdef UNICODE
	WCHAR woldname[FILENAME_MAX];
	WCHAR wnewname[FILENAME_MAX];
	if (MultiByteToWideChar(CP_ACP, 0, oldname, -1, woldname, FILENAME_MAX) <= 0
	 || MultiByteToWideChar(CP_ACP, 0, newname, -1, wnewname, FILENAME_MAX) <= 0)
		return FALSE;
	return MoveFile(woldname, wnewname) != 0;
#else
	return MoveFile(oldname, newname) != 0;
#endif /* UNICODE */
}

#define DO_RENAME

#elif defined(HAVE_RENAME)

static int Device_Rename(const char *oldname, const char *newname)
{
	return rename(oldname, newname) == 0;
}

#define DO_RENAME

#endif


/* Set/Reset Read-Only Attribute abstraction layer ----------------------- */

#ifdef WIN32

/* Enables/disables read-only mode for the file. Returns TRUE on success. */
static int Device_SetReadOnly(const char *filename, int readonly)
{
	DWORD attr;
	FILENAME_CONV;
	attr = GetFileAttributes(FILENAME);
	if (attr == 0xffffffff)
		return FALSE;
	return SetFileAttributes(FILENAME, readonly
		? (attr | FILE_ATTRIBUTE_READONLY)
		: (attr & ~FILE_ATTRIBUTE_READONLY)) != 0;
}

#define DO_LOCK

#elif defined(HAVE_CHMOD)

static int Device_SetReadOnly(const char *filename, int readonly)
{
	return chmod(filename, readonly ? S_IREAD : (S_IREAD | S_IWRITE)) == 0;
}

#define DO_LOCK

#endif /* defined(HAVE_CHMOD) */


/* Make Directory abstraction layer -------------------------------------- */

#ifdef WIN32

static int Device_MakeDirectory(const char *filename)
{
	FILENAME_CONV;
	return CreateDirectory(FILENAME, NULL) != 0;
}

#define DO_MKDIR

#elif defined(HAVE_MKDIR)

static int Device_MakeDirectory(const char *filename)
{
	/* XXX: I don't see any good reason why umask() and limited permissions were used. */
	/* umask(S_IWGRP | S_IWOTH); */
	return (mkdir(filename, 0777
		 /* S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH */) == 0);
}

#define DO_MKDIR

#endif /* defined(HAVE_MKDIR) */


/* Remove Directory abstraction layer ------------------------------------ */

#ifdef WIN32

static UBYTE Device_RemoveDirectory(const char *filename)
{
	FILENAME_CONV;
	if (RemoveDirectory(FILENAME) != 0)
		return 1;
	return (UBYTE) ((HRESULT_CODE(GetLastError()) == ERROR_DIR_NOT_EMPTY) ? 167 : 150);
}

#define DO_RMDIR

#elif defined(HAVE_RMDIR)

static UBYTE Device_RemoveDirectory(const char *filename)
{
	if (rmdir(filename) == 0)
		return 1;
	return (UBYTE) ((errno == ENOTEMPTY) ? 167 : 150);
}

#define DO_RMDIR

#endif /* defined(HAVE_RMDIR) */


/* H: device emulation --------------------------------------------------- */

/* emulator debugging mode */
static int devbug = FALSE;

/* host path for each H: unit */
char atari_h_dir[4][FILENAME_MAX] = { "", "", "", "" };

/* read only mode for H: device */
int h_read_only = TRUE;

/* ';'-separated list of Atari paths checked by the "load executable"
   command. if a path does not start with "Hn:", then the selected device
   is used. */
char h_exe_path[FILENAME_MAX] = DEFAULT_H_PATH;

/* h_current_dir must be empty or terminated with DIR_SEP_CHAR;
   only DIR_SEP_CHAR can be used as a directory separator here */
char h_current_dir[4][FILENAME_MAX];

/* stream open via H: device per IOCB */
static FILE *h_fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* H: text mode per IOCB */
static int h_textmode[8];

/* last read character was CR, per IOCB */
static int h_wascr[8];

/* last operation: 'o': open, 'r': read, 'w': write, per IOCB */
/* (this is needed to apply fseek(fp, 0, SEEK_CUR) between reads and writes
   in update (12) mode) */
static char h_lastop[8];

Util_tmpbufdef(static, h_tmpbuf[8])

/* IOCB #, 0-7 */
static int h_iocb;

/* H: device number, 0-3 */
static int h_devnum;

/* filename as specified after "Hn:" */
static char atari_filename[FILENAME_MAX];

#ifdef DO_RENAME
/* new filename (no directories!) */
static char new_filename[FILENAME_MAX];
#endif

/* atari_filename applied to H:'s current dir, with DIR_SEP_CHARs only */
static char atari_path[FILENAME_MAX];

/* full filename for the current operation */
static char host_path[FILENAME_MAX];

int Device_H_CountOpen(void)
{
	int r = 0;
	int i;
	for (i = 0; i < 8; i++)
		if (h_fp[i] != NULL)
			r++;
	return r;
}

void Device_H_CloseAll(void)
{
	int i;
	for (i = 0; i < 8; i++)
		if (h_fp[i] != NULL) {
			Util_fclose(h_fp[i], h_tmpbuf[i]);
			h_fp[i] = NULL;
		}
}

static void Device_H_Init(void)
{
	if (devbug)
		Aprint("HHINIT");
	h_current_dir[0][0] = '\0';
	h_current_dir[1][0] = '\0';
	h_current_dir[2][0] = '\0';
	h_current_dir[3][0] = '\0';
	Device_H_CloseAll();
}

void Device_Initialise(int *argc, char *argv[])
{
	int i;
	int j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-H1") == 0)
			Util_strlcpy(atari_h_dir[0], argv[++i], FILENAME_MAX);
		else if (strcmp(argv[i], "-H2") == 0)
			Util_strlcpy(atari_h_dir[1], argv[++i], FILENAME_MAX);
		else if (strcmp(argv[i], "-H3") == 0)
			Util_strlcpy(atari_h_dir[2], argv[++i], FILENAME_MAX);
		else if (strcmp(argv[i], "-H4") == 0)
			Util_strlcpy(atari_h_dir[3], argv[++i], FILENAME_MAX);
		else if (strcmp(argv[i], "-Hpath") == 0)
			Util_strlcpy(h_exe_path, argv[++i], FILENAME_MAX);
		else if (strcmp(argv[i], "-hreadonly") == 0)
			h_read_only = TRUE;
		else if (strcmp(argv[i], "-hreadwrite") == 0)
			h_read_only = FALSE;
		else if (strcmp(argv[i], "-devbug") == 0)
			devbug = TRUE;
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Aprint("\t-H1 <path>       Set path for H1: device");
				Aprint("\t-H2 <path>       Set path for H2: device");
				Aprint("\t-H3 <path>       Set path for H3: device");
				Aprint("\t-H4 <path>       Set path for H4: device");
				Aprint("\t-Hpath <path>    Set path for Atari executables on the H: device");
				Aprint("\t-hreadonly       Enable read-only mode for H: device");
				Aprint("\t-hreadwrite      Disable read-only mode for H: device");
				Aprint("\t-devbug          Debugging messages for H: and P: devices");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;
	Device_H_Init();
}

#define IS_DIR_SEP(c) ((c) == '/' || (c) == '\\' || (c) == ':' || (c) == '>')

static int Device_IsValidForFilename(char ch)
{
	if ((ch >= 'A' && ch <= 'Z')
	 || (ch >= 'a' && ch <= 'z')
	 || (ch >= '0' && ch <= '9'))
		return TRUE;
	switch (ch) {
	case '!':
	case '#':
	case '$':
	case '&':
	case '\'':
	case '(':
	case ')':
	case '*':
	case '-':
	case '.':
	case '?':
	case '@':
	case '_':
		return TRUE;
	default:
		return FALSE;
	}
}

UWORD Device_SkipDeviceName(void)
{
	UWORD bufadr;
	for (bufadr = dGetWordAligned(ICBALZ); ; bufadr++) {
		char c = (char) dGetByte(bufadr);
		if (c == ':')
			return (UWORD) (bufadr + 1);
		if (c < '!' || c > '\x7e')
			return 0;
	}
}

/* devnum must be 0-3; p must point inside atari_filename */
static UWORD Device_GetAtariPath(int devnum, char *p)
{
	UWORD bufadr = Device_SkipDeviceName();
	if (bufadr != 0) {
		while (p < atari_filename + sizeof(atari_filename) - 1) {
			char c = (char) dGetByte(bufadr);
			if (Device_IsValidForFilename(c) || IS_DIR_SEP(c) || c == '<') {
				*p++ = c;
				bufadr++;
			}
			else {
				/* end of filename */
				/* now apply it to h_current_dir */
				const char *q = atari_filename;
				*p = '\0';
				if (IS_DIR_SEP(*q)) {
					/* absolute path on H: device */
					q++;
					p = atari_path;
				}
				else {
					strcpy(atari_path, h_current_dir[devnum]);
					p = atari_path + strlen(atari_path);
				}
				for (;;) {
					/* we are here at the beginning of a path element,
					   i.e. at the beginning of atari_path or after DIR_SEP_CHAR */
					if (*q == '<'
					 || (*q == '.' && q[1] == '.' && (q[2] == '\0' || IS_DIR_SEP(q[2])))) {
						/* "<" or "..": parent directory */
						if (p == atari_path) {
							regY = 150; /* Sparta: directory not found */
							SetN;
							return 0;
						}
						do
							p--;
						while (p > atari_path && p[-1] != DIR_SEP_CHAR);
						if (*q == '.') {
							if (q[2] != '\0')
								q++;
							q++;
						}
						q++;
						continue;
					}
					if (IS_DIR_SEP(*q)) {
						/* duplicate DIR_SEP */
						regY = 165; /* bad filename */
						SetN;
						return 0;
					}
					do {
						if (p >= atari_path + sizeof(atari_path) - 1) {
							regY = 165; /* bad filename */
							SetN;
							return 0;
						}
						*p++ = *q;
						if (*q == '\0')
							return bufadr;
						q++;
					} while (!IS_DIR_SEP(*q));
					*p++ = DIR_SEP_CHAR;
					q++;
				}
			}
		}
	}
	regY = 165; /* bad filename */
	SetN;
	return 0;
}

static int Device_GetIOCB(void)
{
	if ((regX & 0x8f) != 0) {
		regY = 134; /* invalid IOCB number */
		SetN;
		return FALSE;
	}
	h_iocb = regX >> 4;
	return TRUE;
}

static int Device_GetNumber(int set_textmode)
{
	int devnum;
	if (!Device_GetIOCB())
		return -1;
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9 || devnum == 0 || devnum == 5) {
		regY = 160; /* invalid unit/drive number */
		SetN;
		return -1;
	}
	if (devnum < 5) {
		if (set_textmode)
			h_textmode[h_iocb] = FALSE;
		return devnum - 1;
	}
	if (set_textmode)
		h_textmode[h_iocb] = TRUE;
	return devnum - 6;
}

static UWORD Device_GetHostPath(int set_textmode)
{
	UWORD bufadr;
	h_devnum = Device_GetNumber(set_textmode);
	if (h_devnum < 0)
		return 0;
	bufadr = Device_GetAtariPath(h_devnum, atari_filename);
	if (bufadr == 0)
		return 0;
	Util_catpath(host_path, atari_h_dir[h_devnum], atari_path);
	return bufadr;
}

static void Device_H_Open(void)
{
	FILE *fp;
	UBYTE aux1;
#ifdef DO_DIR
	UBYTE aux2;
	char entryname[FILENAME_MAX];
	int isdir;
	int readonly;
	int size;
	char timetext[16];
#endif

	if (devbug)
		Aprint("HHOPEN");

	if (Device_GetHostPath(TRUE) == 0)
		return;

	if (h_fp[h_iocb] != NULL)
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);

#if 0
	if (devbug)
		Aprint("atari_filename=\"%s\", atari_path=\"%s\" host_path=\"%s\"", atari_filename, atari_path, host_path);
#endif

	fp = NULL;
	h_wascr[h_iocb] = FALSE;
	h_lastop[h_iocb] = 'o';

	aux1 = dGetByte(ICAX1Z);
	switch (aux1) {
	case 4:
		/* don't bother using "r" for textmode:
		   we want to support LF, CR/LF and CR, not only native EOLs */
		fp = Util_fopen(host_path, "rb", h_tmpbuf[h_iocb]);
		if (fp != NULL) {
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
		fp = Util_tmpopen(h_tmpbuf[h_iocb]);
		if (fp == NULL) {
			regY = 144; /* device done error */
			SetN;
			break;
		}
		if (!Device_OpenDir(host_path)) {
			Util_fclose(fp, h_tmpbuf[h_iocb]);
			fp = NULL;
			regY = 144; /* device done error */
			SetN;
			break;
		}
		aux2 = dGetByte(ICAX2Z);
		if (aux2 >= 128) {
			fprintf(fp, "\nVolume:    HDISK%c\nDirectory: ", '1' + h_devnum);
			/* if (strcmp(dir_path, atari_h_dir[h_devnum]) == 0) */
			if (strchr(atari_path, DIR_SEP_CHAR) == NULL)
				fprintf(fp, "MAIN\n\n");
			else {
				char end_dir_str[FILENAME_MAX];
				Util_splitpath(dir_path, NULL, end_dir_str);
				fprintf(fp, "%s\n\n", /* Util_strupper */(end_dir_str));
			}
		}

		while (Device_ReadDir(NULL, entryname, &isdir, &readonly, &size,
		                      (aux2 >= 128) ? timetext : NULL)) {
			char *ext;
			/* Util_strupper(entryname); */
			ext = strrchr(entryname, '.');
			if (ext == NULL)
				ext = "";
			else {
				/* replace the dot with NUL,
				   so entryname is without extension */
				*ext++ = '\0';
				if (ext[0] != '\0' && ext[1] != '\0' && ext[2] != '\0' && ext[3] != '\0') {
					ext[2] = '+';
					ext[3] = '\0';
				}
			}
			if (strlen(entryname) > 8) {
				entryname[7] = '+';
				entryname[8] = '\0';
			}
			if (aux2 >= 128) {
				if (isdir)
					fprintf(fp, "%-13s<DIR>  %s\n", entryname, timetext);
				else {
					if (size > 999999)
						size = 999999;
					fprintf(fp, "%-9s%-3s %6d %s\n", entryname, ext, size, timetext);
				}
			}
			else {
				char dirchar = ' ';
				size = (size + 255) >> 8;
				if (size > 999)
					size = 999;
				if (isdir) {
					if (dGetByte(0x700) == 'M') /* MyDOS */
						dirchar = ':';
					else /* Sparta */
						ext = "\304\311\322"; /* "DIR" with bit 7 set */
				}
				fprintf(fp, "%c%c%-8s%-3s %03d\n", readonly ? '*' : ' ',
				        dirchar, entryname, ext, size);
			}
		}

		if (aux2 >= 128)
			fprintf(fp, "   999 FREE SECTORS\n");
		else
			fprintf(fp, "999 FREE SECTORS\n");

		Util_rewind(fp);
		h_textmode[h_iocb] = TRUE;
		regY = 1;
		ClrN;
		break;
#endif /* DO_DIR */
	case 8: /* write: "w" */
	case 9: /* write at end of file (append): "a" */
	case 12: /* write and read (update): "r+" || "w+" */
	case 13: /* append and read: "a+" */
		if (h_read_only) {
			regY = 163; /* disk write-protected */
			SetN;
			break;
		}
		{
			char mode[4];
			char *p = mode + 1;
			mode[0] = (aux1 & 1) ? 'a' : (aux1 < 12) ? 'w' : 'r';
			if (!h_textmode[h_iocb])
				*p++ = 'b';
			if (aux1 >= 12)
				*p++ = '+';
			*p = '\0';
			fp = Util_fopen(host_path, mode, h_tmpbuf[h_iocb]);
			if (fp == NULL && aux1 == 12) {
				mode[0] = 'w';
				fp = Util_fopen(host_path, mode, h_tmpbuf[h_iocb]);
			}
		}
		if (fp != NULL) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 144; /* device done error */
			SetN;
		}
		break;
	default:
		regY = 168; /* invalid device command */
		SetN;
		break;
	}
	h_fp[h_iocb] = fp;
}

static void Device_H_Close(void)
{
	if (devbug)
		Aprint("HHCLOS");
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);
		h_fp[h_iocb] = NULL;
	}
	regY = 1;
	ClrN;
}

static void Device_H_Read(void)
{
	if (devbug)
		Aprint("HHREAD");
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] == 'w')
			fseek(h_fp[h_iocb], 0, SEEK_CUR);
		h_lastop[h_iocb] = 'r';
		ch = fgetc(h_fp[h_iocb]);
		if (ch != EOF) {
			if (h_textmode[h_iocb]) {
				switch (ch) {
				case 0x0d:
					h_wascr[h_iocb] = TRUE;
					ch = 0x9b;
					break;
				case 0x0a:
					if (h_wascr[h_iocb]) {
						/* ignore LF next to CR */
						ch = fgetc(h_fp[h_iocb]);
						if (ch != EOF) {
							if (ch == 0x0d) {
								h_wascr[h_iocb] = TRUE;
								ch = 0x9b;
							}
							else
								h_wascr[h_iocb] = FALSE;
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
					h_wascr[h_iocb] = FALSE;
					break;
				}
			}
			regA = (UBYTE) ch;
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

static void Device_H_Write(void)
{
	if (devbug)
		Aprint("HHWRIT");
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] == 'r')
			fseek(h_fp[h_iocb], 0, SEEK_CUR);
		h_lastop[h_iocb] = 'w';
		ch = regA;
		if (ch == 0x9b && h_textmode[h_iocb])
			ch = '\n';
		fputc(ch, h_fp[h_iocb]);
		regY = 1;
		ClrN;
	}
	else {
		regY = 135; /* attempted to write to a read-only device */
		            /* XXX: this seems to be what Atari DOSes return */
		SetN;
	}
}

static void Device_H_Status(void)
{
	if (devbug)
		Aprint("HHSTAT");

	regY = 146; /* function not implemented in handler; XXX: check file existence? */
	SetN;
}

#define CHECK_READ_ONLY \
	if (h_read_only) { \
		regY = 163; \
		SetN; \
		return; \
	}

#ifdef DO_RENAME

static void fillin(const char *pattern, char *filename)
{
	const char *filename_end = filename + strlen(filename);
	for (;;) {
		switch (*pattern) {
		case '\0':
			*filename = '\0';
			return;
		case '?':
			pattern++;
			if (filename < filename_end)
				filename++;
			break;
		case '*':
			if (filename >= filename_end || *filename == pattern[1])
				pattern++;
			else
				filename++;
			break;
		default:
			*filename++ = *pattern++;
			break;
		}
	}
}

static void Device_H_Rename(void)
{
	UWORD bufadr;
	char c;
	char *p;
	int num_changed = 0;
	int num_failed = 0;
	int num_locked = 0;
	int readonly = FALSE;

	if (devbug)
		Aprint("RENAME Command");
	CHECK_READ_ONLY;

	bufadr = Device_GetHostPath(FALSE);
	if (bufadr == 0)
		return;
	/* skip space between filenames */
	for (;;) {
		c = (char) dGetByte(bufadr);
		if (Device_IsValidForFilename(c))
			break;
		if (c == '\0' || (UBYTE) c > 0x80 || IS_DIR_SEP(c)) {
			regY = 165; /* bad filename */
			SetN;
			return;
		}
		bufadr++;
	}
	/* get new filename */
	p = new_filename;
	do {
		if (p >= new_filename + sizeof(new_filename) - 1) {
			regY = 165; /* bad filename */
			SetN;
			return;
		}
		*p++ = c;
		bufadr++;
		c = (char) dGetByte(bufadr);
	} while (Device_IsValidForFilename(c));
	*p = '\0';

#ifdef DO_DIR
	if (!Device_OpenDir(host_path)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(host_path, NULL, NULL, &readonly, NULL, NULL))
#endif /* DO_DIR */
	{
		/* Check file write permission to mimic Atari
		   permission system: read-only ("locked") file
		   cannot be renamed. */
		if (readonly)
			num_locked++;
		else {
			char new_dirpart[FILENAME_MAX];
			char new_filepart[FILENAME_MAX];
			char new_path[FILENAME_MAX];
			/* split old filepath into dir part and file part */
			Util_splitpath(host_path, new_dirpart, new_filepart);
			/* replace old file part with new file part */
			fillin(new_filename, new_filepart);
			/* combine new filepath */
			Util_catpath(new_path, new_dirpart, new_filepart);
			if (Device_Rename(host_path, new_path))
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

#endif /* DO_RENAME */

#ifdef HAVE_UTIL_UNLINK

static void Device_H_Delete(void)
{
	int num_deleted = 0;
	int num_failed = 0;
	int num_locked = 0;
	int readonly = FALSE;

	if (devbug)
		Aprint("DELETE Command");
	CHECK_READ_ONLY;

	if (Device_GetHostPath(FALSE) == 0)
		return;

#ifdef DO_DIR
	if (!Device_OpenDir(host_path)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(host_path, NULL, NULL, &readonly, NULL, NULL))
#endif /* DO_DIR */
	{
		/* Check file write permission to mimic Atari
		   permission system: read-only ("locked") file
		   cannot be deleted. Modern systems have
		   a different permission for file deletion. */
		if (readonly)
			num_locked++;
		else
			if (Util_unlink(host_path) == 0)
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

#endif /* HAVE_UTIL_UNLINK */

#ifdef DO_LOCK

static void Device_H_LockUnlock(int readonly)
{
	int num_changed = 0;
	int num_failed = 0;

	CHECK_READ_ONLY;

	if (Device_GetHostPath(FALSE) == 0)
		return;

#ifdef DO_DIR
	if (!Device_OpenDir(host_path)) {
		regY = 170; /* file not found */
		SetN;
		return;
	}
	while (Device_ReadDir(host_path, NULL, NULL, NULL, NULL, NULL))
#endif /* DO_DIR */
	{
		if (Device_SetReadOnly(host_path, readonly))
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

static void Device_H_Lock(void)
{
	if (devbug)
		Aprint("LOCK Command");
	Device_H_LockUnlock(TRUE);
}

static void Device_H_Unlock(void)
{
	if (devbug)
		Aprint("UNLOCK Command");
	Device_H_LockUnlock(FALSE);
}

#endif /* DO_LOCK */

static void Device_H_Note(void)
{
	if (devbug)
		Aprint("NOTE Command");
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		long pos = ftell(h_fp[h_iocb]);
		if (pos >= 0) {
			int iocb = IOCB0 + h_iocb * 16;
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

static void Device_H_Point(void)
{
	if (devbug)
		Aprint("POINT Command");
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int iocb = IOCB0 + h_iocb * 16;
		long pos = (dGetByte(iocb + ICAX4) << 16) +
			(dGetByte(iocb + ICAX3) << 8) + (dGetByte(iocb + ICAX5));
		if (fseek(h_fp[h_iocb], pos, SEEK_SET) == 0) {
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

static FILE *binf = NULL;
static int runBinFile;
static int initBinFile;

/* Read a word from file */
static int Device_H_BinReadWord(void)
{
	UBYTE buf[2];
	if (fread(buf, 1, 2, binf) != 2) {
		fclose(binf);
		binf = NULL;
		if (start_binloading) {
			start_binloading = FALSE;
			Aprint("binload: not valid BIN file");
			regY = 180; /* MyDOS: not a binary file */
			SetN;
			return -1;
		}
		if (runBinFile)
			regPC = dGetWordAligned(0x2e0);
		regY = 1;
		ClrN;
		return -1;
	}
	return buf[0] + (buf[1] << 8);
}

static void Device_H_BinLoaderCont(void)
{
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
		int temp;
		UWORD from;
		UWORD to;
		do
			temp = Device_H_BinReadWord();
		while (temp == 0xffff);
		if (temp < 0)
			return;
		from = (UWORD) temp;

		temp = Device_H_BinReadWord();
		if (temp < 0)
			return;
		to = (UWORD) temp;

		if (devbug)
			Aprint("H: Load: From %04X to %04X", from, to);

		if (start_binloading) {
			if (runBinFile)
				dPutWordAligned(0x2e0, from);
			start_binloading = FALSE;
		}

		to++;
		do {
			int byte = fgetc(binf);
			if (byte == EOF) {
				fclose(binf);
				binf = NULL;
				if (runBinFile)
					regPC = dGetWordAligned(0x2e0);
				if (initBinFile && (dGetByte(0x2e3) != 0xd7)) {
					/* run INIT routine which RTSes directly to RUN routine */
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);	/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetWordAligned(0x2e2);
				}
				return;
			}
			PutByte(from, (UBYTE) byte);
			from++;
		} while (from != to);
	} while (!initBinFile || dGetByte(0x2e3) == 0xd7);

	regS--;
	Atari800_AddEsc((UWORD) (0x100 + regS), ESC_BINLOADER_CONT, Device_H_BinLoaderCont);
	regS--;
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetWordAligned(0x2e2);
	SetC;

	dPutByte(0x0300, 0x31);		/* for "Studio Dream" */
}

static void Device_H_LoadProceed(int mydos)
{
	/* Aprint("MyDOS %d, AX1 %d, AX2 %d", mydos, dGetByte(ICAX1Z), dGetByte(ICAX2Z)); */
	if (mydos) {
		switch (dGetByte(ICAX1Z) /* XXX: & 7 ? */) {
		case 4:
			runBinFile = TRUE;
			initBinFile = TRUE;
			break;
		case 5:
			runBinFile = TRUE;
			initBinFile = FALSE;
			break;
		case 6:
			runBinFile = FALSE;
			initBinFile = TRUE;
			break;
		case 7:
		default:
			runBinFile = FALSE;
			initBinFile = FALSE;
			break;
		}
	}
	else {
		if (dGetByte(ICAX2Z) < 128)
			runBinFile = TRUE;
		else
			runBinFile = FALSE;
		initBinFile = TRUE;
	}

	start_binloading = TRUE;
	Device_H_BinLoaderCont();
}

static void Device_H_Load(int mydos)
{
	const char *p;
	UBYTE buf[2];
	if (devbug)
		Aprint("LOAD Command");
	h_devnum = Device_GetNumber(FALSE);
	if (h_devnum < 0)
		return;

	/* search for program on h_exe_path */
	for (p = h_exe_path; *p != '\0'; ) {
		int devnum;
		const char *q;
		char *r;
		if (p[0] == 'H' && p[1] >= '1' && p[1] <= '4' && p[2] == ':') {
			devnum = p[1] - '1';
			p += 3;
		}
		else
			devnum = h_devnum;
		for (q = p; *q != '\0' && *q != ';'; q++);
		r = atari_filename + (q - p);
		if (q != p) {
			memcpy(atari_filename, p, q - p);
			if (!IS_DIR_SEP(q[-1]))
				*r++ = '>';
		}
		if (Device_GetAtariPath(devnum, r) == 0)
			return;
		Util_catpath(host_path, atari_h_dir[devnum], atari_path);
		binf = fopen(host_path, "rb");
		if (binf != NULL || *q == '\0')
			break;
		p = q + 1;
	}

	if (binf == NULL) {
		/* open from the specified location */
		if (Device_GetAtariPath(h_devnum, atari_filename) == 0)
			return;
		Util_catpath(host_path, atari_h_dir[h_devnum], atari_path);
		binf = fopen(host_path, "rb");
		if (binf == NULL) {
			regY = 170;
			SetN;
			return;
		}
	}

	/* check header */
	if (fread(buf, 1, 2, binf) != 2 || buf[0] != 0xff || buf[1] != 0xff) {
		fclose(binf);
		binf = NULL;
		Aprint("H: load: not valid BIN file");
		regY = 180;
		SetN;
		return;
	}

	Device_H_LoadProceed(mydos);
}

static void Device_H_FileLength(void)
{
	if (devbug)
		Aprint("Get File Length Command");
	if (!Device_GetIOCB())
		return;
	/* if IOCB is closed then assume it is a MyDOS Load File command */
	if (h_fp[h_iocb] == NULL)
		Device_H_Load(TRUE);
	/* if we are running MyDOS then assume it is a MyDOS Load File command */
	else if (dGetByte(0x700) == 'M') {
		/* XXX: if (binf != NULL) fclose(binf); ? */
		binf = h_fp[h_iocb];
		Device_H_LoadProceed(TRUE);
		/* XXX: don't close binf when complete? */
	}
	/* otherwise assume it is a file length command */
	else {
		int iocb = IOCB0 + h_iocb * 16;
		int filesize;
#if 0
		/* old, less portable implementation */
		struct stat fstatus;
		fstat(fileno(h_fp[h_iocb]), &fstatus);
		filesize = fstatus.st_size;
#else
		FILE *fp = h_fp[h_iocb];
		long currentpos = ftell(fp);
		filesize = Util_flen(fp);
		fseek(fp, currentpos, SEEK_SET);
#endif
		dPutByte(iocb + ICAX3, (UBYTE) filesize);
		dPutByte(iocb + ICAX4, (UBYTE) (filesize >> 8));
		dPutByte(iocb + ICAX5, (UBYTE) (filesize >> 16));
		regY = 1;
		ClrN;
	}
}

#ifdef DO_MKDIR
static void Device_H_MakeDirectory(void)
{
	if (devbug)
		Aprint("MKDIR Command");
	CHECK_READ_ONLY;

	if (Device_GetHostPath(FALSE) == 0)
		return;

	if (Device_MakeDirectory(host_path)) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144; /* device done error */
		SetN;
	}
}
#endif

#ifdef DO_RMDIR
static void Device_H_RemoveDirectory(void)
{
	if (devbug)
		Aprint("RMDIR Command");
	CHECK_READ_ONLY;

	if (Device_GetHostPath(FALSE) == 0)
		return;

	regY = Device_RemoveDirectory(host_path);
	if (regY >= 128)
		SetN;
	else
		ClrN;
}
#endif

static void Device_H_ChangeDirectory(void)
{
	if (devbug)
		Aprint("CD Command");

	if (Device_GetHostPath(FALSE) == 0)
		return;

	if (!Util_direxists(host_path)) {
		regY = 150;
		SetN;
		return;
	}

	if (atari_path[0] == '\0')
		h_current_dir[h_devnum][0] = '\0';
	else {
		char *p = Util_stpcpy(h_current_dir[h_devnum], atari_path);
		p[0] = DIR_SEP_CHAR;
		p[1] = '\0';
	}

	regY = 1;
	ClrN;
}

static void Device_H_DiskInfo(void)
{
	static UBYTE info[16] = {
		0x20,                                                  /* disk version: Sparta >= 2.0 */
		0x00,                                                  /* sector size: 0x100 */
		0xff, 0xff,                                            /* total sectors: 0xffff */
		0xff, 0xff,                                            /* free sectors: 0xffff */
		'H', 'D', 'I', 'S', 'K', '1' /* + devnum */, ' ', ' ', /* disk name */
		1,                                                     /* seq. number (number of writes) */
		1 /* + devnum */                                       /* random number (disk id) */
	};
	int devnum;

	if (devbug)
		Aprint("Get Disk Information Command");

	devnum = Device_GetNumber(FALSE);
	if (devnum < 0)
		return;

	info[11] = (UBYTE) ('1' + devnum);
	info[15] = (UBYTE) (1 + devnum);
	CopyToMem(info, (UWORD) dGetWordAligned(ICBLLZ), 16);

	regY = 1;
	ClrN;
}

static void Device_H_ToAbsolutePath(void)
{
	UWORD bufadr;
	const char *p;

	if (devbug)
		Aprint("To Absolute Path Command");

	if (Device_GetHostPath(FALSE) == 0)
		return;

	/* XXX: we sometimes check here for directories
	   with a trailing DIR_SEP_CHAR. It seems to work on Win32 and DJGPP. */
	if (!Util_direxists(host_path)) {
		regY = 150;
		SetN;
		return;
	}

	bufadr = dGetWordAligned(ICBLLZ);
	if (atari_path[0] != '\0') {
		PutByte(bufadr, '>');
		bufadr++;
		for (p = atari_path; *p != '\0'; p++) {
			if (*p == DIR_SEP_CHAR) {
				if (p[1] == '\0')
					break;
				PutByte(bufadr, '>');
			}
			else
				PutByte(bufadr, (UBYTE) *p);
			bufadr++;
		}
	}
	PutByte(bufadr, 0x00);

	regY = 1;
	ClrN;
}

static void Device_H_Special(void)
{
	if (devbug)
		Aprint("HHSPEC");

	switch (dGetByte(ICCOMZ)) {
#ifdef DO_RENAME
	case 0x20:
		Device_H_Rename();
		return;
#endif
#ifdef HAVE_UTIL_UNLINK
	case 0x21:
		Device_H_Delete();
		return;
#endif
#ifdef DO_LOCK
	case 0x23:
		Device_H_Lock();
		return;
	case 0x24:
		Device_H_Unlock();
		return;
#endif
	case 0x26:
		Device_H_Note();
		return;
	case 0x25:
		Device_H_Point();
		return;
	case 0x27: /* Sparta, MyDOS=Load */
		Device_H_FileLength();
		return;
	case 0x28: /* Sparta */
		Device_H_Load(FALSE);
		return;
#ifdef DO_MKDIR
	case 0x22: /* MyDOS */
	case 0x2a: /* MyDOS, Sparta */
		Device_H_MakeDirectory();
		return;
#endif
#ifdef DO_RMDIR
	case 0x2b: /* Sparta */
		Device_H_RemoveDirectory();
		return;
#endif
	case 0x29: /* MyDOS */
	case 0x2c: /* Sparta */
		Device_H_ChangeDirectory();
		return;
	case 0x2f: /* Sparta */
		Device_H_DiskInfo();
		return;
	case 0x30: /* Sparta */
		Device_H_ToAbsolutePath();
		return;
	case 0xfe:
		if (devbug)
			Aprint("FORMAT Command");
		break;
	default:
		if (devbug)
			Aprint("UNKNOWN Command %02X", dGetByte(ICCOMZ));
		break;
	}

	regY = 168; /* invalid device command */
	SetN;
}


/* P: device emulation --------------------------------------------------- */

char print_command[256] = "lpr %s";

int Device_SetPrintCommand(const char *command)
{
	const char *p = command;
	int was_percent_s = FALSE;
	while (*p != '\0') {
		if (*p++ == '%') {
			char c = *p++;
			if (c == '%')
				continue; /* %% is safe */
			if (c == 's' && !was_percent_s) {
				was_percent_s = TRUE; /* only one %s is safe */
				continue;
			}
			return FALSE;
		}
	}
	strcpy(print_command, command);
	return TRUE;
}

#ifdef HAVE_SYSTEM

static FILE *phf = NULL;
static char spool_file[FILENAME_MAX];

static void Device_P_Close(void)
{
	if (devbug)
		Aprint("PHCLOS");

	if (phf != NULL) {
		fclose(phf);
		phf = NULL;

#ifdef __PLUS
		if (!Misc_ExecutePrintCmd(spool_file))
#endif
		{
			char command[256 + FILENAME_MAX]; /* 256 for print_command + FILENAME_MAX for spool_file */
			sprintf(command, print_command, spool_file);
			system(command);
#if defined(HAVE_UTIL_UNLINK) && !defined(VMS) && !defined(MACOSX)
			if (Util_unlink(spool_file) != 0) {
				perror(spool_file);
			}
#endif
		}
	}
	regY = 1;
	ClrN;
}

static void Device_P_Open(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phf != NULL)
		Device_P_Close();

	phf = Util_uniqopen(spool_file, "w");
	if (phf != NULL) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144; /* device done error */
		SetN;
	}
}

static void Device_P_Write(void)
{
	UBYTE byte;

	if (devbug)
		Aprint("PHWRIT");

	byte = regA;
	if (byte == 0x9b)
		byte = '\n';

	fputc(byte, phf);
	regY = 1;
	ClrN;
}

static void Device_P_Status(void)
{
	if (devbug)
		Aprint("PHSTAT");
}

static void Device_P_Init(void)
{
	if (devbug)
		Aprint("PHINIT");

	if (phf != NULL) {
		fclose(phf);
		phf = NULL;
#ifdef HAVE_UTIL_UNLINK
		Util_unlink(spool_file);
#endif
	}
	regY = 1;
	ClrN;
}

#endif /* HAVE_SYSTEM */


/* K: and E: handlers for BASIC version, using getchar() and putchar() --- */

#ifdef BASIC

static void Device_E_Read(void)
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

static void Device_E_Write(void)
{
	UBYTE ch;

	ch = regA;
	/* XXX: are '\f', '\b' and '\a' fully portable? */
	switch (ch) {
	case 0x7d: /* Clear Screen */
		putchar('\x0c'); /* ASCII Form Feed */
		break;
	case 0x7e:
		putchar('\x08'); /* ASCII Backspace */
		break;
	case 0x7f:
		putchar('\t');
		break;
	case 0x9b:
		putchar('\n');
		break;
	case 0xfd:
		putchar('\x07'); /* ASCII Bell */
		break;
	default:
		if ((ch >= 0x20) && (ch <= 0x7e))
			putchar(ch);
		break;
	}
	regY = 1;
	ClrN;
}

static void Device_K_Read(void)
{
	int ch;
	int ch2;

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
		/* ignore characters until EOF or EOL */
		do
			ch2 = getchar();
		while (ch2 != EOF && ch2 != '\n');
		break;
	}
	regA = (UBYTE) ch;
	regY = 1;
	ClrN;
}

#endif /* BASIC */


/* Atari BASIC loader ---------------------------------------------------- */

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
			Atari800_AddEscRts(ehwrit_addr, ESC_EHWRIT, Device_E_Write);
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
	Device_E_Write();
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
	Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_E_Read);
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
	Atari800_AddEscRts(ehread_addr, ESC_EHREAD, Device_E_Read);
#else
	Device_RestoreEHREAD();
#endif
	rts_handler = Device_RestoreEHCLOS;
	regY = 1;
	ClrN;
}


/* Patches management ---------------------------------------------------- */

int enable_h_patch = TRUE;
int enable_p_patch = TRUE;
int enable_r_patch = FALSE;

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
		return FALSE;
	}

	for (i = 0; i < 5; i++) {
		UWORD devtab = dGetWord(addr + 1);
		switch (dGetByte(addr)) {
#ifdef HAVE_SYSTEM
		case 'P':
			if (enable_p_patch) {
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_OPEN) + 1),
				                   ESC_PHOPEN, Device_P_Open);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_CLOS) + 1),
				                   ESC_PHCLOS, Device_P_Close);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_WRIT) + 1),
				                   ESC_PHWRIT, Device_P_Write);
				Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_STAT) + 1),
				                   ESC_PHSTAT, Device_P_Status);
				Atari800_AddEscRts2((UWORD) (devtab + DEVICE_TABLE_INIT), ESC_PHINIT,
				                    Device_P_Init);
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
				                   ESC_EHWRIT, Device_E_Write);
			Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_READ) + 1),
			                   ESC_EHREAD, Device_E_Read);
			patched = TRUE;
			break;
		case 'K':
			Atari800_AddEscRts((UWORD) (dGetWord(devtab + DEVICE_TABLE_READ) + 1),
			                   ESC_KHREAD, Device_K_Read);
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
   and put the device table and patches in unused address space
   (0xd100-0xd1ff), which is meant for 'new devices' (like hard disk).
   We have to continuously check if our H: is still in HATABS,
   because RESET routine in Atari OS clears HATABS and initializes it
   using a table in ROM (see Device_PatchOS).
   Before we put H: entry in HATABS, we must make sure that HATABS is there.
   For example a program that doesn't use Atari OS can use this memory area
   for its own data, and we shouldn't place 'H' there.
   We also allow an Atari program to change address of H: device table.
   So after we put H: entry in HATABS, we only check if 'H' is still where
   we put it (h_entry_address).
   Device_UpdateHATABSEntry and Device_RemoveHATABSEntry can be used to add
   other devices than H:. */

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
		Atari800_AddEscRts(H_PATCH_OPEN, ESC_HHOPEN, Device_H_Open);
		Atari800_AddEscRts(H_PATCH_CLOS, ESC_HHCLOS, Device_H_Close);
		Atari800_AddEscRts(H_PATCH_READ, ESC_HHREAD, Device_H_Read);
		Atari800_AddEscRts(H_PATCH_WRIT, ESC_HHWRIT, Device_H_Write);
		Atari800_AddEscRts(H_PATCH_STAT, ESC_HHSTAT, Device_H_Status);
		Atari800_AddEscRts(H_PATCH_SPEC, ESC_HHSPEC, Device_H_Special);
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
