#ifndef _BINLOAD_H_
#define _BINLOAD_H_

#include <stdio.h> /* FILE */
#include "atari.h" /* UBYTE */

extern FILE *bin_file;

int BIN_loader(const char *filename);
void BIN_loader_cont(void);
extern int start_binloading;
extern int loading_basic;
#define LOADING_BASIC_SAVED              1
#define LOADING_BASIC_LISTED             2
#define LOADING_BASIC_LISTED_ATARI       3
#define LOADING_BASIC_LISTED_CR          4
#define LOADING_BASIC_LISTED_LF          5
#define LOADING_BASIC_LISTED_CRLF        6
#define LOADING_BASIC_LISTED_CR_OR_CRLF  7
#define LOADING_BASIC_RUN                8
int BIN_loader_start(UBYTE *buffer);

#endif /* _BINLOAD_H_ */
