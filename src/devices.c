/* $Id$ */
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include 	<errno.h>

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
#define DO_SPECIAL

#ifdef AMIGA
#undef	DO_DIR
#undef  DO_SPECIAL
#endif

#ifdef VMS
#undef	DO_DIR
#undef  DO_SPECIAL
#endif

#ifdef DO_DIR
#include	<dirent.h>
#endif

#ifdef DO_SPECIAL
#include        <sys/stat.h>
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
#ifdef DO_SPECIAL
static char newfilename[64];
#endif

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

int Device_isvalid(UBYTE ch)
{
	int valid;

	if (ch < 0x80 && isalnum(ch))
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

#ifdef DO_SPECIAL
void Device_GetFilenames(void)
{
	int bufadr;
	int offset = 0;
	int devnam = TRUE;
	int byte;

	bufadr = (dGetByte(ICBAHZ) << 8) | dGetByte(ICBALZ);

	while (Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);

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

	while(!Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);
		if ((byte > 0x80) || (byte == 0)) {
			newfilename[0] = 0;
			return;
			}
		bufadr++;
		}

	offset = 0;
	while (Device_isvalid(dGetByte(bufadr))) {
		byte = dGetByte(bufadr);

		if (isupper(byte))
			byte = tolower(byte);

		newfilename[offset++] = byte;
		bufadr++;
	}
	newfilename[offset++] = '\0';
}
#endif

int match(char *pattern, char *filename)
{
	int status = TRUE;

	if (strcmp(pattern,"*.*") == 0)
		return TRUE;

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

#ifdef DO_SPECIAL
void fillin(char *pattern, char *filename)
{
	while (*pattern) {
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
			*filename++ = *pattern++;
			break;
		}
	}
        *filename = 0;
}
#endif

void Device_HHOPEN(void)
{
	char fname[FILENAME_MAX];
	int devnum;
	int temp;
#ifdef DO_SPECIAL
	struct stat status;
	char entryname[FILENAME_MAX];
	char *ext;
	int size;
#endif

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
		regY = 160;
		SetN;
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
					if (match(filename,entry->d_name))
#ifndef DO_SPECIAL
                                            fprintf(fp[fid], "%s\n", strtoupper(entry->d_name));
#else
                                            if (entry->d_name[0] != '.') {
                                                sprintf(fname, "%s/%s", H[devnum], entry->d_name);
                                                stat(fname,&status);
                                                if ((status.st_size != 0) && (status.st_size % 256 == 0))
                                                    size = status.st_size/256;
                                                else
                                                    size = status.st_size/256 + 1;
                                                if (size > 999)
                                                    size = 999;
                                                strcpy(entryname,strtoupper(entry->d_name));
                                                    ext = strtok(entryname,".");
                                                    ext = strtok(NULL,".");
                                                    if (ext == NULL)
                                                        ext = "   ";
                                                    fprintf(fp[fid], "%s %-8s%-3s %03d\n", 
                                                            (status.st_mode & S_IWUSR) ? " ":"*",
                                                            entryname,
                                                            ext,
                                                            size);
                                                }
#endif
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
                else if (errno == EACCES) {
                        regY = 135;
                        SetN;
                        }
		else {
			regY = 170;
			SetN;
		}
		break;
#ifdef DO_SPECIAL                
	case 12:		/* read and write  (update) */
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			break;
		}
		fp[fid] = fopen(fname, "rb+");
		if (fp[fid]) {
			regY = 1;
			ClrN;
		}
                else if (errno == EACCES) {
                        regY = 135;
                        SetN;
                        }
		else {
			regY = 170;
			SetN;
		}
		break;
#endif                
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
	char fname[FILENAME_MAX];
	int devnum;
#ifdef DO_SPECIAL        
	char newfname[FILENAME_MAX];
	char renamefname[FILENAME_MAX];
	unsigned long pos;
	unsigned long iocb;
	int status;
	struct stat filestatus;
	int num_changed = 0;
	int num_locked = 0;
#endif        

	if (devbug)
		Aprint("HHSPEC");

	fid = regX >> 4;

	switch (dGetByte(ICCOMZ)) {
	case 0x20:
		if (devbug)
			Aprint("RENAME Command");
#ifdef DO_SPECIAL                        
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			return;
			}
		Device_GetFilenames();
		devnum = dGetByte(ICDNOZ);
		if (devnum > 9) {
			Aprint("Attempt to access H%d: device", devnum);
			regY = 160;
			SetN;
			return;
			}
		if (devnum >= 5) {
			devnum -= 5;
		}
                
                dp = opendir(H[devnum]);
                if (dp) {
                        struct dirent *entry;
                        
                        status = 0;
                        while ((entry = readdir(dp))) {
                                if (match(filename, entry->d_name)) {
                                    sprintf(fname, "%s/%s", H[devnum], entry->d_name);
                                    sprintf(renamefname,"%s",entry->d_name);
                                    fillin(newfilename, renamefname);
                                    sprintf(newfname, "%s/%s", H[devnum], renamefname);
                                    stat(fname,&filestatus);
                                    if (filestatus.st_mode & S_IWUSR) {
                                        if (rename(fname,newfname) != 0)
                                            status = -1;
                                        else
                                            num_changed++;
                                        }
                                    else
                                        num_locked++;
                                    }
                            }

                        closedir(dp);
                        }
                else
                    status = -1;
                
		if (num_locked) {
			regY = 167;
			SetN;
			return;
			}
		else if (status == 0 && num_changed != 0) {
			regY = 1;
			ClrN;
			return;
		    }
		else {
			regY = 170;
			SetN;
			return;
			}
#endif                        
                break;
	case 0x21:
		if (devbug)
			Aprint("DELETE Command");
#ifdef DO_SPECIAL                        
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			return;
			}
		Device_GetFilename();
		devnum = dGetByte(ICDNOZ);
		if (devnum > 9) {
			Aprint("Attempt to access H%d: device", devnum);
			regY = 160;
			SetN;
			return;
			}
		if (devnum >= 5) {
			devnum -= 5;
		}

                dp = opendir(H[devnum]);
                if (dp) {
                        struct dirent *entry;
                        
                        status = 0;
                        while ((entry = readdir(dp))) {
                                if (match(filename, entry->d_name)) {
                                    sprintf(fname, "%s/%s", H[devnum], entry->d_name);
                                    stat(fname,&filestatus);
                                    if (filestatus.st_mode & S_IWUSR) {
                                    	if (unlink(fname) != 0)
                                            status = -1;
                                        else
                                            num_changed++;
                                        }
                                    else
                                        num_locked++;
                                    }

                            }

                        closedir(dp);
                        }
                else
                    status = -1;
                
		if (status == 0 && num_changed != 0) {
			regY = 1;
			ClrN;
			return;
		    }
		else {
			regY = 170;
			SetN;
			return;
			}
#endif                        
                break;
	case 0x23:
		if (devbug)
                    Aprint("LOCK Command");
#ifdef DO_SPECIAL
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			return;
			}
		Device_GetFilename();
		devnum = dGetByte(ICDNOZ);
		if (devnum > 9) {
			Aprint("Attempt to access H%d: device", devnum);
			regY = 160;
			SetN;
			return;
			}
		if (devnum >= 5) {
			devnum -= 5;
		}

                dp = opendir(H[devnum]);
                if (dp) {
                        struct dirent *entry;
                        
                        status = 0;
                        while ((entry = readdir(dp))) {
                                if (match(filename, entry->d_name)) {
                                    sprintf(fname, "%s/%s", H[devnum], entry->d_name);
                                    if (chmod(fname, S_IROTH | S_IRGRP | S_IRUSR) != 0)
                                        status = -1;
                                    else
                                        num_changed++;
                                    }
                            }

                        closedir(dp);
                        }
                else
                    status = -1;
 
		if (status == 0 && num_changed != 0) {
			regY = 1;
			ClrN;
			return;
		    }
		else {
			regY = 170;
			SetN;
			return;
			}
#endif                    
		break;
	case 0x24:
		if (devbug)
                    Aprint("UNLOCK Command");
#ifdef DO_SPECIAL
		if (hd_read_only) {
			regY = 135;	/* device is read only */
			SetN;
			return;
			}
		Device_GetFilename();
		devnum = dGetByte(ICDNOZ);
		if (devnum > 9) {
			Aprint("Attempt to access H%d: device", devnum);
			regY = 160;
			SetN;
			return;
			}
		if (devnum >= 5) {
			devnum -= 5;
		}

                dp = opendir(H[devnum]);
                if (dp) {
                        struct dirent *entry;
                        
                        status = 0;
                        while ((entry = readdir(dp))) {
                                if (match(filename, entry->d_name)) {
                                    sprintf(fname, "%s/%s", H[devnum], entry->d_name);
                                    if (chmod(fname, S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR) != 0)
                                        status = -1;
                                    else
                                        num_changed++;
                                    }
                            }

                        closedir(dp);
                        }
                else
                    status = -1;
 
		if (status == 0 && num_changed != 0) {
			regY = 1;
			ClrN;
			return;
		    }
		else {
			regY = 170;
			SetN;
			return;
			}
#endif                    
		break;
	case 0x26:
		if (devbug)
			Aprint("NOTE Command");
#ifdef DO_SPECIAL                        
		if (fp[fid]) {
			iocb = IOCB0 + ((fid) * 16);
			pos = ftell(fp[fid]);
			if (pos != -1) {
				dPutByte(iocb + ICAX5, (pos & 0xff));
				dPutByte(iocb + ICAX3, ((pos & 0xff00) >> 8));
				dPutByte(iocb + ICAX4, ((pos & 0xff0000) >> 16));
				regY = 1;
				ClrN;
				return;
				}
			else {
				regY = 163;
				SetN;
				return;
				}
			}
		else {
			regY = 163;
			SetN;
			return;
			}
#endif                        
		break;
	case 0x25:
		if (devbug)
			Aprint("POINT Command");
#ifdef DO_SPECIAL
		if (fp[fid]) {
			iocb = IOCB0 + ((fid) * 16);
			pos = (dGetByte(iocb + ICAX4) << 16) +
			      (dGetByte(iocb + ICAX3) << 8)  +
			      (dGetByte(iocb + ICAX5));
			if (fseek(fp[fid], pos, SEEK_SET) == 0) {
				regY = 1;
				ClrN;
				return;
				}
			else {
				regY = 163;
				SetN;
				return;
				}
			}
		else {
			regY = 163;
			SetN;
			return;
			}
#endif                        
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
#if !defined(VMS) && !defined(MACOSX)
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

/* Device_PatchOS is called by Atari800_PatchOS to modify standard device
   handlers in Atari OS. It puts escape codes at beginnings of OS routines,
   so the patches work even if they are called directly, without CIO.
   Returns TRUE if something has been patched.
   Currently we only patch P: and, in BASIC version, E: and K:.
   We don't replace C: with H: now, so the cassette works even
   if H: is enabled.
*/
int Device_PatchOS(void)
{
	UWORD addr;
	UWORD devtab;
	int i;
	int patched = FALSE;

	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		addr = 0xf0e3;
		break;
	case MACHINE_XLXE:
		addr = 0xc42e;
		break;
	default:
		Aprint("Fatal Error in Device_PatchOS(): Unknown machine");
		return patched;
	}

	for (i = 0; i < 5; i++) {
		devtab = dGetWord(addr + 1);
		switch (dGetByte(addr)) {
		case 'P':
			if (enable_p_patch) {
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_OPEN) + 1, ESC_PHOPEN, Device_PHOPEN);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_CLOS) + 1, ESC_PHCLOS, Device_PHCLOS);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) + 1, ESC_PHWRIT, Device_PHWRIT);
				Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_STAT) + 1, ESC_PHSTAT, Device_PHSTAT);
				Atari800_AddEscRts2(devtab + DEVICE_TABLE_INIT, ESC_PHINIT, Device_PHINIT);
				patched = TRUE;
			}
			else {
				Atari800_RemoveEsc(ESC_PHOPEN);
				Atari800_RemoveEsc(ESC_PHCLOS);
				Atari800_RemoveEsc(ESC_PHWRIT);
				Atari800_RemoveEsc(ESC_PHSTAT);
				Atari800_RemoveEsc(ESC_PHINIT);
			}
			break;
#ifdef BASIC
		case 'E':
			Aprint("Editor Device");
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_OPEN) + 1, ESC_EHOPEN, Device_EHOPEN);
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1, ESC_EHREAD, Device_EHREAD);
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_WRIT) + 1, ESC_EHWRIT, Device_EHWRIT);
			patched = TRUE;
			break;
		case 'K':
			Aprint("Keyboard Device");
			Atari800_AddEscRts(dGetWord(devtab + DEVICE_TABLE_READ) + 1, ESC_KHREAD, Device_KHREAD);
			patched = TRUE;
			break;
#endif
		default:
			break;
		}
		addr += 3;				/* Next Device in HATABS */
	}
	return patched;
}

/* New handling of H: device.
   Previously we simply replaced C: device in OS with our H:.
   Now we don't change ROM for H: patch, but add H: to HATABS in RAM
   and put the device table and patches in unused area of address space
   (0xd100-0xd1ff), which is meant for 'new devices' (like hard disk).
   We have to contiunously check if our H: is still in HATABS,
   because RESET routine in Atari OS clears HATABS and initializes it
   using a table in ROM (see Device_PatchOS).
   Before we put H: entry in HATABS, we must make sure that HATABS is there.
   For example a program, that doesn't use Atari OS, could use this memory
   area for its own data, and we shouldn't place 'H' there.
   We also allow an Atari program to change address of H: device table.
   So after we put H: entry in HATABS, we only check if 'H' is still where
   we put it (h_entry_address).
   Device_UpdateHATABSEntry and Device_RemoveHATABSEntry can be used to add
   other devices than H:.
*/

#define HATABS 0x31a

UWORD Device_UpdateHATABSEntry(char device, UWORD entry_address, UWORD table_address)
{
	UWORD address;
	if (entry_address != 0 && dGetByte(entry_address) == device)
		return entry_address;
	if (dGetByte(HATABS) != 'P' || dGetByte(HATABS + 3) != 'C'
		|| dGetByte(HATABS + 6) != 'E' || dGetByte(HATABS + 9) != 'S'
		|| dGetByte(HATABS + 12) != 'K')
		return entry_address;
	for (address = HATABS + 15; address < HATABS + 33; address += 3) {
		if (dGetByte(address) == device)
			return address;
		if (dGetByte(address) == 0) {
			dPutByte(address, device);
			dPutWord(address + 1, table_address);
			return address;
		}
	}
	/* HATABS full */
	return entry_address;
}

void Device_RemoveHATABSEntry(char device, UWORD entry_address, UWORD table_address)
{
	if (entry_address != 0 && dGetByte(entry_address) == device
		&& dGetWord(entry_address + 1) == table_address) {
		dPutByte(entry_address, 0);
		dPutWord(entry_address + 1, 0);
	}
}

static UWORD h_entry_address = 0;

#define H_DEVICE_BEGIN	0xd140
#define H_TABLE_ADDRESS	0xd140
#define H_PATCH_OPEN	0xd150
#define H_PATCH_CLOS	0xd153
#define H_PATCH_READ	0xd156
#define H_PATCH_WRIT	0xd159
#define H_PATCH_STAT	0xd15c
#define H_PATCH_SPEC	0xd15f
#define H_DEVICE_END	0xd161

void Device_Frame(void)
{
	if (enable_h_patch)
		h_entry_address = Device_UpdateHATABSEntry('H', h_entry_address, H_TABLE_ADDRESS);
}

/* this is called when enable_h_patch is toggled */
void Device_UpdatePatches(void)
{
	if (enable_h_patch) {		/* enable H: device */
		/* change memory attributex for the area, where we put
		   H: handler table and patches */
		SetROM(H_DEVICE_BEGIN, H_DEVICE_END);
		/* set handler table */
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_OPEN, H_PATCH_OPEN - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_CLOS, H_PATCH_CLOS - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_READ, H_PATCH_READ - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_WRIT, H_PATCH_WRIT - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_STAT, H_PATCH_STAT - 1);
		dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_SPEC, H_PATCH_SPEC - 1);
		/* set patches */
		Atari800_AddEscRts(H_PATCH_OPEN, ESC_HHOPEN, Device_HHOPEN);
		Atari800_AddEscRts(H_PATCH_CLOS, ESC_HHCLOS, Device_HHCLOS);
		Atari800_AddEscRts(H_PATCH_READ, ESC_HHREAD, Device_HHREAD);
		Atari800_AddEscRts(H_PATCH_WRIT, ESC_HHWRIT, Device_HHWRIT);
		Atari800_AddEscRts(H_PATCH_STAT, ESC_HHSTAT, Device_HHSTAT);
		Atari800_AddEscRts(H_PATCH_SPEC, ESC_HHSPEC, Device_HHSPEC);
		/* H: in HATABS will be added next frame by Device_Frame */
	}
	else {	/* disable H: device */
		/* remove H: entry from HATABS */
		Device_RemoveHATABSEntry('H', h_entry_address, H_TABLE_ADDRESS);
		/* remove patches */
		Atari800_RemoveEsc(ESC_HHOPEN);
		Atari800_RemoveEsc(ESC_HHCLOS);
		Atari800_RemoveEsc(ESC_HHREAD);
		Atari800_RemoveEsc(ESC_HHWRIT);
		Atari800_RemoveEsc(ESC_HHSTAT);
		Atari800_RemoveEsc(ESC_HHSPEC);
		/* fill memory area used for table and patches with 0xff */
		dFillMem(H_DEVICE_BEGIN, 0xff, H_DEVICE_END - H_DEVICE_BEGIN + 1);
	}
}

/*
$Log$
Revision 1.13  2002/12/04 10:09:21  joy
H device supports all DOS functions

Revision 1.12  2001/10/03 16:40:54  fox
rewritten escape codes handling,
corrected Device_isvalid (isalnum((char) 0x9b) == 1 !)

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
