dnl   configure.ac - Autoconf script for building configure
dnl
dnl   Copyright (C) 2001 Krzysztof Nikiel
dnl   Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
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
dnl   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

AC_PREREQ(2.57)
AC_INIT(Atari800, 4.2.0, pstehlik@sophics.cz)
AC_CONFIG_SRCDIR(src/atari.c)
AC_CONFIG_HEADER(src/config.h)
AC_CONFIG_MACRO_DIR([m4])


dnl Set a8_target...

case "$target" in
    default | falcon | firebee | ps2 | rpi | android | windx | x11 | x11-shm | x11-motif | x11-xview | x11-xview-shm | libatari800)
        a8_target="$target"
        ;;
    shm | motif | xview | xview-shm)
        a8_target="x11-$target"
        ;;
    "")
        a8_target=default
        ;;
    *)
        echo
        echo "Usage: configure [--target=<target>] <other options>"
        echo
        echo "The '--target' option, if not given, defaults to 'default'.  Use '--help'"
        echo "to see other available options, including Atari800 specific '--enable' options."
        echo "Possible values for the '--target' option are:"
        echo "    default          (autodetect available graphics and sound libraries)"
        echo "    android          (Android devices)"
        echo "    falcon           (Atari Falcon: set --host=m68k-atari-mint)"
        echo "    firebee          (FireBee: set --host=m68k-atari-mint)"
        echo "    ps2              (Sony PlayStation 2)"
        echo "    rpi              (Raspberry Pi: set --host=arm-linux)"
        echo "    windx            (Windows with DirectX only)"
        echo "    libatari800      (Atari800 as a library (developers only))"
        echo "    x11              (Standard X11)"
        echo "    (x11-)motif      (Motif on X11)"
        echo "    (x11-)shm        (Standard X11 with shared memory extensions)"
        echo "    (x11-)xview      (XView on X11)"
        echo "    (x11-)xview-shm  (XView on X11, with shared memory extensions)"
        echo
        exit 1;
        ;;
esac

AC_CANONICAL_HOST
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE([enable])

dnl Silent rules
# Support silent build rules, requires at least automake-1.11. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

WANT_IDE="yes"
WANT_POKEYREC="yes"

dnl Set a8_host...

if [[ "$host_os" = "cygwin" ]]; then
    CC="gcc -mno-cygwin"
    echo
    echo "Using CC=\"$CC\" to disable cygwin library...";
    echo
    host_os="mingw32"
fi

case $host_os in
    i386-pc-os2-emx)
        a8_host="os/2"
        ;;
    mingw32*)
        a8_host="win"
        if [[ "$a8_target" != "android" ]]; then
            dnl Android target uses normal slashes
            AC_DEFINE(DIR_SEP_BACKSLASH,1,[Define to use back slash as directory separator.])
        fi
        AC_DEFINE(DOS_DRIVES,1,[Define to enable DOS style drives support.])
        ;;
    msdosdjgpp)
        a8_host="dos"
        AC_DEFINE(DIR_SEP_BACKSLASH,1,[Define to use back slash as directory separator.])
        AC_DEFINE(DEFAULT_CFG_NAME,"atari800.cfg",[Alternate config filename due to 8+3 fs limit.])
        AC_DEFINE(DOS_DRIVES,1,[Define to enable DOS style drives support.])
        AC_DEFINE(SYSTEM_WIDE_CFG_FILE,"c:\\atari800.cfg",[Alternate system-wide config file for non-Unix OS.])
        ;;
    linux | linux-gnu)
        a8_host="linux"
        ;;
    darwin*)
        a8_host="macos"
        ;;
    mint)
        if [[ "$a8_target" = "firebee" ]]; then
            a8_host=firebee
            a8_target=default
        else
            a8_host=falcon
        fi
        AC_DEFINE(DIR_SEP_BACKSLASH,1,[Define to use back slash as directory separator.])
        AC_DEFINE(DEFAULT_CFG_NAME,"atari800.cfg",[Alternate config filename due to 8+3 fs limit.])
        AC_DEFINE(DOS_DRIVES,1,[Define to enable DOS style drives support.])
        AC_DEFINE(SYSTEM_WIDE_CFG_FILE,"c:\\atari800.cfg",[Alternate system-wide config file for non-Unix OS.])
        AC_DEFINE(BITPL_SCR,1,[Define to allocate the screen back buffer.])
        WANT_IDE="no"
        ;;
    beos)
        a8_host="beos"
        ;;
    *unix*)
        a8_host="unix"
        ;;
    *)
        a8_host="$host_os"
        ;;
esac


dnl Set CFLAGS and LDFLAGS...

if [[ "X_$CFLAGS" = "X_" ]]; then
    CFLAGS="-O2 -Wall"
fi
if [[ "X_$LDFLAGS" = "X_" ]]; then
    LDFLAGS=""
fi
if [[ "$a8_target" = "ps2" ]]; then
    CC="ee-gcc"
    CFLAGS="$CFLAGS -D_EE -G0 -mno-crt0 -fno-builtin-printf -nostartfiles"
    CFLAGS="$CFLAGS -I${PS2SDK}/common/include -I${PS2SDK}/ee/include"
    CFLAGS="$CFLAGS -I${GSKIT}/ee/dma/include -I${GSKIT}/ee/gs/include"
    CFLAGS="$CFLAGS -I${PS2DEV}/ee/lib/gcc-lib/ee/3.2.2/include"
    CFLAGS="$CFLAGS -I${PS2SDK}/ports/include"
    LDFLAGS="$LDFLAGS -T${PS2SDK}/ee/startup/linkfile"
    LDFLAGS="$LDFLAGS -L${PS2SDK}/ee/lib -L${GSKITSRC}/lib -L${PS2DEV}/ee/lib/gcc-lib/ee/3.2.2"
    LDFLAGS="$LDFLAGS -L${PS2SDK}/ports/lib"
fi
if [[ "$a8_target" = "rpi" ]]; then
    [[ -z "$RPI_SDK" ]] && RPI_SDK="/opt/vc"
    CC="arm-linux-gnueabihf-gcc"
    CFLAGS="$CFLAGS -I${RPI_SDK}/include -I${RPI_SDK}/include/SDL -I${RPI_SDK}/include/interface/vmcs_host/linux -I${RPI_SDK}/include/interface/vcos/pthreads"
    LDFLAGS="$LDFLAGS -Wl,--unresolved-symbols=ignore-in-shared-libs -L${RPI_SDK}/lib"
fi
if [[ "$a8_target" = "android" ]]; then
	CC="arm-linux-androideabi-gcc"
	CPP="arm-linux-androideabi-cpp"
	AC_NO_EXECUTABLES
	CPPFLAGS="$CPPFLAGS -I$ANDROID_NDK_ROOT/platforms/android-9/arch-arm/usr/include/"
	WANT_IDE=no
	with_readline=no
	WANT_EVENT_RECORDING=no
	AC_DEFINE(HAVE_GETTIMEOFDAY,1)
fi
if [[ "$a8_host" = "falcon" ]]; then
	CFLAGS="-m68020-60 -O2 -fomit-frame-pointer -Wall"
	LDFLAGS="-m68020-60"
fi
if [[ "$a8_host" = "firebee" ]]; then
	CFLAGS="-mcpu=5475 -O2 -fomit-frame-pointer -Wall"
	LDFLAGS="-mcpu=5475"
fi
if [[ "$a8_target" = "libatari800" ]]; then
    with_readline=no
    WANT_EVENT_RECORDING=no
fi

dnl Check for programs...

AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_TOOLS(WINDRES, [windres], :)

AC_DEFUN([AM_PROG_AS], []) dnl dummy to make automake happy, since vasm is used instead
CCAS=vasm
CCASFLAGS="-quiet -devpac -opt-allbra -Faout"

a8_use_sdl=no
dnl Check if SDL library exists. If it's found, set a8_use_sdl to yes and AC_DEFINE the SDL symbol.
dnl If called multiple times, it will check for the library only once.
dnl Usage: TRY_USE_SDL()
AC_DEFUN([TRY_USE_SDL],
[
    if [[ "$a8_use_sdl" = no ]]; then
        dnl Check for SDL
        SDL_VERSION=1.2.0
        AM_PATH_SDL($SDL_VERSION,
        [
            AC_DEFINE(SDL,1,[Target: SDL library.])
            AC_DEFINE(SUPPORTS_PLATFORM_TIME,1,[Platform-specific time function.])
            LIBS="$LIBS $SDL_LIBS"
            CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
            if [[ "$a8_host" = "win" ]]; then
dnl Don't redefine main -- SDL.h will do it for us in atari_sdl.c Otherwise
dnl the autoconf tests don't work.
                CPPFLAGS=${CPPFLAGS/-Dmain=SDL_main/}
dnl Avoid making stdout.txt on Windows sdl target by compiling our own SDL_main
dnl Get SDL_win32_main.c from the SDL source and copy it to the src directory.
                LIBS=${LIBS/-lSDLmain/}
                CPPFLAGS="$CPPFLAGS -DNO_STDIO_REDIRECT"
dnl Don't make this a windowed app, make it a console app.
                LIBS=${LIBS/-mwindows/}
            fi
            a8_use_sdl=yes
        ])
    fi
])

dnl Check if host os Java NestedVM...
if [[ "$a8_target" != "android" ]]; then
    dnl Android target cannot perform link tests
    AC_MSG_CHECKING([whether host is Java NestedVM])
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM(
            [[extern int _call_java(int a, int b, int c, int d);]],
            [[_call_java(0, 0, 0, 0);]]
        )],
        [
            a8_host=javanvm
            EXEEXT=".mips"
            enable_unalignedwords=no
            if [[ "$build_os" = "cygwin" ]]; then
                JAVAFLAGS="-classpath '""`cygpath -wp $CLASSPATH`'"
                JAVACFLAGS="-source 1.4 -classpath '""`cygpath -wp $CLASSPATH`'"
            fi
            AC_MSG_RESULT(yes)
        ],
        [
            AC_MSG_RESULT(no)
        ]
    )
fi


dnl Check for header files...

dnl Beware: AC_PATH_X must NOT be inside "case"!
if [[ "$a8_target" != "android" ]]; then
	AC_PATH_X
fi
case "$a8_target" in
    x11*)
        if [[ "X_$x_includes" != "X_" ]]; then
            CFLAGS="$CFLAGS -I$x_includes"
        fi
        if [[ "X_$x_libraries" != "X_" ]]; then
            LDFLAGS="$LDFLAGS -L$x_libraries -Wl,-rpath,$x_libraries"
        fi
        ;;
esac
if [[ "$a8_target" = "android" ]]; then
    echo "hardcoding dirent.h header"
    AC_DEFINE(HAVE_DIRENT_H)
else
    AC_HEADER_DIRENT
fi
AC_HEADER_STDC
AC_HEADER_TIME
AC_TYPE_UINTPTR_T
AC_CHECK_HEADERS([direct.h errno.h file.h signal.h sys/time.h time.h unistd.h unixio.h])
AC_HEADER_TIOCGWINSZ
SUPPORTS_SOUND_OSS=yes
AC_CHECK_HEADERS([fcntl.h sys/ioctl.h sys/soundcard.h],,SUPPORTS_SOUND_OSS=no)
SUPPORTS_RDEVICE=yes
if [[ "$a8_host" = "win" ]]; then
    AC_CHECK_HEADERS([windows.h winsock2.h],,SUPPORTS_RDEVICE=no)
else
    AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h sys/socket.h termios.h],,SUPPORTS_RDEVICE=no)
fi


dnl Check for libraries...

AC_DEFUN([A8_NEED_LIB],AC_CHECK_LIB($1,main,,AC_MSG_ERROR("$1 library not found!")))
if [[ "$a8_target" = "android" ]]; then
    echo "hardcoding libz"
    LIBS="-lz $LIBS"
    AC_DEFINE(HAVE_LIBZ)
    dnl libm is automatically appended by the build system
elif [[ "$a8_target" = "libatari800" ]]; then
    AC_CHECK_LIB(m,cos,[LIBS="-lm $LIBS"])
    AC_CHECK_FUNCS(setjmp)
else
	AC_CHECK_LIB(z,gzopen)
	AC_CHECK_LIB(png,png_get_libpng_ver)
	AC_CHECK_LIB(m,cos,[LIBS="-lm $LIBS"])
	AC_CHECK_LIB(ossaudio,_oss_ioctl,[LIBS="-lossaudio $LIBS"])
fi

dnl Set OBJS and libraries depending on host and target...

case "$a8_target" in
    default)
        if [[ "$a8_host" != "dos" ]]; then
dnl In DJGPP building with zlib fails with -ansi.
            CFLAGS="$CFLAGS -ansi"
        fi
        CFLAGS="$CFLAGS -pedantic -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Winline"
        if [[ "$a8_host" != "beos" ]]; then
dnl BeOS has a real issue with redundant-decls
            CFLAGS="$CFLAGS -Wredundant-decls"
        fi
        ;;
    falcon)
        AC_DEFINE(SUPPORTS_PLATFORM_SLEEP,1,[Platform-specific sleep function.])
        AC_DEFINE(SUPPORTS_PLATFORM_TIME,1,[Platform-specific time function.])
        AC_DEFINE(FALCON,1,[Target: Atari Falcon system.])
        AC_CHECK_LIB(gem, appl_init)
        ;;
    ps2)
        AC_DEFINE(PS2,1,[Target: Sony PlayStation 2.])
        AC_DEFINE(SUPPORTS_PLATFORM_SLEEP,1,[Platform-specific sleep function.])
        AC_DEFINE(SUPPORTS_PLATFORM_TIME,1,[Platform-specific time function.])
        AC_DEFINE(DEFAULT_CFG_NAME,"mc0:/ATARI/ATARI800.CFG",[Alternate config filename due to 8+3 fs limit.])
        A8_NEED_LIB(kernel)
	A8_NEED_LIB(audsrv)
        A8_NEED_LIB(debug)
        A8_NEED_LIB(syscall)
        A8_NEED_LIB(dmakit)
        A8_NEED_LIB(gskit)
        A8_NEED_LIB(pad)
        A8_NEED_LIB(kbd)
        A8_NEED_LIB(mc)
        A8_NEED_LIB(g)
        A8_NEED_LIB(z)
        A8_NEED_LIB(c)
        ;;
    rpi)
        AC_DEFINE(RPI,1,[Target: Raspberry Pi.])
        AC_DEFINE(GUI_SDL,1,[Use SDL for graphics and input.])
        AC_DEFINE(SUPPORTS_PLATFORM_CONFIGURE,1,[Additional config file options.])
        AC_DEFINE(SUPPORTS_PLATFORM_CONFIGSAVE,1,[Save additional config file options.])
        AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
        AC_DEFINE(PLATFORM_MAP_PALETTE,1,[Platform-specific mapping of RGB palette to display surface.])
        A8_NEED_LIB(brcmGLESv2)
        A8_NEED_LIB(brcmEGL)
        A8_NEED_LIB(SDL)
        A8_NEED_LIB(bcm_host)
        ;;
    android)
        AC_DEFINE(ANDROID,1,[Target: Android])
        AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
        AC_DEFINE(DIRTYRECT,1,[Define to use dirty screen partial repaints.])
        dnl AC_DEFINE(NODIRTYCOMPARE,1,[Define to skip memory comparisons - Requires DIRTYRECT.])
        ;;
    windx)
        AC_DEFINE(DIRECTX,1,[Target: Windows with DirectX.])
        AC_DEFINE(SUPPORTS_PLATFORM_CONFIGURE,1,[Additional config file options.])
        AC_DEFINE(SUPPORTS_PLATFORM_CONFIGSAVE,1,[Save additional config file options.])
        AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
        AC_DEFINE(SUPPORTS_SOUND_REINIT,1,[Reinitialise the sound system.])
        A8_NEED_LIB(ddraw)
        A8_NEED_LIB(dinput)
        A8_NEED_LIB(dsound)
        A8_NEED_LIB(dxguid)
        A8_NEED_LIB(d3d9)
        A8_NEED_LIB(gdiplus)
        A8_NEED_LIB(stdc++)
        A8_NEED_LIB(ws2_32)
        A8_NEED_LIB(gdi32)
        CFLAGS="$CFLAGS -ansi -pedantic -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls -Wstrict-prototypes -Winline"
        LDFLAGS="$LDFLAGS -mwindows"
        ;;
    libatari800)
        AC_DEFINE(LIBATARI800,1,[Target: Atari800 as a library.])
        ;;
    x11*)
        AC_DEFINE(X11,1,[Target: Standard X11.])
        AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
        A8_NEED_LIB(X11)
        case "$a8_target" in
            *-shm)
                AC_DEFINE(SHM,1,[Target: X11 with shared memory extensions.])
                A8_NEED_LIB(Xext)
                ;;
        esac
        case "$a8_target" in
            x11-xview*)
                AC_DEFINE(XVIEW,1,[Target: X11 with XView.])
                dnl Dunno if this is the right way...
                CFLAGS="$CFLAGS -I/usr/openwin/include"
                dnl -lxview must come before -lolgx, otherwise it does not link.
                dnl Note that AC_CHECK_LIB (and therefore A8_NEED_LIB) _prepends_
                dnl the found library to LIBS.
                A8_NEED_LIB(olgx)
                A8_NEED_LIB(xview)
                ;;
            x11-motif)
                AC_DEFINE(MOTIF,1,[Target: X11 with Motif.])
                dnl According to the information I was able to find,
                dnl "-lgen -lsocket" is necessary only with old Sun Motif libs.
                AC_CHECK_LIB(socket,main,[LIBS="-lsocket $LIBS"])
                AC_CHECK_LIB(gen,main,[LIBS="-lgen $LIBS"])
                dnl -lXm must come before -lXt, otherwise I get the following
                dnl fatal error at runtime, when calling XtVaCreateManagedWidget:
                dnl X Error of failed request:  BadWindow (invalid Window parameter)
                dnl See MOTIF FAQ "What order should the libraries be linked in?"
                dnl Note that AC_CHECK_LIB (and therefore A8_NEED_LIB) _prepends_
                dnl the found library to LIBS.
                A8_NEED_LIB(Xt)
                A8_NEED_LIB(Xm)
                ;;
        esac
        WANT_X11=yes
        ;;
esac

AM_CONDITIONAL([CONFIGURE_HOST_JAVANVM], test "$a8_host" = javanvm)
AM_CONDITIONAL([CONFIGURE_HOST_DOS], test "$a8_host" = dos)
AM_CONDITIONAL([CONFIGURE_HOST_WIN], test "$a8_host" = win)
AM_CONDITIONAL([CONFIGURE_TARGET_ANDROID], test "$a8_target" = android)
AM_CONDITIONAL([CONFIGURE_TARGET_FALCON], test "$a8_target" = falcon)
AM_CONDITIONAL([CONFIGURE_TARGET_PS2], test "$a8_target" = ps2)
AM_CONDITIONAL([CONFIGURE_TARGET_RPI], test "$a8_target" = rpi)
AM_CONDITIONAL([CONFIGURE_TARGET_WINDX], test "$a8_target" = windx)
AM_CONDITIONAL([CONFIGURE_TARGET_LIBATARI800], test "$a8_target" = libatari800)
AM_CONDITIONAL([CONFIGURE_TARGET_X11], test "$WANT_X11" = yes)


dnl Check for typedefs, structures, and compiler characteristics...

AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_PROG_GCC_TRADITIONAL
AC_STRUCT_TM
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T


dnl Set usage of unaligned word accesses...

AC_ARG_ENABLE(unalignedwords,AC_HELP_STRING(--enable-unalignedwords,[Override usage of unaligned words]))
if [[ "$enable_unalignedwords" != "yes" -a "$enable_unalignedwords" != "no" ]]; then
    case $host_cpu in
        alpha* | arm* | hppa* | ia64 | mips* | sparc*)
            enable_unalignedwords=no
            ;;
        i*86 | m68* | powerpc* | x86_64)
            enable_unalignedwords=yes
            ;;
        *)
            AC_MSG_WARN([$host_cpu architecture is unknown to this script.])
            AC_MSG_WARN([Performance may be sub-optimal. Please contact Atari800 developers.])
            enable_unalignedwords=no
            ;;
    esac
fi

if [[ "$enable_unalignedwords" = "yes" ]]; then
    dnl Make sure it is allowed
    AC_MSG_CHECKING([for unaligned word access validity])
    AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([[#include <stdio.h>]],[[
            unsigned char test_data[] = "Hello, I test unaligned word access validity.";
            unsigned char *p;
            /* step through test_data as far as 4 bytes are available via p */
            for (p = test_data; p[2] != '\0'; p++) {
                unsigned int word_read = *(unsigned short *) p;
                if (word_read != (p[0] + (p[1] << 8))
                 && word_read != (p[1] + (p[0] << 8))) {
                    printf("16-bit access at address %p yields bad data!\n"
                           "Bytes: %02X %02X; Value read: %04X\n",
                           p, p[0], p[1], word_read);
                    return 1;
                }
                word_read = *(unsigned int *) p;
                if (word_read != (p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24))
                 && word_read != (p[3] + (p[2] << 8) + (p[1] << 16) + (p[0] << 24))) {
                    printf("32-bit access at address %p yields bad data!\n"
                           "Bytes: %02X %02X %02X %02X; Value read: %08X\n",
                           p, p[0], p[1], p[2], p[3], word_read);
                    return 1;
                }
            }
        ]])],
        AC_MSG_RESULT([yes]),
        [enable_unalignedwords=no; AC_MSG_RESULT([failed! disabling unaligned word access])],
        AC_MSG_RESULT([skipped because cross-compiling])
    )
    if [[ "$enable_unalignedwords" = "yes" ]]; then
        AC_DEFINE(WORDS_UNALIGNED_OK,1,[Define if unaligned word access is ok.])
    fi
fi


dnl Check for library functions...

# from "Autoconf Archive":
AC_DEFUN([AX_FUNC_MKDIR],
[AC_CHECK_FUNCS([mkdir _mkdir])
AC_CACHE_CHECK([whether mkdir takes one argument],
               [ac_cv_mkdir_takes_one_arg],
[AC_TRY_COMPILE([
#include <sys/stat.h>
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
], [mkdir (".");],
[ac_cv_mkdir_takes_one_arg=yes], [ac_cv_mkdir_takes_one_arg=no])])
if test x"$ac_cv_mkdir_takes_one_arg" = xyes; then
  AC_DEFINE([MKDIR_TAKES_ONE_ARG], 1,
            [Define if mkdir takes only one argument.])
fi
])

AC_FUNC_CLOSEDIR_VOID
AC_FUNC_MEMCMP
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
if [[ "$a8_target" = "android" ]]; then
    echo "hardcoding libc funcs"
    AC_DEFINE(HAVE_VPRINTF) AC_DEFINE(HAVE_ATEXIT) AC_DEFINE(HAVE_CHMOD) AC_DEFINE(HAVE_FDOPEN)
    AC_DEFINE(HAVE_FFLUSH) AC_DEFINE(HAVE_FLOOR) AC_DEFINE(HAVE_FSTAT) AC_DEFINE(HAVE_GETCWD)
    AC_DEFINE(HAVE_GETTIMEOFDAY) AC_DEFINE(HAVE_LOCALTIME) AC_DEFINE(HAVE_MEMMOVE) AC_DEFINE(HAVE_MEMSET)
    AC_DEFINE(HAVE_MKSTEMP) AC_DEFINE(HAVE_MKTEMP) AC_DEFINE(HAVE_MODF) AC_DEFINE(HAVE_NANOSLEEP)
    AC_DEFINE(HAVE_OPENDIR) AC_DEFINE(HAVE_RENAME) AC_DEFINE(HAVE_REWIND) AC_DEFINE(HAVE_RMDIR)
    AC_DEFINE(HAVE_SIGNAL) AC_DEFINE(HAVE_SNPRINTF) AC_DEFINE(HAVE_STAT) AC_DEFINE(HAVE_STRCASECMP)
    AC_DEFINE(HAVE_STRCHR) AC_DEFINE(HAVE_STRDUP) AC_DEFINE(HAVE_STRERROR) AC_DEFINE(HAVE_STRRCHR)
    AC_DEFINE(HAVE_STRSTR) AC_DEFINE(HAVE_STRTOL) AC_DEFINE(HAVE_SYSTEM) AC_DEFINE(HAVE_TIME)
    AC_DEFINE(HAVE_TMPNAM) AC_DEFINE(HAVE_UCLOCK) AC_DEFINE(HAVE_UNLINK)
    AC_DEFINE(HAVE_VSNPRINTF) AC_DEFINE(HAVE_MKDIR)
    AC_DEFINE(HAVE_SELECT) AC_DEFINE(HAVE_USLEEP) AC_DEFINE(HAVE_STRNCPY)
	dnl Leave out tmpfile to force creation of temp files to external
else
    AC_FUNC_VPRINTF
    AC_CHECK_FUNCS([atexit chmod clock fdopen fflush floor fstat getcwd])
    AC_CHECK_FUNCS([gettimeofday localtime memmove memset mkstemp mktemp])
    AC_CHECK_FUNCS([modf nanosleep opendir rename rewind rmdir signal snprintf])
    AC_CHECK_FUNCS([stat strcasecmp strchr strdup strerror strrchr strstr])
    AC_CHECK_FUNCS([strtol system time tmpfile tmpnam uclock unlink vsnprintf popen])
    AX_FUNC_MKDIR
	dnl select usleep strncpy are broken on the NestedVM host
    if test "x$a8_host" != xjavanvm ; then
        AC_CHECK_FUNCS([select usleep strncpy])
    fi
fi
if [[ "$a8_host" != "win" -a "$a8_target" != "android" ]]; then
    AC_CHECK_FUNCS([gethostbyaddr gethostbyname inet_ntoa socket],,SUPPORTS_RDEVICE=no)
fi

dnl Select/detect video interface.

AC_ARG_WITH([video],
    [AC_HELP_STRING([--with-video@<:@=no|yes|curses|ncurses|pdcurses|dosvga|sdl|javanvm@:>@],[Select video interface to use @<:@default=check@:>@])],
    [
        case "$withval" in
            no | yes | check | curses | ncurses | pdcurses | dosvga | sdl | javanvm)
                ;;
            *)
                AC_MSG_ERROR([unrecognized value for --with-video: "$withval"])
                ;;
        esac
    ],
    [with_video=check])

if [[ "$a8_target" = default ]]; then
    if [[ "$with_video" != no ]]; then
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = javanvm ]]; then
            if [[ "$a8_host" = javanvm ]]; then
                with_video=javanvm
            elif [[ "$with_video" = javanvm ]]; then
                AC_MSG_ERROR([--with-video=javanvm was given, but host system is not NestedVM!])
            fi
        fi
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = dosvga ]]; then
            if [[ "$a8_host" = dos ]]; then
                with_video=dosvga
            elif [[ "$with_video" = dosvga ]]; then
                AC_MSG_ERROR([--with-video=dosvga was given, but host system is not DOS!])
            fi
        fi
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = sdl ]]; then
            TRY_USE_SDL()
            if [[ "$a8_use_sdl" = yes ]]; then
                with_video=sdl
            elif [[ "$with_video" = sdl ]]; then
                AC_MSG_FAILURE([--with-video=sdl was given, but SDL library not found!])
            fi
        fi
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = curses ]]; then
            AC_CHECK_LIB([curses], [initscr],
                [LIBS="-lcurses $LIBS"
                 with_video=curses
                ],
                [if [[ "$with_video" = curses ]]; then
                    AC_MSG_FAILURE([--with-video=curses was given, but curses library not found!])
                 fi
                ])
        fi
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = ncurses ]]; then
            AC_CHECK_LIB([ncurses], [initscr],
                [LIBS="-lncurses $LIBS"
                 with_video=ncurses
                 AC_DEFINE(USE_NCURSES,1,[Target: Ncurses library.])
                ],
                [if [[ "$with_video" = ncurses ]]; then
                    AC_MSG_FAILURE([--with-video=ncurses was given, but ncurses library not found!])
                 fi
                ])
        fi
        if [[ "$with_video" = check -o "$with_video" = yes -o "$with_video" = pdcurses ]]; then
            AC_CHECK_LIB([pdcurses], [initscr],
                [LIBS="-lpdcurses $LIBS"
                 with_video=pdcurses
                ],
                [if [[ "$with_video" = pdcurses ]]; then
                    AC_MSG_FAILURE([--with-video=pdcurses was given, but pdcurses library not found!])
                 fi
                ])
        fi
    fi
    if [[ "$with_video" = check ]]; then
        with_video=no
    fi
    case "$with_video" in
        javanvm)
            AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
            AC_DEFINE(SUPPORTS_PLATFORM_SLEEP,1,[Platform-specific sleep function.])
            ;;
        sdl)
            AC_DEFINE(GUI_SDL,1,[Use SDL for graphics and input.])
            AC_DEFINE(SUPPORTS_PLATFORM_CONFIGURE,1,[Additional config file options.])
            AC_DEFINE(SUPPORTS_PLATFORM_CONFIGSAVE,1,[Save additional config file options.])
            AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
            AC_DEFINE(SUPPORTS_CHANGE_VIDEOMODE,1,[Can change video modes on the fly.])
            AC_DEFINE(SUPPORTS_ROTATE_VIDEOMODE,1,[Can display the screen rotated sideways.])
            AC_DEFINE(PLATFORM_MAP_PALETTE,1,[Platform-specific mapping of RGB palette to display surface.])
            WANT_XEP80_EMULATION=yes
            WANT_NTSC_FILTER=yes
            WANT_PAL_BLENDING=yes
            AC_DEFINE(PBI_PROTO80,1,[A prototype 80 column card for the 1090 expansion box.])
            AC_DEFINE(AF80,1,[The Austin Franklin 80 column card.])
            AC_DEFINE(BIT3,1,[The Bit3 Full View 80 column card.])
            ;;
        dosvga)
            AC_DEFINE(DOSVGA,1,[Target: DOS VGA.])
            AC_DEFINE(SUPPORTS_PLATFORM_CONFIGURE,1,[Additional config file options.])
            AC_DEFINE(SUPPORTS_PLATFORM_PALETTEUPDATE,1,[Update the Palette if it changed.])
            ;;
        curses | ncurses | pdcurses)
            AC_DEFINE(USE_CURSES,1,[Target: Curses-compatible library.])
            WANT_EVENT_RECORDING=no
            ;;
        no)
            AC_DEFINE(BASIC,1,[Target: standard I/O.])
            WANT_EVENT_RECORDING=no
            ;;
        *)
            AC_MSG_ERROR([--with-video=$with_video was given, but no video/input library found!])
            ;;
    esac
fi
AM_CONDITIONAL(WITH_VIDEO_JAVANVM, test "$with_video" = javanvm)
AM_CONDITIONAL(WITH_VIDEO_SDL, test "$with_video" = sdl)
AM_CONDITIONAL(WITH_VIDEO_DOSVGA, test "$with_video" = dosvga)
AM_CONDITIONAL(WITH_VIDEO_CURSES, test "$with_video" = curses -o "$with_video" = ncurses -o "$with_video" = pdcurses)
AM_CONDITIONAL(WITH_VIDEO_LIBATARI800, test "$a8_target" = libatari800)
AM_CONDITIONAL(WITH_VIDEO_NO, test "$with_video" = no)


dnl Select optional Atari800 features...

dnl The A8_OPTION macro automates the calling of AC_ARG_ENABLE, AC_HELP_STRING and AC_DEFINE.
dnl         $1    = the name of the feature, what follows "--enable-".
dnl         $2    = "yes" or "no", determines if feature defaults to being active or not.
dnl         $3    = The right side of the help line.
dnl                 The left side is always "--enable-$1".
dnl         $4    = The symbol name which goes to config.h, the C #define symbol.
dnl                 A symbol called "WANT_$4" is defined for use in this configure.ac.
dnl         $5    = The help string which shows up in config.h for the $4 symbol.
AC_DEFUN([A8_OPTION],[
    AC_ARG_ENABLE($1,AC_HELP_STRING(--enable-$1,$3),WANT_$4=$enableval,WANT_$4=$2)
    if [[ "$WANT_$4" = "yes" ]]; then
        AC_DEFINE($4,1,$5)
    fi
])

if [[ "$a8_target" == "libatari800" ]]; then
    WANT_NEW_CYCLE_EXACT=yes
    WANT_VERY_SLOW=no
    WANT_CRASH_MENU=no
    WANT_CURSES_BASIC=no
elif [[ "$a8_target" != "default" -o "$with_video" != no ]]; then

    case "$with_video" in
        *curses)
            A8_OPTION(cursesbasic,yes,
                      [No bitmap graphics emulation (curses targets only) (default=ON)],
                      CURSES_BASIC,[Define to disable bitmap graphics emulation in CURSES target.]
                     )
            ;;
    esac

    if [[ "$WANT_CURSES_BASIC" != "yes" ]]; then

        A8_OPTION(newcycleexact,yes,
                  [Allow color changes inside a scanline (default=ON)],
                  NEW_CYCLE_EXACT,[Define to allow color changes inside a scanline.]
                 )
        if [[ "$WANT_NEW_CYCLE_EXACT" = "yes" ]]; then
            CCASFLAGS="$CCASFLAGS -DNEW_CYCLE_EXACT"
        fi

        A8_OPTION(veryslow,no,
                  [Use very slow computer support (use with the -refresh option) (default=OFF)],
                  VERY_SLOW,[Define to use very slow computer support (faster -refresh).]
                 )

    fi

    A8_OPTION(crashmenu,yes,
              [Display a menu after a CIM instruction (default=ON)],
              CRASH_MENU,[Define to activate crash menu after CIM instruction.]
             )
    if [[ "$WANT_CRASH_MENU" = "yes" ]]; then
        CCASFLAGS="$CCASFLAGS -DCRASH_MENU"
    fi
fi
AM_CONDITIONAL([WANT_CURSES_BASIC], test "$WANT_CURSES_BASIC" = "yes")
AM_CONDITIONAL([WANT_NEW_CYCLE_EXACT], test "$WANT_NEW_CYCLE_EXACT" = "yes")

A8_OPTION(pagedattrib,no,
          [Use page-based attribute array (default=OFF)],
          PAGED_ATTRIB,[Define to use page-based attribute array.]
         )

A8_OPTION(cyclesperopcode,no,
          [Update ANTIC counter in each opcode's emulation (default=OFF)],
          CYCLES_PER_OPCODE,[Define to update ANTIC counter in each opcode's emulation.]
         )

if [[ "$a8_target" = libatari800 ]]; then
    WANT_BUFFERED_LOG=yes
    AC_DEFINE(BUFFERED_LOG,1,[Define to use buffered debug output.])
else
    A8_OPTION(bufferedlog,no,
          [Use buffered debug output (until the graphics mode switches back to text mode) (default=OFF)],
          BUFFERED_LOG,[Define to use buffered debug output.]
         )
fi

A8_OPTION(altirra_bios,yes,
      [Use Altirra OS to allow operation when real ROMs are not available (default=ON)],
      EMUOS_ALTIRRA,[Define to use Altirra emulated OS when real ROMs are not available.]
     )

AM_CONDITIONAL([WANT_EMUOS_ALTIRRA], test "$WANT_EMUOS_ALTIRRA" = "yes")

if [[ "$a8_target" = libatari800 ]]; then
    WANT_MONITOR_ASSEMBLER=no
    WANT_MONITOR_BREAK=no
    WANT_MONITOR_BREAKPOINTS=no
    WANT_MONITOR_HINTS=no
    WANT_MONITOR_PROFILE=no
    WANT_MONITOR_TRACE=no
    WANT_MONITOR_ANSI=no
    WANT_MONITOR_UTF8=no
else
    A8_OPTION(monitorasm,yes,
              [Provide an assembler in the monitor (default=ON)],
              MONITOR_ASSEMBLER,[Define to activate assembler in monitor.]
             )

    A8_OPTION(monitorbreak,yes,
              [Support code breakpoints and execution history (slower emulation) (default=ON)],
              MONITOR_BREAK,[Define to activate code breakpoints and execution history.]
             )
    if [[ "$WANT_MONITOR_BREAK" = "yes" ]]; then
        CCASFLAGS="$CCASFLAGS -DMONITOR_BREAK"
    fi

    A8_OPTION(monitorbreakpoints,no,
              [Support user-defined breakpoints (default=OFF)],
              MONITOR_BREAKPOINTS,[Define to activate user-defined breakpoints.]
             )

    A8_OPTION(monitorhints,yes,
              [Provide hints in the disassembler (human-readable address labels) (default=ON)],
              MONITOR_HINTS,[Define to activate hints in disassembler.]
             )

    A8_OPTION(monitorprofile,no,
              [6502 opcode profiling (default=OFF)],
              MONITOR_PROFILE,[Define to activate 6502 opcode profiling.]
             )
    if [[ "$WANT_MONITOR_PROFILE" = "yes" ]]; then
        CCASFLAGS="$CCASFLAGS -DMONITOR_PROFILE"
    fi

    A8_OPTION(monitortrace,no,
              [Support TRACE command in the monitor (default=OFF)],
              MONITOR_TRACE,[Define to activate TRACE command in monitor.]
             )

    A8_OPTION(monitoransi,yes,
              [Support ANSI terminal control (default=ON)],
              MONITOR_ANSI,[Define to use ANSI terminal control in the monitor.]
             )

    A8_OPTION(monitorutf8,yes,
              [Support UTF-8 for printing ATASCII characters in the monitor (default=ON)],
              MONITOR_UTF8,[Define to use UTF-8 in the monitor.]
             )
fi

if [[ "$WANT_EVENT_RECORDING" != "no" ]]; then
dnl we already checked for libz and added it to LIBS
    AC_CHECK_LIB(z,adler32,
              [A8_OPTION(eventrecording,yes,
                    [Support event recording (default=ON)],
                    EVENT_RECORDING,[Define to enable event recording.]
                   )],
              WANT_EVENT_RECORDING="no"
             )
fi

A8_OPTION(pbi_mio,yes,
      [Emulate the MIO board (default=ON)],
      PBI_MIO,[Define to emulate the MIO board.]
     )
AM_CONDITIONAL([WANT_PBI_MIO], test "$WANT_PBI_MIO" = "yes")

A8_OPTION(pbi_bb,yes,
      [Emulate the Black Box (default=ON)],
      PBI_BB,[Define to emulate the Black Box.]
     )
AM_CONDITIONAL([WANT_PBI_BB], test "$WANT_PBI_BB" = "yes")

dnl Select/detect sound interface.

AC_ARG_WITH([sound],
    [AC_HELP_STRING([--with-sound@<:@=no|yes|dossb|oss|falcon|win|sdl|javanvm@:>@],[Select sound interface to use @<:@default=check@:>@])],
    [
        case "$withval" in
            no | yes | check | dossb | oss | falcon | win | sdl | javanvm)
                ;;
            *)
                AC_MSG_ERROR([unrecognized value for --with-sound: "$withval"])
                ;;
        esac
    ],
    [with_sound=check])

if [[ "$a8_target" = libatari800 ]]; then
    if [[ "$with_sound" = check -o "$with_sound" = yes ]]; then
        with_sound=libatari800
        WANT_SOUND_THIN_API=yes
    else
        AC_MSG_ERROR([--with-audio option not needed to build libatari800!])
    fi
elif [[ "$a8_target" != "ps2" -a "$a8_target" != "android" ]]; then
    if [[ "$with_sound" != no ]]; then
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = javanvm ]]; then
            if [[ "$a8_host" = javanvm ]]; then
                with_sound=javanvm
            elif [[ "$with_sound" = javanvm ]]; then
                AC_MSG_ERROR([--with-sound=javanvm was given, but host system is not NestedVM!])
            fi
        fi
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = win ]]; then
            AC_CHECK_LIB([winmm], [main],
                [LIBS="-lwinmm $LIBS"
                 with_sound=win
                ],
                [if [[ "$with_sound" = win ]]; then
                    AC_MSG_FAILURE([--with-sound=win was given, but WinMM library not found!])
                 fi
                ])
        fi
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = falcon ]]; then
            if [[ "$a8_host" = falcon -o "$a8_host" = firebee ]]; then
                with_sound=falcon
            elif [[ "$with_sound" = falcon ]]; then
                AC_MSG_ERROR([--with-sound=falcon was given, but host system is not Falcon!])
            fi
        fi
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = dossb ]]; then
            if [[ "$a8_host" = dos ]]; then
                with_sound=dossb
            elif [[ "$with_sound" = dossb ]]; then
                AC_MSG_ERROR([--with-sound=dossb was given, but host system is not DOS!])
            fi
        fi
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = sdl ]]; then
            TRY_USE_SDL()
            if [[ "$a8_use_sdl" = yes ]]; then
                with_sound=sdl
            elif [[ "$with_sound" = sdl ]]; then
                AC_MSG_FAILURE([--with-sound=sdl was given, but SDL library not found!])
            fi
        fi
        if [[ "$with_sound" = check -o "$with_sound" = yes -o "$with_sound" = oss ]]; then
            if [[ "$SUPPORTS_SOUND_OSS" = yes ]]; then
                with_sound=oss
            elif [[ "$with_sound" = oss ]]; then
                AC_MSG_ERROR([--with-sound=oss was given, but OSS is not supported!])
            fi
        fi
    fi
    if [[ "$with_sound" = check ]]; then
        with_sound=no
    fi
    case "$with_sound" in
        javanvm)
            WANT_SOUND_THIN_API=yes
            ;;
        sdl)
            WANT_SOUND_THIN_API=yes
            WANT_SOUND_CALLBACK=yes
            ;;
        win)
            AC_DEFINE(SUPPORTS_SOUND_REINIT,1,[Reinitialise the sound system.])
            ;;
        falcon)
            AC_DEFINE(POKEYSND_SIGNED_SAMPLES,1,[Use 8-bit signed samples.])
            WANT_SOUND_THIN_API=yes
            ;;
        oss)
            WANT_SOUND_THIN_API=yes
            ;;
        dossb)
            WANT_SOUND_THIN_API=yes
            WANT_SOUND_CALLBACK=yes
            ;;
        no)
            ;;
        *)
            AC_MSG_ERROR([--with-sound=$with_sound was given, but no sound library found!])
            ;;
    esac
fi
AM_CONDITIONAL([WITH_SOUND], test "$with_sound" != no)
AM_CONDITIONAL([WITH_SOUND_JAVANVM], test "$with_sound" = javanvm)
AM_CONDITIONAL([WITH_SOUND_FALCON], test "$with_sound" = falcon)
AM_CONDITIONAL([WITH_SOUND_OSS], test "$with_sound" = oss)
AM_CONDITIONAL([WITH_SOUND_SDL], test "$with_sound" = sdl)
AM_CONDITIONAL([WITH_SOUND_WIN], test "$with_sound" = win)
AM_CONDITIONAL([WITH_SOUND_DOSSB], test "$with_sound" = dossb)
AM_CONDITIONAL([WITH_SOUND_LIBATARI800], test "$with_sound" = libatari800)
AM_CONDITIONAL([A8_USE_SDL], test "$a8_use_sdl" = yes)

dnl Set sound support options...

if [[ "$with_sound" != no ]]; then

    AC_DEFINE(SOUND, 1, [Define to activate sound support.])

    if [[ "$WANT_SOUND_THIN_API" = "yes" ]]; then
        AC_DEFINE(SOUND_THIN_API,1,[Use new sound API.])
        A8_OPTION(synchronized_sound,yes,
                  [Use synchronized sound (default=ON)],
                  SYNCHRONIZED_SOUND,[Define to use synchronized sound.]
                 )
        if [[ "$WANT_SOUND_CALLBACK" = "yes" ]]; then
            AC_DEFINE(SOUND_CALLBACK,1,[Platform updates sound buffer by callback function.])
        fi
    else
        WANT_SYNCHRONIZED_SOUND="no"
    fi

    A8_OPTION(nonlinear_mixing,yes,
              [Use nonlinear POKEY mixing (default=ON)],
              NONLINEAR_MIXING,[Define to use nonlinear POKEY mixing.]
             )

    A8_OPTION(interpolatesound,yes,
              [Use sound interpolation (default=ON)],
              INTERPOLATE_SOUND,[Define to allow sound interpolation.]
             )

    A8_OPTION(stereosound,yes,
              [Use stereo sound (default=ON)],
              STEREO_SOUND,[Define to allow stereo sound.]
             )

    if [[ "$WANT_SYNCHRONIZED_SOUND" != "yes" ]]; then
        A8_OPTION(volonlysound,yes,
                  [Use volume only sound (digitized sound effects) (default=ON)],
                  VOL_ONLY_SOUND,[Define to allow volume only sound.]
                 )
    fi
    if [[ "$WANT_SYNCHRONIZED_SOUND" = "yes" -o "$WANT_VOL_ONLY_SOUND" = "yes" ]]; then
        A8_OPTION(consolesound,yes,
                  [Use console sound (keyboard clicks) (default=ON)],
                  CONSOLE_SOUND,[Define to allow console sound (keyboard clicks).]
                 )
        A8_OPTION(seriosound,no,
                  [Use serial in/out sound (default=OFF)],
                  SERIO_SOUND,[Define to allow serial in/out sound.]
                 )
    else
        WANT_CONSOLE_SOUND="no"
        WANT_SERIO_SOUND="no"
    fi

    A8_OPTION(clipsound,no,
              [Use sound clipping (default=OFF)],
              CLIP_SOUND,[Define to allow sound clipping.]
             )
    A8_OPTION(pbi_xld,yes,
              [Emulate 1450XLD (default=ON)],
              PBI_XLD,[Define to emulate the 1400XL/1450XLD.]
             )
    A8_OPTION(voicebox,yes,
              [Emulate the Alien Group Voice Box (default=ON)],
              VOICEBOX,[Define to emulate the Alien Group Voice Box.]
             )

    if [[ "$with_sound" == "libatari800" ]]; then
        WANT_SOUND_CALLBACK=no
        WANT_CONSOLE_SOUND=yes
        WANT_SERIO_SOUND=yes
        WANT_SYNCHRONIZED_SOUND=no
        WANT_VOL_ONLY_SOUND="no"
    fi
else
    WANT_NONLINEAR_MIXING="no"
    WANT_SYNCHRONIZED_SOUND="no"
    WANT_INTERPOLATE_SOUND="no"
    WANT_STEREO_SOUND="no"
    WANT_VOL_ONLY_SOUND="no"
    WANT_CONSOLE_SOUND="no"
    WANT_SERIO_SOUND="no"
    WANT_CLIP_SOUND="no"
    WANT_PBI_XLD_SOUND="no"
fi
AM_CONDITIONAL([WANT_PBI_XLD], test "$WANT_PBI_XLD" = "yes")
AM_CONDITIONAL([WANT_VOICEBOX], test "$WANT_VOICEBOX" = "yes")
AM_CONDITIONAL([WANT_PBI_XLD_OR_VOICEBOX], test "$WANT_PBI_XLD" = "yes" -o "$WANT_VOICEBOX" = "yes")
AM_CONDITIONAL([WANT_PBI_MIO_OR_BB], test "$WANT_PBI_MIO" = "yes" -o "$WANT_PBI_BB" = "yes")

A8_OPTION(ide,$WANT_IDE,
          [Provide IDE emulation (default=ON)],
          IDE,[Define to add IDE harddisk emulation.]
         )
if [[ "$WANT_IDE" = "yes" ]]; then
    AC_SYS_LARGEFILE
    AC_FUNC_FSEEKO
fi
AM_CONDITIONAL([WANT_IDE], test "$WANT_IDE" = "yes")

A8_OPTION(pokeyrec,$WANT_POKEYREC,
          [Provide Pokey registers recording (default=ON)],
          POKEYREC,[Define to add Pokey registers recording.]
         )
if [[ "$WANT_POKEYREC" == "yes" ]]; then
    AC_SYS_LARGEFILE
fi
AM_CONDITIONAL([WANT_POKEYREC], test "$WANT_POKEYREC" = "yes")

if [[ "$a8_use_sdl" = yes ]]; then
    A8_OPTION(onscreenkeyboard,no,
              [Enable on-screen keyboard (default=OFF)],
              USE_UI_BASIC_ONSCREEN_KEYBOARD,[Define to enable on-screen keyboard.]
             )
fi

dnl Select/detect features based on external software...

dnl Check for existence of the readline library and headers.
AC_ARG_WITH(readline,
    [AC_HELP_STRING(--with-readline,[Use libreadline for input in monitor @<:@default=check@:>@])],
    [],
    [with_readline=check])

if [[ "$with_readline" != no ]]; then
dnl Readline uses termcap internally. Termcap's functionality is nowadays
dnl provided by several libraries (eg. ncurses). However, on some environments,
dnl Readline is not linked to a Termcap-providing library at compile-time.
dnl So, we check whether Readline is already linked to such library, by checking
dnl existence of the tgetent symbol in readline. If not, we search for tgetent
dnl in a few other libraries. All done with a single AC_SEARCH_LIBS statement.
    have_readline=no
    AC_SEARCH_LIBS(tgetent, [readline termcap ncursesw ncurses curses], [
        AC_SEARCH_LIBS(readline, readline, [
            AC_CHECK_HEADER([readline/readline.h], [
                have_readline=yes
            ], [
                if [[ "$with_readline" = yes ]]; then
                    AC_MSG_ERROR([unable to use libreadline  - readline/readline.h not found or not compilable.])
                fi
            ])
        ], [
            if [[ "$with_readline" = yes ]]; then
                AC_MSG_ERROR([unable to link libreadline  - the library is not installed on this system.])
            fi
        ])
    ], [
        if [[ "$with_readline" = yes ]]; then
            AC_MSG_ERROR([unable to properly link libreadline - a library \
providing termcap functionality is not found. Try installing one of ncursew, \
ncurses, curses, termcap.])
        fi
    ])
    with_readline=$have_readline
fi

if [[ "$with_readline" = yes ]]; then
    AC_DEFINE(MONITOR_READLINE, 1, [Define to activate readline support in monitor.])
fi

dnl Check for availability of the OpenGL header.
AC_ARG_WITH(opengl,
    AC_HELP_STRING(--with-opengl,[Use OpenGL for graphics - only in SDL target @<:@default=check@:>@]),
    [],
    [with_opengl=check])

if [[ "$a8_target" != default -o "$with_video" != sdl ]]; then
    if [[ "$with_opengl" = "yes" ]]; then
        AC_MSG_ERROR([option --with-opengl only supported with --target=default --with-video=sdl!])
    fi
    with_opengl=no
fi

if [[ "$with_opengl" != no ]]; then
    have_opengl=no
    AC_CHECK_HEADER([SDL_opengl.h], [
        have_opengl=yes
    ], [
        if [[ "$with_opengl" = yes ]]; then
            AC_MSG_ERROR([unable to use OpenGL  - SDL_opengl.h not found or not compilable.])
        fi
    ])
    with_opengl=$have_opengl
fi

if [[ "$with_opengl" = "yes" ]]; then
    AC_DEFINE(HAVE_OPENGL, 1, [Support for OpenGL graphics acceleration.])
fi
AM_CONDITIONAL([WITH_OPENGL], test "$with_opengl" = "yes")


dnl Select host/target specific features...

if [[ "$a8_host" = "falcon" ]]; then
    A8_OPTION(falconcpuasm,no,
              [Use m68k assembler CPU core for Falcon target (default=OFF)],
              FALCON_CPUASM,[Define to use m68k assembler CPU core for Falcon target.]
             )
fi
AM_CONDITIONAL([WANT_FALCON_CPUASM], test "$WANT_FALCON_CPUASM" = "yes")

if [[ "$a8_host" = "linux" ]]; then
    case "$a8_target" in
        x11*)
            A8_OPTION(linuxjoystick,yes,
                      [Use LINUX joystick (linux x11 targets only) (default=ON)],
                      LINUX_JOYSTICK,[Define to use LINUX joystick.]
                      )
    esac
fi

if [[ "$a8_target" = "windx" ]]; then
    A8_OPTION(cursorblock,no,
              [Using cursor/ctrl keys for keyboard joystick (windx targets only) (default=OFF)],
              USE_CURSORBLOCK,[Define for using cursor/ctrl keys for keyboard joystick.]
             )
fi

if [[ "$WANT_XEP80_EMULATION" = "yes" ]]; then
    AC_DEFINE(XEP80_EMULATION,1,[Emulate the XEP80.])
fi
AM_CONDITIONAL([WANT_XEP80_EMULATION], test "$WANT_XEP80_EMULATION" = "yes")

if [[ "$WANT_NTSC_FILTER" = "yes" ]]; then
    AC_DEFINE(NTSC_FILTER,1,[Use NTSC video filter.])
fi
AM_CONDITIONAL([WANT_NTSC_FILTER], test "$WANT_NTSC_FILTER" = "yes")

if [[ "$WANT_PAL_BLENDING" = "yes" ]]; then
    AC_DEFINE(PAL_BLENDING,1,[Use accurate PAL color blending.])
fi
AM_CONDITIONAL([WANT_PAL_BLENDING], test "$WANT_PAL_BLENDING" = "yes")

if [[ "$SUPPORTS_RDEVICE" = "yes" ]]; then
    A8_OPTION(riodevice,yes,
              [Use the R: networking device (Linux/Unix/Win32) (default=ON)],
              R_IO_DEVICE,[Define to use R: device.]
             )
    if [[ "$WANT_R_IO_DEVICE" = "yes" ]]; then
        A8_OPTION(rnetwork,yes,
                 [Use IP network connection with the R: networking device (Linux/Unix/Win32) (default=ON)],
                 R_NETWORK,[Define to use IP network connection with the R: device.]
                )
        if [[ "$a8_host" != "win" -a "$a8_host" != "macos" ]]; then
            A8_OPTION(rserial,yes,
                     [Use the host serial port with the R: networking device (Linux/Unix only) (default=ON)],
                     R_SERIAL,[Define to use the host serial port with the R: device.]
                    )
        else 
            if [[ "$a8_host" = "win" ]]; then
                A8_NEED_LIB(ws2_32)
            fi
            WANT_R_SERIAL="no"
        fi
    fi
fi
AM_CONDITIONAL([WANT_R_IO_DEVICE], test "$WANT_R_IO_DEVICE" = "yes")

dnl Wrapup: export and write Makefile...

AC_SUBST(JAVAFLAGS)
AC_SUBST(JAVACFLAGS)
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)
CFLAGS_NOANSI=`echo $CFLAGS | sed -e 's/-ansi//' -e 's/-pedantic//'`
AC_SUBST(CFLAGS_NOANSI)

AC_CONFIG_FILES([
Makefile
DOC/Makefile
src/Makefile
])
if [[ "$a8_target" = "android" ]]; then
	AC_CONFIG_FILES(src/android/jni/Android.mk)
fi
AC_OUTPUT


dnl Print results of configure's work...

echo "-------------------------------------------------------"
echo "                 CONFIGURATION RESULTS:"
echo "-------------------------------------------------------"
echo "Host OS...............................: $a8_host"
echo "Target ...............................: $a8_target"
echo
if [[ "$a8_target" = default ]]; then
    echo "Interface for video...................: $with_video"
    case "$with_video" in
        *curses)
            echo "Using no bitmap graphics emulation?...: $WANT_CURSES_BASIC"
            ;;
    esac
fi
if [[ "$a8_target" != "default" -o "$with_video" != no ]]; then
    if [[ "$WANT_CURSES_BASIC" != "yes" ]]; then
        echo "Using cycle exact?....................: $WANT_NEW_CYCLE_EXACT"
        echo "Using the very slow computer support?.: $WANT_VERY_SLOW"
    fi
    echo "Using the crash menu?.................: $WANT_CRASH_MENU"
fi
echo "Using the paged attribute array?......: $WANT_PAGED_ATTRIB"
echo "Using per opcode cycles update?.......: $WANT_CYCLES_PER_OPCODE"
echo "Using the buffered log?...............: $WANT_BUFFERED_LOG"
echo "Using Altirra BIOS ROM?...............: $WANT_EMUOS_ALTIRRA"
echo "Using the monitor assembler?..........: $WANT_MONITOR_ASSEMBLER"
echo "Using code breakpoints and history?...: $WANT_MONITOR_BREAK"
echo "Using user-defined breakpoints?.......: $WANT_MONITOR_BREAKPOINTS"
echo "Using monitor hints?..................: $WANT_MONITOR_HINTS"
echo "Using 6502 opcode profiling?..........: $WANT_MONITOR_PROFILE"
echo "Using TRACE monitor command?..........: $WANT_MONITOR_TRACE"
echo "Using readline support in monitor?....: $with_readline"
echo "Using UTF-8 support in monitor?.......: $WANT_MONITOR_UTF8"
echo "Using ANSI color support in monitor?..: $WANT_MONITOR_ANSI"
echo "Using event recording?................: $WANT_EVENT_RECORDING"
echo "Using MIO emulation?..................: $WANT_PBI_MIO"
echo "Using Black Box emulation?............: $WANT_PBI_BB"
echo "Using IDE emulation?..................: $WANT_IDE"
echo "Using Pokey registers recording?......: $WANT_POKEYREC"
echo "Interface for sound...................: $with_sound"
if [[ "$with_sound" != no ]]; then
    echo "    Using nonlinear mixing?...........: $WANT_NONLINEAR_MIXING"
    echo "    Using synchronized sound?.........: $WANT_SYNCHRONIZED_SOUND"
    echo "    Using sound interpolation?........: $WANT_INTERPOLATE_SOUND"
    echo "    Using stereo sound?...............: $WANT_STEREO_SOUND"
    echo "    Using volume only sound?..........: $WANT_VOL_ONLY_SOUND"
    if [[ "$WANT_VOL_ONLY_SOUND" = "yes" ]]; then
        echo "        Using console sound?..........: $WANT_CONSOLE_SOUND"
        echo "        Using serial I/O sound?.......: $WANT_SERIO_SOUND"
    else
        echo "        (Volume only sound sub-options disabled)"
    fi
    echo "    Using 1400XL/1450XLD emulation?...: $WANT_PBI_XLD"
    echo "    Using sound clipping?.............: $WANT_CLIP_SOUND"
else
    echo "    (Sound sub-options disabled)"
fi
if [[ "$a8_host" = "falcon" ]]; then
    echo "Using M68K assembler CPU core?........: $WANT_FALCON_CPUASM"
fi
if [[ "$a8_host" = "linux" ]]; then
    case "$a8_target" in
        x11*)
            echo "Using Linux joystick?.................: $WANT_LINUX_JOYSTICK"
    esac
fi
if [[ "$a8_target" = "windx" ]]; then
    echo "Using cursor/ctrl keys?...............: $WANT_USE_CURSORBLOCK"
fi
if [[ "$SUPPORTS_RDEVICE" = "yes" ]]; then
    echo "Using R: device?......................: $WANT_R_IO_DEVICE"
        if [[ "$a8_host" != "win" -a "$a8_host" != "macos" -a "$WANT_R_IO_DEVICE" = "yes" ]]; then
            echo "    Using R: with the host serial port?...: $WANT_R_SERIAL"
            echo "    Using R: with IP network support......: $WANT_R_NETWORK"
        fi
fi
if [[ "$a8_target" = default -a "$with_video" = sdl ]]; then
    echo "Using on-screen keyboard?.............: $WANT_USE_UI_BASIC_ONSCREEN_KEYBOARD"
    echo "Using OpenGL?.........................: $with_opengl"
fi
echo
echo "Main build variables:"
echo "    CC......: \"$CC\""
echo "    ASFLAGS.: \"$CCASFLAGS\""
echo "    CFLAGS..: \"$CFLAGS\""
echo "    CPPFLAGS: \"$CPPFLAGS\""
echo "    LDFLAGS.: \"$LDFLAGS\""
echo "    LIBS....: \"$LIBS\""
echo "-------------------------------------------------------"

if [[ "$WANT_VERY_SLOW" = "yes" ]]; then
	echo
	echo "IMPORTANT: --enable-veryslow provides a bit better performance"
	echo "only when the compiled atari800 is run with the -refresh option."
	if [[ "$WANT_FALCON_CPUASM" != "yes" ]]; then
		if [[ "$WANT_MONITOR_BREAK" = "yes" ]]; then
			echo "It is better to use --disable-monitorbreak."
		fi
		if [[ "$with_sound" != "no" ]]; then
			echo "Try --without-sound, too."
		fi
		if [[ "$WANT_PAGED_ATTRIB" != "yes" ]]; then
			echo "Have you tried --enable-pagedattrib ?"
		fi
		if [[ "$WANT_CYCLES_PER_OPCODE" != "yes" ]]; then
			echo "Have you tried --enable-cyclesperopcode ?"
		fi
	fi
fi

echo
echo "Now run \"gmake\" or \"make\"."
echo
