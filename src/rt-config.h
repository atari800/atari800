#ifndef __RT_CONFIG
#define	__RT_CONFIG

#include <stdio.h>

#define MAX_DIRECTORIES 8

extern char atari_osa_filename[FILENAME_MAX];
extern char atari_osb_filename[FILENAME_MAX];
extern char atari_xlxe_filename[FILENAME_MAX];
extern char atari_basic_filename[FILENAME_MAX];
extern char atari_5200_filename[FILENAME_MAX];
extern char atari_disk_dirs[MAX_DIRECTORIES][FILENAME_MAX];
extern char atari_rom_dir[FILENAME_MAX];
extern char atari_h1_dir[FILENAME_MAX];
extern char atari_h2_dir[FILENAME_MAX];
extern char atari_h3_dir[FILENAME_MAX];
extern char atari_h4_dir[FILENAME_MAX];
extern char atari_exe_dir[FILENAME_MAX];
extern char atari_state_dir[FILENAME_MAX];
extern char print_command[256];
extern int hd_read_only;
extern int refresh_rate;
extern int disable_basic;
extern int enable_sio_patch;
extern int enable_h_patch;
extern int enable_p_patch;
extern int enable_new_pokey;
extern int stereo_enabled;
extern int disk_directories;

int RtConfigLoad(const char *alternate_config_filename);
void RtConfigSave(void);
void RtConfigUpdate(void);

#endif
