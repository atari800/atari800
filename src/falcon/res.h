#ifndef _RES_H_
#define _RES_H_

/* Auflîsungsstruktur */
/* LÑnge der Struktur: 86 Bytes */
typedef struct
{
	char	name[33];				/* Name der Auflîsung */
										/* FÅr Nicht-C-Programmierer: */
										/* der Offset der nÑchsten Variablen */
										/* zum Strukturstart betrÑgt 34 Bytes */
	short	mode;						/* Auflîsungsart (siehe ICB.H) */
	short	bypl;						/* Bytes pro Zeile */
	short	planes;					/* Anzahl Planes */
	unsigned short	colors;		/* Anzahl Farben */
	short	hc_mode;					/* Hardcopy-Modus */
	short	max_x, max_y;			/* maximale x- und y-Koordinate */
	short	real_x, real_y;		/* reale max. x- und y-Koordinate */
										/* folgende Variablen sollten nicht */
										/* beachtet werden */
	short	freq;						/* Frequenz in MHz */
	char	freq2;					/* 2. Frequenz (SIGMA Legend II) */
	char	low_res;					/* halbe Pixelrate */
										/* Bit 2: Erweiterungsbit fÅr h_total */
	unsigned char	r_3c2;					/* Register 3c2 */
	unsigned char	r_3d4[25];				/* Register 3d4, Index 0 bis $18 */
	unsigned char	extended[3];			/* Register 3d4, Index $33 bis $35 */
} RESOLUTION;

#endif /* _RES_H_ */
