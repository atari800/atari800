/* $Id$ */
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>

#include	"config.h"
#include	"ui.h"

#ifdef VMS
#include	<unixio.h>
#include	<file.h>
#else
#include	<fcntl.h>
#ifndef	AMIGA
#include	<unistd.h>
#endif
#endif

#define	DO_DIR

#ifdef AMIGA
#undef	DO_DIR
#endif

#ifdef VMS
#undef	DO_DIR
#endif

#ifdef DO_DIR
#include	<dirent.h>
#endif

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "devices.h"
#include "rt-config.h"
#include "log.h"
#include "binload.h"
#include "sio.h"
#include "ui.h"

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
static DIR	*dp = NULL;
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

	if (isalnum(ch))
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

	fid = regX >> 4;

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
#endif /* DO_DIR */
		{
			regY = 163;
			SetN;
		}
		break;
	case 8:
	case 9:		/* write at the end of file (append) */
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, temp == 9 ? "ab" : "wb");
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

	fid = regX >> 4;

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

	fid = regX >> 4;

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

	fid = regX >> 4;

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

	fid = regX >> 4;

	regY = 146;
	SetN;
}

void Device_HHSPEC(void)
{
	if (devbug)
		Aprint("HHSPEC");

	fid = regX >> 4;

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

static FILE *phf = NULL;
void Device_PHCLOS(void);
static char *spool_file = NULL;

void Device_PHOPEN(void)
{
	if (devbug)
		Aprint("PHOPEN");

	if (phf)
		Device_PHCLOS();

	spool_file = tmpnam(NULL);
	phf = fopen(spool_file, "w");
	if (phf) {
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

	if (phf) {
		char command[256];
		int status;

		fclose(phf);

		sprintf(command, print_command, spool_file);
		system(command);

#ifndef VMS
		status = unlink(spool_file);
		if (status == -1) {
			perror(spool_file);
			exit(1);
		}
#endif

		phf = NULL;
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

	status = fwrite(&byte, 1, 1, phf);
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

	phf = NULL;
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

void Device_KHREAD(void)
{
	UBYTE ch;

	if (feof(stdin)) {
		Atari800_Exit(FALSE);
		exit(0);
	}
	ch = getchar();
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
}

void Device_EHOPEN(void)
{
	Aprint( "Editor device open" );
	regY = 1;
	ClrN;
}

void Device_EHREAD(void)
{
	UBYTE ch;

	if (feof(stdin)) {
		Atari800_Exit(FALSE);
		exit(0);
	}
	ch = getchar();
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
}

void Device_EHWRIT(void)
{
	UBYTE ch;

	ch = regA;
	switch (ch) {
	case 0x7d:
		putchar('*');
		break;
	case 0x9b:
		putchar('\n');
		break;
	default:
		if ((ch >= 0x20) && (ch <= 0x7e))	/* for DJGPP */
			putchar(ch & 0x7f);
		break;
	}
	regY = 1;
	ClrN;
}

#endif /* BASIC */

void AtariEscape(UBYTE esc_code)
{
	switch (esc_code) {
	case ESC_SIOV:
		/* jump to SIO emulation only if it's really our ESC code */
		if (enable_sio_patch && (regPC == (0xe459+2))) {
			SIO();
			return;
		}
		break;
#ifdef BASIC
	case ESC_K_READ:
		Device_KHREAD();
		return;
	case ESC_E_OPEN:
		Device_EHOPEN();
		return;
	case ESC_E_READ:
		Device_EHREAD();
		return;
	case ESC_E_WRITE:
		Device_EHWRIT();
		return;
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

/*
$Log$
Revision 1.11  2001/10/01 17:10:34  fox
#include "ui.h" for CRASH_MENU externs

Revision 1.10  2001/09/21 16:54:11  fox
removed unused variable

Revision 1.9  2001/07/20 00:30:08  fox
replaced K_Device with Device_KHREAD,
replaced E_Device with Device_EHOPEN, Device_EHREAD and Device_EHWRITE,
removed ESC_BREAK

Revision 1.6  2001/03/25 06:57:35  knik
open() replaced by fopen()

Revision 1.5  2001/03/22 06:16:58  knik
removed basic improvement

Revision 1.4  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
