#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#ifndef UBYTE
#define UBYTE unsigned char
#endif

#ifndef UWORD
#define UWORD unsigned short
#endif

#ifndef ULONG
#include <stdint.h>
#define ULONG uint32_t
#endif

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

/* Special keys for use as input_template_t.keycode values. These defines are
   from akey.h in the atari800 source, but are copied here as the internal
   headers aren't available when libatari800 is installed as a library. */
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
#define AKEY_F3 0x13
#define AKEY_F4 0x14

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

typedef struct {
    UBYTE keychar;
    UBYTE keycode;
    UBYTE special;
    UBYTE shift;
    UBYTE control;
    UBYTE start;
    UBYTE select;
    UBYTE option;
    UBYTE joy0;
    UBYTE trig0;
    UBYTE joy1;
    UBYTE trig1;
    UBYTE joy2;
    UBYTE trig2;
    UBYTE joy3;
    UBYTE trig3;
    UBYTE mousex;
    UBYTE mousey;
    UBYTE mouse_buttons;
    UBYTE mouse_mode;
} input_template_t;


#define STATESAV_MAX_SIZE 210000

/* byte offsets into output_template.state array of groups of data
   to prevent the need for a full parsing of the save state data to
   be able to find parts needed for the visualizer display
 */
typedef struct {
    ULONG size;
    ULONG cpu;
    ULONG pc;
    ULONG base_ram;
    ULONG base_ram_attrib;
    ULONG antic;
    ULONG gtia;
    ULONG pia;
    ULONG pokey;
} statesav_tags_t;

typedef struct {
    UBYTE selftest_enabled;
    UBYTE _align1[3];
    ULONG nframes;
    ULONG sample_residual;
} statesav_flags_t;

typedef struct {
    union {
        statesav_tags_t tags;
        UBYTE tags_storage[128];
    };
    union {
        statesav_flags_t flags;
        UBYTE flags_storage[128];
    };
    UBYTE state[STATESAV_MAX_SIZE];
} emulator_state_t;

typedef struct {
    UBYTE A;
    UBYTE P;
    UBYTE S;
    UBYTE X;
    UBYTE Y;
    UBYTE IRQ;
} cpu_state_t;

typedef struct {
    UWORD PC;
} pc_state_t;

typedef struct {
    UBYTE DMACTL;
    UBYTE CHACTL;
    UBYTE HSCROL;
    UBYTE VSCROL;
    UBYTE PMBASE;
    UBYTE CHBASE;
    UBYTE NMIEN;
    UBYTE NMIST;
    UBYTE IR;
    UBYTE anticmode;
    UBYTE dctr;
    UBYTE lastline;
    UBYTE need_dl;
    UBYTE vscrol_off;
    UWORD dlist;
    UWORD screenaddr;
    int xpos;
    int xpos_limit;
    int ypos;
} antic_state_t;

typedef struct {
    UBYTE HPOSP0;
    UBYTE HPOSP1;
    UBYTE HPOSP2;
    UBYTE HPOSP3;
    UBYTE HPOSM0;
    UBYTE HPOSM1;
    UBYTE HPOSM2;
    UBYTE HPOSM3;
    UBYTE PF0PM;
    UBYTE PF1PM;
    UBYTE PF2PM;
    UBYTE PF3PM;
    UBYTE M0PL;
    UBYTE M1PL;
    UBYTE M2PL;
    UBYTE M3PL;
    UBYTE P0PL;
    UBYTE P1PL;
    UBYTE P2PL;
    UBYTE P3PL;
    UBYTE SIZEP0;
    UBYTE SIZEP1;
    UBYTE SIZEP2;
    UBYTE SIZEP3;
    UBYTE SIZEM;
    UBYTE GRAFP0;
    UBYTE GRAFP1;
    UBYTE GRAFP2;
    UBYTE GRAFP3;
    UBYTE GRAFM;
    UBYTE COLPM0;
    UBYTE COLPM1;
    UBYTE COLPM2;
    UBYTE COLPM3;
    UBYTE COLPF0;
    UBYTE COLPF1;
    UBYTE COLPF2;
    UBYTE COLPF3;
    UBYTE COLBK;
    UBYTE PRIOR;
    UBYTE VDELAY;
    UBYTE GRACTL;
} gtia_state_t;

typedef struct {
    UBYTE KBCODE;
    UBYTE IRQST;
    UBYTE IRQEN;
    UBYTE SKCTL;

    int shift_key;
    int keypressed;
    int DELAYED_SERIN_IRQ;
    int DELAYED_SEROUT_IRQ;
    int DELAYED_XMTDONE_IRQ;

    UBYTE AUDF[4];
    UBYTE AUDC[4];
    UBYTE AUDCTL[4];

    int DivNIRQ[4];
    int DivNMax[4];
    int Base_mult[4];
} pokey_state_t;

extern int libatari800_error_code;
#define LIBATARI800_UNIDENTIFIED_CART_TYPE 1
#define LIBATARI800_CPU_CRASH 2
#define LIBATARI800_BRK_INSTRUCTION 3
#define LIBATARI800_DLIST_ERROR 4
#define LIBATARI800_SELF_TEST 5
#define LIBATARI800_MEMO_PAD 6
#define LIBATARI800_INVALID_ESCAPE_OPCODE 7

int libatari800_init(int argc, char **argv);

const char *libatari800_error_message();

void libatari800_continue_emulation_on_brk(int cont);

void libatari800_clear_input_array(input_template_t *input);

int libatari800_next_frame(input_template_t *input);

int libatari800_mount_disk_image(int diskno, const char *filename, int readonly);

int libatari800_reboot_with_file(const char *filename);

UBYTE *libatari800_get_main_memory_ptr();

UBYTE *libatari800_get_screen_ptr();

UBYTE *libatari800_get_sound_buffer();

int libatari800_get_sound_buffer_len();

int libatari800_get_sound_buffer_allocated_size();

int libatari800_get_sound_frequency();

int libatari800_get_num_sound_channels();

int libatari800_get_sound_sample_size();

float libatari800_get_fps();

int libatari800_get_frame_number();

void libatari800_get_current_state(emulator_state_t *state);

void libatari800_restore_state(emulator_state_t *state);

void libatari800_exit();

#endif /* LIBATARI800_H_ */
