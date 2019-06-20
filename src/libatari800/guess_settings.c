#include <stdio.h>
#include <string.h>

#include "libatari800.h"

#define MACHINE_TYPE_800 0x01
#define MACHINE_TYPE_XL 0x02
#define MACHINE_TYPE_XE 0x04
#define MACHINE_TYPE_5200 0x08
#define MACHINE_TYPE_ALL (MACHINE_TYPE_800 | MACHINE_TYPE_XL | MACHINE_TYPE_XE | MACHINE_TYPE_5200)

#define MACHINE_OS_ATARI 0x100
#define MACHINE_OS_ALTIRRA 0x200
#define MACHINE_OS_ALL (MACHINE_OS_ATARI | MACHINE_OS_ALTIRRA)

#define MACHINE_VIDEO_NTSC 0x1000
#define MACHINE_VIDEO_PAL 0x2000
#define MACHINE_VIDEO_ALL (MACHINE_VIDEO_NTSC | MACHINE_VIDEO_PAL)

typedef struct {
	char *label;
	int type;
	int min_frames;
	char *args[20];
} machine_config_t;

machine_config_t machine_config[] = {
	{"400/800 NTSC OS/B   ", MACHINE_TYPE_800 | MACHINE_OS_ATARI | MACHINE_VIDEO_NTSC, 400, {"-atari", "-ntsc", NULL}},
	{"400/800 PAL  OS/B   ", MACHINE_TYPE_800 | MACHINE_OS_ATARI | MACHINE_VIDEO_PAL, 400, {"-atari", "-pal", NULL}},
	{"400/800 NTSC Altirra", MACHINE_TYPE_800 | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_NTSC, 400, {"-atari", "-ntsc", "-800-rev", "altirra", NULL}},
	{"400/800 PAL  Altirra", MACHINE_TYPE_800 | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_PAL, 400, {"-atari", "-pal", "-800-rev", "altirra", NULL}},
	{"64k XL  NTSC XL ROM ", MACHINE_TYPE_XL | MACHINE_OS_ATARI | MACHINE_VIDEO_NTSC, 400, {"-xl", "-ntsc", NULL}},
	{"64k XL  PAL  XL ROM ", MACHINE_TYPE_XL | MACHINE_OS_ATARI | MACHINE_VIDEO_PAL, 400, {"-xl", "-pal", NULL}},
	{"64k XL  NTSC Altirra", MACHINE_TYPE_XL | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_NTSC, 400, {"-xl", "-ntsc", "-xl-rev", "altirra", NULL}},
	{"64k XL  PAL  Altirra", MACHINE_TYPE_XL | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_PAL, 400, {"-xl", "-pal", "-xl-rev", "altirra", NULL}},
	{"128k XE NTSC XL ROM ", MACHINE_TYPE_XE  | MACHINE_OS_ATARI | MACHINE_VIDEO_NTSC, 400, {"-xe", "-ntsc", NULL}},
	{"128k XE PAL  XL ROM ", MACHINE_TYPE_XE  | MACHINE_OS_ATARI | MACHINE_VIDEO_PAL, 400, {"-xe", "-pal", NULL}},
	{"128k XE NTSC Altirra", MACHINE_TYPE_XE  | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_NTSC, 400, {"-xe", "-ntsc", "-xl-rev", "altirra", NULL}},
	{"128k XE PAL  Altirra", MACHINE_TYPE_XE  | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_PAL, 400, {"-xe", "-pal", "-xl-rev", "altirra", NULL}},
	{"5200    NTSC Atari  ", MACHINE_TYPE_5200 | MACHINE_OS_ATARI | MACHINE_VIDEO_NTSC, 400, {"-5200", "-ntsc", NULL}},
	{"5200    NTSC Altirra", MACHINE_TYPE_5200 | MACHINE_OS_ALTIRRA | MACHINE_VIDEO_NTSC, 400, {"-5200", "-ntsc", "-5200-rev", "altirra", NULL}},
	{NULL, 0, 400, {NULL}},
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
};


char memo_pad_text[] = "\x21\x34\x21\x32\x29\x00\x23\x2F\x2D\x30\x35\x34\x25\x32\x00\x0D\x00\x2D\x25\x2D\x2F\x00\x30\x21\x24"; /* ATARI COMPUTER - MEMO PAD */
char memo_pad_altirra[] = "\x21\x6C\x74\x69\x72\x72\x61\x2F\x33"; /* AltirraOS */

int check_memo_pad(emulator_state_t *state) {
	antic_state_t *antic = (antic_state_t *)&state->state[state->tags.antic];
	UBYTE *memory = (UBYTE *)&state->state[state->tags.base_ram];
	UWORD ramtop;
	UWORD gr0;

	/* The default graphics 0 display list and screen RAM are allocated by the OS as
		fixed positions below RAMTOP. The display list is $3e0 bytes below,
		and screen ram is $20 bytes above that. E.g. on a 48k machine with no
		carts inserted, RAMTOP is $c000, the display list is at $bc20, and
		screen ram is at $bc40. When a cart is inserted, RAMTOP moves down an
		equivalent amount, so 8k cart moves RAMTOP to $a000, the DL to $9c20
		and display RAM to $9c40.

		Memo pad text is on the top line of the screen with the normal 2
		character indent, so on a 48k machine with no carts inserted, it is at
		$bc42.
	 */
	ramtop = memory[0x6a] << 8;
	if (antic->dlist == ramtop - 0x3e0) {
		gr0 = ramtop - 0x3c0;
		if (memcmp(&memory[gr0 + 2], memo_pad_text, sizeof(memo_pad_text)) == 0) {
			return TRUE;
		}
		else if (memcmp(&memory[gr0 + 2], memo_pad_altirra, sizeof(memo_pad_altirra)) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

#define BAD_DLIST_MIN_FRAMES 200

int run_emulator(int num_args, int num_frames, int verbose) {
	int i;

	if (verbose > 1) {
		for (i=0; i<num_args; i++) {
			printf("%s ", test_args[i]);
		}
		printf("\n");
	}
	libatari800_init(num_args, test_args);
	if (libatari800_error_code) return 0;

	emulator_state_t state;
	input_template_t input;

	int frame = 0;
	int selftest_count = 0;
	while (frame < num_frames) {
		libatari800_next_frame(&input);
		libatari800_get_current_state(&state);
		if (state.flags.selftest_enabled) {
			selftest_count++;
			if (selftest_count > 10) {
				frame = -frame;
				libatari800_error_code = LIBATARI800_SELF_TEST;
				goto exit;
			}
		}
		switch (libatari800_error_code) {
			case 0:
			if (check_memo_pad(&state)) {
				libatari800_error_code = LIBATARI800_MEMO_PAD;
				frame = -frame;
				goto exit;
			}
			break;

			case LIBATARI800_DLIST_ERROR:
			if (frame < BAD_DLIST_MIN_FRAMES) {
				libatari800_error_code = 0;
				break;
			}

			default:
			frame = -frame;
			goto exit;
		}
		frame++;
	}
exit:
	return frame;
}

cart_types_t *get_first_cart(machine_config_t *machine, int cart_kb) {
	cart_types_t *cart_desc = NULL;

	if (cart_kb > 0) {
		if (machine->type & MACHINE_TYPE_5200) cart_desc = cart_list_5200;
		else cart_desc = cart_list_a8;
		while (cart_desc->size < cart_kb) cart_desc++;
	}
	else if (cart_kb < 0) {
		/* cart_kb is the negative cart type */
		if (machine->type & MACHINE_TYPE_5200) cart_desc = cart_list_5200;
		else cart_desc = cart_list_a8;
		while (cart_desc->size && (cart_desc->type != -cart_kb)) cart_desc++;

		/* if we go through the entire list, the cart must not work in this machine */
		if (!cart_desc->size) cart_desc = NULL;
	}
	return cart_desc;
}

/* Run the emulator using a given set of command line args. If it could be a
   cartridge, run it multiple times trying the various cart types corresponding
   to its size.
*/
int run_machine(machine_config_t *machine, char *pathname, int num_frames, int cart_kb, int verbose) {
	int success;
	int any_success = 0;
	char **machine_args;
	cart_types_t *cart_desc = get_first_cart(machine, cart_kb);
	char cart_type_string[16];

	if (cart_kb < 0 && !cart_desc) {
		/* have an exact match for a cart type, but not compatible machine */
		return 0;
	}
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
		if (cart_desc && ((cart_desc->size == cart_kb) || (cart_kb < 0))) {
			test_args[num_args++] = "-cart-type";
			sprintf(cart_type_string, "%d", cart_desc->type);
			test_args[num_args++] = cart_type_string;
			test_args[num_args++] = "-cart";
		}
		test_args[num_args++] = pathname;

		success = run_emulator(num_args, num_frames, verbose);
		if (success) any_success++;
		if (!verbose) {
			if (success > 0) {
				printf("%s: %s (", pathname, machine->label);
				machine_args = machine->args;
				while (*machine_args) {
					printf("%s", *machine_args);
					machine_args++;
					if (*machine_args) printf(" ");
				}
				if (cart_desc) {
					printf(" -cart-type %d", cart_desc->type);
				}
				printf(")\n");
			}
		}
		else {
			printf("%s: %s", pathname, machine->label);
			if (success > 0) printf(" status: OK through %d frames", success);
			else {
				printf(" status: FAIL");
				if (libatari800_error_code) {
					printf(" (%s)", libatari800_error_message());
				}
			}
			if (cart_desc) {
				printf(" (cart=%d '%s')", cart_desc->type, cart_desc->label);
			}
			printf("\n");
		}
		if (!cart_desc || (cart_kb < 0)) break;
		if (cart_desc) {
			cart_desc++;
			if (cart_desc->size != cart_kb) cart_desc = NULL;
		}
	}
	return any_success;
}

#define CHUNK_SIZE 1024
#define INVALID_FILE_SIZE 999999

/* Scan the file pointed to by pathname as a candidate for a cartridge.
   Return size in KB if a valid cart size, 0 if invalid cart size, or <0 for
   a cart type identified by the cart header.
*/
int guess_cart_kb(char *pathname, int verbose) {
	char buf[CHUNK_SIZE];
	char header[16];
	FILE *fp = fopen(pathname, "rb");
	size_t current_len, total_len = 0;
	int kb, cart_type;
	cart_types_t *cart_desc;

	if (!fp) {
		return INVALID_FILE_SIZE;
	}
	memset(header, 0, sizeof(header));
	do {
		current_len = fread(buf, 1, CHUNK_SIZE, fp);
		if (total_len == 0) {
			memcpy(header, buf, 16);
		}
		total_len += current_len;
	} while (current_len == CHUNK_SIZE);
	if (total_len == 0) {
		return INVALID_FILE_SIZE;
	}

	kb = (int)(total_len / 1024);
	int found = FALSE;
	if (total_len == kb * 1024) {
		cart_desc = cart_list_a8;
		while (cart_desc->size) {
			if (kb == cart_desc->size) {
				found = TRUE;
				break;
			}
			cart_desc++;
		}
		if (verbose > 1) printf("%s: possible cartridge; size=%dkb\n", pathname, kb);
	}
	else if (total_len == (kb * 1024) + 16) {
		/* possible .car file */
		if (header[0] == 0x43 && header[1] == 0x41 && header[2] == 0x52 && header[3] == 0x54) {
			cart_type = header[7];
			if (verbose > 1) printf("%s: cart type %d; size=%dkb\n", pathname, cart_type, kb);
			kb = -cart_type;
			found = TRUE;
		}
	}
	if (!found) {
		if (verbose > 1) printf("%s: not cartridge; %d bytes\n", pathname, (int)total_len);
		kb = 0;
	}
	return kb;
}

int main(int argc, char **argv) {
	input_template_t input;
	libatari800_clear_input_array(&input);

	/* one time init stuff */
	int verbose = 1;
	int machine_flag = MACHINE_TYPE_ALL;
	int machine_flag_encountered = FALSE;
	int os_flag = MACHINE_OS_ALL;
	int os_flag_encountered = FALSE;
	int video_flag = MACHINE_VIDEO_ALL;
	int video_flag_encountered = FALSE;
	int num_frames = 1000;

	int i;
	for (i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "-v") == 0) {
				verbose += 1;
			}
			else if (strcmp(argv[i], "-vv") == 0) {
				verbose += 2;
			}
			else if (strcmp(argv[i], "-vvv") == 0) {
				verbose += 3;
			}
			else if (strcmp(argv[i], "-s") == 0) {
				verbose = 0;
			}
			else if (strcmp(argv[i], "-800") == 0) {
				if (!machine_flag_encountered) machine_flag = 0;
				machine_flag |= MACHINE_TYPE_800;
				machine_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-xl") == 0) {
				if (!machine_flag_encountered) machine_flag = 0;
				machine_flag |= MACHINE_TYPE_XL;
				machine_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-xe") == 0) {
				if (!machine_flag_encountered) machine_flag = 0;
				machine_flag |= MACHINE_TYPE_XE;
				machine_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-5200") == 0) {
				if (!machine_flag_encountered) machine_flag = 0;
				machine_flag |= MACHINE_TYPE_5200;
				machine_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-altirra") == 0) {
				if (!os_flag_encountered) os_flag = 0;
				os_flag |= MACHINE_OS_ALTIRRA;
				os_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-atari") == 0) {
				if (!os_flag_encountered) os_flag = 0;
				os_flag |= MACHINE_OS_ATARI;
				os_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-ntsc") == 0) {
				if (!video_flag_encountered) video_flag = 0;
				video_flag |= MACHINE_VIDEO_NTSC;
				video_flag_encountered = TRUE;
			}
			else if (strcmp(argv[i], "-pal") == 0) {
				if (!video_flag_encountered) video_flag = 0;
				video_flag |= MACHINE_VIDEO_PAL;
				video_flag_encountered = TRUE;
			}
			else {
				printf("WARNING: ignoring unknown argument %s\n", argv[i]);
			}
		}
		else {
			int successful_count = 0;
			machine_config_t *machine = machine_config;
			int cart_kb = guess_cart_kb(argv[i], verbose);
			if (cart_kb == INVALID_FILE_SIZE) continue;
			while (machine->label) {
				if (machine->type & machine_flag && machine->type & os_flag && machine->type & video_flag) {
					if (verbose > 1) {
						printf("trying %s\n", machine->label);
					}
					if (run_machine(machine, argv[i], num_frames, cart_kb, verbose)) successful_count++;
				}
				else if (verbose > 1) {
					printf("skipping %s\n", machine->label);
				}
				machine++;
			}
			if (!successful_count && !verbose) printf("%s: FAIL\n", argv[i]);
		}
	}
	return 0;
}
