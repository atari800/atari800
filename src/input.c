#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "input.h"
#include "pokey.h"

int key_code = 0xff;
int key_break = 0;

void INPUT_Frame(void)
{
	static int last_key = AKEY_NONE;

	if (key_break) {
		IRQST &= ~0x80;
		if (IRQEN & 0x80) {
			GenerateIRQ();
		}
	}

	if (key_code == AKEY_NONE) {
		if (press_space) {
			key_code = AKEY_SPACE;
			press_space = 0;
		}
		else {
			SKSTAT |= 4;
			last_key = AKEY_NONE;
		}
	}
	if (key_code != AKEY_NONE) {
		SKSTAT &= ~4;
		if (key_code != last_key) {
			last_key = key_code;
			KBCODE = key_code;
			IRQST &= ~0x40;
			if (IRQEN & 0x40) {
				GenerateIRQ();
			}
		}
	}
}
