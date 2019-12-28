  Free and portable Atari 800 Emulator for everybody, Version 4.2.0
  -----------------------------------------------------------------

    Copyright (C) 1995 David Firth. E-Mail: david@signus.demon.co.uk
    Copyright (C) 1998-2019 Atari800 Development Team.
                            http://atari800.github.io/

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

This is the emulator of Atari 8-bit computer systems and the 5200 console
for Unix, Linux, Amiga, MS-DOS, Atari TT/Falcon, MS Windows, MS WinCE,
Sega Dreamcast, Android and systems running the SDL library.
Our main objective is to create a freely distributable portable emulator
(i.e. with source code available).
It can be configured to run in the following ways :-

	1. "simple" version (many platforms) - uses only the standard C library
	2. curses (many platforms)
	3. X Window + Optional XVIEW or MOTIF User Interface
	4. CBM Amiga
 	5. MS-DOS (DJGPP)
	6. Atari Falcon/TT and compatible machines
	7. MS Windows (DirectX)
	8. SDL (running on _many_ platforms)
	9. WinCE
	10. Sega Dreamcast
	11. JVM (Java applet)
	12. Android
	13. Raspberry Pi
	14. libatari800 (platform independent) - the emulator core as a library

The "simple" version is only useful for running programs such as MAC65,
Atari BASIC etc. I have had this version running on Linux,
SunOS 4.1.3, Solaris 2.4, Silicon Graphics, VAX/VMS, CBM Amiga
(Dice C and GNU C), DOS/DJGPP and the HP-UX 9000/380.

When using curses, the emulator is similar to the "simple" version, but it
also enables full screen editing capability. Some computer don't seem to
support curses fully - SunOS 4.1.3, VAX/VMS, LINUX (but ncurses is OK).

The X Window version supports graphics and has an optional XVIEW
or MOTIF user interface. The Linux X Window version can be built with
joystick and mouse support.

The Amiga version supports graphics but currently lacks Paddle support.

The MS-DOS version supports 320x200, 320x240 and even 320x480 interlaced
graphics, sound (SoundBlaster compatible sound cards, 8bit),
keyboard, one joystick connected to game port and up to three additional
digital joysticks connected to parallel (printer) ports and mouse.

The Atari Falcon030/040 version supports 320x240 and 336x240 Falcon/TT
8-bit planes graphics modes, NOVA graphics cards, DMA sound and both
joysticks (old CX-40, Atari800 compatible - not the new paddle-like ones).

The SDL version should compile on Unix, Win32, BeOS, etc... It's optimized for
8, 16 and 32 color depth. Of course it will work fastest in 8bit. If you use it
in XFree86 - please set "Depth=8" in XF86Config to gain maximum speed.
Includes support for joystick and mouse.

All versions supporting bitmapped graphics have a User Interface implemented
on the emulator's "screen". The User Interface is enter by pressing F1 at any
time. ESC is used to return to the previous screen.

The libatari800 target is designed for embedding in other programs, depends on
no external libraries, and has no user interface. It allows the user to
calculate successive video frames of emulation, and instead of displaying
anything by itself, it provides the ability to access the raw graphics screen
and main memory that the user can process as desired.

---------------------------------------------------------------------------

Features
--------

Note: Not all features are supported on all platforms.

o Emulated machines: Atari 400, 800, 1200XL, 600XL, 800XL, 65XE, 130XE, 800XE,
  XE Game System, 5200 SuperSystem.

o Configurable 400/800 RAM size, between 8 and 48 KB.

o Optional 4K RAM between 0xc000 and 0xcfff in 400/800 mode.

o Axlon and Mosaic memory expansions for the 400/800.

o 600XL memory expansions to 32 or 48 KB.

o 130XE compatible memory expansions: 192K, 320K, 576K, 1088K.

o MapRAM memory enhancement for the XL/XE.

o Cycle-exact 6502 emulation, all unofficial instructions.

o Cycle-exact NMI interrupts, scanline-based POKEY interrupts.

o Cycle-exact ANTIC and GTIA emulation, all display modes.

o Player/Missile Graphics, exact priority control and collision detection.

o Exact POKEY shift registers (sound and random number generator).

o 8 disk drives, emulated at computer-to-drive communication
  and fast patched SIO levels.

o ATR, XFD, DCM, ATR.GZ, XFD.GZ and .PRO disk images.

o Direct loading of Atari executable files and Atari BASIC programs.

o 59 cartridge types, raw and CART format.

o Cassette recorder, raw and CAS images.

o Printer support.

o Files can be stored directly on your host computer via the H: device.

o Current emulation state can be saved in a state file.

o Sound support on Unix using "/dev/dsp".

o Stereo (two POKEYs) emulation.

o Joystick controller using numeric keypad.

o Real joystick support.

o Paddles, Atari touch tablet, Koala pad, light pen, light gun,
  ST/Amiga mouse, Atari trak-ball, joystick and Atari 5200 analog
  controller emulated using mouse.

o R-Time 8 emulation using host computer clock.

o Atari palette read from a file or calculated basing on user-defined
  parameters.

o Screen snapshots (normal and interlaced) to PCX and PNG files.

o Sound output may be written to WAV files.

o User interface on all versions supporting bitmapped graphics.

o R: device (the Atari850 serial ports) mapped to net or real serial port.

o Recording input events to a file and playing them back

o MIO and Black Box emulation

o 1400XL and 1450XLD emulation
