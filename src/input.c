#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#include "memory.h"
#include "platform.h"
#include "pokey.h"

int key_code = AKEY_NONE;
int key_break = 0;
int key_shift = 0;
int key_consol = CONSOL_NONE;

int joy_autofire[4] = {AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF, AUTOFIRE_OFF};

int mouse_mode = MOUSE_OFF;
int mouse_port = 0;

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
	for(i = 0; i < 4; i++) {
		TRIG[i] = Atari_TRIG(i);
		if ((joy_autofire[i] == AUTOFIRE_FIRE && !TRIG[i]) || (joy_autofire[i] == AUTOFIRE_CONT))
			TRIG[i] = (nframes & 2) ? 1 : 0;
	}
	if (machine_type == MACHINE_XLXE) {
		TRIG[2] = 1;
		TRIG[3] = cartA0BF_enabled;
	}
}
