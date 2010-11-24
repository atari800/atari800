#ifndef VIDEOMODE_H_
#define VIDEOMODE_H_

#include <stdio.h>

#include "config.h"

typedef struct VIDEOMODE_resolution_t {
	unsigned int width;
	unsigned int height;
} VIDEOMODE_resolution_t;

/* The following 8 values describe geometry of the visible screen area.
   SRC means the source Atari screen.
   DEST means the screen surface on which the source screen will be displayed.
   Do not change these values by hand. They are changed when needed, and after
   each change a call to PLATFORM_SetVideoMode() is made. Platform-specific
   parts should base their screen-drawing on these values. */
/* SRC_OFFSET values describe the left-top corner of the visible Atari screen area.
   SRC_WIDTH/HEIGHT describe the size of the visible area.
   Effectively, these values define amount of crop on all four sides of screen. */
extern unsigned int VIDEOMODE_src_offset_left;
extern unsigned int VIDEOMODE_src_offset_top;
extern unsigned int VIDEOMODE_src_width;
extern unsigned int VIDEOMODE_src_height;
/* DEST_OFFSET values describe the left-top corner of the screen surface,
   DEST_WIDTH/HEIGHT describe the screen area into which the source screen should
   be copied.
   When combined, these values describe position and amount of screen stretching
   of the displayed screen. */
extern unsigned int VIDEOMODE_dest_offset_left;
extern unsigned int VIDEOMODE_dest_offset_top;
extern unsigned int VIDEOMODE_dest_width;
extern unsigned int VIDEOMODE_dest_height;

/* Get/set fullscreen/windowed mode. */
/* Don't change this variable directly; instead use VIDEOMODE_SetWindowed(). */
extern int VIDEOMODE_windowed;
int VIDEOMODE_SetWindowed(int value);
int VIDEOMODE_ToggleWindowed(void);
/* Forces windowed display mode. */
void VIDEOMODE_ForceWindowed(int value);

/* Get/set visible horizontal screen area. */
enum {
	VIDEOMODE_HORIZONTAL_NARROW,
	VIDEOMODE_HORIZONTAL_NORMAL,
	VIDEOMODE_HORIZONTAL_FULL,
	/* Number of values in enumerator */
	VIDEOMODE_HORIZONTAL_SIZE,
	VIDEOMODE_HORIZONTAL_CUSTOM = VIDEOMODE_HORIZONTAL_SIZE
};
/* Don't change this variable directly; instead use VIDEOMODE_SetHorizontalArea(). */
extern int VIDEOMODE_horizontal_area;
int VIDEOMODE_SetHorizontalArea(int value);
int VIDEOMODE_ToggleHorizontalArea(void);
/* Don't change this variable directly; instead use VIDEOMODE_SetCustomHorizontalArea(). */
extern unsigned int VIDEOMODE_custom_horizontal_area;
int VIDEOMODE_SetCustomHorizontalArea(unsigned int value);

/* Get/set visible vertical screen area. */
enum {
	VIDEOMODE_VERTICAL_SHORT,
	VIDEOMODE_VERTICAL_NORMAL,
	VIDEOMODE_VERTICAL_FULL,
	/* Number of values in enumerator */
	VIDEOMODE_VERTICAL_SIZE,
	VIDEOMODE_VERTICAL_CUSTOM = VIDEOMODE_VERTICAL_SIZE
};
/* Don't change this variable directly; instead use VIDEOMODE_SetVerticalArea(). */
extern int VIDEOMODE_vertical_area;
int VIDEOMODE_SetVerticalArea(int value);
int VIDEOMODE_ToggleVerticalArea(void);
/* Don't change this variable directly; instead use VIDEOMODE_SetCustomVerticalArea(). */
extern unsigned int VIDEOMODE_custom_vertical_area;
int VIDEOMODE_SetCustomVerticalArea(unsigned int value);

/* Get/set vertical offset. */
/* Don't change this variable directly; instead use VIDEOMODE_SetHorizontalOffset(). */
extern int VIDEOMODE_horizontal_offset;
int VIDEOMODE_SetHorizontalOffset(int value);

/* Get/set vertical offset. */
/* Don't change this variable directly; instead use VIDEOMODE_SetVerticalOffset(). */
extern int VIDEOMODE_vertical_offset;
int VIDEOMODE_SetVerticalOffset(int value);

/* Get/set type of screen stretching. */
enum {
	VIDEOMODE_STRETCH_NONE,
	VIDEOMODE_STRETCH_INTEGER,
	VIDEOMODE_STRETCH_FULL,
	/* Number of values in enumerator */
	VIDEOMODE_STRETCH_SIZE
};

/* Don't change this variable directly; instead use VIDEOMODE_SetStretch(). */
extern int VIDEOMODE_stretch;
int VIDEOMODE_SetStretch(int value);
int VIDEOMODE_ToggleStretch(void);

/* Get/set method of keeping screen aspect ratio. */
enum {
	VIDEOMODE_KEEP_ASPECT_NONE,
	VIDEOMODE_KEEP_ASPECT_1TO1,
	VIDEOMODE_KEEP_ASPECT_REAL,
	/* Number of values in enumerator */
	VIDEOMODE_KEEP_ASPECT_SIZE
};
/* Don't change this variable directly; instead use VIDEOMODE_SetKeepAspect(). */
extern int VIDEOMODE_keep_aspect;
int VIDEOMODE_SetKeepAspect(int value);
int VIDEOMODE_ToggleKeepAspect(void);

#if SUPPORTS_ROTATE_VIDEOMODE
/* Get/set screen rotation. */
/* Don't change this variable directly; instead use VIDEOMODE_SetRotate90(). */
extern int VIDEOMODE_rotate90;
int VIDEOMODE_SetRotate90(int value);
int VIDEOMODE_ToggleRotate90(void);
#endif /* SUPPORTS_ROTATE_VIDEOMODE */

/* Get/set the host display's aspect ratio (4:3, 16:9 etc.) */
/* Don't change these two variables directly; instead use VIDEOMODE_SetHostAspect(). */
extern double VIDEOMODE_host_aspect_ratio_w;
extern double VIDEOMODE_host_aspect_ratio_h;
int VIDEOMODE_SetHostAspect(double w, double h);
int VIDEOMODE_SetHostAspectString(char const *s);
void VIDEOMODE_CopyHostAspect(char *target, unsigned int size);

/* Returns number of available fullscreen resolutions. */
unsigned int VIDEOMODE_NumAvailableResolutions(void);

/* Converts resolution RES_ID to a user-readable format, and writes to
   string TARGET of size SIZE. TARGET is automatically ended with '\0'. */
void VIDEOMODE_CopyResolutionName(unsigned int res_id, char *target, unsigned int size);

/* Get/set id of the fullscreen resolution. */
unsigned int VIDEOMODE_GetFullscreenResolution(void);
int VIDEOMODE_SetFullscreenResolution(unsigned int res_id);

/* Update window size. This function should be called by platform-specific
   parts every time the user changes the window's size. */
int VIDEOMODE_SetWindowSize(unsigned int width, unsigned int height);

/* Additional actions when the TV system changes (change screen aspect ratio). */
void VIDEOMODE_SetVideoSystem(int mode);

/* This enumerator lists all possible "display modes" */
typedef enum {
	VIDEOMODE_MODE_NORMAL,
#if NTSC_FILTER
	VIDEOMODE_MODE_NTSC_FILTER,
#endif
#ifdef XEP80_EMULATION
	VIDEOMODE_MODE_XEP80,
#endif
#ifdef PBI_PROTO80
	VIDEOMODE_MODE_PROTO80,
#endif
#ifdef AF80
	VIDEOMODE_MODE_AF80,
#endif
	VIDEOMODE_MODE_SIZE
} VIDEOMODE_MODE_t;
#if NTSC_FILTER
/* Get/set state of NTSC filtering. */
/* Don't change this variable directly; instead use VIDEOMODE_SetNtscFilter(). */
extern int VIDEOMODE_ntsc_filter;
int VIDEOMODE_SetNtscFilter(int value);
int VIDEOMODE_ToggleNtscFilter(void);
#endif /* NTSC_FILTER */
#if defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80)
/* Indicates that 80 column display should be active when a 80 column card is available.
   Setting to TRUE does not switch to 80 column display when no 80 column card is present. */
/* Don't change this variable directly; instead use VIDEOMODE_Set80Column(). */
extern int VIDEOMODE_80_column;
int VIDEOMODE_Set80Column(int value);
int VIDEOMODE_Toggle80Column(void);
#endif /* defined(XEP80_EMULATION) || defined(PBI_PROTO80) || defined(AF80) */

/* Called when an UI needs to be displayed. Forces the standard 40x25 display mode. */
void VIDEOMODE_ForceStandardScreen(int value);

/* Read/write to configuration file. */
int VIDEOMODE_ReadConfig(char *option, char *parameters);
void VIDEOMODE_WriteConfig(FILE *fp);

/* Performs initial setup and reads command-line parameters. */
int VIDEOMODE_Initialise(int *argc, char *argv[]);
/* Called after initialisation of all modules. Initialises the display. */
int VIDEOMODE_InitialiseDisplay(void);
/* Clean up on exit. */
void VIDEOMODE_Exit(void);

#endif /* VIDEOMODE_H_ */
