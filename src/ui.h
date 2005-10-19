#ifndef _UI_H_
#define _UI_H_

#include "config.h"
#include <stdio.h> /* FILENAME_MAX */
#include "atari.h"

/* Two legitimate entries to UI module. */
int SelectCartType(int k);
void ui(void);

extern int ui_is_active;
extern int alt_function;

#ifdef CRASH_MENU
extern int crash_code;
extern UWORD crash_address;
extern UWORD crash_afterCIM;
#endif

#define MAX_DIRECTORIES 8

extern char atari_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
extern char saved_files_dir[MAX_DIRECTORIES][FILENAME_MAX];
extern int n_atari_files_dir;
extern int n_saved_files_dir;

/* Structure of menu item. Each menu is just an array of items of this structure
   terminated by MENU_END */
typedef struct
{
	char sig[5];   /* Signature identifying item function. Conventionally, four-letter string */
	UBYTE flags;   /* Flags, see values below */
	char *prefix;  /* Text to prepend the item */
	char *item;    /* Main item text */
	char *suffix;  /* Optional (not always) text to show after the item text. Often key shortcut */
	int   retval;  /* Value returned by Select when this item is selected */
} tMenuItem;

/* Set if item is enabled. Disabled items will not be shown */
#define ITEM_ENABLED 0x01
/* For items of type ITEM_CHECK -- check value. For the rest -- ignored */
#define ITEM_CHECKED 0x02
/* Types are mutually exclusive, only one may be present. Only ITEM_CHECK has
   semantical value, the rest are somewhat "informational." However, they can
   be used to sample menu layout this way: setup "sampling" UI driver and call
   ui(). Then you can safely return codes for items of type ITEM_SUBMENU, so you
   can go through the entire menu structure. It's a little cumbersome but should
   work. */
/* Item selection causes immediate action */
#define ITEM_ACTION  0x04
/* Item selection causes submenu */
#define ITEM_SUBMENU 0x08
/* Item is toggle-type checkbox */
#define ITEM_CHECK   0x10
/* Item selection invokes file selection dialog */
#define ITEM_FILESEL 0x20
/* This is type modifier, can be combined with other types. Means that
   the last parameter of Select will contain selection type */
#define ITEM_MULTI   0x40

#if defined(PS2) || defined(_WIN32_WCE)
/* No function keys nor Alt+letter on PlayStation 2 and Windows CE */
#define MENU_ACCEL(keystroke) NULL
#else
#define MENU_ACCEL(keystroke) keystroke
#endif

#define MENU_END { "", 0, NULL, NULL, NULL, 0 }

/* UI driver entry prototypes */

/* Select provides simple selection from menu.
   pTitle can be used as a caption.
   If bFloat!=0 the menu does not have caption and is of submenu type (or floating menu).
   nDefault is initially selected item (not item # but rather retval from menu structure).
   menu is array of items of type tMenuItem, must be termintated by MENU_END.
   The last argument is used to specify kind of selection for items with type modifier ITEM_MULTI.
   See below for possible return values. */
typedef int (*fnSelect)(const char *pTitle, int bFloat, int nDefault, tMenuItem *menu, int *seltype);
/* SelectInt returns an integer chosen by the user from the range min_value..max_value.
   default_value is the initial selection and the value returned if the selection is cancelled. */
typedef int (*fnSelectInt)(int nDefault, int nMin, int nMax);
/* EditString provides string input. pString is shown initially and can be modified by the user.
   It won't exceed nSize characters, including NUL. Note that pString may be modified even
   when the user pressed Esc. */
typedef int (*fnEditString)(const char *pTitle, char *pString, int nSize);
/* GetSaveFilename and GetLoadFilename return fully qualified file name via pFilename.
   pDirectories are "favourite" directories (there are nDirectories of them).
   Selection starts in the directory of the passed pFilename (i.e. pFilename must be initialized)
   or (if pFilename[0] == '\0') in the first "favourite" directory. */
typedef int (*fnGetSaveFilename)(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories);
typedef int (*fnGetLoadFilename)(char *pFilename, char pDirectories[][FILENAME_MAX], int nDirectories);
/* GetDirectoryPath is a directory browser */
typedef int (*fnGetDirectoryPath)(char *pDirectory);
/* Message is just some kind of MessageBox */
typedef void (*fnMessage)(const char *pMessage);
/* AboutBox shows information about emulator */
typedef void (*fnAboutBox)(void);
/* Init is called to initialize driver every time UI code is executed. Driver must be
   protected against multiple inits */
typedef void (*fnInit)(void);

typedef struct
{
	fnSelect           fSelect;
	fnSelectInt        fSelectInt;
    fnEditString       fEditString;
	fnGetSaveFilename  fGetSaveFilename;
	fnGetLoadFilename  fGetLoadFilename;
    fnGetDirectoryPath fGetDirectoryPath;
	fnMessage          fMessage;
	fnAboutBox         fAboutBox;
	fnInit             fInit;
} tUIDriver;

/* Values returned in the last argument of Select */
#define USER_SELECT 0x01
#define USER_TOGGLE 0x02
#define USER_OTHER  0x03

/* Current UI driver. Port can override it and set pointer to port
   specific driver */
extern tUIDriver *ui_driver;
/* "Basic" UI driver. Always present and guaranteed to work. Override
   driver may call into this one to implement filtering */
extern tUIDriver basic_ui_driver;

#endif
