#ifndef _SCREEN_WIN32_H_
#define _SCREEN_WIN32_H_

#include "atari.h"
#define SCREENWIDTH 320
#define SCREENHEIGHT 240
#define LOWRESWIDTH 640
#define LOWRESHEIGHT 480
#define MEDRESWIDTH 1280
#define MEDRESHEIGHT 960
#define CLR_BACK	0x44

typedef enum CHANGEWINDOWSIZE_ {
	STEPUP,
	STEPDOWN,
	SET
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
	SMART
} ASPECTMODE;

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
	GDIPLUS_BILINEAR,
	GDIPLUS_HQBILINEAR,
	GDIPLUS_HQBICUBIC,
	DIRECT3D_NEARESTNEIGHBOR,
	DIRECT3D_BILINEAR,
	GDIPLUS_NEARESTNEIGHBOR,
	GDIPLUS_BICUBIC
} DISPLAYMODE;

typedef enum SCREENMODE_ {
	FULLSCREEN,
	WINDOW
} SCREENMODE;

typedef enum FSRESOLUTION_ {
	LOWRES,
	MEDRES,
	DESKTOPRES
} FSRESOLUTION;

typedef struct FRAMEPARAMS_ {
	HDC hdc;
	INT width;	
	INT height;		
	INT vertoffset;		// vertical offset to center screen in window
	INT horizoffset;	// horizontal offset to center screen in window
	float d3dWidth;
	float d3dHeight;
	BOOL d3dRefresh;	// determines whether vertex buffer should be refreshed
	SCANLINEMODE scanlinemode;
	FILTER filter;
	TILTLEVEL tiltlevel;
	BOOL screensaver;
} FRAMEPARAMS; 

extern BOOL checkparamarg(char arg[]);

BOOL getres(char res[], int *width, int *height); 
int gron(int *argc, char *argv[]);
void groff(void);
void palupd(int beg, int cnt);
void palette(int ent, UBYTE r, UBYTE g, UBYTE b);
void refreshv(UBYTE * scr_ptr);
void togglewindowstate();
void changewindowsize(CHANGEWINDOWSIZE, int);
void getscaledrect(RECT *rect);
void refreshframe();
void changescanlinemode();
void togglescreensaver();
void changetiltlevel();
void getcenteredcoords(RECT rect, int* x, int* y);
void setcursor();

RENDERMODE GetRenderMode();
DISPLAYMODE GetDisplayMode();
DISPLAYMODE GetActiveDisplayMode();
void SetDisplayMode(DISPLAYMODE dm);
void GetDisplayModeName(char *name);

#endif /* _SCREEN_WIN32_H_ */
