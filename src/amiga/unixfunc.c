/* Only Requiered for the Maxon Compiler */

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

