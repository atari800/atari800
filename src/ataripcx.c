/*	ATARI 336x240 pcx screenshot
	Robert Golias, golias@fi.muni.cz
	fixes and interlace screenshot by Piotr Fusik, pfusik@elka.pw.edu.pl

	Note: DPI in pcx-header is set to 0. Does anybody know the correct values?
*/

#include <stdio.h>
#include <stdlib.h>

#include "antic.h"
#include "atari.h"
#include "config.h"
#include "colours.h"

#define bytesPerLine 336
#define XMax 335
#define YMax 239

char *Find_PCX_name(void)
{
	int pcx_no = -1;
	static char filename[20];
	FILE *fp;

	while (++pcx_no < 1000) {
		sprintf(filename, "atari%03i.pcx", pcx_no);
		if ((fp = fopen(filename, "r")) == NULL)
			return filename; /*file does not exist - we can create it */
		fclose(fp);
	}
	return NULL;
}

UBYTE Save_PCX_file(int interlace, char *filename)
{
	ULONG *temp_screen = atari_screen;
	UBYTE *ptr1;
	UBYTE *ptr2 = NULL;
	UBYTE plane = 16;	/* 16 = Red, 8 = Green, 0 = Blue */
	int i;
	int xpos;
	int ypos;
	UBYTE last;
	UBYTE count;
	FILE *fp;

	if (filename == NULL || (fp = fopen(filename,"wb")) == NULL)
		return FALSE;

	/*write header*/
	fputc(0xa,fp); /*pcx signature*/
	fputc(0x5,fp); /*version 5*/
	fputc(0x1,fp); /*RLE encoding*/
	fputc(0x8,fp); /*bits per pixel*/
	fputc(0,fp);fputc(0,fp);fputc(0,fp);fputc(0,fp); /* XMin=0,YMin=0 */
	fputc(XMax&0xff,fp);fputc(XMax>>8,fp);fputc(YMax&0xff,fp);fputc(YMax>>8,fp);
	fputc(0,fp);fputc(0,fp);fputc(0,fp);fputc(0,fp); /*unknown DPI */
	for (i=0;i<48;i++) fputc(0,fp);	/*EGA color palette*/
	fputc(0,fp); /*reserved*/
	fputc(interlace?3:1,fp); /*number of bit planes*/
	fputc(bytesPerLine&0xff,fp);fputc(bytesPerLine>>8,fp);
	fputc(1,fp);fputc(0,fp); /*palette info - unused */
	fputc((XMax+1)&0xff,fp);fputc((XMax+1)>>8,fp);
	fputc((YMax+1)&0xff,fp);fputc((YMax+1)>>8,fp); /*screen resolution*/
	for (i=0;i<54;i++) fputc(0,fp); /*unused*/

	/*write picture*/
	ptr1 = (UBYTE *)atari_screen + (ATARI_WIDTH - bytesPerLine) / 2;
	if (interlace) {	/* make current screen temp_screen and draw second screen */
		atari_screen = (ULONG *) malloc( (ATARI_HEIGHT + 16) * ATARI_WIDTH );
		ptr2 = (UBYTE *)atari_screen + (ATARI_WIDTH - bytesPerLine) / 2;
		ANTIC_RunDisplayList();
	}

	for (ypos = 0; ypos <= YMax; ) {
		xpos = 0;
		do {
			last = interlace ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1;
			count = 0xc0;
			do {
				ptr1++;
				ptr2++;
				count++;
				xpos++;
			} while (last == (interlace ? (((colortable[*ptr1] >> plane) & 0xff) + ((colortable[*ptr2] >> plane) & 0xff)) >> 1 : *ptr1)
						&& count < 0xff && xpos < bytesPerLine);
			if (count>0xc1 || last>=0xc0)
				fputc(count, fp);
			fputc(last,fp);
		} while (xpos < bytesPerLine);

		if (interlace && plane) {
			ptr1 -= bytesPerLine;
			ptr2 -= bytesPerLine;
			plane -= 8;
		}
		else {
			ptr1 += ATARI_WIDTH - bytesPerLine;
			ptr2 += ATARI_WIDTH - bytesPerLine;
			plane = 16;
			ypos++;
		}
	}

	if (interlace) {
		/*free memory for second screen*/
		free(atari_screen);
		atari_screen = temp_screen;
	}
	else {
		/*write palette*/
		fputc(0xc,fp);
		for (i=0;i<256;i++) {
			fputc((colortable[i]>>16)&0xff,fp);
			fputc((colortable[i]>>8)&0xff,fp);
			fputc(colortable[i]&0xff,fp);
		}
	}

	fclose(fp);
	return TRUE;
}
