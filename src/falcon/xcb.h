#ifndef __XCB
#define __XCB
#include "res.h"

/******************************************************************/
/* Es folge eine Beschreibung der XCB-Struktur. Diese Struktur		*/
/* wird vom EMULATOR.PRG angelegt, wenn dieses eine NOVA-Karte		*/
/* im Rechner findet.															*/
/* Zu diesem Zweck legt EMULATOR.PRG einen Cookie mit der Kennung	*/
/* 'NOVA' an. Der Wert dieses Cookies zeigt auf die XCB-Struktur.	*/
/* Alle Werte dieser Struktur sind, wenn nicht anders angegeben,	*/
/* nur zum Lesen gedacht.														*/
/******************************************************************/


typedef struct
{
	long	version;
	unsigned char	resolution;					/* Auflîsungsindex */
	unsigned char	blnk_time;					/* Bildschirmdunkelzeit */
	unsigned char	ms_speed;					/* Mausgeschwindigkeit */
	char	old_res;							/* ursprÅngliche Auflîsung */

												/* Auflîsungsumschaltung */
												/* fll_ofst sollt 0UL sein */
	void	(*p_chres)(RESOLUTION *res, unsigned long fll_ofst);
	short	mode;								/* Auflîsungsmodus, momentan sind */
												/* folgende Werte definiert: */
												/* 0: 16 Farben */
												/* 1: 2 Farben */
												/* 2: 256 Farben */
												/* 3: 32768 Farben (Intelformat) */
												/* 4: 65536 Farben (Intelformat) */
												/*	5: 16.7 Mio. (24 Bit BGR) */
												/* 6: 16.7 Mio. (32 Bit RGBx) */
	short	bypl;								/* Bytes pro Bildschirmzeile, */
												/* dieser Wert muû nicht gleich */
												/* Anzahl x-Pixel * Bytes pro Pixel sein! */
	short	planes;							/* Anzahl Bildschirmplanes: */
												/* 1, 4, 8, 16, 24 oder 32 */
	short	colors;							/* Anzahl Farben, nicht benutzen */
	short	hc;								/* Hardcopymodus: */
												/* 0: 1 Bildpxl. = 1x1 Druckpxl. */
												/* 1: 1 Bildpxl. = 2x2 Druckpxl. */
												/* 2: 1 Bildpxl. = 4x4 Druckpxl. */
	short	max_x, max_y;					/* Bildschirmauflîsung */
												/* fÅr virtuelle Bildschirm- */
												/* verwaltung: */
	short	rmn_x, rmx_x;					/* physikalisch auf dem Monitor */
	short	rmn_y, rmx_y;					/* dargestellter Bereich */
												/* folgende 4 Werte dÅrfen verÑndert */
												/* werden. Dabei ist aber zu */
												/* berÅcksichtigen, daû die Werte */
												/* sinnvoll bleiben, d.h.: */
												/* v_top + v_bottom < rmx_y - rmn_y */
												/* v_left + v_right < rmx_x - rmn_x */
	short	v_top, v_bottom,				/* RÑnder fÅr virt. Speicherverwaltung */
			v_left, v_right;
												/* Zeiger auf Routine zum Farben */
												/* setzen. Index ist der Farb- */
												/* index, colors der Zeiger auf */
												/* das 3-Byte-Array mit den */
												/* Farbwerten */
	void	(*p_setcol)(short index, unsigned char *colors);
												/* virtuelle Bildschirmverwaltung: */
												/* es wird getestet, ob der Punkt */
												/* (x;y) sich im dargestellten */
												/* Bildabschnitt befindet */
												/* falls nicht, wird der darge- */
												/* stellte Abschnitt so verschoben, */
												/* daû der Punkt gerade sichtbar */
												/* wird. Hierbei werden v_top, ... */
												/* berÅcksichtigt. */
	void	(*chng_vrt)(short x, short y);
												/* XBIOS-Routinen fÅr Graphikkarte */
												/* installieren/abschalten */
												/* on != 0: installieren */
												/* inst_xbios ist nur fÅr Menu-Prog. */
												/* wie MENU.PRG und XMENU.PRG */
												/* gedacht. */
	void	(*inst_xbios)(short on);
												/* Bild ein-/ausschalten */
	void	(*pic_on)(short on);			/* 0: Bild aus-, 1: Bild einschalten */
												/* Bildlage verÑndern */
												/* res: Auflîsungsstruktur */
												/* direction = 0: hor. Lage Ñndern */
												/* direction != 0: ver. Lage Ñndern */
												/* offset: Anzahl Einheiten, um die */
												/*        verschoben werden soll */
												/*        bei links oder oben positiv */
	void	(*chng_pos)(RESOLUTION *res, short direction, short offset);
	void	(*p_setscr)(void *adr);		/* physikalische Bildschirmadresse */
												/* umsetzen. Die neue Bildschirm- */
												/* adresse muû im Speicher der */
												/* Grafikkarte liegen! */
	void	*base;							/* Adresse von Bildschirmseite 0 */
	void	*scr_base;						/* Adresse des Bildschirmspeichers */
	unsigned short	scrn_cnt;						/* Anzahl mîglicher phys. Bildschirm- */
												/* seiten */
	long	scrn_sze;						/* Grîûe eines Bildschirms in Bytes */
	unsigned char	*reg_base;						/* Zeiger auf I/O-Adressen, nie benutzen */
	void	(*p_vsync)(void);				/* wartet auf vsync */

	char	name[36];						/* Name der aktuellen Auflîsung */
												/* folgende Variablen sind erst */
												/* ab Version 1.01 definiert: */
	unsigned long	mem_size;						/* Grîûe des Bildschirmspeichers in Byte */
} XCB;

#endif