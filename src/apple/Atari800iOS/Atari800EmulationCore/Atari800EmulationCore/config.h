/* config.h.  */

extern void ControlManagerMessagePrint(char *);

#define SUPPORTS_CHANGE_VIDEOMODE 0
#define PAL_BLENDING 1
#define PACKAGE_VERSION "1.3.6"
#define COLUMN_80 1
#define MACOSX
#undef HAVE_SYSTEM
#define SOUND_THIN_API 1
#undef SOUND_CALLBACK
#undef SYNCHRONIZED_SOUND
#define STEREO
#define STEREO_SOUND

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP

/* Define if you have the fdopen function.  */
#define HAVE_FDOPEN

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

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

/* Define if you have localtime function */
#define HAVE_TIME
#define HAVE_LOCALTIME 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#ifdef __BIG_ENDIAN__   
#define WORDS_BIGENDIAN 1
#else
#undef WORDS_BIGENDIAN
#endif

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the mircosleep function.  */
#define HAVE_USLEEP 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the strncpy function */
#define HAVE_STRNCPY 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Define if you have the function rename.  */
#define HAVE_RENAME 1

/* Define if you have the function unlink.  */
#define HAVE_UNLINK 1

/* Define if you have the function opendir  */
#define HAVE_OPENDIR 1

/* Define if you have the function mkdir  */
#define HAVE_MKDIR 1

/* Define if you have the function mkdir  */
#define HAVE_RMDIR 1

/* Define if you have the function fstat  */
#define HAVE_FSTAT 1

/* Define if you have the function stat  */
#define HAVE_STAT 1

/* Define if you have the function chmod  */
#define HAVE_CHMOD 1

/* Define if you have the function rewind  */
#define HAVE_REWIND 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

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
#define HAVE_SYS_TIME_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H */

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
#define WORDS_UNALIGNED_OK 1

/* Use m68k assembler optimized CPU core */
/* #undef CPUASS */

/* Is your computer very slow (disables generating screen completely) */
/* #undef VERY_SLOW */

/*  Show crash menu after CIM instruction */
#undef CRASH_MENU 

/* Enable BREAK command in monitor (slows CPU a little bit) */
#define MONITOR_BREAK

/* Enable BREAKPOINTS in monitor (slows CPU a little bit) */
#define MONITOR_BREAKPOINTS

/* Enable hints in disassembler (addresses get human readable labels) */
#define MONITOR_HINTS 1

/* Enable assembler in monitor (you can enter ASM insns directly) */
#define MONITOR_ASSEMBLER */

/* Enable tracing of cpu in monitor */
#define MONITOR_TRACE */

/* Enable Snailmeter (shows how much is the emulator slower than original) */
#define SNAILMETER

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
#define SOUND_GAIN 1

/* define to enable volume only sound (digitized sound effects)) */
#define VOL_ONLY_SOUND 

/* define to enable console sound (keyboard clicks) */
#define CONSOLE_SOUND

/* define to enable serial in/out sound */
#define SERIO_SOUND

/* define to enable sound interpolation */
#define INTERPOLATE_SOUND

/* define to enable sound clipping */
/* #define CLIP_SOUND */

/* define to enable stereo sound */
#define STEREO
#define STEREO_SOUND
#define SYNCHRONIZED_SOUND


/* Buffer debug output (until the graphics mode switches back to text mode) */
/* #define BUFFERED_LOG 1 */

/* Display LED on screen */
#define SHOW_DISK_LED */

/* color change inside a scanline */
#define CYCLE_EXACT
#define NEW_CYCLE_EXACT

/* Use Signed Samples in POKEY emulation */
#define SIGNED_SAMPLES

/* Define for Paged Attribute Memory simulation */
#undef PAGED_ATTRIB

/* Define for R: device simulation */
#define R_IO_DEVICE
#undef R_SERIAL
#define R_NETWORK

/* Define for D: device harddisk simulation */
#define D_PATCH	

/* Don't use command line configuration update */
#define DONT_USE_RTCONFIGUPDATE

/* Define two screen arrays used for switching */
#define BITPL_SCR

/* Change name confict with monitor() function */
#define monitor Atari_monitor

#define XEP80_EMULATION

#define PBI_MIO 

#define PBI_BB
