#ifndef _COMPFILE_H_
#define _COMPFILE_H_

#include <stdio.h>  /* FILE */

FILE *opendcm(const char *infilename, char *outfilename);
FILE *openzlib(const char *infilename, char *outfilename);
int dcmtoatr(FILE *fin, FILE *fout, const char *input, char *output);

#endif /* _COMPFILE_H_ */
