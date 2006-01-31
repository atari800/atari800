#ifndef _UI_H_
#define _UI_H_

#include "config.h"
#include <stdio.h> /* FILENAME_MAX */
#include "atari.h"

/* Three legitimate entries to UI module. */
int SelectCartType(int k);
void ui(void);

#ifdef KB_UI
int kb_ui(const char *title, int layout);
/* layout must be one of MACHINE_* values (see atari.h)
   or -1 for base keyboard with no function keys */
#endif

#ifdef DREAMCAST
extern const unsigned char key_to_ascii[256];
#endif

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
	UWORD flags;   /* Flags, see values below */
	SWORD retval;  /* Value returned by Select when this item is selected */
	               /* < 0 means that item is strictly informative and cannot be selected */
	char *prefix;  /* Text to prepend the item */
	char *item;    /* Main item text */
	char *suffix;  /* Optional text to show after the item text (e.g. key shortcut) */
	               /* or (if (flags & ITEM_TIP) != 0) "tooltip" */
} tMenuItem;

/* The following are item types, mutually exclusive. */
#define ITEM_HIDDEN  0x00  /* Item does not appear in the menu */
#define ITEM_ACTION  0x01  /* Item invokes an action */
#define ITEM_CHECK   0x02  /* Item represents a boolean value */
#define ITEM_FILESEL 0x03  /* Item invokes file/directory selection */
#define ITEM_SUBMENU 0x04  /* Item opens a submenu */
/* ITEM_CHECK means that the value of ITEM_CHECKED is shown.
   ITEM_FILESEL and ITEM_SUBMENU are just for optional decorations,
   so the user knows what happens when he/she selects this item. */
#define ITEM_TYPE    0x0f

/* The following are bit masks and should be combined with one of the above item types. */
#define ITEM_CHECKED 0x10  /* The boolean value for ITEM_CHECK is true */
#define ITEM_TIP     0x20  /* suffix is shown when the item is selected rather than on its right */

#if defined(_WIN32_WCE) || defined(DREAMCAST)
/* No function keys nor Alt+letter on Windows CE, Sega DC */
#define MENU_ACCEL(keystroke) NULL
#else
#define MENU_ACCEL(keystroke) keystroke
#endif

#define MENU_LABEL(item)                                    { ITEM_ACTION, -1, NULL, item, NULL }
#define MENU_ACTION(retval, item)                           { ITEM_ACTION, retval, NULL, item, NULL }
#define MENU_ACTION_PREFIX(retval, prefix, item)            { ITEM_ACTION, retval, prefix, item, NULL }
#define MENU_ACTION_PREFIX_TIP(retval, prefix, item, tip)   { ITEM_ACTION | ITEM_TIP, retval, prefix, item, tip }
#define MENU_ACTION_ACCEL(retval, item, keystroke)          { ITEM_ACTION, retval, NULL, item, MENU_ACCEL(keystroke) }
#define MENU_ACTION_TIP(retval, item, tip)                  { ITEM_ACTION | ITEM_TIP, retval, NULL, item, tip }
#define MENU_CHECK(retval, item)                            { ITEM_CHECK, retval, NULL, item, NULL }
#define MENU_FILESEL(retval, item)                          { ITEM_FILESEL, retval, NULL, item, NULL }
#define MENU_FILESEL_PREFIX(retval, prefix, item)           { ITEM_FILESEL, retval, prefix, item, NULL }
#define MENU_FILESEL_PREFIX_TIP(retval, prefix, item, tip)  { ITEM_FILESEL | ITEM_TIP, retval, prefix, item, tip }
#define MENU_FILESEL_ACCEL(retval, item, keystroke)         { ITEM_FILESEL, retval, NULL, item, MENU_ACCEL(keystroke) }
#define MENU_FILESEL_TIP(retval, item, tip)                 { ITEM_FILESEL | ITEM_TIP, retval, NULL, item, tip }
#define MENU_SUBMENU(retval, item)                          { ITEM_SUBMENU, retval, NULL, item, NULL }
#define MENU_SUBMENU_SUFFIX(retval, item, suffix)           { ITEM_SUBMENU, retval, NULL, item, suffix }
#define MENU_SUBMENU_ACCEL(retval, item, keystroke)         { ITEM_SUBMENU, retval, NULL, item, MENU_ACCEL(keystroke) }
#define MENU_END                                            { ITEM_HIDDEN, 0, NULL, NULL, NULL }

/* UI driver entry prototypes */

/* Select provides simple selection from menu.
   title can be used as a caption.
   If (flags & SELECT_POPUP) != 0 the menu is a popup menu.
   default_item is initially selected item (not item # but rather retval from menu structure).
   menu is array of items of type tMenuItem, must be termintated by MENU_END.
   If seltype is non-null, it is used to return selection type (see USER_* below).
   USER_DRAG_UP and USER_DRAG_DOWN are returned only if (flags & SELECT_DRAG) != 0.
   Returned is retval of the selected item or -1 if the user cancelled selection,
   or -2 if the user pressed "magic key" (Tab). */
typedef int (*fnSelect)(const char *title, int flags, int default_item, const tMenuItem *menu, int *seltype);
/* SelectInt returns an integer chosen by the user from the range min_value..max_value.
   default_value is the initial selection and the value returned if the selection is cancelled. */
typedef int (*fnSelectInt)(int default_value, int min_value, int max_value);
/* EditString provides string input. pString is shown initially and can be modified by the user.
   It won't exceed nSize characters, including NUL. Note that pString may be modified even
   when the user pressed Esc. */
typedef int (*fnEditString)(const char *title, char *string, int size);
/* GetSaveFilename and GetLoadFilename return fully qualified file name via pFilename.
   pDirectories are "favourite" directories (there are nDirectories of them).
   Selection starts in the directory of the passed pFilename (i.e. pFilename must be initialized)
   or (if pFilename[0] == '\0') in the first "favourite" directory. */
typedef int (*fnGetSaveFilename)(char *filename, char directories[][FILENAME_MAX], int n_directories);
typedef int (*fnGetLoadFilename)(char *filename, char directories[][FILENAME_MAX], int n_directories);
/* GetDirectoryPath is a directory browser */
typedef int (*fnGetDirectoryPath)(char *directory);
/* Message is just some kind of MessageBox */
typedef void (*fnMessage)(const char *message);
/* InfoScreen displays a "long" message.
   Caution: lines in pMessage should be ended with '\0', the message should be terminated with '\n'. */
typedef void (*fnInfoScreen)(const char *title, const char *message);
/* Init is called to initialize driver every time UI code is executed.
   Driver must be protected against multiple inits. */
typedef void (*fnInit)(void);

/* Bit masks for flags */
#define SELECT_POPUP   0x01
#define SELECT_DRAG    0x02

/* Values returned via seltype */
#define USER_SELECT    1
#define USER_TOGGLE    2
#define USER_DELETE    3
#define USER_DRAG_UP   4
#define USER_DRAG_DOWN 5

typedef struct
{
	fnSelect           fSelect;
	fnSelectInt        fSelectInt;
	fnEditString       fEditString;
	fnGetSaveFilename  fGetSaveFilename;
	fnGetLoadFilename  fGetLoadFilename;
	fnGetDirectoryPath fGetDirectoryPath;
	fnMessage          fMessage;
	fnInfoScreen       fInfoScreen;
	fnInit             fInit;
} tUIDriver;

/* Current UI driver. Port can override it and set pointer to port
   specific driver */
extern tUIDriver *ui_driver;
/* "Basic" UI driver. Always present and guaranteed to work. Override
   driver may call into this one to implement filtering */
extern tUIDriver basic_ui_driver;

#endif
