#ifndef _COMPFILE_H_
#define _COMPFILE_H_

#include <stdio.h>  /* FILE */

int CompFile_ExtractGZ(const char *infilename, FILE *outfp);
int CompFile_DCMtoATR(FILE *infp, FILE *outfp);

#endif /* _COMPFILE_H_ */
