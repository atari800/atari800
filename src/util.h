#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"
#include <stdio.h>
#include <string.h>

/* String functions ------------------------------------------------------ */

#ifdef WIN32
#define Util_stricmp _stricmp
#elif defined(HAVE_STRCASECMP)
#define Util_stricmp strcasecmp
#else
#define Util_stricmp stricmp
#endif

/* Same as strlcpy() in some C libraries: copies src to dest
   and terminates the string. Never writes more than size characters
   to dest (the result may be truncated). Returns dest. */
char *Util_strlcpy(char *dest, const char *src, size_t size);

/* Modifies the string to uppercase and returns it. */
char *Util_strupper(char *s);

/* Modifies the string to lowercase and returns it. */
char *Util_strlower(char *s);

/* Similar to Perl's chomp(): removes trailing LF, CR or CR/LF. */
void Util_chomp(char *s);

/* Similar to Basic's trim(): removes leading and trailing whitespace. */
void Util_trim(char *s);

/* Converts the string to a non-negative integer and returns it.
   The string must represent the number and nothing else.
   -1 indicates an error. */
int Util_sscandec(const char *s);

/* Likewise, but parses hexadecimal numbers. */
int Util_sscanhex(const char *s);

/* Likewise, but allows only 0 and 1. */
int Util_sscanbool(const char *s);


/* Memory management ----------------------------------------------------- */

/* malloc() with out-of-memory checking. Never returns NULL. */
void *Util_malloc(size_t size);

/* realloc() with out-of-memory checking. Never returns NULL. */
void *Util_realloc(void *ptr, size_t size);

/* strdup() with out-of-memory checking. Never returns NULL. */
char *Util_strdup(const char *s);


/* File I/O -------------------------------------------------------------- */

#ifdef BACK_SLASH
#define DIR_SEP_CHAR '\\'
#define DIR_SEP_STR  "\\"
#else
#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR  "/"
#endif

/* Splits a filename into directory part and file part. */
/* dir_part or file_part may be NULL. */
void Util_splitpath(const char *path, char *dir_part, char *file_part);

/* Concatenates file paths.
   Places directory separator char between paths, unless path1 is empty
   or ends with the separator char, or path2 starts with the separator char. */
void Util_catpath(char *result, const char *path1, const char *path2);

/* Returns TRUE if the specified file exists. */
int Util_fileexists(const char *filename);

/* Rewinds the stream to its beginning. */
#ifdef HAVE_REWIND
#define Util_rewind(fp) rewind(fp)
#else
#define Util_rewind(fp) fseek(fp, 0, SEEK_SET)
#endif

/* Returns the length of an open stream.
   May change the current position. */
int Util_flen(FILE *fp);

/* Opens a new temporary file and fills in filename with its name. */
FILE *Util_tmpfile(char *filename, const char *mode);

#endif /* _UTIL_H_ */
