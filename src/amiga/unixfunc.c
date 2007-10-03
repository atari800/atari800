/*
 * unixfunc.c - only required for the Maxon compiler
 *
 * Copyright (c) 2000 Sebastian Bauer
 * Copyright (c) 2000-2003 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef __MAXON__

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <pragma/dos_lib.h>
#include <pragma/exec_lib.h>

int __open(const char *name, int mode,...)
{
  BPTR fh;

  if( mode & O_RDONLY )
  {
    fh = Open((STRPTR)name,MODE_OLDFILE);
  } else
  {
    if(mode & O_WRONLY)
    {
      fh = Open((STRPTR)name,MODE_NEWFILE);
    } else fh = Open((STRPTR)name, MODE_OLDFILE);
  }

  if(!fh) fh = (BPTR)-1;

  return (int)fh;
}

int __close(int fh)
{
  if(fh && fh != -1 ) Close((BPTR)fh);
  return 0;
}

int __write(int fh, const void *buffer, unsigned int length)
{
  return Write((BPTR)fh,(APTR)buffer,length);
}

int __read(int fh, void *buffer, unsigned int length)
{
  int count;

  if (fh == -1) return 0;

  count = Read((BPTR)fh,buffer,length);
/*  if(!count) count = - 1;*/
  return count;
}

int unlink(const char *name)
{
  DeleteFile((STRPTR)name);
  return 0;
}

long lseek(int fh, long rpos, int mode)
{
  long origin = mode;
  Seek((BPTR)fh,rpos,origin);

  return Seek((BPTR)fh,0,OFFSET_CURRENT);
}

char *strdup(const char *s)
{
  char *p = malloc(strlen(s)+1);
  if(p)
  {
    strcpy(p,s);
  }
  return p;
}

char *getcwd(char *b, int size)
{
  struct Process *p = (struct Process*)FindTask(NULL);
  NameFromLock(p->pr_CurrentDir, b, size);
  return b;
}

int stat()
{
  return -1;
}

int readdir()
{
  return -1;
}

int closedir()
{
  return -1;
}

int opendir()
{
  return -1;
}


#endif

