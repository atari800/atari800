#ifndef _UI_H
#define _UI_H
#include "atari.h"	/* for UBYTE */

/* Two legitimate entries to UI module. */
int SelectCartType(int k);
void ui(void);

/* this entry is used by atari_x11.c which implements its own GUI */
void SoundRecording();

extern int ui_is_active;

extern unsigned char ascii_to_screen[128];

#ifdef CRASH_MENU
extern int crash_code;
extern UWORD crash_address;
extern UWORD crash_afterCIM;
#endif

#define FILENAME_SIZE	32

/* Structure of menu item. Each menu is just an array of items of this structure
   terminated by MENU_END */
typedef struct
{
	char* sig;      /* Signature identifying item function. Conventionally, four-letter string */
	long  flags;    /* Flags, see values below */
	char* prefix;   /* Text to prepend the item */
	char* item;     /* Main item text */
	char* suffix;   /* Optional (not always) text to show after the item text. Often key shortcut */
	int   retval;   /* Value returned by Select when this item is selected */
} tMenuItem;

/* Set if item is enabled. Disabled items will not be shown */
#define ITEM_ENABLED 0x00000001
/* For items of type ITEM_CHECK -- check value. For the rest -- ignored */
#define ITEM_CHECKED 0x00000002
/* Types are mutually exclusive, only one may be present. Only ITEM_CHECK has
   semantical value, the rest are somewhat "informational." However, they can
   be used to sample menu layout this way: setup "sampling" UI driver and call
   ui(). Then you can safely return codes for items of type ITEM_SUBMENU, so you
   can go through the entire menu structure. It's a little cumbersome but should
   work. */
/* Item selection causes immediate action */
#define ITEM_ACTION  0x00000100
/* Item selection causes submenu */
#define ITEM_SUBMENU 0x00000200
/* Item is toggle-type checkbox */
#define ITEM_CHECK   0x00000400
/* Item selection invokes file selection dialog */
#define ITEM_FILESEL 0x00000800
/* This is type modifier, can be combined with other types. Means that
   the last parameter of Select will contain selection type */
#define ITEM_MULTI   0x00001000


#define MENU_END { NULL, 0, NULL, NULL, NULL, 0 }

/* UI driver entry prototypes */
/* Select provides simple selection from menu. pTitle can be used as a caption. If bFloat!=0
   the menu does not have caption and is of submenu type (or floating menu). nDefault is 
   initially selected item (not item # but rather retval from menu structure. menu is array
   of items of type tMenuItem, must be termintated by MENU_END. The last argument is used
   to specify kind of selection for items with type modifier ITEM_MULTI. See below for possible
   return values. */
typedef int (*fnSelect)(char* pTitle, int bFloat, int nDefault, tMenuItem* menu, int* seltype);
/* GetSaveFilename returns short filename in pFilename. It is guaranteed to be no more
   than FILENAME_SIZE characters in the name */
typedef int (*fnGetSaveFilename)(char* pFilename);
/* GetLoadFilename returns fully qualified file name. Selection starts in pDirectory */
typedef int (*fnGetLoadFilename)(char* pDirectory, char* pFilename);
/* Message is just some kind of MessageBox */
typedef void (*fnMessage)(char* pMessage);
/* AboutBox shows information about emulator */
typedef void (*fnAboutBox)();
/* Init is called to initialize driver every time UI code is executed. Driver must be
   protected against multiple inits */
typedef void (*fnInit)();

typedef struct
{
	fnSelect          fSelect;
	fnGetSaveFilename fGetSaveFilename;
	fnGetLoadFilename fGetLoadFilename;
	fnMessage         fMessage;
	fnAboutBox        fAboutBox;
	fnInit            fInit;
} tUIDriver;

/* Values returned in the last argument of Select */
#define USER_SELECT 0x01
#define USER_TOGGLE 0x02
#define USER_OTHER  0x03

/* Current UI driver. Port can override it and set pointer to port
   specific driver */
extern tUIDriver* ui_driver;
/* "Basic" UI driver. Always present and guaranteed to work. Override
   driver may call into this one to implement filtering */
extern tUIDriver basic_ui_driver;

#endif
