#ifndef _PBI_SCSI_H_
#define _PBI_SCSI_H_

#include "atari.h"
#include <stdio.h>

extern int SCSI_CD;
extern int SCSI_MSG;
extern int SCSI_IO;
extern int SCSI_BSY;
extern int SCSI_REQ;
extern int SCSI_SEL;
extern int SCSI_ACK;
extern FILE *SCSI_disk;

void SCSI_PutByte(UBYTE byte);
UBYTE SCSI_GetByte(void);
void SCSI_PutSEL(int newsel);
void SCSI_PutACK(int newack);

#endif /* _PBI_MIO_H_ */
