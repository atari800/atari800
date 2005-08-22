#ifndef CONFIG_H
#define CONFIG_H

#define HAVE_UNISTD_H
#define HAVE_GETTIMEOFDAY

#define MONITOR_BREAK

#define DEFAULT_CFG_NAME "PROGDIR:Atari800.cfg"

#define SUPPORTS_ATARI_CONFIGINIT
#define SUPPORTS_ATARI_CONFIGSAVE
#define SUPPORTS_ATARI_CONFIGURE
#define DONT_USE_RTCONFIGUPDATE

#define WORDS_BIGENDIAN

//#define SIGNED_SAMPLES
/*#define STEREO*/
/*#define VERY_SLOW*/
/*#define SET_LED*/

#define SOUND
#define VOL_ONLY_SOUND
#define INTERPOLATE_SOUND
#define CONSOLE_SOUND
#define SERIO_SOUND

#define USE_NEW_BINLOAD
/*#define NO_GOTO*/

#define DO_DIR
#define NEW_CYCLE_EXACT

void usleep(unsigned long usec);

#endif
