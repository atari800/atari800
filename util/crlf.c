/* crlf.c 1.3 by entropy@terminator.rs.itd.umich.edu

   PUBLIC DOMAIN -- NO RIGHTS RESERVED
   NO WARRANTY -- USE AT YOUR OWN RISK!!!!!!!!!
   strips/adds carriage returns from text files
*/
#ifndef __atarist__
#ifdef __TOS__   /* #defined by TurboC / PureC */
  #define __atarist__
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#define UNUSED(x)
#ifdef __atarist__
#include <compiler.h>
#ifdef __TURBOC__
#include <sys\types.h>
#include <sys\stat.h>
#undef UNUSED
#define UNUSED(x) ((void)(x))
#else /* not __TURBOC__ */
#include <sys/types.h>
#include <sys/stat.h>
#endif /* not __TURBOC__ */
#else /* not __atarist__ */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef sun
#include <utime.h>
#endif /* sun */
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 2
#endif /* EXIT_FAILURE */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif /* EXIT_SUCCESS */
#ifdef __STDC__
#ifndef __NO___PROTO__
#define __PROTO(x) x
#endif /* not __NO___PROTO__ */
#define __EXTERN
#else /* not __STDC__ */
#define __EXTERN extern
#define __PROTO(x) ()
#endif /* not __STDC__ */
#endif /* not __atarist__ */

extern char *optarg;
extern int optind, opterr;

#define CR_STRIP 0
#define CR_ADD 1
#define CR_NONE 2

char *program = NULL;
int verbose = 0;

void perror2 __PROTO((char *msg1, char *msg2));
int crlf __PROTO((char *filename, int mode));
int simple_outchar __PROTO((FILE *ofp, int c));
int tounx_outchar __PROTO((FILE *ofp, int c));
int totos_outchar __PROTO((FILE *ofp, int c));
int smart_copy __PROTO((char *ifn, char *ofn,
                      int (*outcharfunc)(FILE *, int)));

void
perror2(msg1, msg2)
  char *msg1;
  char *msg2;
{
  if (msg1 && *msg1)
  {
    fputs(msg1, stderr);
    fputs(": ", stderr);
  }
  perror(msg2);
}

int
simple_outchar(ofp, c)
  FILE *ofp;
  int c;
{
  fputc(c, ofp);
  return (c);
}

int
totos_outchar(ofp, c)
  FILE *ofp;
  int c;
{
  if (c == '\r' ) /* first strip all CRs... */
    return c;
  if (c == '\n')
  {
    fputc('\r', ofp);
  }
  fputc(c, ofp);
  return (c);
}

int
tounx_outchar(ofp, c)
  FILE *ofp;
  int c;
{
  if (c != '\r')
  {
    fputc(c, ofp);
  }
  return (c);
}

int
smart_copy(ifn, ofn, outcharfunc)
  char *ifn;
  char *ofn;
  int (*outcharfunc) __PROTO((FILE *, int));
{
  FILE *ifp;
  FILE *ofp;
  int c;

  if (*ifn) 
  {
    if ((ifp = fopen(ifn, "rb")) == NULL)
    {
      perror2(program, ifn);
      return (-1);
    }
  }
  else
  {
    ifp = stdin;
  }
  if (*ofn) 
  {
    if ((ofp = fopen(ofn, "wb")) == NULL)
    {
      fclose(ifp);
      perror2(program, ofn);
      return (-1);
    }
  }
  else
  {
    ofp = stdout;
  }
  setvbuf(ofp, NULL, _IOFBF, 50*1024L);
  setvbuf(ifp, NULL, _IOFBF, 50*1024L);
  c = fgetc(ifp);
  while (!(feof(ifp)) && !(ferror(ifp)) && !(ferror(ofp)))
  {
    (*outcharfunc)(ofp, c);
    c = fgetc(ifp);
  }
  if (ferror(ifp))
  {
    perror2(program, ifn);
    if (*ifn)
    {
      fclose(ifp);
    }
    if (*ofn)
    {
      fclose(ofp);
    }
    return (-1);
  }
  if (ferror(ofp))
  {
    perror2(program, ofn);
    if (*ifn)
    {
      fclose(ifp);
    }
    if (*ofn)
    {
      fclose(ofp);
    }
    return (-1);
  }
  if (*ifn && fclose(ifp) == EOF)
  {
    perror2(program, ifn);
    if (*ofn)
    {
      fclose(ofp);
    }
    return (-1);
  }
  if (*ofn && fclose(ofp) == EOF)
  {
    perror2(program, ofn);
    return (-1);
  }
  return (0);
}

int
crlf(filename, mode)
  char *filename;
  int mode; /* 0 == strip CR's, 1 == add CR's */
{
  char tempname[FILENAME_MAX];
  char *ev;
  int (*outcharfunc) __PROTO((FILE *, int));

  switch (mode)
  {
    case CR_ADD:
      outcharfunc = totos_outchar;
      break;
    case CR_STRIP:
    default:
      outcharfunc = tounx_outchar;
      break;
  }
  if (verbose)
  {
    fputs(filename, stderr);
    fputc('\n', stderr);
  }
  if (*filename)
  {
    *tempname = '\0';
    if ((ev = getenv("TEMP")) || (ev = getenv("TMP"))
        || (ev = getenv("TMPDIR")))
    {
      strcpy(tempname, ev);
      strcat(tempname, "/");
    }
    strcat(tempname, "crlfXXXX");
    if (mktemp(tempname) == NULL)
    {
      fputs(program, stderr);
      fputs(": could not get a temporary filename\n", stderr);
      return (-1);
    }
    if (smart_copy(filename, tempname, outcharfunc))
    {
      return (-1);
    }
    if (rename(tempname, filename))
    {
      if (smart_copy(tempname, filename, simple_outchar))
      {
        return (-1);
      }
      if (unlink(tempname))
      {
        perror2(program, tempname);
        return (-1);
      }
    }
  }
  else
  {
    if (smart_copy("", "", outcharfunc))
    {
      return (-1);
    }
  }
  return (0);
}

int
main(int argc, char *argv[], char *envp[])
{
  char *fn = NULL;
  int mode = CR_NONE;
  int c;
  int err = 0;
  struct stat st;
  struct utimbuf ut;

  UNUSED(envp);
  program = argv[0];
  while ((c = getopt(argc, argv, "asv")) != EOF)
  {
    switch (c)
    {
      case 'a':
        if (mode == CR_NONE)
        {
          mode = CR_ADD;
        }
        else
        {
          err++;
        }
        break;
      case 's':
        if (mode == CR_NONE)
        {
          mode = CR_STRIP;
        }
        else
        {
          err++;
        }
        break;
      case 'v':
        verbose = 1;
        break;
      case '?':
      default:
        err++;
        break;
    }
  }
  if (mode == CR_NONE)
  {
    err++;
  }
  if (err)
  {
    fputs("usage:  ", stderr);
    fputs(program, stderr);
    fputs(" -s[v] [file [file2 [...]]] (to strip carriage returns)\n", stderr);
    fputs("        ", stderr);
    fputs(program, stderr);
    fputs(" -a[v] [file [file2 [...]]] (to add carriage returns)\n", stderr);
    exit(EXIT_FAILURE);
  }
  if (optind == argc)
  {
    if (crlf("", mode) != 0)
    {
      err++;
    }
  }
  else
  {
    for ( ; optind < argc; optind++)
    {
      fn = argv[optind];
      if (access(fn, 4))
      {
        err++;
        perror2(program, fn);
        continue;
      }
      if (stat(fn, &st))
      {
        err++;
        perror2(program, fn);
      }
      if (!(S_ISREG(st.st_mode)))
      {
        if (verbose)
        {
          fputs("crlf:  ignoring non-regular file ", stderr);
          fputs(fn, stderr);
          fputc('\n', stdout);
        }
        continue;
      }
      ut.actime = st.st_atime;
      ut.modtime = st.st_mtime;
      if (crlf(fn, mode) != 0)
      {
        err++;
        continue;
      }
      if (utime(fn, &ut))
      {
        err++;
        perror2(program, fn);
      }
    }
  }
  if (err)
  {
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
  return (0);
}
