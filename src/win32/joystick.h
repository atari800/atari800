#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#define NUM_STICKS 2
#define MAX_PROG_BUTTONS 9
#define STATE 0
#define ASSIGN 1
#define STICK1 0
#define STICK2 1

/* keyboard joystick modes */
typedef enum KEYJOYMODE_ {
	KEYPAD_MODE,
	KEYPAD_PLUS_MODE,
	ARROW_MODE
} KEYJOYMODE;

/* alternate joystick modes */
typedef enum ALTJOYMODE_ {
	JOY_NORMAL_MODE,
	JOY_DUAL_MODE,
	JOY_SHARED_MODE
} ALTJOYMODE;

int procjoy(int num);
int joyreacquire(int num);
int initjoystick(void);
void uninitjoystick(void);
void clearjoy(void);

typedef struct
{
  int trig;
  
  /* holds stick id, button state, and button assignment */
  int jsbutton[NUM_STICKS][MAX_PROG_BUTTONS][2];
  
  int stick;
  int stick_1;
} tjoystat;
extern tjoystat joystat;

extern int alternateJoystickMode;

#endif /* JOYSTICK_H_ */
