/*
 * atari_vga.c - DOS VGA mode specific port code
 *
 * Copyright (c) 1996 Ivo van Poorten
 * Copyright (c) 1996-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#include <dos.h>
#include <string.h>
#include "config.h"
#include "cpu.h"
#include "colours.h"
#include "input.h"
#include "ui.h"         /* for ui_is_active */
#include "log.h"
#include "sound.h"
#include "monitor.h"
#include "pcjoy.h"
#include "screen.h"

#include "dos/vga_gfx.h"

/* -------------------------------------------------------------------------- */

/* note: if KEYBOARD_EXCLUSIVE is defined, then F7 switches between
    joysticks and keyboard (ie with joy_keyboard off joysticks do not work)
#define KEYBOARD_EXCLUSIVE
*/


static int trig0;
static int stick0;
static int trig1;
static int stick1;
static int trig2;
static int stick2;
static int trig3;
static int stick3;

/*stick & trig values for analog joystick*/
static int astick;
static int atrig;

static int joyswap = FALSE;

/* this should be variables if we could move 320x200 window, but we can't :) */
/* static int first_lno = 24;
static int first_col = 32; */
#define first_lno 20 /* center 320x200 window in 384x240 screen */
#define first_col 32
static int ypos_inc = 1;
static int vga_ptr_inc = 320;
static int scr_ptr_inc = ATARI_WIDTH;

#ifdef MONITOR_BREAK
extern UBYTE break_step;  /* used to prevent switching to gfx mode while doing monitor 'step' command*/
#endif

static int video_mode=1;       /*video mode (0-3)*/
static int use_vesa=1;         /*use vesa mode?*/
static int use_vret=0;           /*control vertical retrace?*/
  static UWORD vesa_mode; /*number of vesa2 videomode*/
  static ULONG vesa_memptr; /*physical address of VESA2 Linear Frame Buffer*/
  static ULONG vesa_memsize; /*size of LFB*/
  static ULONG vesa_linelenght; /*size of line (in bytes)*/
  static ULONG vesa_linear;  /*linear address of mapped LFB*/
  static int vesa_selector;  /*selector for accessing LFB*/

static int vga_started = 0;             /*AAA needed for DOS to see text */
static int keyboard_handler_replaced = FALSE;

extern int SHIFT;
extern int ATKEYPRESSED;

static int SHIFT_LEFT = FALSE;
static int SHIFT_RIGHT = FALSE;
static int alt_key = FALSE;
extern int alt_function;
static int norepkey = FALSE;    /* for "no repeat" of some keys !RS! */

static int PC_keyboard = TRUE;

static int joy_keyboard = FALSE; /* pass joystick scancodes to atari_keyboard? */


/*joystick type for each atari joystick*/
static int joytypes[4]={joy_keyset0,joy_keyset1,joy_off,joy_off};
/*if joytypes[i]==LPT_joy, lptport[i] contains port address of according LPT*/
static int lptport[4]={0,0,0,0};
/*scancodes for joysticks emulated on keyboard*/
static int keysets[4][9]={ {82,79,80,81,75,77,71,72,73}, /*standard kb-joy on numlock */
                           {15,44,45,46,30,32,16,17,18}, /*QWE,AD,ZXC block + TAB as fire */
                           {255,255,255,255,255,255,255,255,255}, /*none*/
                           {255,255,255,255,255,255,255,255,255}};/*none*/
/*is given keyset used?*/
static int keyset_used[4]={1,1,1,1};        /*for speeding up joystick testing*/
static int keypush[256];   /* 1 if according key is pressed */
static int keyforget[256]; /* 1 if we will ignore this scancode */

static int joycfg=0;       /* 1 if joystick configuration found in atari800.cfg */

static int joy_in = FALSE;

static int js0_centre_x;
static int js0_centre_y;


/* -------------------------------------------------------------------------- */
/* JOYSTICK HANDLING                                                          */
/* -------------------------------------------------------------------------- */

/*read analog joystick 0*/
int joystick0(int *x, int *y);
/* - moved to vga_asm.s */

/*read analog joystick and set astick & atrig values*/
void read_joystick(int centre_x, int centre_y)
{
        const int threshold = 50;
        int jsx, jsy;

        astick = STICK_CENTRE;

        atrig = (joystick0(&jsx, &jsy) < 3) ? 0 : 1;

        if (jsx < (centre_x - threshold))
                astick &= 0xfb;
        if (jsx > (centre_x + threshold))
                astick &= 0xf7;
        if (jsy < (centre_y - threshold))
                astick &= 0xfe;
        if (jsy > (centre_y + threshold))
                astick &= 0xfd;
}

/*find port for given LPT joystick*/
int test_LPTjoy(int portno,int *port)
{
        int addr;

        if (portno < 1 || portno > 3)
                return FALSE;
        addr = _farpeekw(_dos_ds, (portno - 1) * 2 + 0x408);
        if (addr == 0)
                return FALSE;

        if (port!=NULL) *port=addr;

        return TRUE;
}

void read_LPTjoy(int port, int joyport)
{
        int state = STICK_CENTRE, trigger = 1, val;

        val = inportb(port + 1);
        if (!(val & (1 << 4)))
                state &= STICK_FORWARD;
        else if (!(val & (1 << 5)))
                state &= STICK_BACK;
        if (!(val & (1 << 6)))
                state &= STICK_RIGHT;
        else if ((val & (1 << 7)))
                state &= STICK_LEFT;

        if (!(val & (1 << 3)))
                trigger = 0;

        switch (joyport)
        {
          case 0:
            stick0 = state;
            trig0 = trigger;
            break;
          case 1:
            stick1 = state;
            trig1 = trigger;
            break;
          case 2:
            stick2 = state;
            trig2 = trigger;
            break;
          case 3:
            stick3 = state;
            trig3 = trigger;
            break;
        }
}

static void update_leds(void)
{
	outportb(0x60,0xed);
	asm("nop; nop");
	outportb(0x60,	(PC_keyboard ? 0 : 2)
			|(joy_keyboard ? 1 : 0));
}

/* -------------------------------------------------------------------------- */
/* KEYBOARD HANDLER                                                           */
/* -------------------------------------------------------------------------- */

unsigned int last_raw_key; /*last key seen by Atari_Keyboard() */
unsigned int raw_key;   /*key that will be processed int Atari_keyboard() */
unsigned int raw_key_r; /*key actually read from PC keyboard*/

_go32_dpmi_seginfo old_key_handler, new_key_handler;

static int control = FALSE;
static int extended_key_follows = FALSE;
volatile static int key_leave=0;
static int ki,kstick,ktrig,k_test; /*variables for key_handler*/

void key_handler(void)
{
    asm("cli; pusha");
    raw_key_r = inportb(0x60);

    if (extended_key_follows==2)
    {
      extended_key_follows=1; /*extended_key_follows==2 is used for PAUSE*/
      raw_key_r=0;
    }
    else
    {
        /*now find the extended scancode of pressed key */
        if (extended_key_follows)
        {
          key_leave=raw_key_r&0x80;
          raw_key_r=(raw_key_r & 0x7f) + 0x80;
          extended_key_follows=0;
        }
        else
        {
          key_leave=raw_key_r&0x80;
          switch (raw_key_r)
          {
            case 0xe1:
              extended_key_follows=2; /*start of pause sequence*/
              raw_key_r=0;
              break;
            case 0xe0:
              extended_key_follows=1;
              raw_key_r=0;
              break;
            default:
              raw_key_r=raw_key_r & 0x7f;
          }
        }
        /*key-joystick*/
        keypush[raw_key_r]=!key_leave;
#ifdef KEYBOARD_EXCLUSIVE
        k_test=keyforget[raw_key_r] && joy_keyboard;
#else
        k_test=keyforget[raw_key_r];
#endif
        /*forget scancodes used by keysticks*/
        if (joy_keyboard && !ui_is_active && keyforget[raw_key_r]) raw_key_r=0;

        switch (raw_key_r) {
                case 0xaa:  /*ignore 0xe0 0xaa sent by gray arrows*/
                        raw_key_r=0;
                        key_leave=0;
                        break;
                case 0x2a:
                        SHIFT_LEFT = !key_leave;
                        break;
                case 0x36:
                        SHIFT_RIGHT = !key_leave;
                        break;
				case 0x38:
						alt_key = !key_leave;					/*ALT KEY*/
						if (!alt_key) alt_function = -1;		/* no alt function */
						break;
                case 0x1d:
                case 0x9d:
                        control = !key_leave;
                        break;
                case 0x41:                                      /*F7*/
                        if (!key_leave)
                        {
                          joy_keyboard=!joy_keyboard;
                          update_leds();
#ifdef KEYBOARD_EXCLUSIVE
                          if (!joy_keyboard)
                          {
                            /* return all keyboard joysticks to default state */
                            if (joytypes[0]>=joy_keyset0 && joytypes[0]<=joy_keyset3)
                            {
                              stick0=STICK_CENTRE;
                              trig0=1;
                            }
                            if (joytypes[1]>=joy_keyset0 && joytypes[1]<=joy_keyset3)
                            {
                              stick1=STICK_CENTRE;
                              trig1=1;
                            }
                            if (joytypes[2]>=joy_keyset0 && joytypes[2]<=joy_keyset3)
                            {
                              stick2=STICK_CENTRE;
                              trig2=1;
                            }
                            if (joytypes[3]>=joy_keyset0 && joytypes[3]<=joy_keyset3)
                            {
                              stick3=STICK_CENTRE;
                              trig3=1;
                            }
                          }
                          else
                          {
                            k_test=1; /*force updatate of all keyboard joysticks*/
                          }
#endif
                        }
                        raw_key_r=0;
                        break;
                case 0x3c:
                        if (!key_leave)
                          key_consol &= ~CONSOL_OPTION;	/* OPTION key ON */
                        else
                          key_consol |= CONSOL_OPTION;	/* OPTION key OFF */
                        break;
                case 0x3d:
                        if (!key_leave)
                          key_consol &= ~CONSOL_SELECT;	/* SELECT key ON */
                        else
                          key_consol |= CONSOL_SELECT;	/* SELECT key OFF */
                        break;
                case 0x3e:
                        if (!key_leave)
                          key_consol &= ~CONSOL_START;	/* START key ON */
                        else
                          key_consol |= CONSOL_START;	/* START key OFF */
                        break;
        }

        if (k_test)  /*keystick key was pressed*/
        {
          for (ki=0;ki<4;ki++)
            if (keyset_used[ki])
            {
              kstick=STICK_CENTRE;
              if (keypush[keysets[ki][1]] || keypush[keysets[ki][4]] || keypush[keysets[ki][6]])
                kstick&=0xfb; /*left*/
              if (keypush[keysets[ki][3]] || keypush[keysets[ki][5]] || keypush[keysets[ki][8]])
                kstick&=0xf7; /*right*/
              if (keypush[keysets[ki][1]] || keypush[keysets[ki][2]] || keypush[keysets[ki][3]])
                kstick&=0xfd; /*down*/
              if (keypush[keysets[ki][6]] || keypush[keysets[ki][7]] || keypush[keysets[ki][8]])
                kstick&=0xfe; /*up*/
              ktrig=keypush[keysets[ki][0]]>0 ? 0:1;

              if (joytypes[0]==(joy_keyset0+ki))
                {  stick0=kstick;trig0=ktrig; }
              else
              if (joytypes[1]==(joy_keyset0+ki))
                {  stick1=kstick;trig1=ktrig; }
              else
              if (joytypes[2]==(joy_keyset0+ki))
                {  stick2=kstick;trig2=ktrig; }
              else
              if (joytypes[3]==(joy_keyset0+ki))
                {  stick3=kstick;trig3=ktrig; }
            }
        }

        key_shift = SHIFT_LEFT | SHIFT_RIGHT;
        raw_key_r=(key_leave?0x200:0)|raw_key_r;
        if (raw_key_r==0x200) raw_key_r=0;
        if (raw_key==last_raw_key && raw_key_r!=0) {raw_key=raw_key_r;raw_key_r=0;}
    }
    outportb(0x20, 0x20);
    asm("popa; sti");
}

void key_init(void)
{
	int i;
	for (i=0;i<256;i++) keypush[i]=0; /*none key is pressed*/
	extended_key_follows=FALSE;
	update_leds();
	raw_key_r=0;raw_key=0;
	new_key_handler.pm_offset = (int) key_handler;
	new_key_handler.pm_selector = _go32_my_cs();
	_go32_dpmi_get_protected_mode_interrupt_vector(0x9, &old_key_handler);
	_go32_dpmi_allocate_iret_wrapper(&new_key_handler);
	_go32_dpmi_set_protected_mode_interrupt_vector(0x9, &new_key_handler);
	keyboard_handler_replaced = TRUE;
}

void key_delete(void)
{
        if (keyboard_handler_replaced) {
                int kflags;
                _go32_dpmi_set_protected_mode_interrupt_vector(0x9, &old_key_handler);
                /*set the original keyboard LED settings*/
                kflags=_farpeekb(_dos_ds,0x417);
                outportb(0x60,0xed);
                asm("nop; nop");
                outportb(0x60,((kflags>>4)&0x7));
        }
}



/* -------------------------------------------------------------------------- */
/* VGA HANDLING                                                               */
/* -------------------------------------------------------------------------- */

void SetupVgaEnvironment(void)
{
        int a, i;
        union REGS rg;
        __dpmi_regs d_rg;
        UBYTE ctab[768];

        if (use_vesa)
        {
          /*try to open VESA mode*/
          use_vesa=VESA_open(vesa_mode,vesa_memptr,vesa_memsize,&vesa_linear,&vesa_selector);
        }
        if (!use_vesa) /*if '-novesa' specified or VESA_open failed */
          switch (video_mode)
          {
            case 0:
              rg.x.ax = 0x0013;
              int86(0x10, &rg, &rg); /*vga 320x200*/
              break;
            case 1:
              x_open(2);             /*xmode 320x240*/
              break;
            case 2:
            case 3:
              x_open(5);             /*xmode 320x480*/
              break;
          }

	screen_visible_x1 = first_col;
	screen_visible_x2 = first_col + 320;
	if (video_mode == 0) {
		screen_visible_y1 = first_lno;
		screen_visible_y2 = first_lno + 200;
	}
	else {
		screen_visible_y1 = 0;
		screen_visible_y2 = 240;
	}

        vga_started = 1;

        /* setting all palette at once is faster....*/
        for (a = 0, i = 0; a < 256; a++) {
          ctab[i++]=(colortable[a] >> 18) & 0x3f;
          ctab[i++]=(colortable[a] >> 10) & 0x3f;
          ctab[i++]=(colortable[a] >> 2) & 0x3f;
        }
        /*copy ctab to djgpp transfer buffer in DOS memory*/
        dosmemput(ctab,768,__tb&0xfffff);
        d_rg.x.ax=0x1012;
        d_rg.x.bx=0;
        d_rg.x.cx=256;
        d_rg.x.dx=__tb&0xf; /*offset of transfer buffer*/
        d_rg.x.es=(__tb >> 4) & 0xffff;  /*segment of transfer buffer*/
        __dpmi_int(0x10,&d_rg);  /*VGA set palette block*/

	/* initialize mouse */
	if (mouse_mode != MOUSE_OFF) {
		union REGS rg;
		rg.x.ax = 0;
		int86(0x33, &rg, &rg);
		if (rg.x.ax != 0xffff) {
			Aprint("Can't find mouse!");
			mouse_mode = MOUSE_OFF;
		}
	}

        key_init(); /*initialize keyboard handler*/
}

void ShutdownVgaEnvironment(void)
{
        union REGS rg;

        if (vga_started) {
                if (use_vesa)
                  VESA_close(&vesa_linear,&vesa_selector);
                else
                {
                  rg.x.ax = 0x0003;
                  int86(0x10, &rg, &rg);
                }
        }
   vga_started=FALSE;
}

void Atari_DisplayScreen(void)
{
        UBYTE *screen = (UBYTE *) atari_screen;
        static int lace = 0;
        unsigned long vga_ptr;

        UBYTE *scr_ptr;
        int ypos;

        vga_ptr = 0xa0000;
        scr_ptr = &ascreen[first_lno * ATARI_WIDTH + first_col];

        if (ypos_inc == 2) {
                if (lace) {
                        vga_ptr += 320;
                        scr_ptr += ATARI_WIDTH;
                }
                lace = 1 - lace;
        }

      if (vga_started) /*draw screen only in graphics mode*/
      {

	if(use_vret)
	  v_ret(); /*vertical retrace control */

        if (use_vesa)
            /*draw screen using VESA2*/
            switch (video_mode)
            {
              case 0:
                VESA_blit(ascreen+first_col + first_lno*ATARI_WIDTH,
                          320,200,ATARI_WIDTH,vesa_linelenght,vesa_selector);
                break;
              case 1:
                VESA_blit(ascreen+first_col,
                          320,240,ATARI_WIDTH,vesa_linelenght,vesa_selector);
                break;
              case 2:
                VESA_blit(ascreen+first_col,
                          320,240,ATARI_WIDTH,vesa_linelenght*2,vesa_selector);
                break;
              case 3:
                VESA_i_blit(ascreen+first_col,
                          320,240,ATARI_WIDTH,vesa_linelenght,vesa_selector);
                break;
            }
        else
            /*draw screen using vga or x-mode*/
            switch (video_mode)
            {
              case 0:
                for (ypos = 0; ypos < 200; ypos += ypos_inc) {
                    _dosmemputl(scr_ptr, 320 / 4, vga_ptr);

                    vga_ptr += vga_ptr_inc;
                    scr_ptr += scr_ptr_inc;
                }
                break;
              case 1:
                x_blit(ascreen+first_col,240,ATARI_WIDTH,80);
                break;
              case 2:
                x_blit(ascreen+first_col,240,ATARI_WIDTH,160);
                break;
              case 3:
                x_blit_i2(ascreen+first_col,240,ATARI_WIDTH,160);
                break;
            }
      } /* if (vga_started) */
}

/* -------------------------------------------------------------------------- */
/* CONFIG & INITIALISATION                                                    */
/* -------------------------------------------------------------------------- */

void Atari_Initialise(int *argc, char *argv[])
{
        int i;
        int j;
        int use_lpt1=0,use_lpt2=0,use_lpt3=0;
        int help_only = FALSE;

        for (i = j = 1; i < *argc; i++) {
                if (strcmp(argv[i], "-interlace") == 0) {
                        ypos_inc = 2;
                        vga_ptr_inc = 320 + 320;
                        scr_ptr_inc = ATARI_WIDTH + ATARI_WIDTH;
                }
                else if (strcmp(argv[i], "-LPTjoy1") == 0)
                        use_lpt1=test_LPTjoy(1,NULL);
                else if (strcmp(argv[i], "-LPTjoy2") == 0)
                        use_lpt2=test_LPTjoy(2,NULL);
                else if (strcmp(argv[i], "-LPTjoy3") == 0)
                        use_lpt3=test_LPTjoy(3,NULL);

                else if (strcmp(argv[i], "-joyswap") == 0)
                        joyswap = TRUE;

                else if (strcmp(argv[i], "-video") == 0)
                {
                  i++;
                  video_mode=atoi(argv[i]);
                  if (video_mode<0 || video_mode>3)
                  {
                    Aprint("Invalid video mode, using default.");
                    video_mode=0;
                  }
                }
                else if (strcmp(argv[i],"-novesa") == 0)
                {
                  use_vesa=FALSE;
                }
                else if (strcmp(argv[i],"-vretrace") == 0)
                {
                  use_vret=TRUE;
                }
		else if (strcmp(argv[i],"-keyboard") == 0)
		{
			i++;
			if (strcmp(argv[i],"0") == 0)
				PC_keyboard = TRUE;
			else
				PC_keyboard = FALSE;
		}
                else {
                        if (strcmp(argv[i], "-help") == 0) {
                                help_only = TRUE;
                                Aprint("\t-interlace       Generate screen with interlace");
                                Aprint("\t-LPTjoy1         Read joystick connected to LPT1");
                                Aprint("\t-LPTjoy2         Read joystick connected to LPT2");
                                Aprint("\t-LPTjoy3         Read joystick connected to LPT3");
                                Aprint("\t-joyswap         Swap joysticks");
                                Aprint("\t-video x         Set video mode:");
                                Aprint("\t\t0 - 320x200\n\t\t1 - 320x240");
                                Aprint("\t\t2 - 320x240, interlaced with black lines");
                                Aprint("\t\t3 - 320x240, interlaced with darker lines (slower!)");
                                Aprint("\t-novesa          Do not use vesa2 videomodes");
                                Aprint("\t-vretrace        Use vertical retrace control");
                                Aprint("\t-keyboard 0      PC keyboard layout");
                                Aprint("\t-keyboard 1      Atari keyboard layout");
                        }
                        argv[j++] = argv[i];
                }
        }

        *argc = j;

#ifdef SOUND
        /* initialise sound routines */
        Sound_Initialise(argc, argv);
#endif

        if (help_only)
                return;

        /* check if joystick is connected */
        printf("Joystick is checked...\n");
        fflush(stdout);
        outportb(0x201, 0xff);
        usleep(100000UL);
        joy_in = ((inportb(0x201) & 3) == 0);
        if (joy_in)
                joystick0(&js0_centre_x, &js0_centre_y);
        if (! joy_in)
                Aprint("Sorry, I see no joystick. Use numeric pad");

        /*find number of VESA2 video mode*/
        if (use_vesa)
        {
          switch (video_mode)
          {
            case 0:
              use_vesa=VESA_getmode(320,200,&vesa_mode,&vesa_memptr,&vesa_linelenght,&vesa_memsize);
              break;
            case 1:
              use_vesa=VESA_getmode(320,240,&vesa_mode,&vesa_memptr,&vesa_linelenght,&vesa_memsize);
              break;
            case 2:
            case 3:
              use_vesa=VESA_getmode(320,480,&vesa_mode,&vesa_memptr,&vesa_linelenght,&vesa_memsize);
              break;
          }
        }

        /* setup joystick */
        stick0 = stick1 = stick2 = stick3 = STICK_CENTRE;
        trig0 = trig1 = trig2 = trig3 = 1;
        /* for compatibility with older versions' command line parameters */
        if (use_lpt3)
        {
          for (i=3;i>0;i--) joytypes[i]=joytypes[i-1]; /*shift joystick types up*/
          joytypes[0]=joy_lpt3;   /*joystick 0 is on lpt3 */
        }
        if (use_lpt2)
        {
          for (i=3;i>0;i--) joytypes[i]=joytypes[i-1]; /*see above*/
          joytypes[0]=joy_lpt2;
        }
        if (use_lpt1)
        {
          for (i=3;i>0;i--) joytypes[i]=joytypes[i-1];
          joytypes[0]=joy_lpt1;
        }
        if (joy_in && !joycfg)
        {
          for (i=3;i>0;i--) joytypes[i]=joytypes[i-1];
          joytypes[0]=joy_analog;
        }
        if (joyswap)
        {
          int help=joytypes[0];
          joytypes[0]=joytypes[1];
          joytypes[1]=help;
        }
        /*end of compatibility part*/


        /*check, if joystick configuration is valid*/
        for (i=0;i<4;i++)
          switch (joytypes[i])
          {
            case joy_lpt1:
              if (!test_LPTjoy(1,lptport+i)) joytypes[i]=joy_off;
              break;
            case joy_lpt2:
              if (!test_LPTjoy(2,lptport+i)) joytypes[i]=joy_off;
              break;
            case joy_lpt3:
              if (!test_LPTjoy(3,lptport+i)) joytypes[i]=joy_off;
              break;
            case joy_analog:
              if (!joy_in) joytypes[i]=joy_off;
              break;
          }

        /* do not forget any scancode */
        for (i=0;i<256;i++)
          keyforget[i]=0;
        /* mark all used scancodes to forget*/
        for (i=0;i<4;i++)
        {
          j=0;
          while (j<4 && joytypes[j]!=(i+joy_keyset0)) j++;
          if (j<4)  /*keyset i is used */
            for (j=0;j<9;j++) keyforget[keysets[i][j]]=1;
          else
            keyset_used[i]=0;
        }

        SetupVgaEnvironment();

}

/* -------------------------------------------------------------------------- */
/* Atari_Configure(...) processes all lines not recognized by RtConfigLoad() */
int Atari_Configure(char* option,char* parameters)
{
  int help=0;
  int no;
  int i;

  if (strcmp(option,"VIDEO")==0)
  {
    sscanf(parameters,"%d",&help);
    if (help<0 || help>3) return 0;
    video_mode=help;
    return 1;
  }
  else if (strcmp(option,"VESA")==0)
  {
    sscanf(parameters,"%d",&help);
    if (help!=0 && help!=1) return 0;
    use_vesa=help;
    return 1;
  }
  else if (strcmp(option,"VRETRACE")==0)
  {
    sscanf(parameters,"%d",&help);
    if (help!=0 && help!=1) return 0;
    use_vret=help;
    return 1;
  }
  else if (strncmp(option,"JOYSTICK_",9)==0)
  {
    no=option[9]-'0';
    if (no<0 || no>3) return 0;
    for (i=0;i<JOYSTICKTYPES;i++)
      if (strcmp(parameters,joyparams[i])==0)
      {
        joytypes[no]=i;
        break;
      }
    if (i==JOYSTICKTYPES) return 0;
    joycfg=1; /*joystick configuration found*/
    return 1;
  }
  else if (strncmp(option,"KEYSET_",7)==0)
  {
    no=option[7]-'0';
    if (no<0 || no>3) return 0;
    help=sscanf(parameters,"%d %d %d %d %d %d %d %d %d",keysets[no]+0,keysets[no]+1,
                keysets[no]+2,keysets[no]+3,keysets[no]+4,keysets[no]+5,
                keysets[no]+6,keysets[no]+7,keysets[no]+8);
    /* make sure that scancodes are in range 0-255*/
    for (i=0;i<9;i++)
      if (keysets[no][i]<0 || keysets[no][i]>255)
        keysets[no][i]=255;
    if (help!=9) return 0;  /*not enough parameters*/
    return 1;
  }
  else if (strcmp(option,"KEYBOARD")==0)
  {
    if (strcmp(parameters,"0")==0) {
      PC_keyboard=TRUE;
      return 1;
    }
    if (strcmp(parameters,"1")==0) {
      PC_keyboard=FALSE;
      return 1;
    }
  }
  return 0;  /* unknown option */
}



/* -------------------------------------------------------------------------- */
/* ATARI EXIT                                                                 */
/* -------------------------------------------------------------------------- */

int Atari_Exit(int run_monitor)
{
        /* restore to text mode */
        ShutdownVgaEnvironment();

        key_delete();                           /* enable keyboard in monitor */

        Aflushlog();

        if (run_monitor) {
#ifdef SOUND
                Sound_Pause();
#endif
                if (monitor()) {
#ifdef MONITOR_BREAK
                        if (!break_step)       /*do not enter videomode when stepping through the code*/
#endif
                        SetupVgaEnvironment();
#ifdef SOUND
                        Sound_Continue();
#endif
                        return 1;                       /* return to emulation */
                }
        }

#ifdef SOUND
        Sound_Exit();
#endif

        return 0;
}




/* -------------------------------------------------------------------------- */
/* ATARI KEYBOARD                                                             */
/* -------------------------------------------------------------------------- */
/* Note: scancodes are a bit changed:
   raw_key & 0x7f = standard scancode
   raw_key & 0x80 = 1 if key was preceeded with 0xe0 (like the gray arrows)
   raw_key & 0x200 = 1 if key was depressed
                                                Robert Golias
*/
int Atari_Keyboard(void)
{
        int i;
        int keycode;
        int shift_mask;

        last_raw_key=raw_key;
/*
 * Trigger and Joystick handling should
 * be moved into the Keyboard Interrupt Routine  - done for keyboard-emulated joysticks
 */

        if (joy_in) {
                read_joystick(js0_centre_x, js0_centre_y);      /* read real PC joystick */
           if (joytypes[0]==joy_analog)
           {  stick0=astick; trig0=atrig; }
           if (joytypes[1]==joy_analog)
           {  stick1=astick; trig1=atrig; }
           if (joytypes[2]==joy_analog)
           {  stick2=astick; trig2=atrig; }
           if (joytypes[3]==joy_analog)
           {  stick3=astick; trig3=atrig; }
        }

        /* read LPT joysticks */
        for (i=0;i<4;i++)
          if (joytypes[i]>=joy_lpt1 && joytypes[i]<=joy_lpt3)
            read_LPTjoy(lptport[i],i);

/*
 * This needs a bit of tidying up - array lookup
 */
/* Atari5200 stuff */
        if (machine_type == MACHINE_5200 && !ui_is_active) {
                switch (raw_key) {
                case 0x3b:
                        keycode = AKEY_UI;
                        break;
                case 0x3f:                              /* F5 */
                        keycode = AKEY_COLDSTART;       /* 5200 has no warmstart */
                        break;
                case 0x44:                              /* F10 */
                        if (!norepkey)
                        {
                            keycode = key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
                            norepkey=TRUE;
                        }else
                            keycode = AKEY_NONE;
                        break;
                case 0x43:
                        keycode = AKEY_EXIT;
                        break;
                case 0x02:
                        keycode = AKEY_5200_1;
                        break;
                case 0x03:
                        keycode = AKEY_5200_2;
                        break;
                case 0x04:
                        keycode = AKEY_5200_3;
                        break;
                case 0x0D:
                        keycode = AKEY_5200_HASH;         /* = = * */
                        break;
                case 0x05:
                        keycode = AKEY_5200_4;
                        break;
                case 0x06:
                        keycode = AKEY_5200_5;
                        break;
                case 0x07:
                        keycode = AKEY_5200_6;
                        break;
                case 0x08:
                        keycode = AKEY_5200_7;
                        break;
                case 0x09:
                        keycode = AKEY_5200_8;
                        break;
                case 0x0C:
                        keycode = AKEY_5200_ASTERISK;         /* - = * */
                        break;
                case 0x0a:
                        keycode = AKEY_5200_9;
                        break;
                case 0x0b:
                        keycode = AKEY_5200_0;
                        break;
                case 0x3e:                              /* 1f : */
                        keycode = AKEY_5200_START;         /* start */
                        break;
                case 0x19:
                        keycode = AKEY_5200_PAUSE;         /* pause */
                        break;
                case 0x13:
                        keycode = AKEY_5200_RESET;         /* reset */
                        break;
                case 0x42:                              /* F8 */
                        if (!norepkey) {
                                keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;        /* invoke monitor */
                                norepkey = TRUE;
                        }
                        else
                                keycode = AKEY_NONE;
                        break;
                default:
                        keycode = AKEY_NONE;
                        norepkey = FALSE;
                        break;
                }
                if (raw_key_r!=0) {raw_key=raw_key_r;raw_key_r=0;}
                return keycode;
        }

        /* preinitialize of keycode */
        shift_mask = key_shift ? 0x40 : 0;
        keycode = shift_mask | (control ? 0x80 : 0);

        switch (raw_key) {
        case 0x3b:                                      /* F1 */
                if (control) {
                        PC_keyboard = TRUE;     /* PC keyboard mode (default) */
                        keycode = AKEY_NONE;
			update_leds();
                }
                else if (key_shift) {
                        PC_keyboard = FALSE;    /* Atari keyboard mode */
                        keycode = AKEY_NONE;
			update_leds();
                }
                else
                        keycode = AKEY_UI;
                break;
        case 0xCF:                                      /* gray END*/
                keycode = AKEY_BREAK;
                break;
        case 0x44:                                      /* F10 */
                if (!norepkey)
                {
                     keycode = key_shift ? AKEY_SCREENSHOT_INTERLACE : AKEY_SCREENSHOT;
                     norepkey = TRUE;
                }
                else
                     keycode = AKEY_NONE;
                break;
        case 0x3f:                                      /* F5 */
                keycode = key_shift ? AKEY_COLDSTART : AKEY_WARMSTART;
                break;
        case 0x40:                                      /* F6 */
                keycode = AKEY_HELP;
                break;
        case 0x42:                                      /* F8 */
                if (!norepkey && !ui_is_active) {
                        keycode = Atari_Exit(1) ? AKEY_NONE : AKEY_EXIT;        /* invoke monitor */
                        norepkey = TRUE;
                }
                else
                        keycode = AKEY_NONE;
                break;
        case 0x43:                                      /* F9 */
                keycode = AKEY_EXIT;
                break;
		case 0x57:					/* F11 */
		 		keycode = AKEY_NONE;
				for (i = 0; i < 4; i++) {
					if (++joy_autofire[i] > 2)
						joy_autofire[i] = 0;
		 		}
		 		raw_key = 0;	/* avoid continuous change */
		 		break;
        case 0x01:
                keycode |= AKEY_ESCAPE;
                break;
        case 0xD0:
        case 0x50:
                if (PC_keyboard)
                        keycode = AKEY_DOWN;
                else {
                        keycode |= AKEY_EQUAL;
                        keycode ^= shift_mask;
                }
                break;
        case 0xCB:
        case 0x4b:
                if (PC_keyboard)
                        keycode = AKEY_LEFT;
                else {
                        keycode |= AKEY_PLUS;
                        keycode ^= shift_mask;
                }
                break;
        case 0xCD:
        case 0x4d:
                if (PC_keyboard)
                        keycode = AKEY_RIGHT;
                else {
                        keycode |= AKEY_ASTERISK;
                        keycode ^= shift_mask;
                }
                break;
        case 0xC8:
        case 0x48:
                if (PC_keyboard)
                        keycode = AKEY_UP;
                else {
                        keycode |= AKEY_MINUS;
                        keycode ^= shift_mask;
                }
                break;
        case 0x29:                                      /* "`" key on top-left */
                keycode |= AKEY_ATARI;  /* Atari key (inverse video) */
                break;
        case 0x3a:
                keycode |= AKEY_CAPSTOGGLE;             /* Caps key */
                break;
        case 0x02:
                keycode |= AKEY_1;              /* 1 */
                break;
        case 0x03:
                if (!PC_keyboard)
                        keycode |= AKEY_2;
                else if (key_shift)
                        keycode = AKEY_AT;
                else
                        keycode |= AKEY_2;      /* 2,@ */
                break;
        case 0x04:
                keycode |= AKEY_3;              /* 3 */
                break;
        case 0x05:
                keycode |= AKEY_4;              /* 4 */
                break;
        case 0x06:
                keycode |= AKEY_5;              /* 5 */
                break;
        case 0x07:
                if (!PC_keyboard)
                        keycode |= AKEY_6;
                else if (key_shift)
                        keycode = AKEY_CIRCUMFLEX;      /* 6,^ */
                else
                        keycode |= AKEY_6;
                break;
        case 0x08:
                if (!PC_keyboard)
                        keycode |= AKEY_7;
                else if (key_shift)
                        keycode = AKEY_AMPERSAND;       /* 7,& */
                else
                        keycode |= AKEY_7;
                break;
        case 0x09:
                if (!PC_keyboard)
                        keycode |= AKEY_8;
                else if (key_shift)
                        keycode = AKEY_ASTERISK;        /* 8,* */
                else
                        keycode |= AKEY_8;
                break;
        case 0x0a:
                if (!PC_keyboard)
                        keycode |= AKEY_9;
                else if (key_shift)
                        keycode = AKEY_PARENLEFT;
                else
                        keycode |= AKEY_9;      /* 9,( */
                break;
        case 0x0b:
                if (!PC_keyboard)
                        keycode |= AKEY_0;
                else if (key_shift)
                        keycode = AKEY_PARENRIGHT;      /* 0,) */
                else
                        keycode |= AKEY_0;
                break;
        case 0x0c:
                if (!PC_keyboard)
                        keycode |= AKEY_LESS;
                else if (key_shift)
                        keycode = AKEY_UNDERSCORE;
                else
                        keycode |= AKEY_MINUS;
                break;
        case 0x0d:
                if (!PC_keyboard)
                        keycode |= AKEY_GREATER;
                else if (key_shift)
                        keycode = AKEY_PLUS;
                else
                        keycode |= AKEY_EQUAL;
                break;
        case 0x0e:
                keycode |= AKEY_BACKSPACE;
                break;


        case 0x0f:
                keycode |= AKEY_TAB;
                break;
        case 0x10:
                keycode |= AKEY_q;
                break;
        case 0x11:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_SOUND_RECORDING;	/* ALT+W .. Select system */
				}
				else
                	keycode |= AKEY_w;
                break;
        case 0x12:
                keycode |= AKEY_e;
                break;
        case 0x13:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_RUN;			/* ALT+R .. Run file */
				}
				else
	                keycode |= AKEY_r;
                break;
        case 0x14:
                keycode |= AKEY_t;
                break;
        case 0x15:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_SYSTEM;			/* ALT+Y .. Select system */
				}
				else
	                keycode |= AKEY_y;
                break;
        case 0x16:
                keycode |= AKEY_u;
                break;
        case 0x17:
                keycode |= AKEY_i;
                break;
        case 0x18:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_SOUND;			/* ALT+O .. mono/stereo sound */
				}
				else
	                keycode |= AKEY_o;
                break;
        case 0x19:
                keycode |= AKEY_p;
                break;
        case 0x1a:
                if (!PC_keyboard)
                        keycode |= AKEY_MINUS;
                else if (control | key_shift)
                        keycode |= AKEY_UP;
                else
                        keycode = AKEY_BRACKETLEFT;
                break;
        case 0x1b:
                if (!PC_keyboard)
                        keycode |= AKEY_EQUAL;
                else if (control | key_shift)
                        keycode |= AKEY_DOWN;
                else
                        keycode = AKEY_BRACKETRIGHT;
                break;
        case 0x9c:
        case 0x1c:
                keycode |= AKEY_RETURN;
                break;

        case 0x1e:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_ABOUT;			/* ALT+A .. About */
				}
				else
					keycode |= AKEY_a;
                break;
        case 0x1f:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_SAVESTATE;			/* ALT+S .. Save state */
				}
				else
	                keycode |= AKEY_s;
                break;
        case 0x20:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_DISK;			/* ALT+D .. Disk management */
				}
				else
	                keycode |= AKEY_d;
                break;
        case 0x21:
                keycode |= AKEY_f;
                break;
        case 0x22:
                keycode |= AKEY_g;
                break;
        case 0x23:
                keycode |= AKEY_h;
                break;
        case 0x24:
                keycode |= AKEY_j;
                break;
        case 0x25:
                keycode |= AKEY_k;
                break;
        case 0x26:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_LOADSTATE;			/* ALT+L .. Load state */
				}
				else
	                keycode |= AKEY_l;
                break;
        case 0x27:
                keycode |= AKEY_SEMICOLON;
                break;
        case 0x28:
                if (!PC_keyboard)
                        keycode |= AKEY_PLUS;
                else if (key_shift)
                        keycode = AKEY_DBLQUOTE;
                else
                        keycode = AKEY_QUOTE;
                break;
        case 0x2b:                                      /* PC key "\,|" */
        case 0x56:                                      /* PC key "\,|" */
                if (!PC_keyboard)
                        keycode |= AKEY_ASTERISK;
                else if (key_shift)
                        keycode = AKEY_BAR;
                else
                        keycode |= AKEY_BACKSLASH;
                break;


        case 0x2c:
                keycode |= AKEY_z;
                break;
        case 0x2d:
                keycode |= AKEY_x;
                break;
        case 0x2e:
				if (alt_key)
				{
					keycode = AKEY_UI;
					alt_function = MENU_CARTRIDGE;			/* ALT+C .. Cartridge management */
				}
				else
	                keycode |= AKEY_c;
                break;
        case 0x2f:
                keycode |= AKEY_v;
                break;
        case 0x30:
                keycode |= AKEY_b;
                break;
        case 0x31:
                keycode |= AKEY_n;
                break;
        case 0x32:
                keycode |= AKEY_m;
                break;
        case 0x33:
                if (!PC_keyboard)
                        keycode |= AKEY_COMMA;
                else if (key_shift && !control)
                        keycode = AKEY_LESS;
                else
                        keycode |= AKEY_COMMA;
                break;
        case 0x34:
                if (!PC_keyboard)
                        keycode |= AKEY_FULLSTOP;
                else if (key_shift && !control)
                        keycode = AKEY_GREATER;
                else
                        keycode |= AKEY_FULLSTOP;
                break;
        case 0x35:
                keycode |= AKEY_SLASH;
                break;
        case 0x39:
                keycode |= AKEY_SPACE;
                break;


        case 0xc7:                                      /* HOME key */
                keycode |= AKEY_CLEAR;
                break;
        case 0xd2:                                      /* INSERT key */
                if (key_shift)
                        keycode = AKEY_INSERT_LINE;
                else
                        keycode = AKEY_INSERT_CHAR;
                break;
        case 0xd3:                                      /* DELETE key */
                if (key_shift)
                        keycode = AKEY_DELETE_LINE;
                else
                        keycode = AKEY_DELETE_CHAR;
                break;
        default:
                keycode = AKEY_NONE;
                norepkey = FALSE;
                break;
        }

        if (raw_key_r!=0) {raw_key=raw_key_r;raw_key_r=0;}

        return keycode;
}

/* -------------------------------------------------------------------------- */

int Atari_PORT(int num)
{
	if (num == 0)
		return (stick1 << 4) | stick0;
	else
		return (stick3 << 4) | stick2;
}

/* -------------------------------------------------------------------------- */

int Atari_TRIG(int num)
{
        switch (num) {
        case 0:
                return trig0;
        case 1:
                return trig1;
        case 2:
                return trig2;
        case 3:
                return trig3;
        }
        return 0;
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* main loop */
	for (;;) {
		key_code = Atari_Keyboard();

		if (mouse_mode != MOUSE_OFF) {
			union REGS rg;
			rg.x.ax = 0x0b;
			int86(0x33, &rg, &rg);
			mouse_delta_x = (short) rg.x.cx;	/* signed! */
			mouse_delta_y = (short) rg.x.dx;	/* signed! */
			rg.x.ax = 0x03;
			int86(0x33, &rg, &rg);
			mouse_buttons = rg.x.bx;
		}

		Atari800_Frame();
		if (display_screen)
			Atari_DisplayScreen();
	}
}
