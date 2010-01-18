#ifndef _SCREEN_WIN32_H_
#define _SCREEN_WIN32_H_

#include "atari.h"
#define SCREENWIDTH 336
#define SCREENHEIGHT 240
#define CROPPEDWIDTH 320
#define CLR_BACK	0x44

typedef enum CHANGEWINDOWSIZE_ {
	STEPUP,
	STEPDOWN,
	SET,
	RESET
} CHANGEWINDOWSIZE;

typedef enum TILTLEVEL_ {
	TILTLEVEL0,
	TILTLEVEL1,
	TILTLEVEL2,
	TILTLEVEL3
} TILTLEVEL;

typedef enum SCANLINEMODE_ {
	NONE = 1,
	LOW,
	MEDIUM,
	HIGH
} SCANLINEMODE;

typedef enum ASPECTMODE_ {
	OFF,
	NORMAL,
	ADAPTIVE
} ASPECTMODE;

typedef enum ASPECTRATIO_ {
	HYBRID,
	WIDE,
	CROPPED,
	COMPRESSED
} ASPECTRATIO;

typedef enum RENDERMODE_ {
	DIRECTDRAW,
	GDI,
	GDIPLUS,
	DIRECT3D
} RENDERMODE;

typedef enum FILTER_ {
	NEARESTNEIGHBOR,
	BILINEAR,
	BICUBIC,
	HQBILINEAR,
	HQBICUBIC
} FILTER; 	

typedef enum DISPLAYMODE_ {
	GDI_NEARESTNEIGHBOR,
	GDIPLUS_NEARESTNEIGHBOR,
	GDIPLUS_BILINEAR,
	GDIPLUS_HQBILINEAR,
	GDIPLUS_HQBICUBIC,
	DIRECT3D_NEARESTNEIGHBOR,
	DIRECT3D_BILINEAR,
	GDIPLUS_BICUBIC
} DISPLAYMODE;

typedef enum SCREENMODE_ {
	FULLSCREEN,
	WINDOW
} SCREENMODE;

typedef enum FSRESOLUTION_ {
	VGA,         /* 4:3   [640x480]   (2x)     */
	SXGA,        /* 4:3   [1280x960]  (4x)     */
	UXGA,        /* 4:3   [1600x1200] (5x)     */
	DESKTOPRES   /* Current Desktop Resolution */
} FSRESOLUTION;

typedef struct FRAMEPARAMS_ {
	HDC hdc;
	INT width;	
	INT height;		
	INT vertoffset;		/* vertical offset to center screen in window */
	INT horizoffset;	/* horizontal offset to center screen in window */
	float d3dWidth;
	float d3dHeight;
	BOOL d3dRefresh;	/* determines whether vertex buffer should be refreshed */
	SCANLINEMODE scanlinemode;
	FILTER filter;
	TILTLEVEL tiltlevel;
	BOOL screensaver;
    INT cropamount;
} FRAMEPARAMS; 

extern BOOL checkparamarg(char arg[]);

BOOL getres(char res[], int *width, int *height); 
int gron(int *argc, char *argv[]);
void groff(void);
void palupd(int beg, int cnt);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);
void togglewindowstate(void);
void changewindowsize(CHANGEWINDOWSIZE, int);
void getscaledrect(RECT *rect);
void refreshframe(void);
void changescanlinemode(void);
void togglescreensaver(void);
void changetiltlevel(void);
void getcenteredcoords(RECT rect, int* x, int* y);
void setcursor(void);

RENDERMODE GetRenderMode(void);
DISPLAYMODE GetDisplayMode(void);
DISPLAYMODE GetActiveDisplayMode(void);
void SetDisplayMode(DISPLAYMODE dm);
void GetDisplayModeName(char *name);

#endif /* _SCREEN_WIN32_H_ */
