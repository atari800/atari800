#include "antic.h"
#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#include "log.h"
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
int mouse_speed = 3;
int mouse_pen_ofs_h = 0;
int mouse_pen_ofs_v = 0;

#ifndef MOUSE_SHIFT
#define MOUSE_SHIFT 4
#endif
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_pen_show_pointer = 0;

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

	INPUT_CenterMousePointer();
	*argc = j;
}

void INPUT_Frame(void)
{
	int i;
	static int last_key_code = AKEY_NONE;
	static int last_key_break = 0;
	UBYTE STICK[4];
	static int last_mouse_buttons = 0;

	/* handle keyboard */

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
	i = Atari_PORT(0);
	STICK[0] = i & 0x0f;
	STICK[1] = (i >> 4) & 0x0f;
	i = Atari_PORT(1);
	STICK[2] = i & 0x0f;
	STICK[3] = (i >> 4) & 0x0f;

	for (i = 0; i < 4; i++) {
		TRIG[i] = Atari_TRIG(i);
		if ((joy_autofire[i] == AUTOFIRE_FIRE && !TRIG[i]) || (joy_autofire[i] == AUTOFIRE_CONT))
			TRIG[i] = (nframes & 2) ? 1 : 0;
	}

	/* handle paddles */
	for (i = 0; i < 8; i++)
		POT_input[i] = Atari_POT(i);

	/* handle mouse */
	switch (mouse_mode) {
	case MOUSE_PAD:
	case MOUSE_TOUCH:
	case MOUSE_KOALA:
		if (mouse_mode != MOUSE_PAD || machine_type == MACHINE_5200)
			mouse_x += mouse_delta_x * mouse_speed;
		else
			mouse_x -= mouse_delta_x * mouse_speed;
		if (mouse_x < 0)
			mouse_x = 0;
		else if (mouse_x > 228 << MOUSE_SHIFT)
			mouse_x = 228 << MOUSE_SHIFT;
		if (mouse_mode == MOUSE_KOALA || machine_type == MACHINE_5200)
			mouse_y += mouse_delta_y * mouse_speed;
		else
			mouse_y -= mouse_delta_y * mouse_speed;
		if (mouse_y < 0)
			mouse_y = 0;
		else if (mouse_y > 228 << MOUSE_SHIFT)
			mouse_y = 228 << MOUSE_SHIFT;
		POT_input[mouse_port << 1] = mouse_x >> MOUSE_SHIFT;
		POT_input[(mouse_port << 1) + 1] = mouse_y >> MOUSE_SHIFT;
		if (machine_type == MACHINE_5200) {
			if (mouse_buttons & 1)
				TRIG[mouse_port] = 0;
			if (mouse_buttons & 2)
				SKSTAT &= ~8;
		}
		else
			STICK[mouse_port] &= ~(mouse_buttons << 2);
		break;
	case MOUSE_PEN:
		mouse_x += mouse_delta_x * mouse_speed;
		if (mouse_x < 0)
			mouse_x = 0;
		else if (mouse_x > 167 << MOUSE_SHIFT)
			mouse_x = 167 << MOUSE_SHIFT;
		mouse_y += mouse_delta_y * mouse_speed;
		if (mouse_y < 0)
			mouse_y = 0;
		else if (mouse_y > 119 << MOUSE_SHIFT)
			mouse_y = 119 << MOUSE_SHIFT;
		PENH_input = 44 + mouse_pen_ofs_h + (mouse_x >> MOUSE_SHIFT);
		PENV_input = 4 + mouse_pen_ofs_v + (mouse_y >> MOUSE_SHIFT);
		if (mouse_buttons & 1)
			STICK[mouse_port] &= ~1;
		if ((mouse_buttons & 2) && !(last_mouse_buttons & 2))
			mouse_pen_show_pointer = !mouse_pen_show_pointer;
		break;
	case MOUSE_AMIGA:
	case MOUSE_ST:
	case MOUSE_JOY:
		if (mouse_buttons & 1)
			TRIG[mouse_port] = 0;
		if (machine_type == MACHINE_5200 && mouse_buttons & 2)
			SKSTAT &= ~8;
		break;
	default:
		break;
	}
	last_mouse_buttons = mouse_buttons;

	if (machine_type == MACHINE_XLXE) {
		TRIG[2] = 1;
		TRIG[3] = cartA0BF_enabled;
	}

	PORT_input[0] = (STICK[1] << 4) | STICK[0];
	PORT_input[1] = (STICK[3] << 4) | STICK[2];
}

void INPUT_CenterMousePointer(void)
{
	switch (mouse_mode) {
	case MOUSE_PAD:
	case MOUSE_TOUCH:
	case MOUSE_KOALA:
		mouse_x = 114 << MOUSE_SHIFT;
		mouse_y = 114 << MOUSE_SHIFT;
		break;
	case MOUSE_PEN:
		mouse_x = 84 << MOUSE_SHIFT;
		mouse_y = 60 << MOUSE_SHIFT;
		break;
	}
}

/* draw light pen cursor */
void INPUT_DrawMousePointer(void)
{
	if (mouse_mode == MOUSE_PEN && mouse_pen_show_pointer
	 && mouse_x >= 0 && (mouse_x >> MOUSE_SHIFT) < 168
	 && mouse_y >= 0 && (mouse_y >> MOUSE_SHIFT) < 120) {
		UWORD *ptr = & ((UWORD *) atari_screen)[12 + (mouse_x >> MOUSE_SHIFT)
					 + ATARI_WIDTH * (mouse_y >> MOUSE_SHIFT)];
		*ptr ^= 0xffff;
		ptr[ATARI_WIDTH / 2] ^= 0xffff;
	}
}
