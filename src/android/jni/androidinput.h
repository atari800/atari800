#include "atari.h"

struct RECT
{
	int l;
	int t;
	union {
		int r;
		int w;
	};
	union {
		int b;
		int h;
	};
};
struct POINT
{
	int x;
	int y;
};
struct joy_overlay_state
{
	int ovl_visible;

	struct RECT joyarea;
	float  areaopacitycur;
	float  areaopacityset;
	int    areaopacityfrm;
	int    anchor;
	float  deadarea;
	float  gracearea;

	struct POINT joystick;
	float  stickopacity;

	struct POINT fire;
	float  fireopacity;
	int    firewid;
};

enum con_vst {
	COVL_HIDDEN = 0,
	COVL_FADEIN,
	COVL_READY,
	COVL_FADEOUT
};
enum con_key {
	CONK_NOKEY = -1,
	CONK_HELP = 0,
	CONK_START,
	CONK_SELECT,
	CONK_OPTION,
	CONK_RESET
};
struct consolekey_overlay_state
{
	enum con_vst ovl_visible;

	UWORD  *keycoo;
	struct RECT bbox;
	float  opacity;
	enum   con_key hitkey;
	int    statecnt;
	int    resetcnt;
	int    hotlen;

	#define COVL_MAX_OPACITY 0.5f
	#define COVL_HOLD_TIME   150

	#define RESET_SOFT 30
	#define RESET_HARD 60
};

extern struct joy_overlay_state AndroidInput_JoyOvl;
extern struct consolekey_overlay_state AndroidInput_ConOvl;

extern UWORD Android_PortStatus;
extern UBYTE Android_TrigStatus;

enum
{
	SOFTJOY_LEFT = 0,
	SOFTJOY_RIGHT,
	SOFTJOY_UP,
	SOFTJOY_DOWN,
	SOFTJOY_FIRE,
	SOFTJOY_MAXKEYS
};
#define SOFTJOY_MAXACTIONS 3
#define SOFTJOY_ACTIONBASE SOFTJOY_MAXKEYS
#define ACTION_NONE 0xFF
extern SWORD softjoymap[SOFTJOY_MAXKEYS + SOFTJOY_MAXACTIONS][2];

extern int Android_SoftjoyEnable;
extern int Android_Joyleft;
extern float Android_Splitpct;
extern int Android_Split;
extern int Android_DerotateKeys;
extern int Android_Paddle;
extern int Android_PlanetaryDefense;
extern SWORD Android_POTX;
extern SWORD Android_POTY;
extern UBYTE Android_ReversePddle;

int  Android_TouchEvent(int x1, int y1, int s1, int x2, int y2, int s2);
void Android_KeyEvent(int k, int s);
void Input_Initialize(void);
void Keyboard_Enqueue(int key);
int  Keyboard_Dequeue(void);
int  Keyboard_Peek(void);
void Android_SplitCalc(void);
void Joy_Reposition(void);
