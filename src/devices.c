/*
 * devices.c - H: devices emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
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

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <ctype.h>
#include    <time.h>
#include    <errno.h>
#include	"config.h"
#ifndef HAVE_MKSTEMP
# ifndef O_BINARY
#  define O_BINARY	0
# endif
# define mkstemp(a) open(mktemp(a),O_RDWR|O_CREAT|O_BINARY,0600)
#endif

#include    "config.h"
#include    "ui.h"

#ifdef VMS
#include    <unixio.h>
#include    <file.h>
#else
#include    <fcntl.h>
#ifndef AMIGA
#include    <unistd.h>
#endif
#endif

#define DO_DIR

#ifdef AMIGA
#undef  DO_DIR
#endif

#ifdef VMS
#undef  DO_DIR
#endif

#ifdef DO_DIR
#include    <dirent.h>
#endif

#include        <sys/stat.h>

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "rt-config.h"
#include "log.h"
#include "binload.h"
#include "sio.h"
#include "ui.h"

#if defined(R_IO_DEVICE)
#include "rdevice.h"
#endif

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
static char pathname[FILENAME_MAX];
static char newfilename[FILENAME_MAX];

#ifdef DO_DIR
static DIR *dp = NULL;
#endif

char *strtoupper(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = toupper(*ptr);

	return str;
}

char *strtolower(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = tolower(*ptr);

	return str;
}

static void Device_GeneratePath()
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
		else
			argv[j++] = argv[i];
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

void Device_GetFilename(void)
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

void Device_GetFilenames(void)
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

int match(char *pattern, char *filename)
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
			if (*filename == pattern[1]) {
				pattern++;
			}
			else {
				filename++;
			}
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

void fillin(char *pattern, char *filename)
{
	while (*pattern) {
		switch (*pattern) {
		case '?':
			pattern++;
			filename++;
			break;
		case '*':
			if (*filename == pattern[1]) {
				pattern++;
			}
			else {
				filename++;
			}
			break;
		default:
			*filename++ = *pattern++;
			break;
		}
	}
	*filename = 0;
}

void apply_relative_path(char *rel_path, char *current)
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
#ifdef BACK_SLASH
		*end_current++ = '\\';
#else
		*end_current++ = '/';
#endif
	}

	while (*rel_path) {
		if (*rel_path == '>' || *rel_path == ':') {
			if (*(rel_path + 1) == '>' || *(rel_path + 1) == ':') {
				/* Move up a directory */
#ifdef BACK_SLASH
				while ((end_current > current) && (*end_current != '\\'))
#else
				while ((end_current > current) && (*end_current != '/'))
#endif
					end_current--;
				rel_path += 2;
				if ((*rel_path != '>') && (*rel_path != ':')
					&& (*rel_path != 0))
#ifdef BACK_SLASH
					*end_current++ = '\\';
#else
					*end_current++ = '/';
#endif
				else
					*end_current = 0;
			}
			else {
#ifdef BACK_SLASH
				*end_current++ = '\\';
#else
				*end_current++ = '/';
#endif
				rel_path++;
			}
		}
		else {
			*end_current++ = *rel_path++;
		}
	}

	*end_current = 0;
}


void Device_ApplyPathToFilename(int devnum)
{
	char path[FILENAME_MAX];

	strcpy(path, current_dir[devnum]);
	apply_relative_path(filename, path);
	strcpy(filename, path);
}

void Device_SeparateFileFromPath()
{
	char *ptr;

	strcpy(pathname, filename);
	ptr = pathname + strlen(pathname);
	while (ptr > pathname) {
#ifdef BACK_SLASH
		if (*ptr == '\\') {
#else
		if (*ptr == '/') {
#endif
			*ptr = 0;
			strcpy(filename, ptr + 1);
			break;
		}
		ptr--;
	}
}

void Device_HHOPEN(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int temp;
	struct stat status;
	char entryname[FILENAME_MAX];
	char *ext;
	int size;
	int extended;
	struct tm *time;
	char *end_dir;
	char end_dir_str[128];

	if (devbug)
		Aprint("HHOPEN");

	fid = regX >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	Device_GetFilename();
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
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
	if (filename[0] == ':')
		sprintf(fname, "%s%s", H[devnum], filename);
	else
		sprintf(fname, "%s:%s", H[devnum], filename);
#else
#ifdef BACK_SLASH
	if (filename[0] == '\\')
		sprintf(fname, "%s%s", H[devnum], filename);
	else
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
	if (filename[0] == '/')
		sprintf(fname, "%s%s", H[devnum], filename);
	else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif
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
			regY = 170;
			SetN;
		}
		break;
	case 6:
	case 7:
#ifdef DO_DIR
		Device_SeparateFileFromPath();
		extended = dGetByte(ICAX2Z);
		fp[fid] = tmpfile();
		if (fp[fid]) {
			dp = opendir(pathname);
			if (dp) {
				struct dirent *entry;

				if (extended >= 128) {
					fprintf(fp[fid], "\nVolume:    HDISK%d\nDirectory: ",
							devnum);
					if (strcmp(pathname, H[devnum]) == 0)
						fprintf(fp[fid], "MAIN\n\n");
					else {
						end_dir = pathname + strlen(pathname);
						while (end_dir > pathname) {
#ifdef BACK_SLASH
							if (*end_dir == '\\') {
#else
							if (*end_dir == '/') {
#endif
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
					strcpy(entryname, entry->d_name);
					if (match(strtoupper(filename), strtoupper(entryname)))
						if (entry->d_name[0] != '.') {
#ifdef BACK_SLASH
							sprintf(fname, "%s\\%s", pathname,
									entry->d_name);
#else
							sprintf(fname, "%s/%s", pathname,
									entry->d_name);
#endif
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
								if (status.st_mode & S_IFDIR) {
									fprintf(fp[fid], "%-8s     <DIR>  ",
											entryname);
								}
								else {
									fprintf(fp[fid], "%-8s %-3s %6d ",
											entryname, ext,
											(int) status.st_size);
								}
								time = localtime(&status.st_mtime);
								if (time->tm_year >= 100)
									time->tm_year -= 100;
								fprintf(fp[fid],
										"%02d-%02d-%02d %02d:%02d\n",
										time->tm_mon, time->tm_mday,
										time->tm_year, time->tm_hour,
										time->tm_min);
							}
							else {
								if (status.st_mode & S_IFDIR)
									ext = "\304\311\322";
								fprintf(fp[fid], "%s %-8s%-3s %03d\n",
										(status.
										 st_mode & S_IWUSR) ? " " : "*",
										entryname, ext, size);
							}
						}
				}

				if (extended >= 128)
					fprintf(fp[fid], "   999 FREE SECTORS\n");
				else
					fprintf(fp[fid], "999 FREE SECTORS\n");

				closedir(dp);

				regY = 1;
				ClrN;

				rewind(fp[fid]);

				flag[fid] = TRUE;
			}
			else {
				regY = 163;
				SetN;
				fclose(fp[fid]);
				fp[fid] = NULL;
			}
		}
		else
#endif							/* DO_DIR */
		{
			regY = 163;
			SetN;
		}
		break;
	case 8:
	case 9:					/* write at the end of file (append) */
		if (hd_read_only) {
			regY = 135;			/* device is read only */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, temp == 9 ? "ab" : "wb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	case 12:					/* read and write  (update) */
		if (hd_read_only) {
			regY = 135;			/* device is read only */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, "rb+");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	default:
		regY = 163;
		SetN;
		break;
	}
}

void Device_HHCLOS(void)
{
	if (devbug)
		Aprint("HHCLOS");

	fid = regX >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	regY = 1;
	ClrN;
}

void Device_HHREAD(void)
{
	if (devbug)
		Aprint("HHREAD");

	fid = regX >> 4;

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
			regY = 136;
			SetN;
		}
	}
	else {
		regY = 163;
		SetN;
	}
}

void Device_HHWRIT(void)
{
	if (devbug)
		Aprint("HHWRIT");

	fid = regX >> 4;

	if (fp[fid]) {
		int ch;

		ch = regA;
		if (flag[fid]) {
			switch (ch) {
			case 0x9b:
				ch = 0x0a;
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
		regY = 163;
		SetN;
	}
}

void Device_HHSTAT(void)
{
	if (devbug)
		Aprint("HHSTAT");

	fid = regX >> 4;

	regY = 146;
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
		binf = 0;
		if (start_binloading) {
			start_binloading = 0;
			Aprint("binload: not valid BIN file");
			regY = 180;
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

void Device_HSPEC_BIN_loader_cont(void)
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

void Device_HHSPEC_Rename(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	char newfname[FILENAME_MAX];
	char renamefname[FILENAME_MAX];
	int status;
	struct stat filestatus;
	int num_changed = 0;
	int num_locked = 0;

	if (devbug)
		Aprint("RENAME Command");
	if (hd_read_only) {
		regY = 135;				/* device is read only */
		SetN;
		return;
	}

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilenames();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif
	strcpy(filename, fname);
	Device_SeparateFileFromPath();

	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
#ifdef BACK_SLASH
				sprintf(fname, "%s\\%s", pathname, entry->d_name);
#else
				sprintf(fname, "%s/%s", pathname, entry->d_name);
#endif
				sprintf(renamefname, "%s", entry->d_name);
				fillin(newfilename, renamefname);
#ifdef BACK_SLASH
				sprintf(newfname, "%s\\%s", pathname, renamefname);
#else
				sprintf(newfname, "%s/%s", pathname, renamefname);
#endif
				stat(fname, &filestatus);
				if (filestatus.st_mode & S_IWUSR) {
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
	else
		status = -1;

	if (num_locked) {
		regY = 167;
		SetN;
		return;
	}
	else if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 170;
		SetN;
		return;
	}
}

void Device_HHSPEC_Delete(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int status;
	struct stat filestatus;
	int num_changed = 0;
	int num_locked = 0;

	if (devbug)
		Aprint("DELETE Command");
	if (hd_read_only) {
		regY = 135;				/* device is read only */
		SetN;
		return;
	}

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif
	strcpy(filename, fname);
	Device_SeparateFileFromPath();

	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
#ifdef BACK_SLASH
				sprintf(fname, "%s\\%s", pathname, entry->d_name);
#else
				sprintf(fname, "%s/%s", pathname, entry->d_name);
#endif
				stat(fname, &filestatus);
				if (filestatus.st_mode & S_IWUSR) {
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
	else
		status = -1;

	if (num_locked) {
		regY = 167;
		SetN;
		return;
	}
	else if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 170;
		SetN;
		return;
	}
}

void Device_HHSPEC_Lock(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int status;

	if (devbug)
		Aprint("LOCK Command");
	if (hd_read_only) {
		regY = 135;				/* device is read only */
		SetN;
		return;
	}

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif
	strcpy(filename, fname);
	Device_SeparateFileFromPath();

	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
#ifdef BACK_SLASH
				sprintf(fname, "%s\\%s", pathname, entry->d_name);
#else
				sprintf(fname, "%s/%s", pathname, entry->d_name);
#endif
				if (chmod(fname, S_IRUSR) != 0)
					status = -1;
				else
					num_changed++;
			}
		}

		closedir(dp);
	}
	else
		status = -1;

	if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 170;
		SetN;
		return;
	}
}

void Device_HHSPEC_Unlock(void)
{

	char fname[FILENAME_MAX];
	int devnum;
	int num_changed = 0;
	int status;

	if (devbug)
		Aprint("UNLOCK Command");
	if (hd_read_only) {
		regY = 135;				/* device is read only */
		SetN;
		return;
	}

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif
	strcpy(filename, fname);
	Device_SeparateFileFromPath();

	dp = opendir(pathname);
	if (dp) {
		struct dirent *entry;

		status = 0;
		while ((entry = readdir(dp))) {
			if (match(strtoupper(filename), strtoupper(entry->d_name))) {
				strtolower(entry->d_name);
#ifdef BACK_SLASH
				sprintf(fname, "%s\\%s", pathname, entry->d_name);
#else
				sprintf(fname, "%s/%s", pathname, entry->d_name);
#endif
				if (chmod(fname, S_IRUSR | S_IWUSR) != 0)
					status = -1;
				else
					num_changed++;
			}
		}

		closedir(dp);
	}
	else
		status = -1;

	if (status == 0 && num_changed != 0) {
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 170;
		SetN;
		return;
	}
}

void Device_HHSPEC_Note(void)
{
	unsigned long pos;
	unsigned long iocb;

	if (devbug)
		Aprint("NOTE Command");
	if (fp[fid]) {
		iocb = IOCB0 + ((fid) * 16);
		pos = ftell(fp[fid]);
		if (pos != -1) {
			dPutByte(iocb + ICAX5, (pos & 0xff));
			dPutByte(iocb + ICAX3, ((pos & 0xff00) >> 8));
			dPutByte(iocb + ICAX4, ((pos & 0xff0000) >> 16));
			regY = 1;
			ClrN;
			return;
		}
		else {
			regY = 163;
			SetN;
			return;
		}
	}
	else {
		regY = 163;
		SetN;
		return;
	}
}


void Device_HHSPEC_Point(void)
{
	unsigned long pos;
	unsigned long iocb;

	if (devbug)
		Aprint("POINT Command");
	if (fp[fid]) {
		iocb = IOCB0 + ((fid) * 16);
		pos = (dGetByte(iocb + ICAX4) << 16) +
			(dGetByte(iocb + ICAX3) << 8) + (dGetByte(iocb + ICAX5));
		if (fseek(fp[fid], pos, SEEK_SET) == 0) {
			regY = 1;
			ClrN;
			return;
		}
		else {
			regY = 163;			/* Need to change to invalid Point MDG */
			SetN;
			return;
		}
	}
	else {
		regY = 163;
		SetN;
		return;
	}
}

void Device_HHSPEC_Load(int mydos)
{
	char fname[FILENAME_MAX];
	char loadpath[FILENAME_MAX];
	char *drive_root;
	int devnum;
	int i;
	UBYTE buf[2];

	if (devbug)
		Aprint("LOAD Command");

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();

	for (i = 0; i < HPathCount; i++) {
		if (HPathDrive[i] == -1)
			drive_root = H[devnum];
		else
			drive_root = H[HPathDrive[i]];
		loadpath[0] = 0;
		apply_relative_path(HPaths[i], loadpath);
#ifdef BACK_SLASH
		if (filename[0] == '\\')
			sprintf(fname, "%s\\%s%s", drive_root, loadpath, filename);
#else
		if (filename[0] == '/')
			sprintf(fname, "%s/%s%s", drive_root, loadpath, filename);
#endif
		else
#ifdef BACK_SLASH
			sprintf(fname, "%s\\%s\\%s", drive_root, loadpath, filename);
#else
			sprintf(fname, "%s/%s/%s", drive_root, loadpath, filename);
#endif
		binf = fopen(fname, "rb");
		if (binf)
			break;
	}
	if (!binf) {
		Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
		if (filename[0] == '\\')
#else
		if (filename[0] == '/')
#endif
			sprintf(fname, "%s%s", H[devnum], filename);
		else
#ifdef BACK_SLASH
			sprintf(fname, "%s\\%s", H[devnum], filename);
#else
			sprintf(fname, "%s/%s", H[devnum], filename);
#endif
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
	printf("Mydos %d, AX1 %d, AX2 %d\n", mydos, dGetByte(ICAX1Z),
		   dGetByte(ICAX2Z));
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
	return;
}

void Device_HHSPEC_File_Length(void)
{
	unsigned long iocb;
	int filesize;
	struct stat fstatus;
	printf("File Length Command\n");
	if (devbug)
		Aprint("Get File Length Command");
	/* If IOCB is open, then assume it is a file length command.. */
	if (fp[fid]) {
		iocb = IOCB0 + ((fid) * 16);
		fstat(fileno(fp[fid]), &fstatus);
		filesize = fstatus.st_size;
		dPutByte(iocb + ICAX3, (filesize & 0xff));
		dPutByte(iocb + ICAX4, ((filesize & 0xff00) >> 8));
		dPutByte(iocb + ICAX5, ((filesize & 0xff0000) >> 16));
		regY = 1;
		ClrN;
		return;
	}
	/* Otherwise, we are going to assume it is a MYDOS Load File
	   command, since they use a different XIO value */
	else {
		Device_HHSPEC_Load(TRUE);
		return;
	}
}

void Device_HHSPEC_Mkdir(void)
{
	char fname[FILENAME_MAX];
	int devnum;

	if (devbug)
		Aprint("MKDIR Command");

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif

#ifdef WIN32
	if (mkdir(fname) == 0) {
#else
	umask(S_IWGRP | S_IWOTH);
	if (mkdir
		(fname,
		 S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP |
		 S_IXOTH) == 0) {
#endif
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 170;				/* Need to change to invalid Point MDG */
		SetN;
		return;
	}
}

void Device_HHSPEC_Deldir(void)
{
	char fname[FILENAME_MAX];
	int devnum;

	if (devbug)
		Aprint("DELDIR Command");

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	Device_GetFilename();
	Device_ApplyPathToFilename(devnum);
#ifdef BACK_SLASH
	if (filename[0] == '\\')
#else
	if (filename[0] == '/')
#endif
		sprintf(fname, "%s%s", H[devnum], filename);
	else
#ifdef BACK_SLASH
		sprintf(fname, "%s\\%s", H[devnum], filename);
#else
		sprintf(fname, "%s/%s", H[devnum], filename);
#endif

	if (rmdir(fname) == 0) {
		regY = 1;
		ClrN;
		return;
	}
	else {
		if (errno == ENOTEMPTY)
			regY = 167;
		else
			regY = 150;
		SetN;
		return;
	}
}

void Device_HHSPEC_Cd(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	struct stat filestatus;
	char new_path[FILENAME_MAX];

	if (devbug)
		Aprint("CD Command");

	Device_GetFilename();
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	strcpy(new_path, current_dir[devnum]);
	apply_relative_path(filename, new_path);
	if (new_path[0] != 0) {
#ifdef BACK_SLASH
		if (new_path[0] == '\\')
#else
		if (new_path[0] == '/')
#endif
			sprintf(fname, "%s%s", H[devnum], new_path);
		else
#ifdef BACK_SLASH
			sprintf(fname, "%s\\%s", H[devnum], new_path);
#else
			sprintf(fname, "%s/%s", H[devnum], new_path);
#endif
	}
	else
		strcpy(fname, H[devnum]);


	if (stat(fname, &filestatus) == 0) {
#ifdef BACK_SLASH
		if (new_path[0] == '\\')
#else
		if (new_path[0] == '/')
#endif
			strcpy(current_dir[devnum], &new_path[1]);
		else
			strcpy(current_dir[devnum], new_path);
		strcpy(current_path[devnum], H[devnum]);
		if (current_dir[devnum][0] != 0) {
#ifdef BACK_SLASH
			strcat(current_path[devnum], "\\");
#else
			strcat(current_path[devnum], "/");
#endif
			strcat(current_path[devnum], current_dir[devnum]);
		}
		regY = 1;
		ClrN;
		return;
	}
	else {
		regY = 150;
		SetN;
		return;
	}
}

void Device_HHSPEC_Disk_Info(void)
{
	int devnum;
	int bufadr;

	if (devbug)
		Aprint("Get Disk Information Command");

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

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

void Device_HHSPEC_Current_Dir(void)
{
	int devnum;
	char new_path[FILENAME_MAX];
	int bufadr;
	int pathlen;
	int i;

	if (devbug)
		Aprint("Get Current Directory Command");

	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		regY = 160;
		SetN;
		return;
	}
	if (devnum >= 5) {
		devnum -= 5;
	}

	bufadr = (dGetByte(ICBLHZ) << 8) | dGetByte(ICBLLZ);

	new_path[0] = '>';
	strcpy(&new_path[1], current_dir[devnum]);

	pathlen = strlen(new_path);

	for (i = 0; i < pathlen; i++) {
#ifdef BACK_SLASH
		if (new_path[i] == '\\')
#else
		if (new_path[i] == '/')
#endif
			new_path[i] = '>';
	}
	strtoupper(new_path);
	new_path[pathlen] = 155;

	for (i = 0; i < pathlen + 1; i++)
		dPutByte(bufadr + i, new_path[i]);

	regY = 1;
	ClrN;
}

void Device_HHSPEC(void)
{
	if (devbug)
		Aprint("HHSPEC");

	fid = regX >> 4;

	switch (dGetByte(ICCOMZ)) {
	case 0x20:
		Device_HHSPEC_Rename();
		return;
	case 0x21:
		Device_HHSPEC_Delete();
		return;
	case 0x23:
		Device_HHSPEC_Lock();
		return;
	case 0x24:
		Device_HHSPEC_Unlock();
		return;
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
	case 0x2A:
		Device_HHSPEC_Mkdir();
		return;
	case 0x2B:
		Device_HHSPEC_Deldir();
		return;
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

	regY = 146;
	SetN;
}

void Device_HHINIT(void)
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
void Device_PHCLOS(void);
static char spool_file[13];

void Device_PHOPEN(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phf)
		Device_PHCLOS();

	strcpy(spool_file,"SPOOL_XXXXXX\0");
	phf = fdopen(mkstemp(spool_file), "w");
	if (phf) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 130;
		SetN;
	}
}

void Device_PHCLOS(void)
{
	if (devbug)
		Aprint("PHCLOS");

	if (phf) {
		char command[256];
		int status;

		fclose(phf);

		sprintf(command, print_command, spool_file);
		system(command);
#if !defined(VMS) && !defined(MACOSX)
		status = unlink(spool_file);
		if (status == -1) {
			perror(spool_file);
			exit(1);
		}
#endif
		phf = NULL;
	}
	regY = 1;
	ClrN;
}

void Device_PHREAD(void)
{
	if (devbug)
		Aprint("PHREAD");

	regY = 146;
	SetN;
}

void Device_PHWRIT(void)
{
	unsigned char byte;
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
		regY = 144;
		SetN;
	}
}

void Device_PHSTAT(void)
{
	if (devbug)
		Aprint("PHSTAT");
}

void Device_PHSPEC(void)
{
	if (devbug)
		Aprint("PHSPEC");

	regY = 1;
	ClrN;
}

void Device_PHINIT(void)
{
	if (devbug)
		Aprint("PHINIT");

	phf = NULL;
	regY = 1;
	ClrN;
}

/* K: and E: handlers for BASIC version, using getchar() and putchar() */

#ifdef BASIC
/*
   ================================
   N = 0 : I/O Successful and Y = 1
   N = 1 : I/O Error and Y = error#
   ================================
 */

void Device_KHREAD(void)
{
	UBYTE ch;

	if (feof(stdin)) {
		Atari800_Exit(FALSE);
		exit(0);
	}
	ch = getchar();
	switch (ch) {
	case '\n':
		ch = (char) 0x9b;
		break;
	default:
		break;
	}
	regA = ch;
	regY = 1;
	ClrN;
}

void Device_EHOPEN(void)
{
	Aprint("Editor device open");
	regY = 1;
	ClrN;
}

void Device_EHREAD(void)
{
	UBYTE ch;

	if (feof(stdin)) {
		Atari800_Exit(FALSE);
		exit(0);
	}
	ch = getchar();
	switch (ch) {
	case '\n':
		ch = 0x9b;
		break;
	default:
		break;
	}
	regA = ch;
	regY = 1;
	ClrN;
}

void Device_EHWRIT(void)
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
			putchar(ch & 0x7f);
		break;
	}
	regY = 1;
	ClrN;
}

#endif							/* BASIC */

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

#if defined(R_IO_DEVICE)
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
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_OPEN) + 1,
							   ESC_EHOPEN, Device_EHOPEN);
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1,
							   ESC_EHREAD, Device_EHREAD);
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) + 1,
							   ESC_EHWRIT, Device_EHWRIT);
			patched = TRUE;
			break;
		case 'K':
			Aprint("Keyboard Device");
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1,
							   ESC_KHREAD, Device_KHREAD);
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
#if defined(R_IO_DEVICE)
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

#if defined(R_IO_DEVICE)
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
		h_entry_address =
			Device_UpdateHATABSEntry('H', h_entry_address,
									 H_TABLE_ADDRESS);

#if defined(R_IO_DEVICE)
	if (enable_r_patch)
		r_entry_address = Device_UpdateHATABSEntry('R', r_entry_address, R_TABLE_ADDRESS);
#endif
}

/* this is called when enable_h_patch is toggled */
void Device_UpdatePatches(void)
{
	if (enable_h_patch) {		/* enable H: device */
		/* change memory attributex for the area, where we put
		   H: handler table and patches */
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

#if defined(R_IO_DEVICE)
	if (enable_r_patch) {		/* enable R: device */
		/* change memory attributex for the area, where we put
		   R: handler table and patches */
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
