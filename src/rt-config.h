#ifndef __RT_CONFIG
#define	__RT_CONFIG

#define MAX_FILENAME_LEN 256
#define MAX_DIRECTORIES 8

extern char atari_osa_filename[MAX_FILENAME_LEN];
extern char atari_osb_filename[MAX_FILENAME_LEN];
extern char atari_xlxe_filename[MAX_FILENAME_LEN];
extern char atari_basic_filename[MAX_FILENAME_LEN];
extern char atari_5200_filename[MAX_FILENAME_LEN];
extern char atari_disk_dirs[MAX_DIRECTORIES][MAX_FILENAME_LEN];
extern char atari_rom_dir[MAX_FILENAME_LEN];
extern char atari_h1_dir[MAX_FILENAME_LEN];
extern char atari_h2_dir[MAX_FILENAME_LEN];
extern char atari_h3_dir[MAX_FILENAME_LEN];
extern char atari_h4_dir[MAX_FILENAME_LEN];
extern char atari_exe_dir[MAX_FILENAME_LEN];
extern char atari_state_dir[MAX_FILENAME_LEN];
extern char print_command[256];
extern int hd_read_only;
extern int refresh_rate;
extern int default_system;
extern int default_tv_mode;
extern int hold_option;
extern int enable_c000_ram;
extern int enable_sio_patch;
extern int enable_h_patch;
extern int enable_p_patch;
extern int disk_directories;

int RtConfigLoad(char *rtconfig_filename);
void RtConfigSave(void);
void RtConfigUpdate(void);

#endif
