#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#include "memory.h"
#include "pia.h"
#include "platform.h"
#include "pokey.h"

int key_code = AKEY_NONE;
int key_break = 0;
int key_shift = 0;
int key_consol = CONSOL_NONE;

int joy_autofire[4] = {AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF};

int mouse_mode = MOUSE_OFF;
int mouse_port = 0;
int mouse_delta_x = 0;
int mouse_delta_y = 0;
int mouse_buttons = 0;
int mouse_pen_ofs_h = 0;
int mouse_pen_ofs_v = 0;

void INPUT_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-mouse") == 0) {
			char *mode = argv[++i];
			if (strcmp(mode, "off") == 0)
				mouse_mode = MOUSE_OFF;
			else if (strcmp(mode, "pad") == 0)
				mouse_mode = MOUSE_PAD;
			else if (strcmp(mode, "touch") == 0)
				mouse_mode = MOUSE_TOUCH;
			else if (strcmp(mode, "koala") == 0)
				mouse_mode = MOUSE_KOALA;
			else if (strcmp(mode, "pen") == 0)
				mouse_mode = MOUSE_PEN;
			else if (strcmp(mode, "amiga") == 0)
				mouse_mode = MOUSE_AMIGA;
			else if (strcmp(mode, "st") == 0)
				mouse_mode = MOUSE_ST;
			else if (strcmp(mode, "joy") == 0)
				mouse_mode = MOUSE_JOY;
		}
		else if (strcmp(argv[i], "-mouseport") == 0) {
			mouse_port = argv[++i][0] - '0';
			if (mouse_port < 0 || mouse_port > 3) {
				Aprint("Invalid mouse port, using 0");
				mouse_port = 0;
			}
		}
		else {
			argv[j++] = argv[i];
		}
	}

	*argc = j;
}

void INPUT_Frame(void)
{
	int i;

	/* handle keyboard */
	static int last_key_code = AKEY_NONE;
	static int last_key_break = 0;

	if (key_break && !last_key_break) {
		IRQST &= ~0x80;
		if (IRQEN & 0x80) {
			GenerateIRQ();
		}
	}
	last_key_break = key_break;

	SKSTAT |= 0xc;
	if (key_shift)
		SKSTAT &= ~8;

	if (key_code == AKEY_NONE) {
		if (press_space) {
			key_code = AKEY_SPACE;
			press_space = 0;
		}
		else {
			last_key_code = AKEY_NONE;
		}
	}
	if (key_code != AKEY_NONE) {
		SKSTAT &= ~4;
		if ((key_code ^ last_key_code) & ~AKEY_SHFTCTRL) {
		/* ignore if only shift or control has changed its state */
			last_key_code = key_code;
			KBCODE = (UBYTE) key_code;
			IRQST &= ~0x40;
			if (IRQEN & 0x40) {
				GenerateIRQ();
			}
		}
	}

	/* handle joysticks */
	PORT_input[0] = Atari_PORT(0);
	PORT_input[1] = Atari_PORT(1);
	for (i = 0; i < 4; i++) {
		TRIG[i] = Atari_TRIG(i);
		if ((joy_autofire[i] == AUTOFIRE_FIRE && !TRIG[i]) || (joy_autofire[i] == AUTOFIRE_CONT))
			TRIG[i] = (nframes & 2) ? 1 : 0;
	}

	/* handle paddles */
	for (i = 0; i < 8; i++)
		POT_input[i] = Atari_POT(i);

	if (machine_type == MACHINE_XLXE) {
		TRIG[2] = 1;
		TRIG[3] = cartA0BF_enabled;
	}
}
