@BOTTOM@

#undef BASIC

#undef CURSES

#undef SVGALIB

#undef NCURSES

#ifndef WIN32
# undef WIN32
#endif

#undef DIRECTX

#undef X11

#undef XVIEW

#undef MOTIF

#undef SHM

/* dos vga support */
#undef VGA

/* define to enable dos style drives support */
#undef DOS_DRIVES

/* define to use back slash */
#undef BACK_SLASH

/* define if unaligned long access is ok */
#undef UNALIGNED_LONG_OK

/* Is your computer very slow (disables generating screen completely) */
#undef VERY_SLOW

/*  Show crash menu after CIM instruction */
#undef CRASH_MENU

/* Enable BREAK command in monitor (slows CPU a little bit) */
#undef MONITOR_BREAK

/* Enable hints in disassembler (addresses get human readable labels) */
#undef MONITOR_HINTS

/* Enable assembler in monitor (you can enter ASM insns directly) */
#undef MONITOR_ASSEMBLER

/* Compile internal palettes */
#undef COMPILED_PALETTE

/* Enable Snailmeter (shows how much is the emulator slower than original) */
#undef SNAILMETER

/* Draw every 1/50 sec only 1/refresh of screen */
#undef SVGA_SPEEDUP

/* Use cursor keys/Ctrl for keyboard joystick */
#undef USE_CURSORBLOCK

/* Support for Toshiba Joystick Mouse (Linux SVGALIB Only) */
#undef JOYMOUSE

/* Run atari800 as Linux realtime process */
#undef REALTIME

/* Enable LINUX Joystick */
#undef LINUX_JOYSTICK

/* define to enable sound */
#undef SOUND

/* define to disable volume only sound (digitized sound effects)) */
#undef NO_VOL_ONLY

/* define to disable console sound (keyboard clicks) */
#undef NO_CONSOL_SOUND

/* define to enable serial in/out sound */
#undef SERIO_SOUND

/* define to disable sound interpolation */
#undef NOSNDINTER

/* define to enable sound clipping */
#undef CLIP

/* define to enable stereo sound */
#undef STEREO

/* Buffer debug output (until the graphics mode switches back to text mode) */
#undef BUFFERED_LOG

/* Emulate disk LED diode */
#undef SET_LED

/* Display LED on screen */
#undef NO_LED_ON_SCREEN

/* color change inside a scanline */
#undef NO_CYCLE_EXACT
