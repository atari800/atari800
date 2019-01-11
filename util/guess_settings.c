#include <stdio.h>
#include <string.h>

#include "libatari800.h"

typedef struct {
	char *label;
	int is_5200;
	int min_frames;
	char *args[20];
} machine_config_t;

machine_config_t machine_config[] = {
	"400/800 NTSC OS/B   ", FALSE, 400, {"-atari", "-ntsc", NULL},
	"400/800 PAL  OS/B   ", FALSE, 400, {"-atari", "-pal", NULL},
	"400/800 NTSC Altirra", FALSE, 400, {"-atari", "-ntsc", "-800-rev", "altirra", NULL},
	"400/800 PAL  Altirra", FALSE, 400, {"-atari", "-pal", "-800-rev", "altirra", NULL},
	"64k XL  NTSC XL ROM ", FALSE, 400, {"-xl", "-ntsc", NULL},
	"64k XL  PAL  XL ROM ", FALSE, 400, {"-xl", "-pal", NULL},
	"64k XL  NTSC Altirra", FALSE, 400, {"-xl", "-ntsc", "-xl-rev", "altirra", NULL},
	"64k XL  PAL  Altirra", FALSE, 400, {"-xl", "-pal", "-xl-rev", "altirra", NULL},
	"128k XE NTSC XL ROM ", FALSE, 400, {"-xe", "-ntsc", NULL},
	"128k XE PAL  XL ROM ", FALSE, 400, {"-xe", "-pal", NULL},
	"128k XE NTSC Altirra", FALSE, 400, {"-xe", "-ntsc", "-xl-rev", "altirra", NULL},
	"128k XE PAL  Altirra", FALSE, 400, {"-xe", "-pal", "-xl-rev", "altirra", NULL},
	"5200    NTSC Atari  ", TRUE, 400, {"-5200", "-ntsc", NULL},
	"5200    NTSC Altirra", TRUE, 6000, {"-5200", "-ntsc", "-5200-rev", "altirra", NULL},
	NULL, FALSE, 400, {NULL},
};

typedef struct {
	int size;
	int type;
	char *label;
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
	{0, 0, NULL},
};

cart_types_t cart_list_5200[] = {
	{4, 20, "Standard 4 KB 5200",},
	{8, 19, "Standard 8 KB 5200",},
	{16, 16, "One chip 16 KB 5200",},
	{16, 6,  "Two chip 16 KB 5200",},
	{32, 4,  "Standard 32 KB 5200",},
	{40, 7,  "Bounty Bob 40 KB 5200",},
	{0, 0, NULL},
};

char *test_args[32];
char *default_args[] = {
	"atari800",
	"-nobasic",
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
	while (frame < num_frames) {
		// libatari800_get_current_state(state, &tags);
		if (!libatari800_next_frame(&input)) {
			printf("Crash encountered at frame %d\n", frame);
			frame = -frame;
			break;
		}
		frame++;
	}

	return frame;
}

cart_types_t *get_first_cart(machine_config_t *machine, int cart_kb) {
	cart_types_t *cart_desc = NULL;

	if (cart_kb > 0) {
		if (machine->is_5200) cart_desc = cart_list_5200;
		else cart_desc = cart_list_a8;

		while (cart_desc->size < cart_kb) cart_desc++;
	}
	return cart_desc;
}

char cart_type_string[16];

void run_machine(machine_config_t *machine, char *pathname, int num_frames, int cart_kb, int verbose) {
	int success;
	char **machine_args;
	cart_types_t *cart_desc = get_first_cart(machine, cart_kb);

	num_frames = num_frames < machine->min_frames ? machine->min_frames : num_frames;
	while (1) {
		/* args array is modified by atari800, so need to recreate it each time */
		int num_args = 0;
		while (num_args < (sizeof(default_args) / sizeof(default_args[0]))) {
			test_args[num_args] = default_args[num_args];
			// printf("default arg %d: %s\n", num_args, default_args[num_args]);
			num_args++;
		}
		machine_args = machine->args;
		while (*machine_args) {
			// printf("arg %d: %s\n", num_args, *machine_args);
			test_args[num_args++] = *machine_args++;
		}
		if (cart_desc && cart_desc->size == cart_kb) {
			test_args[num_args++] = "-cart-type";
			sprintf(cart_type_string, "%d", cart_desc->type);
			test_args[num_args++] = cart_type_string;
			test_args[num_args++] = "-cart";
		}
		test_args[num_args++] = pathname;

		success = run_emulator(num_args, num_frames, verbose);
		printf("%s: %s", pathname, machine->label);
		if (success > 0) printf(" boot: OK through %d frames", success);
		else {
			printf(" boot: FAIL");
			if (libatari800_unidentified_cart_type) {
				printf(" (unidentified cartridge)");
			}
		}
		if (cart_desc) {
			printf(" (cart=%d '%s')", cart_desc->type, cart_desc->label);
		}
		printf("\n");
		if (!cart_desc) break;
		if (cart_desc) {
			cart_desc++;
			if (cart_desc->size != cart_kb) cart_desc = NULL;
		}
	}
}

#define CHUNK_SIZE 1024

int guess_cart_kb(char *pathname) {
	char buf[CHUNK_SIZE];
	FILE *fp = fopen(pathname, "rb");
	size_t current_len, total_len = 0;
	int kb;
	cart_types_t *cart_desc;

	do {
		current_len = fread(buf, 1, CHUNK_SIZE, fp);
		total_len += current_len;
	} while (current_len == CHUNK_SIZE);

	kb = (int)(total_len / 2048) * 2048; /* 2k smallest cart */
	int found = FALSE;
	if (total_len == kb) {
		kb /= 1024;
		cart_desc = cart_list_a8;
		while (cart_desc->size) {
			if (kb == cart_desc->size) {
				found = TRUE;
				break;
			}
			cart_desc++;
		}
		printf("%s: possible cartridge; size=%dkb\n", pathname, kb);
	}
	if (!found) {
		printf("%s: not cartridge; %d bytes\n", pathname, (int)total_len);
		kb = -1;
	}
	return kb;
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
			int machine_index, success;
			machine_config_t *machine = machine_config;
			int cart_kb = guess_cart_kb(argv[i]);
			while (machine->label) {
				run_machine(machine, argv[i], num_frames, cart_kb, verbose);
				machine++;
			}
		}
	}
}
