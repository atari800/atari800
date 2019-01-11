#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#ifndef UBYTE
#define UBYTE unsigned char
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

#include <setjmp.h>
extern jmp_buf libatari800_cpu_crash;

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
    ULONG cpu;
    ULONG pc;
    ULONG base_ram;
    ULONG base_ram_attrib;
    ULONG antic;
    ULONG gtia;
    ULONG pia;
    ULONG pokey;
} statesav_tags_t;

extern int libatari800_unidentified_cart_type;

int libatari800_init(int argc, char **argv);

void libatari800_clear_input_array(input_template_t *input);

int libatari800_next_frame(input_template_t *input);

int libatari800_mount_disk_image(int diskno, const char *filename, int readonly);

int libatari800_reboot_with_file(const char *filename);

UBYTE *libatari800_get_main_memory_ptr();

UBYTE *libatari800_get_screen_ptr();

void libatari800_get_current_state(UBYTE *state, statesav_tags_t *tags);

void libatari800_restore_state(UBYTE *state);

#endif /* LIBATARI800_H_ */
