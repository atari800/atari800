/*
 * devices.c - H: devices emulation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "config.h"

#ifdef HAVE_UNIXIO_H
#include <unixio.h>
#endif

#ifdef HAVE_FILE_H
#include <file.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_DIRENT_H) && defined(HAVE_OPENDIR)
/* XXX: <sys/dir.h>, <ndir.h>, <sys/ndir.h> */
/* !VMS */
#define DO_DIR
#endif

#ifdef DO_DIR
#include <dirent.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

#ifndef HAVE_MKSTEMP
# ifndef O_BINARY
#  define O_BINARY	0
# endif
/* XXX: race condition */
# define mkstemp(a) open(mktemp(a), O_RDWR | O_CREAT | O_BINARY, 0600)
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "rt-config.h"
#include "log.h"
#include "binload.h"
#include "sio.h"

#ifdef R_IO_DEVICE
#include "rdevice.h"
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

#define DEFAULT_H_PATH  "H1:>DOS;>DOS"
char HPath[FILENAME_MAX] = DEFAULT_H_PATH;
static char *HPaths[20];
static int HPathDrive[20];
static int HPathCount = 0;

static char current_dir[5][FILENAME_MAX];
static char current_path[5][FILENAME_MAX];

static int devbug = FALSE;

static FILE *fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int flag[8];

static int fid;
static char filename[FILENAME_MAX];
#ifdef HAVE_RENAME
static char newfilename[FILENAME_MAX];
#endif

#ifdef DO_DIR
static char pathname[FILENAME_MAX];
static DIR *dp = NULL;
#endif

static char *strtoupper(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = toupper(*ptr);

	return str;
}

static char *strtolower(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = tolower(*ptr);

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
	int valid;

	if (ch < 0x80 && isalnum(ch))
		valid = TRUE;
	else
		switch (ch) {
		case ':':
		case '.':
		case '_':
		case '*':
		case '?':
		case '>':
			valid = TRUE;
			break;
		default:
			valid = FALSE;
			break;
		}

	return valid;
}

static void Device_GetFilename(void)
{
	int bufadr;
	int offset = 0;
	int devnam = TRUE;

	bufadr = (dGetByte(ICBAHZ) << 8) | dGetByte(ICBALZ);

	while (Device_isvalid(dGetByte(bufadr))) {
		int byte = dGetByte(bufadr);

		if (!devnam) {
			if (isupper(byte))
				byte = tolower(byte);

			filename[offset++] = byte;
		}
		else if (byte == ':')
			devnam = FALSE;

		bufadr++;
	}

	filename[offset++] = '\0';
}

#ifdef HAVE_RENAME
static void Device_GetFilenames(void)
{
	int bufadr;
	int offset = 0;
	int devnam = TRUE;
	int byte;

	bufadr = (dGetByte(ICBAHZ) << 8) | dGetByte(ICBALZ);

	while (Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);

		if (!devnam) {
			if (isupper(byte))
				byte = tolower(byte);

			filename[offset++] = byte;
		}
		else if (byte == ':')
			devnam = FALSE;

		bufadr++;
	}
	filename[offset++] = '\0';

	while (!Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);
		if ((byte > 0x80) || (byte == 0)) {
			newfilename[0] = 0;
			return;
		}
		bufadr++;
	}

	offset = 0;
	while (Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);

		if (isupper(byte))
			byte = tolower(byte);

		newfilename[offset++] = byte;
		bufadr++;
	}
	newfilename[offset++] = '\0';
}
#endif

#ifdef DO_DIR
static int match(char *pattern, char *filename)
{
	int status = TRUE;

	if (strcmp(pattern, "*.*") == 0)
		return TRUE;

	while (status && *filename && *pattern) {
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
			status = (*pattern++ == *filename++);
			break;
		}
	}
	if ((*filename)
		|| ((*pattern)
			&& (((*pattern != '*') && (*pattern != '?'))
				|| pattern[1]))) {
		status = 0;
	}
	return status;
}
#endif

#if defined(HAVE_RENAME) && defined(DO_DIR)
static void fillin(char *pattern, char *filename)
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
	*filename = 0;
}
#endif

#ifdef BACK_SLASH
#define DIR_SEP_CHAR '\\'
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR "/"
#endif

/* Concatenate file paths.
   Place directory separator char between, unless path1 is empty
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

#ifdef DO_DIR
static void Device_SeparateFileFromPath()
{
	char *ptr;

	strcpy(pathname, filename);
	ptr = pathname + strlen(pathname);
	while (ptr > pathname) {
		if (*ptr == DIR_SEP_CHAR) {
			*ptr = 0;
			strcpy(filename, ptr + 1);
			break;
		}
		ptr--;
	}
}
#endif

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
		flag[fid] = TRUE;
		devnum -= 5;
	}
	else {
		flag[fid] = FALSE;
	}
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
		Device_SeparateFileFromPath();
		fp[fid] = tmpfile();
		if (fp[fid]) {
			dp = opendir(pathname);
			if (dp) {
				struct dirent *entry;
				int extended = dGetByte(ICAX2Z);
				if (extended >= 128) {
					fprintf(fp[fid], "\nVolume:    HDISK%d\nDirectory: ",
							devnum);
					if (strcmp(pathname, H[devnum]) == 0)
						fprintf(fp[fid], "MAIN\n\n");
					else {
						const char *end_dir;
						char end_dir_str[FILENAME_MAX];
						end_dir = pathname + strlen(pathname);
						while (end_dir > pathname) {
							if (*end_dir == DIR_SEP_CHAR) {
								end_dir++;
								break;
							}
							end_dir--;
						}
						strcpy(end_dir_str, end_dir);
						fprintf(fp[fid], "%s\n\n",
								strtoupper(end_dir_str));
					}
				}

				while ((entry = readdir(dp))) {
					char entryname[FILENAME_MAX];
					strcpy(entryname, entry->d_name);
					if (entry->d_name[0] != '.'
					 && match(strtoupper(filename), strtoupper(entryname))) {
						const char *ext;
						int size;
						struct stat status;
						cat_path(fname, pathname, entry->d_name);
						stat(fname, &status);
						if ((status.st_size != 0)
							&& (status.st_size % 256 == 0))
							size = status.st_size / 256;
						else
							size = status.st_size / 256 + 1;
						if (size > 999)
							size = 999;
						ext = strtok(entryname, ".");
						ext = strtok(NULL, ".");
						if (ext == NULL)
							ext = "   ";
						if (extended >= 128) {
							struct tm *filetime;
							if (status.st_mode & S_IFDIR) {
								fprintf(fp[fid], "%-8s     <DIR>  ",
										entryname);
							}
							else {
								fprintf(fp[fid], "%-8s %-3s %6d ",
										entryname, ext,
										(int) status.st_size);
							}
							filetime = localtime(&status.st_mtime);
							if (filetime->tm_year >= 100)
								filetime->tm_year -= 100;
							fprintf(fp[fid],
									"%02d-%02d-%02d %02d:%02d\n",
									filetime->tm_mon, filetime->tm_mday,
									filetime->tm_year, filetime->tm_hour,
									filetime->tm_min);
						}
						else {
							if (status.st_mode & S_IFDIR)
								ext = "\304\311\322"; /* "DIR" with bit 7 set */
							fprintf(fp[fid], "%c %-8s%-3s %03d\n",
									(status.st_mode & S_IWRITE) ? ' ' : '*',
									entryname, ext, size);
						}
					}
				}

				if (extended >= 128)
					fprintf(fp[fid], "   999 FREE SECTORS\n");
				else
					fprintf(fp[fid], "999 FREE SECTORS\n");

				closedir(dp);

				rewind(fp[fid]);

				flag[fid] = TRUE;

				regY = 1;
				ClrN;
			}
			else {
				fclose(fp[fid]);
				fp[fid] = NULL;
				regY = 144; /* device done error */
				SetN;
			}
		}
		else {
			regY = 144; /* device done error */
			SetN;
		}
		break;
#endif /* DO_DIR */
	case 8:
	case 9: /* write at the end of file (append) */
		if (hd_read_only) {
			regY = 163; /* disk write-protected */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, temp == 9 ? "ab" : "wb");
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
		fp[fid] = fopen(fname, "rb+");
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
			if (flag[fid]) {
				switch (ch) {
				case '\n':
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
		if (flag[fid]) {
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
	return buf[0] | (buf[1] << 8);
}

static void Device_HSPEC_BIN_loader_cont(void)
{
	SLONG temp;
	UWORD from;
	UWORD to;
	UBYTE byte;

	if (!binf)
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
				binf = 0;
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
	Atari800_AddEsc(0x100 + regS, ESC_BINLOADER_CONT,
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
	int status;
	int num_changed = 0;
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
	strcpy(filename, fname);

#ifdef DO_DIR
	Device_SeparateFileFromPath();
	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				char newfname[FILENAME_MAX];
				char renamefname[FILENAME_MAX];
				struct stat filestatus;
				strtolower(entry->d_name);
				cat_path(fname, pathname, entry->d_name);
				strcpy(renamefname, entry->d_name);
				fillin(newfilename, renamefname);
				cat_path(newfname, pathname, renamefname);
				stat(fname, &filestatus);
				if (filestatus.st_mode & S_IWRITE) {
					if (rename(fname, newfname) != 0)
						status = -1;
					else
						num_changed++;
				}
				else
					num_locked++;
			}
		}
		closedir(dp);
	}
#else
	status = 0;
	if (rename(filename, newfilename) == 0)
		num_changed++;
#endif
	else
		status = -1;

	if (num_locked) {
		regY = 167; /* file locked */
		SetN;
	}
	else if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 170; /* file not found */
		SetN;
	}
}
#endif

#ifdef HAVE_UNLINK
static void Device_HHSPEC_Delete(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int status;
	int num_changed = 0;
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
	strcpy(filename, fname);

#ifdef DO_DIR
	Device_SeparateFileFromPath();
	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				struct stat filestatus;

				strtolower(entry->d_name);
				cat_path(fname, pathname, entry->d_name);
				stat(fname, &filestatus);
				if (filestatus.st_mode & S_IWRITE) {
					if (unlink(fname) != 0)
						status = -1;
					else
						num_changed++;
				}
				else
					num_locked++;
			}

		}
		closedir(dp);
	}
#else
	if (unlink(filename) == 0) {
		status = 0;
		num_changed++;
	}
#endif
	else
		status = -1;

	if (num_locked) {
		regY = 167; /* file locked */
		SetN;
	}
	else if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 170; /* file not found */
		SetN;
	}
}
#endif

#ifdef HAVE_CHMOD
static void Device_HHSPEC_Lock(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int status;

	if (devbug)
		Aprint("LOCK Command");
	if (hd_read_only) {
		regY = 163; /* disk write-protected */
		SetN;
		return;
	}

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);
	strcpy(filename, fname);

#ifdef DO_DIR
	Device_SeparateFileFromPath();
	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
				cat_path(fname, pathname, entry->d_name);
				if (chmod(fname, S_IREAD) != 0)
					status = -1;
				else
					num_changed++;
			}
		}

		closedir(dp);
	}
#else
	if (chmod(filename, S_IREAD) == 0) {
		status = 0;
		num_changed++;
	}
#endif
	else
		status = -1;

	if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 170; /* file not found */
		SetN;
	}
}

static void Device_HHSPEC_Unlock(void)
{

	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int status;

	if (devbug)
		Aprint("UNLOCK Command");
	if (hd_read_only) {
		regY = 163; /* disk write-protected */
		SetN;
		return;
	}

	GET_DEVNUM;

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
	cat_path(fname, H[devnum], filename);
	strcpy(filename, fname);

#ifdef DO_DIR
	Device_SeparateFileFromPath();
	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
				cat_path(fname, pathname, entry->d_name);
				if (chmod(fname, S_IREAD | S_IWRITE) != 0)
					status = -1;
				else
					num_changed++;
			}
		}

		closedir(dp);
	}
#else
	if (chmod(fname, S_IREAD | S_IWRITE) == 0) {
		status = 0;
		num_changed++;
	}
#endif
	else
		status = -1;

	if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 170;
		SetN;
	}
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
			dPutByte(iocb + ICAX5, (pos & 0xff));
			dPutByte(iocb + ICAX3, ((pos & 0xff00) >> 8));
			dPutByte(iocb + ICAX4, ((pos & 0xff0000) >> 16));
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
		if (binf)
			break;
	}
	if (!binf) {
		Device_ApplyPathToFilename(devnum);
		cat_path(fname, H[devnum], filename);
		if (!(binf = fopen(fname, "rb"))) {	/* open */
			Aprint("H: load: can't open %s", filename);
			regY = 170;
			SetN;
			return;
		}
	}
	if (fread(buf, 1, 2, binf) != 2 || buf[0] != 0xff || buf[1] != 0xff) {	/* check header */
		fclose(binf);
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
		dPutByte(iocb + ICAX3, (filesize & 0xff));
		dPutByte(iocb + ICAX4, ((filesize & 0xff00) >> 8));
		dPutByte(iocb + ICAX5, ((filesize & 0xff0000) >> 16));
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

#ifdef WIN32
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
	int devnum;
	int bufadr;

	if (devbug)
		Aprint("Get Disk Information Command");

	GET_DEVNUM;

	bufadr = (dGetByte(ICBLHZ) << 8) | dGetByte(ICBLLZ);

	dPutByte(bufadr, 3);
	dPutByte(bufadr + 1, 0);
	dPutByte(bufadr + 2, 231);
	dPutByte(bufadr + 3, 3);
	dPutByte(bufadr + 4, 231);
	dPutByte(bufadr + 5, 3);
	dPutByte(bufadr + 6, 'H');
	dPutByte(bufadr + 7, 'D');
	dPutByte(bufadr + 8, 'I');
	dPutByte(bufadr + 9, 'S');
	dPutByte(bufadr + 10, 'K');
	dPutByte(bufadr + 11, '0' + devnum);
	dPutByte(bufadr + 12, ' ');
	dPutByte(bufadr + 13, ' ');
	dPutByte(bufadr + 14, devnum);
	dPutByte(bufadr + 15, devnum);
	dPutByte(bufadr + 16, 0);

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

	bufadr = (dGetByte(ICBLHZ) << 8) | dGetByte(ICBLLZ);

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
		dPutByte(bufadr + i, new_path[i]);

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

static FILE *phf = NULL;
static void Device_PHCLOS(void);
static char spool_file[13];

static void Device_PHOPEN(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phf)
		Device_PHCLOS();

	strcpy(spool_file, "SPOOL_XXXXXX");
	phf = fdopen(mkstemp(spool_file), "w");
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
		char command[256];

		fclose(phf);
		phf = NULL;

#ifdef HAVE_SNPRINTF
		snprintf(command, 256, print_command, spool_file);
#else
		sprintf(command, print_command, spool_file);
#endif
		system(command);
#if defined(HAVE_UNLINK) && !defined(VMS) && !defined(MACOSX)
		if (unlink(spool_file) == -1) {
			perror(spool_file);
		}
#endif
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
		case 'P':
			if (enable_p_patch) {
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_OPEN) +
								   1, ESC_PHOPEN, Device_PHOPEN);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_CLOS) +
								   1, ESC_PHCLOS, Device_PHCLOS);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) +
								   1, ESC_PHWRIT, Device_PHWRIT);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_STAT) +
								   1, ESC_PHSTAT, Device_PHSTAT);
				Atari800_AddEscRts2(devtab + DEVICE_TABLE_INIT, ESC_PHINIT,
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

#ifdef R_IO_DEVICE
		/* XXX: what ROM has R: ? */
		case 'R':
			if (enable_r_patch) {
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_OPEN) +
								   1, ESC_ROPEN, Device_ROPEN);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_CLOS) +
								   1, ESC_RCLOS, Device_RCLOS);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) +
								   1, ESC_RWRIT, Device_RWRIT);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_STAT) +
								   1, ESC_RSTAT, Device_RSTAT);
				Atari800_AddEscRts2(devtab + DEVICE_TABLE_INIT, ESC_RINIT,
									Device_RINIT);
				patched = TRUE;
			}
			else {
				Atari800_RemoveEsc(ESC_ROPEN);
				Atari800_RemoveEsc(ESC_RCLOS);
				Atari800_RemoveEsc(ESC_RWRIT);
				Atari800_RemoveEsc(ESC_RSTAT);
				Atari800_RemoveEsc(ESC_RINIT);
			}
			break;
#endif

#ifdef BASIC
		case 'E':
			Aprint("Editor Device");
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1,
							   ESC_EHREAD, Device_EHREAD);
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) + 1,
							   ESC_EHWRIT, Device_EHWRIT);
			patched = TRUE;
			break;
		case 'K':
			Aprint("Keyboard Device");
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1,
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
#ifndef PAGED_ATTRIB
		SetROM(H_DEVICE_BEGIN, H_DEVICE_END);
#else
#warning H: device not working yet
#endif
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
#ifndef PAGED_ATTRIB
		SetROM(R_DEVICE_BEGIN, R_DEVICE_END);
#else
#warning R: device not working yet
#endif
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
