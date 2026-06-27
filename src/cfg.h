#ifndef CFG_H_
#define CFG_H_

/* Load Atari800 text configuration file. */
int CFG_LoadConfig(const char *alternate_config_filename);

/* Writes Atari800 text configuration file. */
int CFG_WriteConfig(void);

/* Controls whether the configuration file will be saved on emulator exit. */
extern int CFG_save_on_exit;

/* Directory of the config file (or HOME if default config). Set by CFG_LoadConfig(). */
extern char CFG_data_dir[FILENAME_MAX];

/* Compares the string PARAM with each entry in the CFG_STRINGS array
   (of size CFG_STRINGS_SIZE), and returns index under which PARAM is found.
   If PARAM does not exist in CFG_STRINGS, returns value lower than 0.
   String comparison is case-insensitive. */
int CFG_MatchTextParameter(char const *param, char const * const cfg_strings[], int cfg_strings_size);

/* If another default path config path is defined use it otherwise use the default one */
#ifndef DEFAULT_CFG_NAME
#define DEFAULT_CFG_NAME ".atari800.cfg"
#endif

#endif /* CFG_H_ */
