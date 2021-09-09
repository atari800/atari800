/*
 * libatari800/api.c - Atari800 as a library - application programming interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2021 Rob McMullen
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Atari800 includes */
#include "atari.h"
#include "akey.h"
#include "afile.h"
#include "../input.h"
#include "log.h"
#include "antic.h"
#include "cpu.h"
#include "platform.h"
#include "memory.h"
#include "screen.h"
#include "sio.h"
#include "../sound.h"
#include "util.h"
#include "libatari800/main.h"
#include "libatari800/cpu_crash.h"
#include "libatari800/init.h"
#include "libatari800/input.h"
#include "libatari800/video.h"
#include "libatari800/sound.h"
#include "libatari800/statesav.h"


#ifdef HAVE_SETJMP
jmp_buf libatari800_cpu_crash;
#endif

/* global variable indicating that BRK instruction should exit emulation */
int libatari800_continue_on_brk = 0;

/* global variable indicating last error code */
int libatari800_error_code;


/** Initialize emulator configuration
 * 
 * Sets emulator configuration using the supplied argument list. The arguments
 * correspond to command line arguments for the `atari800` program, see its manual
 * page for more information on arguments and their functions.
 * 
 * @param argc number of arguments in @a argv, or -1 if \a argv contains a NULL
 * terminated list.
 * 
 * @param argv list of arguments.
 * 
 * @retval FALSE if error in argument list
 * @retval TRUE if successful
 */
int libatari800_init(int argc, char **argv) {
	int i;
	int status;
	int argv_alloced = FALSE;
	char **argv_ptr = NULL;

	/* There are two ways to specify arguments.

	   If argc is negative, argv must be specified in the form of a NULL
	   terminated list of arguments.

	   If argc is positive, argv must contain the number of elements specified
	   by argc.

	   It is not necessory to specify the argv[0] entry as NULL or atari800. If
	   it is not there, however, this routine inserts a dummy zeroth argv entry
	   to satisfy the requirements of Atari800_Initialise.
	   */
	if (argc < 0) {
		for (i = 0;; i++) {
			if (!argv[i]) break;
		}
		argc = i;
	}
	if ((argc == 0) || ((argc > 0) && argv[0] && (!Util_striendswith(argv[0], "atari800")))) {
		argv_alloced = argc + 1;
		argv_ptr = (Util_malloc(sizeof(char *) * argv_alloced));
		argv_ptr[0] = NULL;
		for (i = 0; i < argc; i++) {
			argv_ptr[i + 1] = argv[i];
		}
		argc++;
	}
	else {
		argv_ptr = argv;
	}

	CPU_cim_encountered = 0;
	libatari800_error_code = 0;
	Atari800_nframes = 0;
	MEMORY_selftest_enabled = 0;
	status = Atari800_Initialise(&argc, argv_ptr);
	if (status) {
		Log_flushlog();
	}
	if (argv_alloced) {
		free(argv_ptr);
	}
	return status;
}

char *error_messages[] = {
	"no error",
	"unidentified cartridge",
	"CPU crash",
	"BRK instruction",
	"invalid display list",
	"self test",
	"memo pad",
	"invalid escape opcode"
};
char *unknown_error = "unknown error";


/** Get text description of latest error message
 * 
 * If the \a libatari800_next_frame return value indicates an error condition,
 * this function will return an error message suitable for display to the user.
 * 
 * @returns text description of error
 */
const char *libatari800_error_message() {
	if ((libatari800_error_code < 0) || (libatari800_error_code > (sizeof(error_messages)))) {
		return unknown_error;
	}
	return error_messages[libatari800_error_code];
}


/** Set whether encountering a BRK instruction exits emulation.
 * 
 * Choose what happens when a BRK instruction is encountered. Most often this will lead
 * to the program inside the emulator failing and forcing the emulator into a lockup
 * condition. But some programs trap the BRK instruction to implement extra functionality
 * like a debugger.
 * 
 * @param cont if True, the emulation will continue without notification when a BRK is
 * encountered. If False, emulation immediately exits with the error code
 * LIBATARI800_BRK_INSTRUCTION
 */
void libatari800_continue_emulation_on_brk(int cont) {
	libatari800_continue_on_brk = cont;
}


/** Clears input array structure
 * 
 * Clears any user input (keystrokes, joysticks, paddles) in the input array
 * structure to indicate that no user input is taking place.
 * 
 * This should be called to initialize the input array before the first call
 * to \a libatari800_next_frame.
 * 
 * @param input pointer to input array structure
 */
void libatari800_clear_input_array(input_template_t *input)
{
	/* Initialize input and output arrays to zero */
	memset(input, 0, sizeof(input_template_t));
	INPUT_key_code = AKEY_NONE;
}


/** Perform one video frame's worth of emulation
 * 
 * This is the main driver for libatari800. This function runs the emulator for enough
 * CPU cycles to produce one video frame's worth of emulation. Results of the frame
 * can be retrieved using the \a libatari800_get_* functions.
 * 
 * @param input input template structure defining the user input for the frame
 * 
 * @retval 0 successfully emulated frame
 * @retval 1 unidentified cartridge type
 * @retval 2 CPU crash
 * @retval 3 BRK instruction encountered
 * @retval 4 display list error
 * @retval 5 entered self-test mode
 * @retval 6 entered Memo Pad
 * @retval 7 encountered invalid escape opcode
 */
int libatari800_next_frame(input_template_t *input)
{
	LIBATARI800_Input_array = input;
	INPUT_key_code = PLATFORM_Keyboard();
	LIBATARI800_Mouse();
#ifdef HAVE_SETJMP
	if ((libatari800_error_code = setjmp(libatari800_cpu_crash))) {
		/* called from within CPU_GO to indicate crash */
		Log_print("libatari800_next_frame: notified of CPU crash: %d\n", CPU_cim_encountered);
	}
	else
#endif /* HAVE_SETJMP */
	{
		/* normal operation */
		LIBATARI800_Frame();
		if (CPU_cim_encountered) {
			libatari800_error_code = LIBATARI800_CPU_CRASH;
		}
		else if (ANTIC_dlist == 0) {
			libatari800_error_code = LIBATARI800_DLIST_ERROR;
		}
	}
	PLATFORM_DisplayScreen();
	return !libatari800_error_code;
}


/** Use disk image in a disk drive
 * 
 * Insert a virtual floppy image into one of the emulated disk drives. Currently
 * supported formats include ATR, ATX, DCM, PRO, XFD.
 * 
 * @param diskno disk drive number (1 - 8)
 * @param filename path to disk image
 * @param readonly if \a TRUE will be mounted as read-only
 * 
 * @retval FALSE if error
 * @retval TRUE if successful
 */
int libatari800_mount_disk_image(int diskno, const char *filename, int readonly)
{
	return SIO_Mount(diskno, filename, readonly);
}


/** Restart emulation using file
 * 
 * Perform a cold start with a disk image, executable file, cartridge, cassette image,
 * BASIC file, or atari800 state save file.
 * 
 * This is currently the only way to load and have the emulator run an executable
 * file or BASIC program without a boot disk image. The atari800 emulator
 * includes a built-in bootloader for these files but is only available at machine
 * start.
 * 
 * @param path to file
 * 
 * @returns file type, or 0 for error
 */
int libatari800_reboot_with_file(const char *filename)
{
	int file_type;

	file_type = AFILE_OpenFile(filename, FALSE, 1, FALSE);
	if (file_type != AFILE_ERROR) {
		Atari800_Coldstart();
	}
	return file_type;
}


/** Return pointer to main memory
 *
 * This is actual array containing the emulator's main bank of 64k of RAM.
 * Changes made here will be reflected in the state of the emulator, potentially
 * causing the emulated CPU to halt if it encounters an illegal instruction.
 *
 * Accessing memory through this pointer will not return hardware register
 * information, this provides access to the RAM only.
 *
 * @returns pointer to the beginning of the 64k block of main memory
 */
UBYTE *libatari800_get_main_memory_ptr()
{
	return MEMORY_mem;
}


/** Return pointer to screen data
 *
 * The emulated screen is an array of 384x240 bytes in scan line order with the
 * top of the screen at the beginning of the array. Each byte represents the
 * color index of a pixel in the usual Atari palette (high 4 bits are the hue
 * and low 4 bits are the luminance).
 *
 * The large size of the screen includes the overscan areas not usually
 * displayed. Typical margins will be 24 on each the right and left, and 16 each
 * on the top and bottom, leaving an 8 pixel margin on all sides for the normal
 * 320x192 addressable pixels.
 * 
 * Note that the screen is output only, and changes to this array will have no
 * effect on the emulation.
 *
 * @returns pointer to the beginning of the 92160 bytes of data holding the
 * emulated screen.
 */
UBYTE *libatari800_get_screen_ptr()
{
	return (UBYTE *)Screen_atari;
}


/** Return pointer to sound data
 *
 * If sound is used, each emulated frame will fill the sound buffer with samples
 * at the configured audio sample rate.
 *
 * Because the emulation runs at a non-integer frame rate (approximately 59.923
 * frames per second in NTSC, 49.861 fps in PAL), the number of samples is not a
 * constant for all frames -- it can vary by one sample per frame. For example,
 * in NTSC with a sample rate of 44.1KHz, most frames will contain 736 samples,
 * but one out of about every 19 frames will contain 735 samples.
 *
 * Use the function \a libatari800_get_sound_buffer_len to determine usable size
 * of the sound buffer.
 *
 * @returns pointer to the beginning of the sound sample buffer
 */
UBYTE *libatari800_get_sound_buffer()
{
	return (UBYTE *)LIBATARI800_Sound_array;
}


/** Return the usable size of the sound buffer.
 *
 * @returns number of bytes of valid data in the sound buffer
 */
int libatari800_get_sound_buffer_len() {
	return (int)sound_array_fill;
}


/** Return the maximum size of the sound buffer.
 *
 * @returns number of bytes allocated in sound buffer
 */

int libatari800_get_sound_buffer_allocated_size() {
	return (int)sound_hw_buffer_size;
}


/** Return the audio sample rate in samples per second
 *
 * @returns the audio sample rate, typically 44100 or 48000
 */
int libatari800_get_sound_frequency() {
	return (int)Sound_out.freq;
}


/** Return the number of audio channels
 *
 * @retval 1 mono
 * @retval 2 stereo
 */
int libatari800_get_num_sound_channels() {
	return (int)Sound_out.channels;
}


/** Return the sample size in bytes of each audio sample
 *
 * @retval 1 8-bit audio
 * @retval 2 16-bit audio
 */
int libatari800_get_sound_sample_size() {
	return Sound_out.sample_size;
}


/** Return the video frame rate
 *
 * It is important to note that libatari800 can run as fast as the host computer will
 * allow, but simulates operation as if it were running at NTSC or PAL frame rates. It
 * is up to the calling program to display frames at the correct rate.
 * 
 * The NTSC frame rate is 59.9227434 frames per second, and PAL is 49.8607597 fps.
 * 
 * @returns floating point number representing the frame rate.
 */
float libatari800_get_fps() {
	return Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
}


/** Return the number of frames of emulation in the current state of the
 * emulator
 *
 * This is also equivalent to the number of times \a libatari800_next_frame has
 * been called since the initialization of libatarii800.
 *
 * Calls to \a libatari800_init, cold starts, and warm starts do not reset this
 * number; it will continue to grow after these events. This value can get
 * reset, however, if the emulator state is restored from a previously saved
 * state. In this case, the number of frames will be restored to the value from
 * the saved state.
 *
 * @returns number of frames that have been generated, or zero if no \a
 * libatari800_next_frame has not been called yet.
 */
int libatari800_get_frame_number() {
	return Atari800_nframes;
}


/** Save the state of the emulator
 *
 * Save the state of the emulator into a data structure that can later be used
 * to restore the emulator to this state using a call to \a
 * libatari800_restore_state.
 *
 * The emulator state structure can be examined by using the structure member
 * tags as an offset to locate the subsystem of interest, and casting the
 * resulting offset into its own struct. E.g. to find the value of the CPU
 * registers and the current program counter, this code:
 * 
 * \code{c}
 * emulator_state_t state;
 * cpu_state_t *cpu;
 * pc_state_t *pc;
 * 
 * libatari800_get_current_state(&state);
 * cpu = (cpu_state_t *)&state.state[state.tags.cpu];
 * pc = (pc_state_t *)&state.state[state.tags.pc];
 * printf("CPU A=%02x X=%02x Y=%02x PC=%04x\\n", cpu->A, cpu->X, cpu->Y, pc->PC);
 * \endcode
 * 
 * gets the current state of the emulator, locates the \a cpu_state_t structure
 * and the \a pc_state_t structure within it, and prints the values of interest.
 *
 * @param state pointer to an already allocated \a emulator_state_t structure
 */
void libatari800_get_current_state(emulator_state_t *state)
{
	LIBATARI800_StateSave(state->state, &state->tags);
	state->flags.selftest_enabled = MEMORY_selftest_enabled;
	state->flags.nframes = (ULONG)Atari800_nframes;
	state->flags.sample_residual = (ULONG)(0xffffffff * sample_residual);
}


/** Restore the state of the emulator
 *
 * Return the emulator to a previous state as defined by a previous call to
 * \a libatari800_get_current_state.
 * 
 * Minimal error checking is performed on the data in \a state, so if the
 * data in \a state has been altered it is possible that the emulator will
 * be returned to an invalid state and further emulation will fail.
 *
 * @param state pointer to an already allocated \a emulator_state_t structure
 */
void libatari800_restore_state(emulator_state_t *state)
{
	LIBATARI800_StateLoad(state->state);
	MEMORY_selftest_enabled = state->flags.selftest_enabled;
	Atari800_nframes = state->flags.nframes;
	sample_residual = (double)state->flags.sample_residual / (double)0xffffffff;
}


/** Free resources used by the emulator.
 *
 * Release any memory or other resources used by the emulator. Further calls to
 * \a libatari800_* functions are not permitted after a call to this function,
 * and attempting to do so will have undefined behavior and likely crash the
 * program.
 */
void libatari800_exit() {
	Atari800_Exit(0);
}

/*
vim:ts=4:sw=4:
*/
