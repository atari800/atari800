/* $Id$ */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"
#include "log.h"

char memory_log[MAX_LOG_SIZE]="";

void Aprint(char *format, ... )
{
	va_list args;
	char buffer[256];
#ifdef BUFFERED_LOG
	int buflen;
#endif

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

#ifdef BUFFERED_LOG
	strcat(buffer, "\n");
	buflen = strlen(buffer);

	if ((strlen(memory_log) + strlen(buffer) + 1) > MAX_LOG_SIZE)
		*memory_log = 0;

	strcat(memory_log, buffer);
#else
	printf("%s\n", buffer);
#endif
}

#ifdef BUFFERED_LOG
void Aflushlog(void)
{
	if (*memory_log) {
		printf(memory_log);
		*memory_log = 0;
	}
}
#endif

/*
$Log$
Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
