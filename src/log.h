#ifndef _LOG_H_
#define _LOG_H_

#define LOG_BUFFER_SIZE 8192
extern char Log_buffer[LOG_BUFFER_SIZE];

void Log_print(char *format, ...);
void Log_flushlog(void);

#endif /* _LOG_H_ */
