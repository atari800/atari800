#include "config.h"

#define MAX_LOG_SIZE		8192
extern char	memory_log[MAX_LOG_SIZE];
void Aprint(char *format, ... );
#ifdef BUFFERED_LOG
void Aflushlog(void);
#endif
