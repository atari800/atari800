#ifndef __SIO__
#define __SIO__

#include "atari.h"

#define MAX_DRIVES 8
#ifdef WIN32
#include "windows.h"
#define FILENAME_LEN MAX_PATH
#else
#define FILENAME_LEN 256
#endif	/* WIN32 */

/*
 * it seems, there are two different ATR formats with different handling for
 * DD sectors
 */
#define SIO2PC_ATR	1
#define OTHER_ATR	2

typedef enum tagUnitStatus {
	Off,
	NoDisk,
	ReadOnly,
	ReadWrite
} UnitStatus;

extern char sio_status[256];
extern UnitStatus drive_status[MAX_DRIVES];
extern char sio_filename[MAX_DRIVES][FILENAME_LEN];

int SIO_Mount(int diskno, const char *filename, int b_open_readonly );
void SIO_Dismount(int diskno);
void SIO_DisableDrive(int diskno);
void SIO(void);
UnitStatus SIO_Drive_Status( int diskno );

void SIO_SEROUT(unsigned char byte, int cmd);
int SIO_SERIN(void);

#define SIO_NoFrame         (0x00)
#define SIO_CommandFrame    (0x01)
#define SIO_StatusRead      (0x02)
#define SIO_ReadFrame       (0x03)
#define SIO_WriteFrame      (0x04)
#define SIO_FinalStatus     (0x05)
#define SIO_FormatFrame     (0x06)

void SwitchCommandFrame(int onoff);
void SIO_PutByte(int byte);
int SIO_GetByte(void);
void SIO_Initialize(void);
void SIO_Initialise(int *argc, char *argv[]);

/* Some defines about the serial I/O timing. Currently fixed! */
#define XMTDONE_INTERVAL 15
#define SERIN_INTERVAL 8
#define SEROUT_INTERVAL 8
#define ACK_INTERVAL 36
#ifndef NO_SECTOR_DELAY
#define SECTOR_DELAY 3200
#endif	/* NO_SECTOR_DELAY */

#endif	/* __SIO__ */
