#define STATIC_MAXKEYS 256

#define KEY_SHIFT     256 
#define KEY_CONTROL   257
#define KEY_BACKSPACE 255
#define KEY_UP        254
#define KEY_DOWN      253
#define KEY_LEFT      252
#define KEY_RIGHT     251
#define KEY_FIRE      250
#define KEY_ENTER     249
#define KEY_ESCAPE    248
#define KEY_BREAK     242

/* Disable warnings caused by initializing some elements of skeyxlat twice. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winitializer-overrides"
static const SWORD skeyxlat[STATIC_MAXKEYS] = {
	[0 ... (STATIC_MAXKEYS - 1)] = AKEY_NONE,

	['0'] = AKEY_0,
	['1'] = AKEY_1,
	['2'] = AKEY_2,
	['3'] = AKEY_3,
	['4'] = AKEY_4,
	['5'] = AKEY_5,
	['6'] = AKEY_6,
	['7'] = AKEY_7,
	['8'] = AKEY_8,
	['9'] = AKEY_9,

	['a'] = AKEY_a,
	['b'] = AKEY_b,
	['c'] = AKEY_c,
	['d'] = AKEY_d,
	['e'] = AKEY_e,
	['f'] = AKEY_f,
	['g'] = AKEY_g,
	['h'] = AKEY_h,
	['i'] = AKEY_i,
	['j'] = AKEY_j,
	['k'] = AKEY_k,
	['l'] = AKEY_l,
	['m'] = AKEY_m,
	['n'] = AKEY_n,
	['o'] = AKEY_o,
	['p'] = AKEY_p,
	['q'] = AKEY_q,
	['r'] = AKEY_r,
	['s'] = AKEY_s,
	['t'] = AKEY_t,
	['u'] = AKEY_u,
	['v'] = AKEY_v,
	['w'] = AKEY_w,
	['x'] = AKEY_x,
	['y'] = AKEY_y,
	['z'] = AKEY_z,

	['A'] = AKEY_A,
	['B'] = AKEY_B,
	['C'] = AKEY_C,
	['D'] = AKEY_D,
	['E'] = AKEY_E,
	['F'] = AKEY_F,
	['G'] = AKEY_G,
	['H'] = AKEY_H,
	['I'] = AKEY_I,
	['J'] = AKEY_J,
	['K'] = AKEY_K,
	['L'] = AKEY_L,
	['M'] = AKEY_M,
	['N'] = AKEY_N,
	['O'] = AKEY_O,
	['P'] = AKEY_P,
	['Q'] = AKEY_Q,
	['R'] = AKEY_R,
	['S'] = AKEY_S,
	['T'] = AKEY_T,
	['U'] = AKEY_U,
	['V'] = AKEY_V,
	['W'] = AKEY_W,
	['X'] = AKEY_X,
	['Y'] = AKEY_Y,
	['Z'] = AKEY_Z,

	['\e'] = AKEY_ESCAPE,
	['~']  = AKEY_ESCAPE,
	['\t'] = AKEY_TAB,
	['\n'] = AKEY_RETURN,
	[' ']  = AKEY_SPACE,
	['!']  = AKEY_EXCLAMATION,
	['\"'] = AKEY_DBLQUOTE,
	['#']  = AKEY_HASH,
	['$']  = AKEY_DOLLAR,
	['%']  = AKEY_PERCENT,
	['&']  = AKEY_AMPERSAND,
	['\''] = AKEY_QUOTE,
	['@']  = AKEY_AT,
	['(']  = AKEY_PARENLEFT,
	[')']  = AKEY_PARENRIGHT,
	['<']  = AKEY_LESS,
	['>']  = AKEY_GREATER,
	['=']  = AKEY_EQUAL,
	['?']  = AKEY_QUESTION,
	['-']  = AKEY_MINUS,
	['+']  = AKEY_PLUS,
	['*']  = AKEY_ASTERISK,
	['/']  = AKEY_SLASH,
	[':']  = AKEY_COLON,
	[';']  = AKEY_SEMICOLON,
	[',']  = AKEY_COMMA,
	['.']  = AKEY_FULLSTOP,
	['_']  = AKEY_UNDERSCORE,
	['[']  = AKEY_BRACKETLEFT,
	[']']  = AKEY_BRACKETRIGHT,
	['^']  = AKEY_CIRCUMFLEX,
	['\\'] = AKEY_BACKSLASH,
	['|']  = AKEY_BAR,
	['`']  = AKEY_CAPSTOGGLE,
	['{']  = AKEY_ATARI,

	[KEY_BACKSPACE] = AKEY_BACKSPACE,
	[KEY_UP       ] = AKEY_UP,
	[KEY_DOWN     ] = AKEY_DOWN,
	[KEY_LEFT     ] = AKEY_LEFT,
	[KEY_RIGHT    ] = AKEY_RIGHT,
	[KEY_ENTER    ] = AKEY_RETURN,
	[KEY_ESCAPE   ] = AKEY_ESCAPE,
	[KEY_BREAK    ] = AKEY_BREAK,



/* unmapped codes
#define AKEY_HELP 0x11
#define AKEY_DELETE_CHAR 0xb4
#define AKEY_DELETE_LINE 0x74
#define AKEY_INSERT_CHAR 0xb7
#define AKEY_INSERT_LINE 0x77
#define AKEY_SETTAB 0x6c
#define AKEY_CLRTAB 0xac
*/
};
#pragma GCC diagnostic pop
