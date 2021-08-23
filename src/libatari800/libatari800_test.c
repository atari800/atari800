#include <stdio.h>
#include <string.h>

#include "libatari800.h"

static void debug_screen()
{
	/* print out portion of screen, assuming graphics 0 display list */
	unsigned char *screen = libatari800_get_screen_ptr();
	int x, y;

	screen += 384 * 24 + 24;
	for (y = 0; y < 32; y++) {
		for (x = 8; x < 88; x++) {
			unsigned char c = screen[x];
			if (c == 0)
				printf(" ");
			else if (c == 0x94)
				printf(".");
			else if (c == 0x9a)
				printf("X");
			else
				printf("?");
		}
		printf("\n");
		screen += 384;
	}
}


/* This wav writing code is from file_export.c, duplicated almost exactly except using
   libatari800_* methods to demonstrate usage of the library.  */

static FILE *wav;

static ULONG byteswritten;

/* write 16-bit word as little endian */
static void fputw(UWORD x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
}

/* write 32-bit long as little endian */
static void fputl(ULONG x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
	fputc((x >> 16) & 0xff, fp);
	fputc((x >> 24) & 0xff, fp);
}

FILE *wavOpen(const char *szFileName)
{
	FILE *fp;

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;

	fputs("RIFF", fp);
	fputl(0, fp); /* length to be filled in upon file close */
	fputs("WAVE", fp);

	fputs("fmt ", fp);
	fputl(16, fp);
	fputw(1, fp);
	fputw(libatari800_get_num_sound_channels(), fp);
	fputl(libatari800_get_sound_frequency(), fp);
	fputl(libatari800_get_sound_frequency() * libatari800_get_sound_sample_size(), fp);
	fputw(libatari800_get_num_sound_channels() * libatari800_get_sound_sample_size(), fp);
	fputw(libatari800_get_sound_sample_size() * 8, fp);

	fputs("data", fp);
	fputl(0, fp); /* length to be filled in upon file close */

	if (ftell(fp) != 44) {
		fclose(fp);
		return NULL;
	}

	byteswritten = 0;
	return fp;
}

int wavClose()
{
	int bSuccess = TRUE;
	char aligned = 0;

	if (wav != NULL) {
		/* A RIFF file's chunks must be word-aligned. So let's align. */
		if (byteswritten & 1) {
			if (putc(0, wav) == EOF)
				bSuccess = FALSE;
			else
				aligned = 1;
		}

		if (bSuccess) {
			/* Sound file is finished, so modify header and close it. */
			if (fseek(wav, 4, SEEK_SET) != 0)	/* Seek past RIFF */
				bSuccess = FALSE;
			else {
				/* RIFF header's size field must equal the size of all chunks
				 * with alignment, so the alignment byte is added.
				 */
				fputl(byteswritten + 36 + aligned, wav);
				if (fseek(wav, 40, SEEK_SET) != 0)
					bSuccess = FALSE;
				else {
					/* But in the "data" chunk size field, the alignment byte
					 * should be ignored. */
					fputl(byteswritten, wav);
				}
			}
		}
		fclose(wav);
		wav = NULL;
	}

	return bSuccess;
}

int wavWrite()
{
	if (wav) {
		int result;

		printf("frame %d: writing %d bytes in sound buffer\n", libatari800_get_frame_number(), libatari800_get_sound_buffer_len());
		result = fwrite(libatari800_get_sound_buffer(), 1, libatari800_get_sound_buffer_len(), wav);
		byteswritten += result;
		if (result != libatari800_get_sound_buffer_len()) {
			wavClose();
		}

		return result;
	}

	return 0;
}

int main(int argc, char **argv) {
	input_template_t input;
	int i;
	int save_wav = FALSE;
	int show_screen = TRUE;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-wav") == 0) {
			save_wav = TRUE;
			show_screen = FALSE;
		}
	}

	/* force the 400/800 OS to get the Memo Pad */
	char *test_args[] = {
		"-atari",
		NULL,
	};
	libatari800_init(-1, test_args);

	libatari800_clear_input_array(&input);

	emulator_state_t state;
	cpu_state_t *cpu;
	pc_state_t *pc;

	printf("emulation: fps=%f\n", libatari800_get_fps());

	printf("sound: freq=%d, bytes/sample=%d, channels=%d, max buffer size=%d\n", libatari800_get_sound_frequency(), libatari800_get_sound_sample_size(), libatari800_get_num_sound_channels(), libatari800_get_sound_buffer_allocated_size());

	if (save_wav) {
		wav = wavOpen("libatari800_test.wav");
	}
	while (libatari800_get_frame_number() < 200) {
		libatari800_get_current_state(&state);
		cpu = (cpu_state_t *)&state.state[state.tags.cpu];  /* order: A,SR,SP,X,Y */
		pc = (pc_state_t *)&state.state[state.tags.pc];
		if (show_screen) printf("frame %d: A=%02x X=%02x Y=%02x SP=%02x SR=%02x PC=%04x\n", libatari800_get_frame_number(), cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S, pc->PC);
		libatari800_next_frame(&input);
		if (libatari800_get_frame_number() > 100) {
			if (show_screen) debug_screen();
			input.keychar = 'A';
		}
		if (wav) {
			wavWrite();
		}
	}
	if (wav) {
		wavClose();
	}
	libatari800_get_current_state(&state);
	cpu = (cpu_state_t *)&state.state[state.tags.cpu];  /* order: A,SR,SP,X,Y */
	pc = (pc_state_t *)&state.state[state.tags.pc];
	printf("frame %d: A=%02x X=%02x Y=%02x SP=%02x SR=%02x PC=%04x\n", libatari800_get_frame_number(), cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S, pc->PC);

	libatari800_exit();
}
