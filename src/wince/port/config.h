/* Config header for Windows CE port. */

/* Define to empty if the keyword does not work.  */
#undef const

/* Define if you don't have vprintf but do have _doprnt.  */
#undef HAVE_DOPRNT

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF

/* Define as __inline if that's what the C compiler calls it.  */
#undef inline

/* Define as the return type of signal handlers (int or void).  */
#undef RETSIGTYPE

/* Define if you have the ANSI C header files.  */
#undef STDC_HEADERS

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#undef TIME_WITH_SYS_TIME

/* Define if your <sys/time.h> declares struct tm.  */
#undef TM_IN_SYS_TIME

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#undef WORDS_BIGENDIAN

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* Define if you have the getcwd function.  */
#undef HAVE_GETCWD

/* Define if you have the gettimeofday function.  */
#undef HAVE_GETTIMEOFDAY

/* Define if you have the select function.  */
#undef HAVE_SELECT

/* Define if you have the strdup function.  */
#define HAVE_STRDUP

/* Define if you have the strerror function.  */
#undef HAVE_STRERROR

/* Define if you have the strstr function.  */
#define HAVE_STRSTR

/* Define if you have the strtol function.  */
#define HAVE_STRTOL

/* Define if you have the <dirent.h> header file.  */
#undef HAVE_DIRENT_H

/* Define if you have the <fcntl.h> header file.  */
#undef HAVE_FCNTL_H

/* Define if you have the <ndir.h> header file.  */
#undef HAVE_NDIR_H

/* Define if you have the <strings.h> header file.  */
#undef HAVE_STRINGS_H

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/ioctl.h> header file.  */
#undef HAVE_SYS_IOCTL_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the X11 library (-lX11).  */
#undef HAVE_LIBX11

/* Define if you have the Xext library (-lXext).  */
#undef HAVE_LIBXEXT

/* Define if you have the Xm library (-lXm).  */
#undef HAVE_LIBXM

/* Define if you have the Xt library (-lXt).  */
#undef HAVE_LIBXT

/* Define if you have the curses library (-lcurses).  */
#undef HAVE_LIBCURSES

/* Define if you have the ddraw library (-lddraw).  */
#undef HAVE_LIBDDRAW

/* Define if you have the dinput library (-ldinput).  */
#undef HAVE_LIBDINPUT

/* Define if you have the dsound library (-ldsound).  */
#undef HAVE_LIBDSOUND

/* Define if you have the dxguid library (-ldxguid).  */
#undef HAVE_LIBDXGUID

/* Define if you have the gdi32 library (-lgdi32).  */
#undef HAVE_LIBGDI32

/* Define if you have the gen library (-lgen).  */
#undef HAVE_LIBGEN

/* Define if you have the m library (-lm).  */
#undef HAVE_LIBM

/* Define if you have the ncurses library (-lncurses).  */
#undef HAVE_LIBNCURSES

/* Define if you have the olgx library (-lolgx).  */
#undef HAVE_LIBOLGX

/* Define if you have the socket library (-lsocket).  */
#undef HAVE_LIBSOCKET

/* Define if you have the vga library (-lvga).  */
#undef HAVE_LIBVGA

/* Define if you have the winmm library (-lwinmm).  */
#undef HAVE_LIBWINMM

/* Define if you have the xview library (-lxview).  */
#undef HAVE_LIBXVIEW

/* Define if you have the z library (-lz).  */
#undef HAVE_LIBZ

#undef BASIC

#undef CURSES

#undef NCURSES

#undef WIN32

#undef DIRECTX

#undef X11

#undef XVIEW

#undef MOTIF

#undef SHM

#define WINCE

/* dos vga support */
#undef VGA

/* define to enable dos style drives support */
#undef DOS_DRIVES

/* define to use back slash */
#define BACK_SLASH

/* define if unaligned long access is ok */
#undef UNALIGNED_LONG_OK

/* Is your computer very slow (disables generating screen completely) */
#undef VERY_SLOW

/*  Show crash menu after CIM instruction */
#define CRASH_MENU

/* Enable BREAK command in monitor (slows CPU a little bit) */
#undef MONITOR_BREAK

/* Enable hints in disassembler (addresses get human readable labels) */
#undef MONITOR_HINTS

/* Enable assembler in monitor (you can enter ASM insns directly) */
#undef MONITOR_ASSEMBLER

/* Compile internal palettes */
#define COMPILED_PALETTE

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
#define SOUND

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
#define NO_LED_ON_SCREEN

/* This one is particularly ugly */
#define NO_GOTO

/* Trying to get better synchronization */
//#define USE_CLOCK
#define CLK_TCK 1000
//#define DONT_SYNC_WITH_HOST

#define DIRTYRECT
//#define NODIRTYCOMPARE

#define NO_YPOS_BREAK_FLICKER

#define DONT_USE_RTCONFIGUPDATE

/* Some port-specific workarounds */
#define strcasecmp _stricmp

#define perror(s) wce_perror(s)
void wce_perror(char* s);

#ifndef _FILE_DEFINED
   typedef void FILE;
   #define _FILE_DEFINED
#endif
FILE* __cdecl wce_fopen(const char* fname, const char* fmode);
#define fopen wce_fopen

/* Those are somewhat Unix-specific... */
#define S_IRUSR 1
#define S_IWUSR 2
#define S_IXUSR 2
#define S_IRGRP 1
#define S_IWGRP 2
#define S_IXGRP 2
#define S_IROTH 1
#define S_IWOTH 2
#define S_IXOTH 2

char* getenv(const char* varname);
int rename(const char* oldname, const char* newname);
int chmod(const char* name, int mode);
int fstat(int handle, struct stat* buffer);
int mkdir(const char* name);
int rmdir(const char* name);
int umask(int pmode);

/* truncation from int to char */
#pragma warning(disable:4305)
