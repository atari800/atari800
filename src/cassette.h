#include "atari.h"		/* for UBYTE */
#include "rt-config.h"	/* for MAX_FILENAME_LEN */

void CASSETTE_Initialise(int *argc, char *argv[]);

int CASSETTE_Insert(char *filename);
void CASSETTE_Remove(void);
extern char cassette_filename[MAX_FILENAME_LEN];

extern int cassette_current_block;
extern int cassette_max_block;

UBYTE CASSETTE_Sio(void);
extern UBYTE cassette_buffer[4096];
