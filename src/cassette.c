#include <stdio.h>
#include <string.h>

#include "atari.h"
#include "cassette.h"
#include "memory.h"
#include "sio.h"

static FILE *cassette_file = NULL;
UBYTE cassette_buffer[4096];

char cassette_filename[FILENAME_LEN];
int cassette_current_block;
int cassette_max_block;

void CASSETTE_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-tape") == 0)
			CASSETTE_Insert(argv[++i]);
		else
			argv[j++] = argv[i];
	}

	*argc = j;
}

int CASSETTE_Insert(char *filename)
{
	ULONG file_length;
	cassette_file = fopen(filename, "rb");
	if (cassette_file == NULL)
		return FALSE;
	strcpy(cassette_filename, filename);
	fseek(cassette_file, 0L, SEEK_END);
	file_length = ftell(cassette_file);
	fseek(cassette_file, 0L, SEEK_SET);
	cassette_current_block = 1;
	cassette_max_block = (file_length + 127) / 128 + 1;
	return TRUE;
}

void CASSETTE_Remove(void)
{
	if (cassette_file != NULL) {
		fclose(cassette_file);
		cassette_file = NULL;
	}
	strcpy(cassette_filename, "None");
}

static void ReadRecord(void)
{
	cassette_buffer[0] = 0x55;
	cassette_buffer[1] = 0x55;
	if (cassette_current_block == cassette_max_block) {
		/* EOF record */
		cassette_buffer[2] = 0xfe;
		memset(cassette_buffer + 3, 0, 128);
	}
	else {
		int bytes;
		fseek(cassette_file, (cassette_current_block - 1) * 128, SEEK_SET);
		bytes = fread(cassette_buffer + 3, 1, 128, cassette_file);
		if (bytes < 128) {
			cassette_buffer[2] = 0xfa;	/* non-full record */
			memset(cassette_buffer + 3 + bytes, 0, 127 - bytes);
			cassette_buffer[0x82] = bytes;
		}
		else
			cassette_buffer[2] = 0xfc;	/* full record */
	}
	cassette_buffer[0x83] = SIO_ChkSum(cassette_buffer, 0x84);
	cassette_current_block++;
}

UBYTE CASSETTE_Sio(void)
{
	/* Only 0x52 (Read) command is supported */
	if (dGetByte(0x302) != 0x52)
		return 'N';
	if (cassette_file == NULL)
		return 'E';
	if (cassette_current_block > cassette_max_block)
		return 'E';	/* EOF */
	ReadRecord();
	return 'C';
}
