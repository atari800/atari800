/*
 * log.c - A logging facility for debugging
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"
#include "log.h"

#define MAX_LOG_SIZE		8192
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

void Aflushlog(void)
{
#ifdef BUFFERED_LOG
	if (*memory_log) {
		printf(memory_log);
		*memory_log = 0;
	}
#endif
}

/*
$Log$
Revision 1.5  2003/02/24 09:33:01  joy
header cleanup

Revision 1.4  2002/04/07 05:44:47  vasyl
Log buffer is completely hidden inside C file (no extern in header). This allows
log-less ports to save extra 8K.

Revision 1.3  2001/12/04 14:17:52  joy
Aflushlog() should be always available though it does nothing when BUFFERED_LOG is undefined

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
