/* Missing types and forward declarations for windows ce port */

#ifndef _WCEMISSING_H_
#define _WCEMISSING_H_

#include <stdlib.h>
#include <stdio.h>

/* some extra defines not covered in config.h */

#define NO_YPOS_BREAK_FLICKER			1
#define DIRTYRECT						1
#define SUPPORTS_ATARI_CONFIGINIT		1
#undef WIN32
#define WIN32							1

#define perror(s) wce_perror(s)
void wce_perror(char *s);

FILE * __cdecl wce_fopen(const char *fname, const char *fmode);
#define fopen wce_fopen

/* forward declarations of missing functions */
char *tmpnam(char *string);
FILE *tmpfile();
char *getcwd(char *buffer, int maxlen);
char *getenv(const char *varname);


#endif
