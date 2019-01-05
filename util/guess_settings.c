#include <stdio.h>
#include <string.h>

#include "libatari800.h"

char *machines[] = {
	"-atari",
	"-xl",
	"-xe",
	"-xegs",
	NULL
};

char *tv_modes[] = {
	"-pal",
	"-ntsc",
	NULL
};

typedef struct {
	int size;
	int type;
	char *name;
} cart_types_t;

cart_types_t cart_list_a8[] = {
	{2,57,"Standard 2 KB",},
	{4,46,"Blizzard 4 KB",},
	{4,58,"Standard 4 KB",},
	{4,59,"Right slot 4 KB",},
	{8,1,"Standard 8 KB",},
	{8,21,"Right slot 8 KB",},
	{8,39,"Phoenix 8 KB",},
	{8,44,"OSS 8 KB",},
	{8,53,"Low bank 8 KB",},
	{16,2,"Standard 16 KB",},
	{16,3,"OSS two chip (034M) 16 KB",},
	{16,15,"OSS one chip 16 KB",},
	{16,26,"MegaCart 16 KB",},
	{16,40,"Blizzard 16 KB",},
	{16,45,"OSS two chip (043M) 16 KB",},
	{32,5,"DB 32 KB",},
	{32,12,"XEGS 32 KB",},
	{32,22,"Williams 32 KB",},
	{32,27,"MegaCart 32 KB",},
	{32,33,"Switchable XEGS 32 KB",},
	{32,47,"AST 32 KB",},
	{32,52,"Ultracart 32 KB",},
	{32,60,"Blizzard 32 KB",},
	{40,18,"Bounty Bob 40 KB",},
	{64,8,"Williams 64 KB",},
	{64,9,"Express 64 KB",},
	{64,10,"Diamond 64 KB",},
	{64,11,"SpartaDOS X 64 KB",},
	{64,13,"XEGS (banks 0-7) 64 KB",},
	{64,28,"MegaCart 64 KB",},
	{64,34,"Switchable XEGS 64 KB",},
	{64,48,"Atrax SDX 64 KB",},
	{64,50,"Turbosoft 64 KB",},
	{64,67,"XEGS (banks 8-15) 64 KB",},
	{128,14,"XEGS 128 KB",},
	{128,17,"Atrax 128 KB",},
	{128,29,"MegaCart 128 KB",},
	{128,35,"Switchable XEGS 128 KB",},
	{128,41,"Atarimax 128 KB Flash",},
	{128,43,"SpartaDOS X 128 KB",},
	{128,49,"Atrax SDX 128 KB",},
	{128,51,"Turbosoft 128 KB",},
	{128,54,"SIC! 128 KB",},
	{256,23,"XEGS 256 KB",},
	{256,30,"MegaCart 256 KB",},
	{256,36,"Switchable XEGS 256 KB",},
	{256,55,"SIC! 256 KB",},
	{512,24,"XEGS 512 KB",},
	{512,31,"MegaCart 512 KB",},
	{512,37,"Switchable XEGS 512 KB",},
	{512,56,"SIC! 512 KB",},
	{1024,25,"XEGS 1 MB",},
	{1024,32,"MegaCart 1 MB",},
	{1024,38,"Switchable XEGS 1 MB",},
	{1024,42,"Atarimax 1 MB Flash",},
	{2048,61,"MegaMax 2 MB",},
	{2048,64,"MegaCart 2 MB",},
	{4096,63,"Flash MegaCart 4 MB",},
	{32768,65,"The!Cart 32 MB",},
	{65536,66,"The!Cart 64 MB",},
	{131072,62,"The!Cart 128 MB",},
};

cart_types_t cart_list_5200[] = {
	{4, 20, "Standard 4 KB 5200",},
	{8, 19, "Standard 8 KB 5200",},
	{16, 16, "One chip 16 KB 5200",},
	{16, 6,  "Two chip 16 KB 5200",},
	{32, 4,  "Standard 32 KB 5200",},
	{40, 7,  "Bounty Bob 40 KB 5200",},
};

char *test_args[32];
char *default_args[] = {
	"atari800",
	"-nobasic",
	"-800-rev",
	"altirra",
	"-xl-rev",
	"altirra",
	// "-config",
	// "/dev/null",
};

int run_emulator(int num_args, int num_frames, int verbose) {
	int i;

	if (verbose) {
		for (i=0; i<num_args; i++) {
			printf("%s ", test_args[i]);
		}
		printf("\n");
	}
	libatari800_init(num_args, test_args);

	unsigned char state[STATESAV_MAX_SIZE];
	statesav_tags_t tags;
	input_template_t input;

	int frame = 0;
	int success = 1;
	while (frame < num_frames) {
		// libatari800_get_current_state(state, &tags);
		if (!libatari800_next_frame(&input)) {
			printf("Crash encountered at frame %d\n", frame);
			success = 0;
			break;
		}
		frame++;
	}

	return success;
}

int main(int argc, char **argv) {
	int frame;
	input_template_t input;
	libatari800_clear_input_array(&input);

	/* one time init stuff */
	int verbose = 0;
	int num_frames = 1000;

	int i;
	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "-v") == 0) {
				verbose = 1;
			}
		}
		else {
			char **machine, **tv_mode;
			tv_mode = tv_modes;
			int tv_index, machine_index, success;
			while (*tv_mode) {
				machine = machines;
				while (*machine) {
					/* args array is modified by atari800, so need to recreate it each time */
					int num_args = 0;
					while (num_args < (sizeof(default_args) / sizeof(default_args[0]))) {
						test_args[num_args] = default_args[num_args];
						num_args++;
					}
					test_args[num_args++] = *tv_mode;
					test_args[num_args++] = *machine;
					test_args[num_args++] = argv[i];
					success = run_emulator(num_args, num_frames, verbose);
					printf("%s: %s %s  Emulation ", argv[i], *tv_mode, *machine);
					if (success) printf("succeeded.\n");
					else printf("failed.\n");
					machine++;
				}
				tv_mode++;
			}
		}
	}
}
