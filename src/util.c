/*
 * util.c - utility functions
 *
 * Copyright (c) 2005 Piotr Fusik
 * Copyright (c) 2005 Atari800 development team (see DOC/CREDITS)
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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif

#include "atari.h"
#include "util.h"

char *Util_strlcpy(char *dest, const char *src, size_t size)
{
	strncpy(dest, src, size);
	dest[size - 1] = '\0';
	return dest;
}

char *Util_strupper(char *s)
{
	char *p;
	for (p = s; *p != '\0'; p++)
		if (*p >= 'a' && *p <= 'z')
			*p += 'A' - 'a';
	return s;
}

char *Util_strlower(char *s)
{
	char *p;
	for (p = s; *p != '\0'; p++)
		if (*p >= 'A' && *p <= 'Z')
			*p += 'a' - 'A';
	return s;
}

void Util_chomp(char *s)
{
	int len;

	len = strlen(s);
	if (len >= 2 && s[len - 1] == '\n' && s[len - 2] == '\r')
		s[len - 2] = '\0';
	else if (len >= 1 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
		s[len - 1] = '\0';
}

void Util_trim(char *s)
{
	char *p = s;
	char *q;
	/* skip leading whitespace */
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	/* now p points at the first non-whitespace character */

	if (*p == '\0') {
		/* only whitespace */
		*s = '\0';
		return;
	}

	q = s + strlen(s);
	/* skip trailing whitespace */
	/* we have found p < q such that *p is non-whitespace,
	   so this loop terminates with q >= p */
	do
		q--;
	while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n');

	/* now q points at the last non-whitespace character */
	/* cut off trailing whitespace */
	*++q = '\0';

	/* move to string */
	memmove(s, p, q + 1 - p);
}

int Util_sscandec(const char *s)
{
	int result;
	if (*s == '\0')
		return -1;
	result = 0;
	for (;;) {
		if (*s >= '0' && *s <= '9')
			result = 10 * result + *s - '0';
		else if (*s == '\0')
			return result;
		else
			return -1;
		s++;
	}
}

int Util_sscanhex(const char *s)
{
	int result;
	if (*s == '\0')
		return -1;
	result = 0;
	for (;;) {
		if (*s >= '0' && *s <= '9')
			result = 16 * result + *s - '0';
		else if (*s >= 'A' && *s <= 'F')
			result = 16 * result + *s - 'A' + 10;
		else if (*s >= 'a' && *s <= 'f')
			result = 16 * result + *s - 'a' + 10;
		else if (*s == '\0')
			return result;
		else
			return -1;
		s++;
	}
}

int Util_sscanbool(const char *s)
{
	if (*s == '0' && s[1] == '\0')
		return 0;
	if (*s == '1' && s[1] == '\0')
		return 1;
	return -1;
}

void *Util_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL) {
		Atari800_Exit(FALSE);
		printf("Fatal error: out of memory\n");
		exit(1);
	}
	return ptr;
}

void *Util_realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL) {
		Atari800_Exit(FALSE);
		printf("Fatal error: out of memory\n");
		exit(1);
	}
	return ptr;
}

char *Util_strdup(const char *s)
{
	/* don't use strdup(): it is unavailable on WinCE */
	size_t size = strlen(s) + 1;
	char *ptr = (char *) Util_malloc(size);
	memcpy(ptr, s, size); /* faster than strcpy(ptr, s) */
	return ptr;
}

void Util_splitpath(const char *path, char *dir_part, char *file_part)
{
	const char *p;
	/* find the last DIR_SEP_CHAR except the last character */
	for (p = path + strlen(path) - 2; p >= path; p--) {
		if (*p == DIR_SEP_CHAR) {
			if (dir_part != NULL) {
				int len = p - path;
				if (p == path || (p == path + 2 && path[1] == ':'))
					/* root dir: include DIR_SEP_CHAR in dir_part */
					len++;
				memcpy(dir_part, path, len);
				dir_part[len] = '\0';
			}
			if (file_part != NULL)
				strcpy(file_part, p + 1);
			return;
		}
	}
	/* no DIR_SEP_CHAR: current dir */
	if (dir_part != NULL)
		dir_part[0] = '\0';
	if (file_part != NULL)
		strcpy(file_part, path);
}

void Util_catpath(char *result, const char *path1, const char *path2)
{
#ifdef HAVE_SNPRINTF
	snprintf(result, FILENAME_MAX,
#else
	sprintf(result,
#endif
		(path1[0] == '\0' || path2[0] == DIR_SEP_CHAR || path1[strlen(path1) - 1] == DIR_SEP_CHAR)
			? "%s%s" : "%s" DIR_SEP_STR "%s", path1, path2);
}

int Util_fileexists(const char *filename)
{
	FILE *fp;
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return FALSE;
	fclose(fp);
	return TRUE;
}

#ifdef WIN32

int Util_direxists(const char *filename)
{
	DWORD attr;
#ifdef UNICODE
	WCHAR wfilename[FILENAME_MAX];
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0)
		return FALSE;
	attr = GetFileAttributes(wfilename);
#else
	attr = GetFileAttributes(filename);
#endif /* UNICODE */
	if (attr == 0xffffffff)
		return FALSE;
	return (attr & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}

#elif defined(HAVE_STAT)

int Util_direxists(const char *filename)
{
	struct stat filestatus;
	return stat(filename, &filestatus) == 0 && (filestatus.st_mode & S_IFDIR);
}

#else

int Util_direxists(const char *filename)
{
	return TRUE;
}

#endif /* defined(HAVE_STAT) */


int Util_flen(FILE *fp)
{
	fseek(fp, 0, SEEK_END);
	return (int) ftell(fp);
}

/* Opens a new temporary file and fills in filename with its name.
   filename must point to FILENAME_MAX characters buffer, but doesn't need
   to be initialized. */
FILE *Util_tmpfile(char *filename, const char *mode)
{
	/* We cannot simply call tmpfile(), because we don't want the file
	   to be deleted when we close it, and we need the filename. */

#if defined(HAVE_MKSTEMP) && defined(HAVE_FDOPEN)
	/* this is the only implementation without a race condition */
	strcpy(filename, "a8XXXXXX");
	/* mkstemp() modifies the 'X'es and returns an open descriptor */
	return fdopen(mkstemp(filename), mode);
#elif defined(HAVE_TMPNAM)
	/* tmpnam() is better than mktemp(), because it creates filenames
	   in system's temporary directory. It is also more portable. */
	return fopen(tmpnam(filename), mode);
#elif defined(HAVE_MKTEMP)
	strcpy(filename, "a8XXXXXX");
	/* mktemp() modifies the 'X'es and returns filename */
	return fopen(mktemp(filename), mode);
#else
	/* Roll-your-own */
	int no;
	for (no = 0; no < 1000000; no++) {
		sprintf(filename, "a8%06d", no);
		if (!Util_fileexists(filename))
			return fopen(filename, mode);
	}
	return NULL;
#endif
}

#if defined(WIN32) && defined(UNICODE)
int Util_unlink(const char *filename)
{
	WCHAR wfilename[FILENAME_MAX];
#ifdef _WIN32_WCE
	char cwd[FILENAME_MAX];
	char fullfilename[FILENAME_MAX];
	if (filename[0] != '\\' && filename[0] != '/') {
		getcwd(cwd, FILENAME_MAX);
		Util_catpath(fullfilename, cwd, filename);
		if (MultiByteToWideChar(CP_ACP, 0, fullfilename, -1, wfilename, FILENAME_MAX) <= 0)
			return -1;
	}
	else
#endif
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0)
		return -1;
	return (DeleteFile(wfilename) != 0) ? 0 : -1;
}
#endif /* defined(WIN32) && defined(UNICODE) */
