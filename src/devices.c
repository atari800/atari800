#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#include	"config.h"

#ifdef VMS
#include	<unixio.h>
#include	<file.h>
#else
#include	<fcntl.h>
#ifndef	AMIGA
#ifndef WIN32
#include	<unistd.h>
#endif
#endif
#endif

#define	DO_DIR

#ifdef AMIGA
#undef	DO_DIR
#endif

#ifdef VMS
#undef	DO_DIR
#endif

#ifdef WIN32
#include <io.h>
#include <sys/stat.h>
#include "windows.h"
#endif

#ifdef DO_DIR
#ifndef WIN32
#include	<dirent.h>
#endif
#endif

#ifdef BASIC
#ifdef SOUND
#include "sound.h"
#endif
static int current_column = 0;
static int row_offset = 0;
static int escape_char = 0;

#ifndef NO_TEXT_MODES
#include <term.h>
typedef enum { ATNORMAL=0, ATREVERSE=1, ATBOLD=2 } ATMode;
static ATMode current_mode = ATNORMAL;
void enter_mode(ATMode mode)
{
   char *str = NULL;
   switch(mode)
   {
      case ATNORMAL: break;
      case ATREVERSE: str = enter_reverse_mode; break;
      case ATBOLD: str = enter_bold_mode; break;
   }
   if(str) printf(str);
}
void exit_mode(ATMode mode)
{
   if(mode != ATNORMAL)
      printf(exit_attribute_mode);
}

void set_text_mode(ATMode new_mode)
{
   if(current_mode != new_mode)
   {
      exit_mode(current_mode);
      enter_mode(new_mode);
      current_mode = new_mode;
   }
}
#endif

#ifdef USE_GETCHAR
#define getkey getchar
#else
#ifdef DOS
/* include something for getch in DOS/Windows ??? */
#define getkey getch
#else
int getkey()
{
   char ch;
   return (read(STDIN_FILENO, &ch, 1) < 0)?-1:
      ch;
}
#endif
#endif
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "rt-config.h"
#include "log.h"
#include "binload.h"
#include "sio.h"

#ifdef CRASH_MENU
extern int crash_code;
extern UWORD crash_address;
extern UWORD crash_afterCIM;
extern void CrashMenu(UBYTE *screen, UBYTE cimcode, UWORD address);
#endif


#define	ICHIDZ	0x0020
#define	ICDNOZ	0x0021
#define	ICCOMZ	0x0022
#define	ICSTAZ	0x0023
#define	ICBALZ	0x0024
#define	ICBAHZ	0x0025
#define	ICPTLZ	0x0026
#define	ICPTHZ	0x0027
#define	ICBLLZ	0x0028
#define	ICBLHZ	0x0029
#define	ICAX1Z	0x002a
#define	ICAX2Z	0x002b

static char *H[5] =
{
	".",
	atari_h1_dir,
	atari_h2_dir,
	atari_h3_dir,
	atari_h4_dir
};

static int devbug = FALSE;

static FILE *fp[8] =
{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static int flag[8];

static int fid;
static char filename[64];

#ifdef DO_DIR
#ifndef WIN32
static DIR	*dp = NULL;
#endif
#endif

char *strtoupper(char *str)
{
	char *ptr;
	for (ptr = str; *ptr; ptr++)
		*ptr = toupper(*ptr);

	return str;
}

void Device_Initialise(int *argc, char *argv[])
{
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-H1") == 0)
			H[1] = argv[++i];
		else if (strcmp(argv[i], "-H2") == 0)
			H[2] = argv[++i];
		else if (strcmp(argv[i], "-H3") == 0)
			H[3] = argv[++i];
		else if (strcmp(argv[i], "-H4") == 0)
			H[4] = argv[++i];
		else if (strcmp(argv[i], "-devbug") == 0)
			devbug = TRUE;
		else if (strcmp(argv[i], "-hdreadonly") == 0)
			hd_read_only = (argv[++i][0] != '0');
		else
			argv[j++] = argv[i];
	}

	*argc = j;
}

int Device_isvalid(char ch)
{
	int valid;

#ifdef WIN32
  if( isalnum(ch) && ch!=-101 )
#else
	if (isalnum(ch))
#endif
		valid = TRUE;
	else
		switch (ch) {
		case ':':
		case '.':
		case '_':
		case '*':
		case '?':
			valid = TRUE;
			break;
		default:
			valid = FALSE;
			break;
		}

	return valid;
}

void Device_GetFilename(void)
{
	int bufadr;
	int offset = 0;
	int devnam = TRUE;

	bufadr = (dGetByte(ICBAHZ) << 8) | dGetByte(ICBALZ);

	while (Device_isvalid(dGetByte(bufadr))) {
		int byte = dGetByte(bufadr);

		if (!devnam) {
			if (isupper(byte))
				byte = tolower(byte);

			filename[offset++] = byte;
		}
		else if (byte == ':')
			devnam = FALSE;

		bufadr++;
	}

	filename[offset++] = '\0';
}

int match(char *pattern, char *filename)
{
	int status = TRUE;

	while (status && *filename && *pattern) {
		switch (*pattern) {
		case '?':
			pattern++;
			filename++;
			break;
		case '*':
			if (*filename == pattern[1]) {
				pattern++;
			}
			else {
				filename++;
			}
			break;
		default:
			status = (*pattern++ == *filename++);
			break;
		}
	}
	if ((*filename)
		|| ((*pattern)
			&& (((*pattern != '*') && (*pattern != '?'))
				|| pattern[1]))) {
		status = 0;
	}
	return status;
}

void Device_HHOPEN(void)
{
	char fname[128];
	int devnum;
	int temp;

	if (devbug)
		Aprint("HHOPEN");

	fid = dGetByte(0x2e) >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	Device_GetFilename();
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9) {
		Aprint("Attempt to access H%d: device", devnum);
		return;
	}
	if (devnum >= 5) {
		flag[fid] = TRUE;
		devnum -= 5;
	}
	else {
		flag[fid] = FALSE;
	}

#ifdef VMS
/* Assumes H[devnum] is a directory _logical_, not an explicit directory
   specification! */
	sprintf(fname, "%s:%s", H[devnum], filename);
#else
#ifdef BACK_SLASH
	sprintf (fname, "%s\\%s", H[devnum], filename);
#else
	sprintf(fname, "%s/%s", H[devnum], filename);
#endif
#endif

	temp = dGetByte(ICAX1Z);

	switch (temp) {
	case 4:
		fp[fid] = fopen(fname, "rb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	case 6:
	case 7:
#ifdef DO_DIR
#ifndef WIN32
		fp[fid] = tmpfile();
		if (fp[fid]) {
			dp = opendir(H[devnum]);
			if (dp) {
				struct dirent *entry;

				while ((entry = readdir(dp))) {
					if (match(filename, entry->d_name))
						fprintf(fp[fid], "%s\n", strtoupper(entry->d_name));
				}

				closedir(dp);

				regY = 1;
				ClrN;

				rewind(fp[fid]);

				flag[fid] = TRUE;
			}
			else {
				regY = 163;
				SetN;
				fclose(fp[fid]);
				fp[fid] = NULL;
			}
		}
          else
#else	/* WIN32 DIR code */
		  fp[fid] = tmpfile ();
	  if( fp[fid] )
	  {
		  WIN32_FIND_DATA FindFileData;
		  HANDLE	hSearch;
		  char filesearch[ MAX_PATH ];
		  
		  strcpy( filesearch, H[devnum] );
		  strcat( filesearch, "\\*.*" );
		  
		  hSearch = FindFirstFile( filesearch, &FindFileData );
		  
		  if( hSearch )
		  {
			  FindNextFile( hSearch, &FindFileData );
			  
			  while( FindNextFile( hSearch, &FindFileData ) )
			  {
				  if( (match( filename, FindFileData.cFileName )) )
					  fprintf (fp[fid],"%s\n", FindFileData.cFileName );
			  }
			  
			  FindClose( hSearch );
			  regY = 1;
			  ClrN;
			  
			  rewind (fp[fid]);
			  
			  flag[fid] = TRUE;
		  }
		  else
		  {
			  regY = 163;
			  SetN;
			  fclose (fp[fid]);
			  fp[fid] = NULL;
		  }
	  }
	  else
#endif /* Win32 */
#endif /* DO_DIR */
		{
			regY = 163;
			SetN;
		}
		break;
	case 8:
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, "wb");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
		else {
			regY = 170;
			SetN;
		}
		break;
	default:
		regY = 163;
		SetN;
		break;
	}
}

void Device_HHCLOS(void)
{
	if (devbug)
		Aprint("HHCLOS");

	fid = dGetByte(0x2e) >> 4;

	if (fp[fid]) {
		fclose(fp[fid]);
		fp[fid] = NULL;
	}
	regY = 1;
	ClrN;
}

void Device_HHREAD(void)
{
	if (devbug)
		Aprint("HHREAD");

	fid = dGetByte(0x2e) >> 4;

	if (fp[fid]) {
		int ch;

		ch = fgetc(fp[fid]);
		if (ch != EOF) {
			if (flag[fid]) {
				switch (ch) {
				case '\n':
					ch = 0x9b;
					break;
				default:
					break;
				}
			}
			regA = ch;
			regY = 1;
			ClrN;
		}
		else {
			regY = 136;
			SetN;
		}
	}
	else {
		regY = 163;
		SetN;
	}
}

void Device_HHWRIT(void)
{
	if (devbug)
		Aprint("HHWRIT");

	fid = dGetByte(0x2e) >> 4;

	if (fp[fid]) {
		int ch;

		ch = regA;
		if (flag[fid]) {
			switch (ch) {
			case 0x9b:
				ch = 0x0a;
				break;
			default:
				break;
			}
		}
		fputc(ch, fp[fid]);
		regY = 1;
		ClrN;
	}
	else {
		regY = 163;
		SetN;
	}
}

void Device_HHSTAT(void)
{
	if (devbug)
		Aprint("HHSTAT");

	fid = dGetByte(0x2e) >> 4;

	regY = 146;
	SetN;
}

void Device_HHSPEC(void)
{
	if (devbug)
		Aprint("HHSPEC");

	fid = dGetByte(0x2e) >> 4;

	switch (dGetByte(ICCOMZ)) {
	case 0x20:
		Aprint("RENAME Command");
		break;
	case 0x21:
		Aprint("DELETE Command");
		break;
	case 0x23:
		Aprint("LOCK Command");
		break;
	case 0x24:
		Aprint("UNLOCK Command");
		break;
	case 0x25:
		Aprint("NOTE Command");
		break;
	case 0x26:
		Aprint("POINT Command");
		break;
	case 0xFE:
		Aprint("FORMAT Command");
		break;
	default:
		Aprint("UNKNOWN Command");
		break;
	}

	regY = 146;
	SetN;
}

void Device_HHINIT(void)
{
	if (devbug)
		Aprint("HHINIT");
}

static int phfd = -1;
void Device_PHCLOS(void);
static char *spool_file = NULL;

void Device_PHOPEN(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phfd != -1)
		Device_PHCLOS();

	spool_file = tmpnam(NULL);
	phfd = open(spool_file, O_CREAT | O_TRUNC | O_WRONLY, 0777);
	if (phfd != -1) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 130;
		SetN;
	}
}

void Device_PHCLOS(void)
{
	if (devbug)
		Aprint("PHCLOS");

	if (phfd != -1) {
		char command[256];
		int status;

		close(phfd);

		sprintf(command, print_command, spool_file);
		system(command);

#ifndef VMS
#ifndef WIN32
		status = unlink(spool_file);
		if (status == -1) {
			perror(spool_file);
			exit(1);
		}
#endif
#endif

		phfd = -1;
	}
	regY = 1;
	ClrN;
}

void Device_PHREAD(void)
{
	if (devbug)
		Aprint("PHREAD");

	regY = 146;
	SetN;
}

void Device_PHWRIT(void)
{
	unsigned char byte;
	int status;

	if (devbug)
		Aprint("PHWRIT");

	byte = regA;
	if (byte == 0x9b)
		byte = '\n';

	status = write(phfd, &byte, 1);
	if (status == 1) {
		regY = 1;
		ClrN;
	}
	else {
		regY = 144;
		SetN;
	}
}

void Device_PHSTAT(void)
{
	if (devbug)
		Aprint("PHSTAT");
}

void Device_PHSPEC(void)
{
	if (devbug)
		Aprint("PHSPEC");

	regY = 1;
	ClrN;
}

void Device_PHINIT(void)
{
	if (devbug)
		Aprint("PHINIT");

	phfd = -1;
	regY = 1;
	ClrN;
}

/* K: and E: handlers for BASIC version, using getchar() and putchar() */

#ifdef BASIC
/*
   ================================
   N = 0 : I/O Successful and Y = 1
   N = 1 : I/O Error and Y = error#
   ================================
 */

void K_Device(UBYTE esc_code)
{
	char ch;
#ifdef SOUND
	Sound_Update();
#endif

	switch (esc_code) {
	case ESC_K_OPEN:
	case ESC_K_CLOSE:
		regY = 1;
		ClrN;
		break;
	case ESC_K_WRITE:
	case ESC_K_STATUS:
	case ESC_K_SPECIAL:
		regY = 146;
		SetN;
		break;
	case ESC_K_READ:
		if (feof(stdin)) {
			Atari800_Exit(FALSE);
			exit(0);
		}
                ch = getkey();

		switch (ch) {
		case '\n':
			ch = (char)0x9b;
			break;
		default:
			break;
		}
		regA = ch;
		regY = 1;
		ClrN;
		break;
	}
#ifdef SOUND
	Sound_Update();
#endif
}

void E_Device(UBYTE esc_code)
{
	UBYTE ch;
#ifdef SOUND
	Sound_Update();
#endif

	switch (esc_code) {
	case ESC_E_OPEN:
		Aprint("Editor device open" );
		regY = 1;
		ClrN;
		break;
	case ESC_E_READ:
		if (feof(stdin)) {
			Atari800_Exit(FALSE);
			exit(0);
		}
                ch = getkey();
		switch (ch) {
		case '\n':
			ch = 0x9b;
			break;

		default:
			break;
		}
		regA = ch;
		regY = 1;
		ClrN;
		break;
	case ESC_E_WRITE:

		ch = regA;

              if(escape_char && ch > 0x20)
              {
                 escape_char = 0;
              }
              else if(escape_char && ch <= 0x20)
              {
                 ch = '*';
                 escape_char = 0;
              }
              else switch (ch)
              {
		case 0x7d:
                    ch='\0';
/*                    ch='*'; whatisthis */
                    break;
                 case 0xfc:
			break;
		case 0x9b:
                    ch='\n';
                    current_column=0;
                    break;
                 case 0x9c: /* delete line */
                    ch='\0';
                    row_offset++;
                    break;
                 case 0x9d: /* insert line */
                 case 0x9e: /* CTRL-Tab */
                 case 0x9f:
                    ch='\0';
                    break;
                 case 0x1b:
                    escape_char = 1;
                    ch='\0';
                    break;
                 case 0x1c: /* up arrow */
                    row_offset--;
                    ch='\0';
                    break;
                 case 0x1d: /* down arrow */
                    row_offset++;
                    ch='\0';
                    break;
                    /* this is not the correct way to handle l+r arrows, but...*/
                 case 0x1e: /* left arrow */
                    current_column--;
                    ch='\0';
			break;
                 case 0x1f: /* right arrow */
                    ch = ' ';
                    break;

		default:
			if ((ch >= 0x20) && (ch <= 0x7e))	/* for DJGPP */
                    {
                       set_text_mode(ATNORMAL);
                       ch= ch & 0x7f;
                    }
                    else if(ch >= 0x9f && ch <= 0xef )
                    {
                       set_text_mode(ATREVERSE);
                       ch = ch & 0x7f;
                    }
                    else
                    {
                       set_text_mode(ATNORMAL);
                       ch='?';
                    }
			break;
		}
              if(ch)
              {
                 for(;row_offset > 0;row_offset--)
                 {
                    putchar('\n');
                    current_column=1;
                 }

                 putchar(ch);
                 if(ch != '\n') current_column++;
                 /* if column is greater than right margin, wrap */
                 if(current_column /*1-38*/ >
                    (memory[83]/* right(39) */ - memory[82]/*left(2)*/))
                 {
                    putchar('\n');
                    current_column = 0;
                 }
                 fflush(stdout);
              }
		regY = 1;
		ClrN;
		break;
	}
#ifdef SOUND
	Sound_Update();
#endif
}

#endif /* BASIC */

#define CDTMV1 0x0218

void AtariEscape(UBYTE esc_code)
{
	switch (esc_code) {
#ifdef MONITOR_BREAK
	case ESC_BREAK:
		if (!Atari800_Exit(TRUE))
			exit(0);
		return;
#endif
	case ESC_SIOV:
		/* jump to SIO emulation only if it's really our ESC code */
		if (enable_sio_patch && (regPC == (0xe459+2))) {
			SIO();
			return;
		}
		break;
#ifdef BASIC
	case ESC_K_OPEN:
	case ESC_K_CLOSE:
	case ESC_K_READ:
	case ESC_K_WRITE:
	case ESC_K_STATUS:
	case ESC_K_SPECIAL:
		K_Device(esc_code);
		return;
		break;
	case ESC_E_OPEN:
	case ESC_E_READ:
	case ESC_E_WRITE:
		E_Device(esc_code);
		return;
		break;
#endif
	case ESC_PHOPEN:
		Device_PHOPEN();
		return;
		break;
	case ESC_PHCLOS:
		Device_PHCLOS();
		return;
		break;
	case ESC_PHREAD:
		Device_PHREAD();
		return;
		break;
	case ESC_PHWRIT:
		Device_PHWRIT();
		return;
		break;
	case ESC_PHSTAT:
		Device_PHSTAT();
		return;
		break;
	case ESC_PHSPEC:
		Device_PHSPEC();
		return;
		break;
	case ESC_PHINIT:
		Device_PHINIT();
		return;
		break;
	case ESC_HHOPEN:
		Device_HHOPEN();
		return;
		break;
	case ESC_HHCLOS:
		Device_HHCLOS();
		return;
		break;
	case ESC_HHREAD:
		Device_HHREAD();
		return;
		break;
	case ESC_HHWRIT:
		Device_HHWRIT();
		return;
		break;
	case ESC_HHSTAT:
		Device_HHSTAT();
		return;
		break;
	case ESC_HHSPEC:
		Device_HHSPEC();
		return;
		break;
	case ESC_HHINIT:
		Device_HHINIT();
		return;
		break;
	case ESC_BINLOADER_CONT:
		BIN_loader_cont();
		return;
		break;
	}
	/* for all codes that fall through the cases */

	
#ifdef CRASH_MENU
	regPC -= 2;
	crash_address = regPC;
	crash_afterCIM = regPC+2;
	crash_code = dGetByte(crash_address);
	ui((UBYTE*)atari_screen);
#else
	Aprint("Invalid ESC Code %x at Address %x", esc_code, regPC - 2);
	if(!Atari800_Exit(TRUE))
		exit(0);
#endif
}
