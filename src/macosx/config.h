/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader 2.13.  */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
/* #undef STDC_HEADERS */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#define WORDS_BIGENDIAN 1

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
/* #undef HAVE_FCNTL_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <strings.h> header file.  */
/* #undef HAVE_STRINGS_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ioctl.h> header file.  */
/* #undef HAVE_SYS_IOCTL_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/time.h> header file.  */
/* #undef HAVE_SYS_TIME_H */

/* Define if you have the <unistd.h> header file.  */
/* #undef HAVE_UNISTD_H */

/* Define if you have the X11 library (-lX11).  */
/* #undef HAVE_LIBX11 */

/* Define if you have the Xext library (-lXext).  */
/* #undef HAVE_LIBXEXT */

/* Define if you have the Xm library (-lXm).  */
/* #undef HAVE_LIBXM */

/* Define if you have the Xt library (-lXt).  */
/* #undef HAVE_LIBXT */

/* Define if you have the curses library (-lcurses).  */
/* #undef HAVE_LIBCURSES */

/* Define if you have the ddraw library (-lddraw).  */
/* #undef HAVE_LIBDDRAW */

/* Define if you have the dinput library (-ldinput).  */
/* #undef HAVE_LIBDINPUT */

/* Define if you have the dsound library (-ldsound).  */
/* #undef HAVE_LIBDSOUND */

/* Define if you have the dxguid library (-ldxguid).  */
/* #undef HAVE_LIBDXGUID */

/* Define if you have the gdi32 library (-lgdi32).  */
/* #undef HAVE_LIBGDI32 */

/* Define if you have the gen library (-lgen).  */
/* #undef HAVE_LIBGEN */

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM 1

/* Define if you have the ncurses library (-lncurses).  */
/* #undef HAVE_LIBNCURSES */

/* Define if you have the olgx library (-lolgx).  */
/* #undef HAVE_LIBOLGX */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the vga library (-lvga).  */
/* #undef HAVE_LIBVGA */

/* Define if you have the winmm library (-lwinmm).  */
/* #undef HAVE_LIBWINMM */

/* Define if you have the xview library (-lxview).  */
/* #undef HAVE_LIBXVIEW */

/* Define if you have the z library (-lz).  */
#define HAVE_LIBZ 1

/* #undef BASIC */

/* #undef CURSES */

/* #undef SVGALIB */

/* #undef NCURSES */

#ifndef WIN32
/* # undef WIN32 */
#endif

/* #undef DIRECTX */

/* #undef X11 */

/* #undef XVIEW */

/* #undef MOTIF */

/* #undef SHM */

/* dos vga support */
/* #undef VGA */

/* define to enable dos style drives support */
/* #undef DOS_DRIVES */

/* define to use back slash */
/* #undef BACK_SLASH */

/* define if unaligned long access is ok */
#define UNALIGNED_LONG_OK 1

/* Use m68k assembler optimized CPU core */
/* #undef CPUASS */

/* Is your computer very slow (disables generating screen completely) */
/* #undef VERY_SLOW */

/*  Show crash menu after CIM instruction */
#define CRASH_MENU 1

/* Enable BREAK command in monitor (slows CPU a little bit) */
/* #undef MONITOR_BREAK */

/* Enable hints in disassembler (addresses get human readable labels) */
#define MONITOR_HINTS 1

/* Enable assembler in monitor (you can enter ASM insns directly) */
/* #undef MONITOR_ASSEMBLER */

/* Enable Snailmeter (shows how much is the emulator slower than original) */
/* #define SNAILMETER */

/* define to use clock() instead of gettimeofday() */
/* #undef USE_CLOCK */

/* Draw every 1/50 sec only 1/refresh of screen */
/* #undef SVGA_SPEEDUP */

/* Use cursor keys/Ctrl for keyboard joystick */
/* #undef USE_CURSORBLOCK */

/* Support for Toshiba Joystick Mouse (Linux SVGALIB Only) */
/* #undef JOYMOUSE */

/* Run atari800 as Linux realtime process */
/* #undef REALTIME */

/* Enable LINUX Joystick */
/* #undef LINUX_JOYSTICK */

/* define to enable sound */
#define SOUND 1

/* define to disable volume only sound (digitized sound effects)) */
/* #define NO_VOL_ONLY */

/* define to disable console sound (keyboard clicks) */
/* #undef NO_CONSOL_SOUND */

/* define to enable serial in/out sound */
#define SERIO_SOUND

/* define to disable sound interpolation */
/* #define NOSNDINTER */

/* define to enable sound clipping */
/* #define CLIP */

/* define to enable stereo sound */
#define STEREO

/* Buffer debug output (until the graphics mode switches back to text mode) */
#define BUFFERED_LOG 1

/* Emulate disk LED diode */
/* #define SET_LED */

/* Display LED on screen */
/* #define NO_LED_ON_SCREEN */

/* color change inside a scanline */
/* #undef NO_CYCLE_EXACT */

/* Use Signed Samples in POKEY emulation */
#define SIGNED_SAMPLES

/* Don't use command line configuration update */
#define DONT_USE_RTCONFIGUPDATE

/* Change name confict with monitor() function */
#define monitor Atari_monitor
