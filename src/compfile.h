#ifndef _COMPFILE_H_
#define _COMPFILE_H_

#include <stdio.h>  /* FILE */

int CompressedFile_ExtractGZ(const char *infilename, FILE *outfp);
int CompressedFile_DCMtoATR(FILE *infp, FILE *outfp);

#endif /* _COMPFILE_H_ */
