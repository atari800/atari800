dnl   configure.ac - Autoconf script for building configure
dnl
dnl Copyright (C) 2001 Krzysztof Nikiel
dnl   Copyright (C) 2001-2003 Atari800 development team (see DOC/CREDITS)
dnl
dnl   This file is part of the Atari800 emulator project which emulates
dnl   the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
dnl
dnl   Atari800 is free software; you can redistribute it and/or modify
dnl   it under the terms of the GNU General Public License as published by
dnl   the Free Software Foundation; either version 2 of the License, or
dnl   (at your option) any later version.
dnl
dnl   Atari800 is distributed in the hope that it will be useful,
dnl   but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl   GNU General Public License for more details.
dnl  
dnl   You should have received a copy of the GNU General Public License
dnl   along with Atari800; if not, write to the Free Software
dnl   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

AC_INIT
AC_CONFIG_HEADER(config.h)

a800_target=none
case "$target" in
  basic | curses | ncurses | dosvga | falcon | sdl | svgalib | windx )
        a800_target="$target"
        ;;
  x11 | x11-motif | x11-shm | x11-xview | x11-xview-shm )
        a800_target="$target"
        ;;
  shm | motif | xview | xview-shm )
        a800_target=x11-"$target"
        ;;
esac

if test "$a800_target" = none; then
        echo
        echo "Usage: configure --target=<target>"
        echo
        echo "The '--target' option is required.  Use '--help' to see other available"
        echo "options, including Atari800 specific '--enable' options.  Possible values"
        echo "for the '--target' option are:"
        echo "    basic"
        echo "    (n)curses"
        echo "    dosvga           (DOS only)"
        echo "    falcon           (Atari Falcon systems only)"
        echo "    svgalib          (Linux system with SVGALib library)"
        echo "    sdl              (System with Simple Direct Media Layer library)"
        echo "    windx            (Windows with DirectX only)"
        echo "    x11              (Standard X11)"
        echo "    (x11-)motif      (Motif on X11)"
        echo "    (x11-)shm        (Standard X11 with shared memory extensions)"
        echo "    (x11-)xview      (XView on X11)"
        echo "    (x11-)xview-shm  (XView on X11, with shared memory extensions)"
        echo
        exit 1;
fi

if test "${LDFLAGS+set}" != set; then
LDFLAGS=-s
fi

if test "${CFLAGS+set}" != set; then
CFLAGS='-O2 -Wall'
fi

a800_host=unknown
AC_CANONICAL_HOST
case $host_os in
  cygwin )
        a800_host=win
        if test "$a800_target" = dx; then
                CC="gcc -mno-cygwin"
                ac_cv_prog_CC=$CC
                echo "using CC=$CC to disable cygwin library";
        fi
        ;;
  mingw32 )
        a800_host=win
        ;;
  msdosdjgpp )
        a800_host=dos
        ;;
  linux | linux-gnu )
        a800_host=linux
        ;;
  mint )
        a800_host=falcon
        ;;
esac

if test "$a800_host" = unknown; then
        echo " "
        echo "WARNING:  Unrecognized host operating system!"
        echo "    Please report this to the atari800 maintainer."
        echo " "
else
        echo "$a800_host environment found."
fi

case "$a800_host" in
  dos)
        AC_DEFINE(DOS_DRIVES,, [define to enable dos style drives support])
        AC_DEFINE(BACK_SLASH,, [define to use back slash])
        AC_DEFINE(DEFAULT_CFG_NAME, "atari800.cfg", [alternate config filename due to 8+3 fs limit])
        AC_DEFINE(SYSTEM_WIDE_CFG_FILE, "c:\\atari800.cfg", [alternate system-wide config file for non-Unix OS])
        ;;
  win)
        AC_DEFINE(BACK_SLASH,, [define to use back slash])
        AC_DEFINE(DOS_DRIVES,, [define to enable dos style drives support])
        ;;
  falcon)
        AC_DEFINE(DOS_DRIVES,, [define to enable dos style drives support])
        AC_DEFINE(BACK_SLASH,, [define to use back slash])
        AC_DEFINE(USE_CLOCK,, [define to use clock() instead of gettimeofday()])
        AC_DEFINE(DEFAULT_CFG_NAME, "atari800.cfg", [alternate config filename due to 8+3 fs limit])
        AC_DEFINE(SYSTEM_WIDE_CFG_FILE, "c:\\atari800.cfg", [alternate system-wide config file for non-Unix OS])
        ;;
esac

dnl Checks for programs.
dnl AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl Checks for libraries.
AC_DEFUN(NEED_LIB,[AC_CHECK_LIB($1, main,,echo $1 library not found; exit 1)])
AC_CHECK_LIB(z, gzopen)

if test "$a800_host" != win; then
        dnl Replace `main' with a function in -lm:
        AC_CHECK_LIB(m, main)
fi

OBJ=

if test "$a800_host" = win; then
        NEED_LIB(gdi32, main)
        NEED_LIB(winmm, main)
        OBJ=win32/atari.ro
fi

dnl set OBJ depending on target
case "$a800_target" in
  basic)
        AC_DEFINE(BASIC,, [Text mode only])
        OBJ=atari_basic.o
        ;;
  dosvga)
        if test "$a800_host" != dos; then
                echo " "
                echo "ERROR:  The dosvga target is only valid on a DOS system!"
                echo " "
                exit 1
        fi
        AC_DEFINE(DOSVGA,, [DOS vga])
        OBJ="dos/atari_vga.o dos/vga_gfx.o dos/vga_asm.o"
        ;;
  curses)
        AC_DEFINE(CURSES,, [System with Curses library])
        dnl Replace `main' with a function in -lcurses:
        NEED_LIB(curses, main)
        OBJ=atari_curses.o
        ;;
  falcon)
        AC_DEFINE(FALCON,, [Atari Falcon system])
        LIBS="$LIBS -lgem"
        OBJ="$OBJ atari_falcon.o falcon/c2p_uni.o falcon/c2p_unid.o \
        falcon/videl.o falcon/ikbd.o"
        ;;
  ncurses)
        AC_DEFINE(NCURSES,, [System with Ncurses library])
        dnl Replace `main' with a function in -lncurses:
        NEED_LIB(ncurses, main)
        OBJ=atari_curses.o
        ;;
  sdl)
        AC_DEFINE(SDL,, [System with SDL library])
        CFLAGS="$CFLAGS `sdl-config --cflags`"
        LIBS="$LIBS `sdl-config --libs`"
        OBJ="$OBJ atari_sdl.o"
        ;;
  svgalib)
        AC_DEFINE(SVGALIB,, [Linux with SVGALib])
        dnl Replace `main' with a function in -lvga:
        NEED_LIB(vga, main)
        OBJ="atari_svgalib.o"
        ;;
  windx)
        AC_DEFINE(DIRECTX,, [Windows with DirectX])
        dnl Replace `main' with a function in -lddraw:
        NEED_LIB(ddraw, main)
        dnl Replace `main' with a function in -ldinput:
        NEED_LIB(dinput, main)
        dnl Replace `main' with a function in -ldsound:
        NEED_LIB(dsound, main)
        dnl Replace `main' with a function in -ldxguid:
        NEED_LIB(dxguid, main)
        OBJ="win32/atari_win32.o win32/screen.o win32/main.o \
        win32/keyboard.o win32/atari.ro"
        ;;
  x11*)
        AC_DEFINE(X11,, [Standard X11])
        AC_PATH_X
        if test -n "$x_includes"; then
                CFLAGS="$CFLAGS -I$x_includes"
        fi
        if test -n "$x_libraries"; then
                LDFLAGS="$LDFLAGS -L$x_libraries"
        fi
        dnl Replace `main' with a function in -lX11:
        NEED_LIB(X11, main)
        case "$a800_target" in
          *-shm)
                dnl Replace `main' with a function in -lXext:
                AC_CHECK_LIB(Xext, main)
                AC_DEFINE(SHM,, [X11 with shared memory extensions])
                ;;
        esac
        case "$a800_target" in
          x11-xview*)
                AC_DEFINE(XVIEW,, [X11 with XView])
                dnl Replace `main' with a function in -lxview:
                NEED_LIB(xview, main)
                dnl Replace `main' with a function in -lolgx:
                NEED_LIB(olgx, main)
                ;;
          x11-motif)
                AC_DEFINE(MOTIF,, [X11 with Motif])
                dnl Replace `main' with a function in -lXm:
                NEED_LIB(Xm, main)
                dnl Replace `main' with a function in -lXt:
                NEED_LIB(Xt, main)
                dnl Replace `main' with a function in -lgen:
                NEED_LIB(gen, main)
                dnl Replace `main' with a function in -lsocket:
                NEED_LIB(socket, main)
                ;;
        esac
        OBJ="$OBJ atari_x11.o"
        ;;
esac


dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h strings.h sys/ioctl.h sys/time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(getcwd gettimeofday select strdup strerror strstr strtol)

dnl user defined part of configuration
echo

dnl DEF_FEATURE(symbol name, feature name, enable/disable,
dnl feature description, default action)
dnl 'enable/disable' means defining symbol enables/disables feature
dnl default action: y - define symbol, n - don't define
AC_DEFUN(DEF_FEATURE,
[
a800_ans=$5
AC_ARG_ENABLE($2,
[  --$3-$2      ifelse($3, enable, Enable, Disable) $4 (default is ifelse(
$5, y, yes, $5, n, no,
[errprint(__file__ __line__: bad default action '$5')m4_exit(1)]
))],
[if test "$enableval" = ifelse($3, enable, yes, $3, disable, no,
[errprint(__file__ __line__: bad enable/disable word '$3')m4_exit(1)]); then
a800_ans=y
else
a800_ans=n
fi
])
if test "$a800_ans" = y ; then
        AC_DEFINE($1,, define to $3 $4)
        AC_MSG_RESULT([$4 $3]d)
else
        AC_MSG_RESULT([$4 not $3]d)
fi
])

DEF_FEATURE(CRASH_MENU, crashmenu, enable, crash menu after CIM instruction, y)
DEF_FEATURE(MONITOR_BREAK, monitorbreak, enable, BREAK command in monitor (slows CPU a little bit), y)
DEF_FEATURE(MONITOR_HINTS, monitorhints, enable, hints in disassembler (addresses get human readable labels), y)
DEF_FEATURE(MONITOR_ASSEMBLER, monitorasm, enable, assembler in monitor (you can enter ASM insns directly), y)
DEF_FEATURE(CYCLE_EXACT, cycleexact, enable, color changes inside a scanline (cycle exact), y)
DEF_FEATURE(NEW_CYCLE_EXACT, newcycleexact, enable, new cycle-exact scanline changes, y)

dnl sound support
DEF_FEATURE(SOUND, sound, enable, Sound Support, y)
if test "$a800_ans" = y; then
        case "$a800_host" in
          dos)
                OBJ_SND="dos/sound_dos.o dos/dos_sb.o"
                ;;
          win)
                OBJ_SND="win32/sound.o"
                ;;
          falcon)
                OBJ_SND="sound_falcon.o falcon/sound_dma.o"
                ;;
          *)
                OBJ_SND="sound.o"
        esac
        if test "$a800_target" = sdl; then
                OBJ_SND=""
        fi
        OBJ="$OBJ $OBJ_SND"
        DEF_FEATURE(INTERPOLATE_SOUND, interpolatesound, enable, Sound Interpolation, y)
        DEF_FEATURE(STEREO_SOUND, stereosound, enable, Stereo Sound, y)
        DEF_FEATURE(VOL_ONLY_SOUND, volonlysound, enable, volume only sound (digitized sound effects), y)
        if test "$a800_ans" = y; then
                DEF_FEATURE(CONSOLE_SOUND, consolesound, enable, console sound (keyboard clicks), y)
                DEF_FEATURE(SERIO_SOUND, seriosound, enable, serial in/out sound, n)
        fi      dnl digitized effects enabled
        DEF_FEATURE(CLIP_SOUND, clipsound, enable, Sound Clipping, n)
fi

DEF_FEATURE(SNAILMETER, snailmeter, enable, Snailmeter (shows how much is the emulator slower than original), n)
DEF_FEATURE(PAGED_ATTRIB, pagedattrib, enable, page-based attribute array, n)
DEF_FEATURE(BUFFERED_LOG, bufferedlog, enable, buffered debug output (until the graphics mode switches back to text mode), n)
DEF_FEATURE(SHOW_DISK_LED, showdiskled, enable, Show disk LED emulation on screen display, n)
DEF_FEATURE(VERY_SLOW, veryslow, enable, very slow computer support (disables generating screen completely), n)

if test "$a800_host" = falcon; then dnl falcon host specific
        DEF_FEATURE(FALCON_CPUASM, falconcpuasm, enable, using m68k assembler CPU core (falcon only), n)
        if test "$a800_ans" = y; then
                OBJ="$OBJ cpu_m68k.o"
        fi
fi

if test "$a800_host" = linux; then
        case "$a800_target" in
          svgalib | x11*)
                DEF_FEATURE(LINUX_JOYSTICK, linuxjoystick, enable, LINUX Joystick (linux svgalib/x11 only), n)
        esac
fi

if test "$a800_target" = svgalib; then dnl svgalib specific
        DEF_FEATURE(SVGA_SPEEDUP, svgaspeedup, enable, drawing every 1/50 sec only 1/refresh of screen (svgalib only), n)
        DEF_FEATURE(SVGA_CURSORBLOCK, svgacursorblock, enable, using cursor keys/Ctrl for keyboard joystick (svgalib only), n)
        DEF_FEATURE(SVGA_JOYMOUSE, svgajoymouse, enable, support for Toshiba Joystick Mouse (svgalib only), n)
fi



AC_CACHE_CHECK(unaligned long accesses, ac_cv_unaligned_long_ok,
[AC_TRY_RUN([
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
jmp_buf jmpbuf;
void bus_err()
{
        longjmp(jmpbuf, 1);
}
int main(void)
{
#ifndef SIGBUS
        return 0;
#else
        long l[2];

        if (setjmp(jmpbuf) == 0) {
                signal(SIGBUS, bus_err);
                *((long *) ((char *) l + 1)) = 1;
                signal(SIGBUS, SIG_DFL);
                return 0;
        }
        else {
                signal(SIGBUS, SIG_DFL);
                return 1;
        }
#endif
}], ac_cv_unaligned_long_ok=yes, ac_cv_unaligned_long_ok=no,
ac_cv_unaligned_long_ok=no)])
if test "$ac_cv_unaligned_long_ok" = yes; then
        AC_DEFINE(UNALIGNED_LONG_OK,, [define if unaligned long access is ok])
fi

AC_CHECK_SIZEOF(long, 4)

AC_C_BIGENDIAN

AC_SUBST(OBJ)

AC_OUTPUT(Makefile)
