/*
 * chdir.c - chdir stuff
 *
 * original (C) 2000 Jordan DeLong
 * some changes (C) 2006 Christian Groessler
 */
#include <arch/types.h>
#include <kos/limits.h>
#include <kos/fs.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

/* the current directory */
static char cwd[MAX_FN_LEN];

/* read a pathname based on the current directory and turn it into an abs one, we
   don't check for validity, or really do anything except handle ..'s and .'s */
void makeabspath(char *buff, const char *path, size_t size) {
	int numtxts;
	int i;
	char *txts[32];		/* max of 32...should be nuff */
	char *rslash;
        char *workpath, *workpathf;

	/* check if path is already absolute */
	if (path[0] == '/') {
		strncpy(buff, path, size);
		return;
	}

	numtxts = 0;

	if ((i = strlen(path))) {
		workpath = workpathf = malloc(i);
		if (! workpath) goto fallback;
		memcpy(workpath, path, i);
	}
	else
		goto fallback;  /* shouldn't happen */

	/* split the path into tokens */
	for (numtxts = 0; numtxts < 32;) {
		if ((txts[numtxts] = strsep(&workpath, "/")) == NULL)
			break;
		if (*txts[numtxts] != '\0')
			numtxts++;
	}

	/* start from the current directory */
	strncpy(buff, cwd, size);

	for (i = 0; i < numtxts; i++) {
		if (strcmp(txts[i], "..") == 0) {
			if ((rslash = strrchr(buff, '/')) != NULL)
				*rslash = '\0';
		} else if (strcmp(txts[i], ".") == 0) {
			/* do nothing */
		} else {
			if (buff[strlen(buff) - 1] != '/')
				strncat(buff, "/", size - 1 - strlen(buff));
			strncat(buff, txts[i], size - 1 - strlen(buff));
		}
	}
	free(workpathf);

	/* make sure it's not empty */
	if (buff[0] == '\0') {
	fallback:
		buff[0] = '/';
		buff[1] = '\0';
	}

	return;
}

/* change the current directory (dir is an absolute path for now) */
int chdir(const char *dir) {
	char buff[MAX_FN_LEN];

	makeabspath(buff, dir, MAX_FN_LEN);
	strncpy(cwd, buff, MAX_FN_LEN);

	fs_chdir(cwd);

	return 0;
}

/* get the current directory */
char *getcwd(char *buf, size_t size) {
	strncpy(buf, cwd, size);
	return buf;
}
