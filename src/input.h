#ifndef _INPUT_H_
#define _INPUT_H_

/* key_code values */
#define AKEY_NONE -1

#define AKEY_SHFT 0x40
#define AKEY_CTRL 0x80
#define AKEY_SHFTCTRL 0xc0

#define AKEY_0 0x32
#define AKEY_1 0x1f
#define AKEY_2 0x1e
#define AKEY_3 0x1a
#define AKEY_4 0x18
#define AKEY_5 0x1d
#define AKEY_6 0x1b
#define AKEY_7 0x33
#define AKEY_8 0x35
#define AKEY_9 0x30

#define AKEY_CTRL_0 (AKEY_CTRL | AKEY_0)
#define AKEY_CTRL_1 (AKEY_CTRL | AKEY_1)
#define AKEY_CTRL_2 (AKEY_CTRL | AKEY_2)
#define AKEY_CTRL_3 (AKEY_CTRL | AKEY_3)
#define AKEY_CTRL_4 (AKEY_CTRL | AKEY_4)
#define AKEY_CTRL_5 (AKEY_CTRL | AKEY_5)
#define AKEY_CTRL_6 (AKEY_CTRL | AKEY_6)
#define AKEY_CTRL_7 (AKEY_CTRL | AKEY_7)
#define AKEY_CTRL_8 (AKEY_CTRL | AKEY_8)
#define AKEY_CTRL_9 (AKEY_CTRL | AKEY_9)

#define AKEY_a 0x3f
#define AKEY_b 0x15
#define AKEY_c 0x12
#define AKEY_d 0x3a
#define AKEY_e 0x2a
#define AKEY_f 0x38
#define AKEY_g 0x3d
#define AKEY_h 0x39
#define AKEY_i 0x0d
#define AKEY_j 0x01
#define AKEY_k 0x05
#define AKEY_l 0x00
#define AKEY_m 0x25
#define AKEY_n 0x23
#define AKEY_o 0x08
#define AKEY_p 0x0a
#define AKEY_q 0x2f
#define AKEY_r 0x28
#define AKEY_s 0x3e
#define AKEY_t 0x2d
#define AKEY_u 0x0b
#define AKEY_v 0x10
#define AKEY_w 0x2e
#define AKEY_x 0x16
#define AKEY_y 0x2b
#define AKEY_z 0x17

#define AKEY_A (AKEY_SHFT | AKEY_a)
#define AKEY_B (AKEY_SHFT | AKEY_b)
#define AKEY_C (AKEY_SHFT | AKEY_c)
#define AKEY_D (AKEY_SHFT | AKEY_d)
#define AKEY_E (AKEY_SHFT | AKEY_e)
#define AKEY_F (AKEY_SHFT | AKEY_f)
#define AKEY_G (AKEY_SHFT | AKEY_g)
#define AKEY_H (AKEY_SHFT | AKEY_h)
#define AKEY_I (AKEY_SHFT | AKEY_i)
#define AKEY_J (AKEY_SHFT | AKEY_j)
#define AKEY_K (AKEY_SHFT | AKEY_k)
#define AKEY_L (AKEY_SHFT | AKEY_l)
#define AKEY_M (AKEY_SHFT | AKEY_m)
#define AKEY_N (AKEY_SHFT | AKEY_n)
#define AKEY_O (AKEY_SHFT | AKEY_o)
#define AKEY_P (AKEY_SHFT | AKEY_p)
#define AKEY_Q (AKEY_SHFT | AKEY_q)
#define AKEY_R (AKEY_SHFT | AKEY_r)
#define AKEY_S (AKEY_SHFT | AKEY_s)
#define AKEY_T (AKEY_SHFT | AKEY_t)
#define AKEY_U (AKEY_SHFT | AKEY_u)
#define AKEY_V (AKEY_SHFT | AKEY_v)
#define AKEY_W (AKEY_SHFT | AKEY_w)
#define AKEY_X (AKEY_SHFT | AKEY_x)
#define AKEY_Y (AKEY_SHFT | AKEY_y)
#define AKEY_Z (AKEY_SHFT | AKEY_z)

#define AKEY_CTRL_a (AKEY_CTRL | AKEY_a)
#define AKEY_CTRL_b (AKEY_CTRL | AKEY_b)
#define AKEY_CTRL_c (AKEY_CTRL | AKEY_c)
#define AKEY_CTRL_d (AKEY_CTRL | AKEY_d)
#define AKEY_CTRL_e (AKEY_CTRL | AKEY_e)
#define AKEY_CTRL_f (AKEY_CTRL | AKEY_f)
#define AKEY_CTRL_g (AKEY_CTRL | AKEY_g)
#define AKEY_CTRL_h (AKEY_CTRL | AKEY_h)
#define AKEY_CTRL_i (AKEY_CTRL | AKEY_i)
#define AKEY_CTRL_j (AKEY_CTRL | AKEY_j)
#define AKEY_CTRL_k (AKEY_CTRL | AKEY_k)
#define AKEY_CTRL_l (AKEY_CTRL | AKEY_l)
#define AKEY_CTRL_m (AKEY_CTRL | AKEY_m)
#define AKEY_CTRL_n (AKEY_CTRL | AKEY_n)
#define AKEY_CTRL_o (AKEY_CTRL | AKEY_o)
#define AKEY_CTRL_p (AKEY_CTRL | AKEY_p)
#define AKEY_CTRL_q (AKEY_CTRL | AKEY_q)
#define AKEY_CTRL_r (AKEY_CTRL | AKEY_r)
#define AKEY_CTRL_s (AKEY_CTRL | AKEY_s)
#define AKEY_CTRL_t (AKEY_CTRL | AKEY_t)
#define AKEY_CTRL_u (AKEY_CTRL | AKEY_u)
#define AKEY_CTRL_v (AKEY_CTRL | AKEY_v)
#define AKEY_CTRL_w (AKEY_CTRL | AKEY_w)
#define AKEY_CTRL_x (AKEY_CTRL | AKEY_x)
#define AKEY_CTRL_y (AKEY_CTRL | AKEY_y)
#define AKEY_CTRL_z (AKEY_CTRL | AKEY_z)

#define AKEY_CTRL_A (AKEY_CTRL | AKEY_A)
#define AKEY_CTRL_B (AKEY_CTRL | AKEY_B)
#define AKEY_CTRL_C (AKEY_CTRL | AKEY_C)
#define AKEY_CTRL_D (AKEY_CTRL | AKEY_D)
#define AKEY_CTRL_E (AKEY_CTRL | AKEY_E)
#define AKEY_CTRL_F (AKEY_CTRL | AKEY_F)
#define AKEY_CTRL_G (AKEY_CTRL | AKEY_G)
#define AKEY_CTRL_H (AKEY_CTRL | AKEY_H)
#define AKEY_CTRL_I (AKEY_CTRL | AKEY_I)
#define AKEY_CTRL_J (AKEY_CTRL | AKEY_J)
#define AKEY_CTRL_K (AKEY_CTRL | AKEY_K)
#define AKEY_CTRL_L (AKEY_CTRL | AKEY_L)
#define AKEY_CTRL_M (AKEY_CTRL | AKEY_M)
#define AKEY_CTRL_N (AKEY_CTRL | AKEY_N)
#define AKEY_CTRL_O (AKEY_CTRL | AKEY_O)
#define AKEY_CTRL_P (AKEY_CTRL | AKEY_P)
#define AKEY_CTRL_Q (AKEY_CTRL | AKEY_Q)
#define AKEY_CTRL_R (AKEY_CTRL | AKEY_R)
#define AKEY_CTRL_S (AKEY_CTRL | AKEY_S)
#define AKEY_CTRL_T (AKEY_CTRL | AKEY_T)
#define AKEY_CTRL_U (AKEY_CTRL | AKEY_U)
#define AKEY_CTRL_V (AKEY_CTRL | AKEY_V)
#define AKEY_CTRL_W (AKEY_CTRL | AKEY_W)
#define AKEY_CTRL_X (AKEY_CTRL | AKEY_X)
#define AKEY_CTRL_Y (AKEY_CTRL | AKEY_Y)
#define AKEY_CTRL_Z (AKEY_CTRL | AKEY_Z)

#define AKEY_HELP 0x11
#define AKEY_DOWN 0x8f
#define AKEY_LEFT 0x86
#define AKEY_RIGHT 0x87
#define AKEY_UP 0x8e
#define AKEY_BACKSPACE 0x34
#define AKEY_DELETE_CHAR 0xb4
#define AKEY_DELETE_LINE 0x74
#define AKEY_INSERT_CHAR 0xb7
#define AKEY_INSERT_LINE 0x77
#define AKEY_ESCAPE 0x1c
#define AKEY_ATARI 0x27
#define AKEY_CAPSLOCK 0x7c
#define AKEY_CAPSTOGGLE 0x3c
#define AKEY_TAB 0x2c
#define AKEY_SETTAB 0x6c
#define AKEY_CLRTAB 0xac
#define AKEY_RETURN 0x0c
#define AKEY_SPACE 0x21
#define AKEY_EXCLAMATION 0x5f
#define AKEY_DBLQUOTE 0x5e
#define AKEY_HASH 0x5a
#define AKEY_DOLLAR 0x58
#define AKEY_PERCENT 0x5d
#define AKEY_AMPERSAND 0x5b
#define AKEY_QUOTE 0x73
#define AKEY_AT 0x75
#define AKEY_PARENLEFT 0x70
#define AKEY_PARENRIGHT 0x72
#define AKEY_LESS 0x36
#define AKEY_GREATER 0x37
#define AKEY_EQUAL 0x0f
#define AKEY_QUESTION 0x66
#define AKEY_MINUS 0x0e
#define AKEY_PLUS 0x06
#define AKEY_ASTERISK 0x07
#define AKEY_SLASH 0x26
#define AKEY_COLON 0x42
#define AKEY_SEMICOLON 0x02
#define AKEY_COMMA 0x20
#define AKEY_FULLSTOP 0x22
#define AKEY_UNDERSCORE 0x4e
#define AKEY_BRACKETLEFT 0x60
#define AKEY_BRACKETRIGHT 0x62
#define AKEY_CIRCUMFLEX 0x47
#define AKEY_BACKSLASH 0x46
#define AKEY_BAR 0x4f
#define AKEY_CLEAR (AKEY_SHFT | AKEY_LESS)
#define AKEY_CARET (AKEY_SHFT | AKEY_ASTERISK)
#define AKEY_F1 0x03
#define AKEY_F2 0x04
#define AKEY_F3	0x13
#define AKEY_F4 0x14

/* Following keys cannot be read with both shift and control pressed:
   J K L ; + * Z X C V B F1 F2 F3 F4 HELP	 */

/* 5200 key codes */
#define AKEY_5200_START 0x39
#define AKEY_5200_PAUSE 0x31
#define AKEY_5200_RESET 0x29
#define AKEY_5200_0 0x25
#define AKEY_5200_1 0x3f
#define AKEY_5200_2 0x3d
#define AKEY_5200_3 0x3b
#define AKEY_5200_4 0x37
#define AKEY_5200_5 0x35
#define AKEY_5200_6 0x33
#define AKEY_5200_7 0x2f
#define AKEY_5200_8 0x2d
#define AKEY_5200_9 0x2b
#define AKEY_5200_HASH 0x23
#define AKEY_5200_ASTERISK 0x27

/* key_consol masks */
/* Note: key_consol should be CONSOL_NONE if no consol key is pressed.
   When a consol key is pressed, corresponding bit should be cleared.
 */
#define CONSOL_NONE		0x07
#define CONSOL_START	0x01
#define CONSOL_SELECT	0x02
#define CONSOL_OPTION	0x04

extern int key_code;	/* regular Atari key code */
extern int key_shift;	/* Shift key pressed */
extern int key_consol;	/* Start, Select and Option keys */

/* Joysticks ----------------------------------------------------------- */

/* joystick position */
#define	STICK_LL		0x09
#define	STICK_BACK		0x0d
#define	STICK_LR		0x05
#define	STICK_LEFT		0x0b
#define	STICK_CENTRE	0x0f
#define	STICK_RIGHT		0x07
#define	STICK_UL		0x0a
#define	STICK_FORWARD	0x0e
#define	STICK_UR		0x06

/* joy_autofire values */
#define AUTOFIRE_OFF	0
#define AUTOFIRE_FIRE	1	/* Fire dependent */
#define AUTOFIRE_CONT	2	/* Continuous */

extern int joy_autofire[4];		/* autofire mode for each Atari port */

extern int joy_block_opposite_directions;	/* can't move joystick left
											   and right simultaneously */

extern int joy_multijoy;	/* emulate MultiJoy4 interface */

/* 5200 joysticks values */
extern int joy_5200_min;
extern int joy_5200_center;
extern int joy_5200_max;

/* Mouse --------------------------------------------------------------- */

/* mouse_mode values */
#define MOUSE_OFF		0
#define MOUSE_PAD		1	/* Paddles */
#define MOUSE_TOUCH		2	/* Atari touch tablet */
#define MOUSE_KOALA		3	/* Koala pad */
#define MOUSE_PEN		4	/* Light pen */
#define MOUSE_GUN		5	/* Light gun */
#define MOUSE_AMIGA		6	/* Amiga mouse */
#define MOUSE_ST		7	/* Atari ST mouse */
#define MOUSE_TRAK		8	/* Atari CX22 Trak-Ball */
#define MOUSE_JOY		9	/* Joystick */

extern int mouse_mode;			/* device emulated with mouse */
extern int mouse_port;			/* Atari port, to which the emulated device is attached */
extern int mouse_delta_x;		/* x motion since last frame */
extern int mouse_delta_y;		/* y motion since last frame */
extern int mouse_buttons;		/* buttons (b0=1: first button pressed, b1=1: 2nd pressed, etc. */
extern int mouse_speed;			/* how fast the mouse pointer moves */
extern int mouse_pot_min;		/* min. value of POKEY's POT register */
extern int mouse_pot_max;		/* max. value of POKEY's POT register */
extern int mouse_pen_ofs_h;		/* light pen/gun horizontal offset (for calibration) */
extern int mouse_pen_ofs_v;		/* light pen/gun vertical offset (for calibration) */
extern int mouse_joy_inertia;	/* how long the mouse pointer can move (time in Atari frames)
								   after a fast motion of mouse */

/* Functions ----------------------------------------------------------- */

void INPUT_Initialise(int *argc, char *argv[]);
void INPUT_Frame(void);
void INPUT_Scanline(void);
void INPUT_SelectMultiJoy(int no);
void INPUT_CenterMousePointer(void);
void INPUT_DrawMousePointer(void);

#endif /* _INPUT_H_ */
