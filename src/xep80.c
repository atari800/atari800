/*
 * xep80.c - XEP80 emulation
 *
 * Copyright (C) 2007 Mark Grebe
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

#include "config.h"
#ifdef XEP80_EMULATION
#include "xep80.h"
#include "xep80_fonts.h"
#include "statesav.h"
#include <string.h>
#include "platform.h"
#include "util.h"
#include "log.h"

#define IN_QUEUE_SIZE		10

/* Definitions for command protocol between the XEP and the Atari */
#define CMD_XX_MASK      0xC0
#define CMD_00           0x00
#define CMD_X_CUR_LOW    0x00
#define CMD_01           0x40
#define CMD_01_MASK      0x70
#define CMD_X_CUR_UPPER  0x40
#define CMD_X_CUR_HIGH   0x50
#define CMD_LEFT_MAR_L   0x60
#define CMD_LEFT_MAR_H   0x70
#define CMD_10           0x80
#define CMD_10_MASK      0xF0
#define CMD_Y_CUR_LOW    0x80
#define CMD_1001         0x90
#define CMD_1001_MASK    0xF8
#define CMD_Y_CUR_HIGH   0x90
#define CMD_Y_CUR_STATUS 0x98
#define CMD_GRAPH_50HZ   0x99
#define CMD_GRAPH_60HZ   0x9A
#define CMD_RIGHT_MAR_L  0xA0
#define CMD_RIGHT_MAR_H  0xB0
#define CMD_11           0xC0
#define CMD_GET_CHAR     0xC0
#define CMD_REQ_X_CUR    0xC1
#define CMD_MRST         0xC2
#define CMD_PRT_STAT     0xC3
#define CMD_FILL_PREV    0xC4
#define CMD_FILL_SPACE   0xC5
#define CMD_FILL_EOL     0xC6
#define CMD_CLR_LIST     0xD0
#define CMD_SET_LIST     0xD1
#define CMD_SCR_NORMAL   0xD2
#define CMD_SCR_BURST    0xD3
#define CMD_CHAR_SET_A   0xD4
#define CMD_CHAR_SET_B   0xD5
#define CMD_CHAR_SET_INT 0xD6
#define CMD_TEXT_50HZ    0xD7
#define CMD_CUR_OFF      0xD8
#define CMD_CUR_ON       0xD9
#define CMD_CUR_BLINK    0xDA
#define CMD_CUR_ST_LINE  0xDB
#define CMD_SET_SCRL_WIN 0xDC
#define CMD_SET_PRINT    0xDD
#define CMD_WHT_ON_BLK   0xDE
#define CMD_BLK_ON_WHT   0xDF
#define CMD_VIDEO_CTRL   0xED
#define CMD_ATTRIB_A	 0xF4
#define CMD_ATTRIB_B	 0xF5

#define CHAR_SET_A			0
#define CHAR_SET_B			1
#define CHAR_SET_INTERNAL	2  

/* These center the graphics screen inside of the XEP80 screen */
#define XEP80_GRAPH_X_OFFSET ((XEP80_SCRN_WIDTH - XEP80_GRAPH_WIDTH) / 2)
#define XEP80_GRAPH_Y_OFFSET ((XEP80_SCRN_HEIGHT - XEP80_GRAPH_HEIGHT) / 2)

/* Used to determine if a charcter is double width */
#define IS_DOUBLE(x,y) (((xep80_data[y][x] & 0x80) && font_b_double) || \
						(((xep80_data[y][x] & 0x80) == 0) && font_a_double))

/* Global variables */
int XEP80_enabled = FALSE;
int XEP80_port = 0;
int XEP80_first_row = XEP80_SCRN_HEIGHT - 1;
int XEP80_last_row = 0;

/* Local procedures */
static void XEP80_InputWord(int word);
static int XEP80_NextInputWord(void);
static void XEP80_OutputWord(int word);
static void XEP80_ReceiveChar(UBYTE byte);
static void	XEP80_SendCursorStatus(void);
static void XEP80_SetXCur(UBYTE cursor);
static void XEP80_SetXCurHigh(UBYTE cursor);
static void XEP80_SetLeftMarginLow(UBYTE margin);
static void XEP80_SetLeftMarginHigh(UBYTE margin);
static void XEP80_SetYCur(UBYTE cursor);
static void XEP80_SetScreenMode(int graphics, int pal);
static void XEP80_SetRightMarginLow(UBYTE margin);
static void XEP80_SetRightMarginHigh(UBYTE margin);
static void XEP80_GetChar(void);
static void XEP80_GetXCur(void);
static void XEP80_MasterReset(void);
static void XEP80_GetPrinterStatus(void);
static void XEP80_FillMem(UBYTE c);
static void XEP80_SetList(int list);
static void XEP80_SetOutputDevice(int screen, int burst);
static void XEP80_SetCharSet(int set);
static void XEP80_SetCursor(int on, int blink);
static void XEP80_SetXCurStart(void);
static void XEP80_SetScrollWindow(void);
static void XEP80_SetInverse(int inverse);
static void XEP80_SetVideoCtrl(UBYTE video_ctrl);
static void XEP80_SetAttributeA(UBYTE attrib);
static void XEP80_SetAttributeB(UBYTE attrib);
static void XEP80_UpdateCursor(void);
static void XEP80_AddCharAtCursor(UBYTE byte);
static void XEP80_AddGraphCharAtCursor(UBYTE byte);
static void XEP80_CursorUp(void);
static void XEP80_CursorDown(void);
static void XEP80_CursorLeft(void);
static void XEP80_CursorRight(void);
static void	XEP80_ClearScreen(void);
static void XEP80_Backspace(void);
static void XEP80_AddEOL(void);
static void XEP80_DeleteChar(void);
static void XEP80_InsertChar(void);
static void XEP80_DeleteLogicalLine(void);
static void XEP80_InsertLine(void);
static void XEP80_GoToNextTab(void);
static void XEP80_ScrollUpLast(void);
static void XEP80_ScrollDown(int y);
static void XEP80_ScrollUp(int y_start, int y_end);
static void XEP80_FindEndLogicalLine(int *x, int *y);
static void XEP80_FindStartLogicalLine(int *x, int *y);
static void XEP80_BlitChar(int x, int y, int cur);
static void XEP80_BlitScreen(void);
static void XEP80_BlitRows(int y_start, int y_end);
static void XEP80_BlitGraphChar(int x, int y);

/* Local state variables */
static int output_state = -1;
static int output_word = 0;
static int input_state = -1;
static int input_word = 0;
static UWORD input_queue[IN_QUEUE_SIZE];
static int input_queue_read = 0;
static int input_queue_write = 0;

static int xcur = 0;
static int xscroll = 0;
static int ycur = 0;
static int new_xcur = 0;
static int new_ycur = 0;
static int old_xcur = 0;
static int old_ycur = 0;
static int lmargin = 0;
static int rmargin = 0x4f;
static UBYTE attrib_a = 0;
static UBYTE attrib_b = 0;
static int list_mode = FALSE;
static int char_set = CHAR_SET_A;
static int cursor_on = TRUE;
static int cursor_blink = FALSE;
static int cursor_overwrite = FALSE;
static int blink_reverse = FALSE;
static int inverse_mode = FALSE;
static int screen_output = TRUE;
static int burst_mode = FALSE;
static int graphics_mode = FALSE;
static int pal_mode = FALSE;
static int escape_mode = FALSE;
static int font_a_index = 0;
static int font_a_double = FALSE;
static int font_a_blank = FALSE;
static int font_a_blink = FALSE;
static int font_b_index = 0;
static int font_b_double = FALSE;
static int font_b_blank = FALSE;
static int font_b_blink = FALSE;
static UBYTE input_mask[2] = {0x02,0x20};
static UBYTE output_mask[2] = {0x01,0x10};

static UBYTE xep80_data[XEP80_HEIGHT][XEP80_WIDTH];
static UBYTE xep80_graph_data[XEP80_GRAPH_HEIGHT][XEP80_GRAPH_WIDTH/8];
UBYTE XEP80_screen_1[XEP80_SCRN_WIDTH*XEP80_SCRN_HEIGHT];
UBYTE XEP80_screen_2[XEP80_SCRN_WIDTH*XEP80_SCRN_HEIGHT];

UBYTE (*font)[XEP80_FONTS_CHAR_COUNT][XEP80_CHAR_HEIGHT][XEP80_CHAR_WIDTH];

static int tab_stops[256] = 
{0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,
 0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1};
 
static int eol_at_margin[XEP80_HEIGHT] = 
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void XEP80_Initialise(int *argc, char *argv[])
{
	int i, j;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-xep80") == 0) {
			XEP80_enabled = TRUE;
		}
		else if (strcmp(argv[i], "-xep80port") == 0) {
			XEP80_port = Util_sscandec(argv[++i]);
			if (XEP80_port != 0 && XEP80_port != 1) {
				Log_print("Invalid XEP80 port #");
				XEP80_port = 0;
			}
		}
		else {
		 	if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-xep80           Emulate the XEP80");
				Log_print("\t-xep80port <n>   Use XEP80 on joystick port <n>");
			}
			argv[j++] = argv[i];
		}
	}
	*argc = j;
}

static void XEP80_InputWord(int word)
{
	/* If we aren't already sending a word..*/
    if (input_state == -1) {
        input_word = word;
        input_state = 0;
    }
	/* Otherwise put it on the queue */
    else {
        input_queue[input_queue_write++] = word;
        if (input_queue_write >= IN_QUEUE_SIZE) {
            input_queue_write = 0;
        }
    }
}

static int XEP80_NextInputWord(void)
{
	/* If there is nothing on the queue */
    if (input_queue_read == input_queue_write) {
        return(-1);
    }
	/* Otherwise get the next word off of the queue */
    else {
        input_word = input_queue[input_queue_read++];
        if (input_queue_read >= IN_QUEUE_SIZE) {
            input_queue_read = 0;
        }
        return(0);
    }
}

UBYTE XEP80_GetBit(void) 
{
	UBYTE ret;
	
    /* Shift input words into the Atari for each read */
	switch(input_state) {
    case -1:
        ret = 0xFF;
		break;
		return(0xff);
    case 0:
		ret = 0xFF & ~input_mask[XEP80_port];
        input_state++;
		break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
	case 9:
        if (input_word & (1 << (input_state-1))) {
            ret = 0xFF;
        }
        else {
            ret = 0xFF & ~input_mask[XEP80_port];
        }
        input_state++;
		/* Get a new word if there is one */
		if (input_state == 10)
			input_state = XEP80_NextInputWord();
		break;
	default:
		return(0xFF);
		break;
    }
	return(ret);
}

void XEP80_PutBit(UBYTE byte)
{
    byte &= output_mask[XEP80_port];

    /* Shift output words from the Atari for each read */
	switch(output_state) {
    case -1:
        if (!byte) {
            output_state = 0;
            output_word = 0;
        }
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        if (byte) {
            output_word |= 1 << (output_state);
        }
        output_state ++;
        break;
    case 9:
        output_state = -1;
        if (byte) {
			/* Handle the new word */
            XEP80_OutputWord(output_word);
        }
        break;
    }
}

static void XEP80_OutputWord(int word)
{
    UBYTE byte = word & 0xFF;
    int cmd_flag = word & 0x100;
    static UBYTE lastChar = 0;

    /* Is it a command or data word? */
	if (cmd_flag) {
        switch(byte & CMD_XX_MASK) {
        case CMD_00:
            XEP80_SetXCur(byte & 0x3F);
            break;
        case CMD_01:
            switch(byte & CMD_01_MASK) {
            case CMD_X_CUR_UPPER:
                XEP80_SetXCur(0x40 + (byte & 0x0F));
                break;
            case CMD_X_CUR_HIGH:
                XEP80_SetXCurHigh(byte & 0x0F);
                break;
            case CMD_LEFT_MAR_L:
                XEP80_SetLeftMarginLow(byte & 0x0f);
                break;
            case CMD_LEFT_MAR_H:
                XEP80_SetLeftMarginHigh(byte & 0x0f);
                break;
            }
            break;
        case CMD_10:
            switch(byte & CMD_10_MASK) {
            case CMD_Y_CUR_LOW:
                XEP80_SetYCur(byte & 0x0F);
                break;
            case CMD_1001:
				if ((byte & CMD_1001_MASK) == CMD_Y_CUR_HIGH) {
                    XEP80_SetYCur(0x10 + (byte & 0x07));
				}
				else {
					switch(byte) {
					case CMD_Y_CUR_STATUS:
						XEP80_SetYCur(24);
						break;
					case CMD_GRAPH_50HZ:
						XEP80_SetScreenMode(TRUE, TRUE);
						break;
					case CMD_GRAPH_60HZ:
						XEP80_SetScreenMode(TRUE, FALSE);
						break;
					}
                }
                break;
            case CMD_RIGHT_MAR_L:
                XEP80_SetRightMarginLow(byte & 0x0f);
                break;
            case CMD_RIGHT_MAR_H:
                XEP80_SetRightMarginHigh(byte & 0x0f);
                break;
            }
            break;
        case CMD_11:
            switch (byte) {
            case CMD_GET_CHAR:
                XEP80_GetChar();
                break;
            case CMD_REQ_X_CUR:
                XEP80_GetXCur();
                break;
            case CMD_MRST:
                XEP80_MasterReset();
                break;
            case CMD_PRT_STAT:
                XEP80_GetPrinterStatus();
                break;
            case CMD_FILL_PREV:
                XEP80_FillMem(lastChar);
                break;
            case CMD_FILL_SPACE:
                XEP80_FillMem(0x20);
                break;
            case CMD_FILL_EOL:
                XEP80_FillMem(0x9B);
                break;
            case CMD_CLR_LIST:
                XEP80_SetList(FALSE);
                break;
            case CMD_SET_LIST:
                XEP80_SetList(TRUE);
                break;
            case CMD_SCR_NORMAL:
                XEP80_SetOutputDevice(TRUE, FALSE);
                break;
            case CMD_SCR_BURST:
                XEP80_SetOutputDevice(TRUE, TRUE);
                break;
            case CMD_SET_PRINT:
                XEP80_SetOutputDevice(FALSE, TRUE);
                break;
            case CMD_CHAR_SET_A:
                XEP80_SetCharSet(CHAR_SET_A);
				XEP80_BlitScreen();
                break;
            case CMD_CHAR_SET_B:
                XEP80_SetCharSet(CHAR_SET_B);
				XEP80_BlitScreen();
                break;
            case CMD_CHAR_SET_INT:
                XEP80_SetCharSet(CHAR_SET_INTERNAL);
				XEP80_BlitScreen();
                break;
            case CMD_TEXT_50HZ:
                XEP80_SetScreenMode(FALSE, TRUE);
                break;
            case CMD_CUR_OFF:
                XEP80_SetCursor(FALSE, FALSE);
                break;
            case CMD_CUR_ON:
                XEP80_SetCursor(TRUE, FALSE);
                break;
            case CMD_CUR_BLINK:
                XEP80_SetCursor(TRUE, TRUE);
                break;
            case CMD_CUR_ST_LINE:
                XEP80_SetXCurStart();
                break;
            case CMD_SET_SCRL_WIN:
                XEP80_SetScrollWindow();
                break;
            case CMD_WHT_ON_BLK:
                XEP80_SetInverse(FALSE);
                break;
            case CMD_BLK_ON_WHT:
                XEP80_SetInverse(TRUE);
                break;
            case CMD_VIDEO_CTRL:
				XEP80_Backspace();
                XEP80_SetVideoCtrl(lastChar);
                break;
            case CMD_ATTRIB_A:
				XEP80_Backspace();
                XEP80_SetAttributeA(lastChar);
                break;
            case CMD_ATTRIB_B:
				XEP80_Backspace();
                XEP80_SetAttributeB(lastChar);
                break;
            }
            break;
        }
    }
	/* If it's data, then handle it as a character */
    else {
		old_xcur = xcur;
		old_ycur = ycur;
		XEP80_ReceiveChar(byte);
        lastChar = byte;
		if (!burst_mode)
			XEP80_SendCursorStatus();
    }
}

static void XEP80_ReceiveChar(UBYTE byte)
{
	if (graphics_mode) {
        XEP80_AddGraphCharAtCursor(byte);
	}
	else if (escape_mode) {
        XEP80_AddCharAtCursor(byte);
		escape_mode = FALSE;
	}
	/* List mode prints control chars like escape mode, except for EOL */
	else if (list_mode) {
		if (byte == ATARI_EOL) {
			XEP80_AddEOL();
		}
		else {
			XEP80_AddCharAtCursor(byte);
		}
	}
	else if (!screen_output) {
		/* Printer characters are thrown away, handled elsewhere.  The
		 * XEP80 driver needs to be set up to send printer characters
		 * to the existing P: driver. */
	}
	else {
		/* If we are on the status line, only allow Delete Line and char adds */
        if (ycur == 24) {
            if (byte == 0x9c) { /* Delete Line */
				memset(xep80_data[ycur],ATARI_EOL,XEP80_WIDTH);
            }
            else {
                XEP80_AddCharAtCursor(byte);
            }
        }
        else {
		switch(byte) {
			case 0x1b: /* Escape - Print next char even if a control char */
				escape_mode = TRUE;
				break;
			case 0x1c: /* Cursor Up */
				XEP80_CursorUp();
				break;
			case 0x1d: /* Cursor Down */
				XEP80_CursorDown();
				break;
			case 0x1e: /* Cursor Left */
				XEP80_CursorLeft();
				break;
			case 0x1f: /* Cursor Right */
				XEP80_CursorRight();
				break;
			case 0x7d: /* Clear Screen */
				XEP80_ClearScreen();
				break;
			case 0x7e: /* Backspace */
				XEP80_Backspace();
				break;
			case 0x7f: /* Tab */
				XEP80_GoToNextTab();
				break;
			case ATARI_EOL: /* Atari EOL */
				XEP80_AddEOL();
				break;
			case 0x9c: /* Delete Line */
				XEP80_DeleteLogicalLine();
				break;
			case 0x9d: /* Insert Line */
				XEP80_InsertLine();
				break;
			case 0x9e: /* Clear tab */
				tab_stops[xcur] = FALSE;
				break;
			case 0x9f: /* Set Tab */
				tab_stops[xcur] = TRUE;
				break;
			case 0xfd: /* Sound Bell */
				/* Do nothing here */
				break;
			case 0xfe: /* Delete Char */
				XEP80_DeleteChar();
				break;
			case 0xff: /* Insert Char */
				XEP80_InsertChar();
				break;
			default:
				XEP80_AddCharAtCursor(byte);
				break;
			}
        }
	}

}

static void	XEP80_SendCursorStatus()
{
	/* Send X cursor only */
	if ((old_xcur == xcur && old_ycur == ycur) ||
		(old_ycur == ycur)) {
		if (xcur > 0x4f) {
			XEP80_InputWord(0x150);
		}
		else {
			XEP80_InputWord(0x100 | (xcur & 0x4F));
		}
		}
	/* Send Y cursor only */
	else if (old_xcur == xcur) {
			XEP80_InputWord(0x1E0 | (ycur & 0x1F));
		}
	/* Send X followed by Y cursor */
	else {
		if (xcur > 0x4f) {
			XEP80_InputWord(0x1D0);
			}
		else {
			XEP80_InputWord(0x180 | (xcur & 0x4F));
			}
		XEP80_InputWord(0x1E0 | (ycur & 0x1F));
		}
}

static void XEP80_SetXCur(UBYTE cursor)
{
	new_xcur = cursor;
    new_ycur = ycur;
    XEP80_UpdateCursor();
}

static void XEP80_SetXCurHigh(UBYTE cursor)
{
	new_xcur = ((UBYTE)xcur & 0x3f) | (cursor << 4);
    new_ycur = ycur;
    XEP80_UpdateCursor();
}

static void XEP80_SetLeftMarginLow(UBYTE margin)
{
	lmargin = margin;
}

static void XEP80_SetLeftMarginHigh(UBYTE margin)
{
	lmargin = ((UBYTE)lmargin & 0x3f) | (margin << 4);
}

static void XEP80_SetYCur(UBYTE cursor)
{
    new_xcur = xcur;
	new_ycur = cursor;
    XEP80_UpdateCursor();
}

static void XEP80_SetScreenMode(int graphics, int pal)
{
	graphics_mode = graphics;
	pal_mode = pal;
	if (graphics_mode) {
        xcur = 0;
        ycur = 0;
		burst_mode = TRUE;
        /* Clear the old text screen */
		XEP80_FillMem(0x20);
        XEP80_BlitScreen();
        /* Clear the graphics memory */
        memset(xep80_graph_data,0,
               (XEP80_GRAPH_WIDTH/8)*XEP80_GRAPH_HEIGHT);
		}
	else {
        xcur = 0;
        ycur = 0;
		burst_mode = FALSE;
		XEP80_FillMem(0x9b);
        XEP80_BlitScreen();
        new_xcur = xcur;
        new_ycur = ycur;
        XEP80_UpdateCursor();
		}
}

static void XEP80_SetRightMarginLow(UBYTE margin)
{
	rmargin = margin;
}

static void XEP80_SetRightMarginHigh(UBYTE margin)
{
	rmargin = ((UBYTE)rmargin & 0x3f) | (margin << 4);
}

static void XEP80_GetChar(void)
{
    static int at_eol_at_margin = FALSE;

    if (xcur == rmargin && at_eol_at_margin) {
        XEP80_InputWord(ATARI_EOL);
        at_eol_at_margin = FALSE;
    } else {
        XEP80_InputWord(xep80_data[ycur][xcur]);
    }
	
    old_xcur = xcur;
	old_ycur = ycur;
    new_xcur = xcur + 1;
    new_ycur = ycur;
    
    if (new_xcur > rmargin) 
        {
        if (eol_at_margin[new_ycur]) {
            new_ycur = ycur;
            new_xcur -= 1;
            at_eol_at_margin = TRUE;
        }
        else {
            new_xcur = lmargin;
            if (new_ycur < XEP80_HEIGHT-2)
                new_ycur++;
        }
    }
    XEP80_UpdateCursor();
	XEP80_SendCursorStatus();
}

static void XEP80_GetXCur(void)
{
	XEP80_InputWord(xcur);
}

static void XEP80_MasterReset(void)
{
	int i;
	
	input_state = -1;
	input_queue_read = 0;
	input_queue_write = 0;

	xcur = 0;
	xscroll = 0;
	ycur = 0;
	new_xcur = 0;
	new_ycur = 0;
	old_xcur = 0;
	old_ycur = 0;
	lmargin = 0;
	rmargin = 0x4f;
	attrib_a = 0;
	attrib_b = 0;
	list_mode = FALSE;
	char_set = CHAR_SET_A;
	cursor_on = TRUE;
	cursor_blink = FALSE;
	cursor_overwrite = FALSE;
	blink_reverse = FALSE;
	inverse_mode = FALSE;
	screen_output = TRUE;
	burst_mode = FALSE;
	graphics_mode = FALSE;
	pal_mode = FALSE;
	escape_mode = FALSE;

    font_a_index = 0;
    font_a_double = FALSE;
    font_a_blank = FALSE;
    font_a_blink = FALSE;
    font_b_index = 0;
    font_b_double = FALSE;
    font_b_blank = FALSE;
    font_b_blink = FALSE;
	memset(xep80_data,ATARI_EOL,XEP80_WIDTH*XEP80_HEIGHT);
    if (!XEP80_Fonts_inited)
        XEP80_Fonts_InitFonts();
	for (i=0;i<XEP80_HEIGHT;i++)
		eol_at_margin[i] = FALSE;
    XEP80_BlitScreen();
	XEP80_UpdateCursor();
	XEP80_InputWord(0x01);
}

static void XEP80_GetPrinterStatus(void)
{
	XEP80_InputWord(0x01);
}

static void XEP80_FillMem(UBYTE c)
{
	int i;
	
	for (i=0;i<XEP80_HEIGHT;i++)
		eol_at_margin[i] = FALSE;

	memset(xep80_data,c,XEP80_WIDTH*XEP80_HEIGHT);
	XEP80_InputWord(0x01);
}

static void XEP80_SetList(int list)
{
	list_mode = list;
}

static void XEP80_SetOutputDevice(int screen, int burst)
{
	screen_output = screen;
	burst_mode = burst;
}

static void XEP80_SetCharSet(int set)
{
	char_set = set;
}

static void XEP80_SetCursor(int on, int blink)
{
	cursor_on = on;
	cursor_blink = blink;
    if (!cursor_on) {
        XEP80_BlitChar(xcur, ycur, FALSE);
    }
    else {
        new_xcur = xcur;
        new_ycur = ycur;
        XEP80_UpdateCursor();
    }
}

static void XEP80_SetXCurStart(void)
{
    int x_start = xcur;
    int y_start = ycur;

    XEP80_FindStartLogicalLine(&x_start, &y_start);
    new_xcur = x_start;
    new_ycur = y_start;
    XEP80_UpdateCursor();
}

static void XEP80_SetScrollWindow(void)
{
	xscroll = xcur;
	XEP80_BlitScreen();
}

static void XEP80_SetInverse(int inverse)
{
	inverse_mode = inverse;
	XEP80_BlitScreen();
}

static void XEP80_SetVideoCtrl(UBYTE video_ctrl)
{
    if (video_ctrl & 0x08) {
        inverse_mode = TRUE;
    }
    else {
        inverse_mode = FALSE;
    }
    if (video_ctrl & 0x02) {
        cursor_blink = FALSE;
    }
    else {
        cursor_blink = TRUE;
    }
    if (video_ctrl & 0x04) {
        cursor_overwrite = FALSE;
    }
    else {
        cursor_overwrite = TRUE;
    }
    if (video_ctrl & 0x01) {
        blink_reverse = TRUE;
    }
    else {
        blink_reverse = FALSE;
    }
	XEP80_BlitScreen();
    new_xcur = xcur;
    new_ycur = ycur;
	XEP80_UpdateCursor();
}

static void XEP80_SetAttributeA(UBYTE attrib)
{
	attrib_a = ~attrib;

    font_a_index = 0;
    if (attrib_a & 0x01) {
        font_a_index |= XEP80_FONTS_REV_FONT_BIT;
    }
    if (attrib_a & 0x20) {
        font_a_index |= XEP80_FONTS_UNDER_FONT_BIT;
    }
    if (attrib_a & 0x80) {
        font_a_index |= XEP80_FONTS_BLK_FONT_BIT;
    }
    if (attrib_a & 0x10) {
        font_a_double = TRUE;
    }
    else {
        font_a_double = FALSE;
    }
    if (attrib_a & 0x40) {
        font_a_blank = TRUE;
    }
    else {
        font_a_blank = FALSE;
    }
    if (attrib_a & 0x04) {
        font_a_blink = TRUE;
    }
    else {
        font_a_blink = FALSE;
    }
	XEP80_BlitScreen();
}

static void XEP80_SetAttributeB(UBYTE attrib)
{
	attrib_b = ~attrib;

    font_b_index = 0;
    if (attrib_b & 0x01) {
        font_b_index |= XEP80_FONTS_REV_FONT_BIT;
    }
    if (attrib_b & 0x20) {
        font_b_index |= XEP80_FONTS_UNDER_FONT_BIT;
    }
    if (attrib_b & 0x80) {
        font_b_index |= XEP80_FONTS_BLK_FONT_BIT;
    }
    if (attrib_b & 0x10) {
        font_b_double = TRUE;
    }
    else {
        font_b_double = FALSE;
    }
    if (attrib_b & 0x40) {
        font_b_blank = TRUE;
    }
    else {
        font_b_blank = FALSE;
    }
    if (attrib_b & 0x04) {
        font_b_blink = TRUE;
    }
    else {
        font_b_blink = FALSE;
    }
	XEP80_BlitScreen();
}

static void XEP80_UpdateCursor(void)
{
    if (cursor_on) {
		/* Redraw character cursor was at */
        XEP80_BlitChar(xcur, ycur, FALSE);
        /* Handle reblitting double wide's which cursor may have overwritten */
        if (xcur != 0) {
            XEP80_BlitChar(xcur-1, ycur, FALSE);
        }
		/* Redraw cursor at new location */
        XEP80_BlitChar(new_xcur, new_ycur, TRUE);
    }
    xcur = new_xcur;
    ycur = new_ycur;
}

static void XEP80_AddCharAtCursor(UBYTE byte)
{
    xep80_data[ycur][xcur] = byte;
    XEP80_BlitChar(xcur, ycur, FALSE);

    new_xcur = xcur + 1;
    new_ycur = ycur;
    if (new_xcur > rmargin) 
        {
        new_xcur = lmargin;
        if (new_ycur == XEP80_HEIGHT-2) {
            XEP80_ScrollUpLast();
        }
        else if (new_ycur != XEP80_HEIGHT-1) {
			new_ycur++;
            XEP80_ScrollDown(new_ycur);
        }
    }
	else {
		XEP80_UpdateCursor();
	}
}

static void XEP80_AddGraphCharAtCursor(UBYTE byte)
{
    xep80_graph_data[ycur][xcur] = byte;
    XEP80_BlitGraphChar(xcur, ycur);
    xcur++;
    if (xcur >= 40) {
        xcur = 0;
        ycur++;
        if (ycur >= 240) {
            ycur = 0;
        }
    }
}

static void XEP80_CursorUp(void)
{
	new_ycur = ycur - 1;
	new_xcur = xcur;
	if (new_ycur < 0) 
		{
		new_ycur = XEP80_HEIGHT-2;
		}
	XEP80_UpdateCursor();
}

static void XEP80_CursorDown(void)
{
	new_ycur = ycur + 1;
	new_xcur = xcur;
	if (new_ycur > XEP80_HEIGHT-2) 
		{
		new_ycur = 0;
		}
	XEP80_UpdateCursor();
}

static void XEP80_CursorLeft(void)
{
	new_xcur = xcur - 1;
	new_ycur = ycur;
	if (new_xcur < lmargin) 
		{
		new_xcur = rmargin;
		}
	XEP80_UpdateCursor();
}

static void XEP80_CursorRight(void)
{
	if (xep80_data[ycur][xcur] == ATARI_EOL) {
		xep80_data[ycur][xcur] = 0x20;
	}
	new_xcur = xcur + 1;
	new_ycur = ycur;
	if (new_xcur > rmargin) 
		{
		new_xcur = lmargin;
		}
	XEP80_UpdateCursor();
}

static void	XEP80_ClearScreen(void)
{
	int y;

	for (y=0;y<XEP80_HEIGHT-1;y++) {
		memset(&xep80_data[y][xscroll],ATARI_EOL,XEP80_LINE_LEN);
		eol_at_margin[y] = FALSE;
	}
	XEP80_BlitScreen();
	new_xcur = 0;
	new_ycur = 0;
	XEP80_UpdateCursor();
}


static void XEP80_Backspace(void)
{
    int x_start = xcur;
    int y_start = ycur;

    XEP80_FindStartLogicalLine(&x_start, &y_start);

    if (xcur == lmargin && x_start == xcur && y_start == ycur) {
        return;
    }
	
    if (xcur == lmargin) {
        new_ycur = ycur-1;
        new_xcur = rmargin;
    }
    else {
        new_xcur = xcur-1;
        new_ycur = ycur;
    }
    xep80_data[new_ycur][new_xcur] = 0x20;
    XEP80_UpdateCursor();
}

static void XEP80_DeleteChar(void)
{
    int x_end = xcur;
    int y_end = ycur;
    int x_del, y_del;

    XEP80_FindEndLogicalLine(&x_end, &y_end);
    
    x_del = xcur;
    y_del = ycur;
	
    while(x_del != x_end || y_del != y_end) {
        if (x_del == rmargin) {
            if (y_del == XEP80_HEIGHT-2) {
                xep80_data[y_del][x_del] = ATARI_EOL;
                break;
            }
            xep80_data[y_del][x_del] = xep80_data[y_del+1][lmargin];
            if (xep80_data[y_del+1][lmargin+1] == ATARI_EOL) {
                XEP80_ScrollUp(y_del+1, y_del+1);
				eol_at_margin[y_del] = TRUE;
               break;
            }
            y_del++;
            x_del=lmargin;
        }
        else {
            xep80_data[y_del][x_del] = xep80_data[y_del][x_del+1];
			if (x_del == rmargin-1 && eol_at_margin[y_del]) {
				xep80_data[y_del][x_del+1] = ATARI_EOL;
				eol_at_margin[y_del] = FALSE;
			}
			x_del++;
        }
    }
    XEP80_BlitRows(ycur,y_end);
    if (cursor_on) 
        XEP80_BlitChar(xcur, ycur, TRUE);
}


static void XEP80_InsertChar(void)
{
    int x_end = xcur;
    int y_end = ycur;
    int x_ins, y_ins;

    XEP80_FindEndLogicalLine(&x_end, &y_end);

    x_ins = x_end;
    y_ins = y_end;

	if (x_ins == rmargin) {
		if (eol_at_margin[y_end]) {
            if (y_end == XEP80_HEIGHT-2) {
                new_ycur = ycur-1;
                new_xcur = xcur;
                XEP80_ScrollUpLast();
                y_ins--;
                eol_at_margin[y_end-1] = FALSE;
                xep80_data[y_end][lmargin] = ATARI_EOL;
            } else {
                XEP80_ScrollDown(y_ins+1);
                eol_at_margin[y_end] = FALSE;
                y_end++;
                xep80_data[y_end][lmargin] = ATARI_EOL;
            }
			}
		else if (xep80_data[y_end][x_end] == ATARI_EOL) {
			x_ins--;
			eol_at_margin[y_end] = TRUE;
		}
	}

    while(x_ins != xcur || y_ins != ycur) {
		if (y_ins == ycur && x_ins < xcur)
			break;
        if (x_ins == rmargin) {
			if (y_ins != XEP80_HEIGHT-2) {
				xep80_data[y_ins+1][lmargin] = xep80_data[y_ins][x_ins];
				}
        }
        else {
            xep80_data[y_ins][x_ins+1] = xep80_data[y_ins][x_ins];
        }
        if (x_ins == lmargin) {
			x_ins = rmargin;
			y_ins--;
        }
        else {
            x_ins--;
        }
    }

	if (x_ins == rmargin) {
		xep80_data[y_ins+1][lmargin] = xep80_data[y_ins][x_ins];
	}
	else {
		xep80_data[y_ins][x_ins+1] = xep80_data[y_ins][x_ins];
	}
	xep80_data[y_ins][x_ins] = 0x20;
    XEP80_BlitRows(ycur,y_end);
    if (cursor_on) 
        XEP80_BlitChar(xcur, ycur, TRUE);
}

static void XEP80_GoToNextTab(void)
{
	int x_search = xcur+1;
	int y_search = ycur;
	
    while(1) {
		while(x_search<=rmargin) {
            if (tab_stops[x_search]) {
				new_xcur = x_search;
				new_ycur = y_search;
                XEP80_UpdateCursor();
				return;
			}
			x_search++;
		}
		y_search++;
		x_search = lmargin;
		if (y_search >= XEP80_HEIGHT-1) {
            new_ycur = XEP80_HEIGHT-2;
            new_xcur = lmargin;
            XEP80_ScrollUpLast();
            return;
        }
	}
}

static void XEP80_AddEOL(void)
{
	int x_end = xcur;
	int y_end = ycur;

	XEP80_FindEndLogicalLine(&x_end,&y_end);
	xep80_data[y_end][x_end] = ATARI_EOL;
	new_xcur = lmargin;
	if (y_end == XEP80_HEIGHT-2) {
		new_ycur = y_end;
		XEP80_ScrollUpLast();
	}
	else {
		new_ycur = y_end+1;
		XEP80_UpdateCursor();
	}
}

static void XEP80_DeleteLogicalLine(void)
{
    int x_start = xcur;
    int y_start = ycur;
    int x_end = xcur;
    int y_end = ycur;

    XEP80_FindStartLogicalLine(&x_start, &y_start);
    XEP80_FindEndLogicalLine(&x_end, &y_end);
    new_ycur = y_start;
    new_xcur = lmargin;
    XEP80_ScrollUp(y_start, y_end);
}

static void XEP80_InsertLine(void)
{
    new_xcur = lmargin;
    new_ycur = ycur;

    XEP80_ScrollDown(ycur);;
}

static void XEP80_ScrollUpLast(void)
{
    int row;

    if (cursor_on) {
        XEP80_BlitChar(xcur, ycur, FALSE);
        if (xcur != 0) {
            XEP80_BlitChar(xcur-1, ycur, FALSE);
        }
    }

    for (row=1;row<=XEP80_HEIGHT-2;row++) {
        memcpy(xep80_data[row-1],
               xep80_data[row],
               XEP80_WIDTH);
		eol_at_margin[row-1] = eol_at_margin[row] ;
    }
    
    memset(xep80_data[23],ATARI_EOL,XEP80_WIDTH);
	eol_at_margin[23] = FALSE ;
    
    XEP80_BlitScreen();

    if (cursor_on) {
        XEP80_BlitChar(new_xcur, new_ycur, TRUE);
    }
    xcur = new_xcur;
    ycur = new_ycur;
}

static void XEP80_ScrollDown(int y)
{
    int row;
    
    if (cursor_on) {
        XEP80_BlitChar(xcur, ycur, FALSE);
        if (xcur != 0) {
            XEP80_BlitChar(xcur-1, ycur, FALSE);
        }
    }

    for (row=XEP80_HEIGHT-3;row>=y;row--) {
        memcpy(xep80_data[row+1],
               xep80_data[row],
               XEP80_WIDTH);
		eol_at_margin[row+1] = eol_at_margin[row] ;
    }
    
    memset(xep80_data[y],ATARI_EOL,XEP80_WIDTH);
	eol_at_margin[y] = FALSE ;
    
    XEP80_BlitRows(y,23);

    if (cursor_on) {
        XEP80_BlitChar(new_xcur, new_ycur, TRUE);
    }
    xcur = new_xcur;
    ycur = new_ycur;
}

static void XEP80_ScrollUp(int y_start, int y_end)
{
    int row;
	int num_rows = y_end - y_start + 1;

    if (cursor_on) {
        XEP80_BlitChar(xcur, ycur, FALSE);
        if (xcur != 0) {
            XEP80_BlitChar(xcur-1, ycur, FALSE);
        }
    }

    for (row=y_start;row<XEP80_HEIGHT-2;row++) {
        memcpy(xep80_data[row],
               xep80_data[row+num_rows],
               XEP80_WIDTH);
		eol_at_margin[row] = eol_at_margin[row+num_rows] ;
    }
    
	for (row=24-num_rows; row < 24; row++) {
		memset(xep80_data[row],ATARI_EOL,XEP80_WIDTH);
		eol_at_margin[row] = FALSE;
		}

    XEP80_BlitRows(y_start,23);

    if (cursor_on) {
        XEP80_BlitChar(new_xcur, new_ycur, TRUE);
    }
    xcur = new_xcur;
    ycur = new_ycur;
}

static void XEP80_FindEndLogicalLine(int *x, int *y)
{
	int x_search = *x;
	int y_search = *y;
	int found = FALSE;
	
	while(1) {
		while(x_search<=rmargin) {
			if (xep80_data[y_search][x_search] == ATARI_EOL) {
				found = TRUE;
				break;
			}
			x_search++;
		}
		if (found)
			break;
		if (eol_at_margin[y_search]) {
			x_search = rmargin;
			found = TRUE;
			break;
		}
		y_search++;
		if (y_search >= XEP80_HEIGHT-1)
			break;
		x_search = lmargin;
	}
	
	if (!found) {
		*x = rmargin;
		*y = XEP80_HEIGHT-2;
	}
	else {
		if (x_search == 0 && y_search != ycur) {
			*x = rmargin;
			*y = y_search-1;
		}
		else {
			*x = x_search;
			*y = y_search;
		}
	}
}

static void XEP80_FindStartLogicalLine(int *x, int *y)
{
	int y_search = *y;
	int x_search = rmargin;
	int found = FALSE;
	
	if (y_search==0) {
		*x = lmargin;
		return;
		}
	
	y_search--;
	while(1) {
		while(x_search>=lmargin) {
			if (xep80_data[y_search][x_search] == ATARI_EOL) {
				found = TRUE;
				break;
			}
            x_search--;
		}
		if (found)
			break;
		y_search--;
		if (y_search < 0)
			break;
		x_search = rmargin;
	}

	*x = lmargin;
	
	if (!found) {
		*y = 0;
	}
	else {
		*y = y_search + 1;
	}
}

static void XEP80_BlitChar(int x, int y, int cur)
{
    int screen_col;
    int font_row, font_col;
    UBYTE *from, *to;
    UBYTE ch;
    UBYTE on, off, blink;
    int font_index, font_double, font_blank, font_blink;
	int blink_rev;
	int last_double_cur = FALSE;

    /* Don't Blit characters that aren't on the screen at the moment. */
    if (x < xscroll || x >= xscroll + XEP80_LINE_LEN) 
        return;
    
    screen_col = x-xscroll;
    ch = xep80_data[y][x];
	
	/* Dispaly Atari EOL's as spaces */
	if (ch == ATARI_EOL && ((font_a_index & XEP80_FONTS_BLK_FONT_BIT) == 0) 
        && char_set != CHAR_SET_INTERNAL)
		ch = 0x20;
	
    if (ch & 0x80) {
        font_index = font_b_index;
        font_double = font_b_double;
        font_blank = font_b_blank;
        font_blink = font_b_blink;
    }
    else {
        font_index = font_a_index;
        font_double = font_a_double;
        font_blank = font_a_blank;
        font_blink = font_a_blink;
    }
	
	if (font_blink && blink_reverse && (font_index & XEP80_FONTS_REV_FONT_BIT)) {
		blink_rev = TRUE;
	}
	else {
		blink_rev = FALSE;
	}
	
    if (inverse_mode) {
		font_index ^= XEP80_FONTS_REV_FONT_BIT;
    }
	
    if (ch==ATARI_EOL) {
        if (inverse_mode) {
            font_index |= XEP80_FONTS_REV_FONT_BIT;
        }
        else {
            font_index &= ~XEP80_FONTS_REV_FONT_BIT;
        }
    }

	/* Skip the charcter if the last one was a displayed double */
	if (screen_col != 0 && !cur) {
		if (IS_DOUBLE(x-1,y)) {
			int firstd;
			
			firstd = x-1;
			while (firstd > xscroll) {
				if (!IS_DOUBLE(firstd,y)) {
					firstd++;
					break;
					}
				firstd--;
			}
			if ((x-firstd) % 2)
				return;
		}
	}
	
	/* Check if we are doing a cursor, and the charcter before is double */
	if (cur) {
		if (screen_col != 0) {
			if (IS_DOUBLE(x-1,y))
				last_double_cur = TRUE;
		}
	}

	if (y*XEP80_CHAR_HEIGHT < XEP80_first_row)
		XEP80_first_row = y*XEP80_CHAR_HEIGHT;
	if (y*XEP80_CHAR_HEIGHT + XEP80_CHAR_HEIGHT - 1 > XEP80_last_row)
		XEP80_last_row = y*XEP80_CHAR_HEIGHT + XEP80_CHAR_HEIGHT - 1;

    if (inverse_mode) {
        on = XEP80_Fonts_offcolor;
        off = XEP80_Fonts_oncolor;
    }
    else {
        on = XEP80_Fonts_oncolor;
        off = XEP80_Fonts_offcolor;
    }

	if (font_index & XEP80_FONTS_REV_FONT_BIT) {
		blink = on;
		}
	else {
		blink = off;
		}

    if (font_blank) {
		UBYTE color;
		
        to = &XEP80_screen_1[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
        for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
            if (cur || (font_index & XEP80_FONTS_REV_FONT_BIT)) {
				color = on;
            }
            else {
				color = off;
            }

            for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
				if (font_double)
					*to++ = color;
                *to++ = color;
            }
			if (font_double)
				to += XEP80_SCRN_WIDTH - 2*XEP80_CHAR_WIDTH;
			else
				to += XEP80_SCRN_WIDTH - 1*XEP80_CHAR_WIDTH;
        }

        to = &XEP80_screen_2[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
        for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
            if ((cur && !cursor_blink) || (font_index & XEP80_FONTS_REV_FONT_BIT)) {
				color = on;
            }
            else {
				color = off;
            }

            for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
				if (font_double)
					*to++ = color;
                *to++ = color;
            }
			if (font_double)
				to += XEP80_SCRN_WIDTH - 2*XEP80_CHAR_WIDTH;
			else
				to += XEP80_SCRN_WIDTH - 1*XEP80_CHAR_WIDTH;
        }
    }
    else if (font_double && !cur) {
		int width;
		
		if (screen_col == 79)
			width = XEP80_CHAR_WIDTH/2;
		else
			width = XEP80_CHAR_WIDTH;
		
        to = &XEP80_screen_1[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
        for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
            from = XEP80_Fonts_atari_fonts[char_set][font_index][ch][font_row];

            for (font_col=0; font_col < width; font_col++) {
                *to++ = *from;
                *to++ = *from++;
            }
            to += XEP80_SCRN_WIDTH - 2*XEP80_CHAR_WIDTH;
        }

        to = &XEP80_screen_2[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
        for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
			if (blink_rev)
				from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row];
			else
				from = XEP80_Fonts_atari_fonts[char_set][font_index][ch][font_row];

            for (font_col=0; font_col < width; font_col++) {
                if (font_blink && !cur && !blink_rev) {
					if ((font_index & XEP80_FONTS_UNDER_FONT_BIT) && font_row == XEP80_FONTS_UNDER_ROW) {
						*to++ = *from;
						*to++ = *from++;
					}
					else {
						*to++ = blink;
						*to++ = blink;
						from++;
					}
                }
                else {
                    *to++ = *from;
                    *to++ = *from++;
                }
            }
            to += XEP80_SCRN_WIDTH - 2*XEP80_CHAR_WIDTH;
        }
    }
    else if ((font_double || last_double_cur) && cur && !cursor_overwrite) {
		int first_half, start_col, end_col;
		
		/* Determine if this is a double first or second half */
		if (screen_col == 0) {
			first_half = TRUE;
		} else {
			if (IS_DOUBLE(x-1,y)) {
				int firstd;
			
				firstd = x-1;
				while (firstd > xscroll) {
					if (!IS_DOUBLE(firstd,y)) {
						firstd++;
						break;
						}
					firstd--;
				}
				first_half = (((x-firstd) % 2) == 0);
			}
			else {
				first_half = TRUE;
			}
		}
		
		if (first_half) {
			start_col = 0;
			end_col = 3;
		}
		else {
			start_col = 3;
			end_col = 6;
			ch = xep80_data[y][x-1];
		}
		
        to = &XEP80_screen_1[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
		for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
			from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row] + start_col;
			if (first_half)
				*to++ = *from++;
			for (font_col=start_col; font_col < end_col; font_col++) {
				*to++ = *from;
				*to++ = *from++;
			}
			if (!first_half)
				*to++ = *from;
			to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
		}
        to = &XEP80_screen_2[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
		for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
			if (!cursor_blink) {
				from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row] + start_col;
			}
			else {
				from = XEP80_Fonts_atari_fonts[char_set][font_index][ch][font_row] + start_col;
			}
			if (first_half)
				*to++ = *from++;
			for (font_col=start_col; font_col < end_col; font_col++) {
				*to++ = *from;
				*to++ = *from++;
			}
			if (!first_half)
				*to++ = *from;
			to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
		}
	}
    else {
        to = &XEP80_screen_1[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
		if (cur & cursor_overwrite) {
			for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
				for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
					*to++ = on;
				}
				to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
			}
		}
		else {
			for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
				if (cur) {
					from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row];
				}
				else {
					from = XEP80_Fonts_atari_fonts[char_set][font_index][ch][font_row];
				}

				for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
					*to++ = *from++;
				}
				to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
			}
		}

        to = &XEP80_screen_2[XEP80_SCRN_WIDTH * XEP80_CHAR_HEIGHT * y + 
                             screen_col * XEP80_CHAR_WIDTH];
		if (cur & cursor_overwrite) {
			if (cursor_blink) {
				for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
					for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
						*to++ = off;
					}
				to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
				}
			}
			else {
				for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
					for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
						*to++ = on;
					}
				to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
				}
			}
		}
		else {
			for (font_row=0;font_row < XEP80_CHAR_HEIGHT; font_row++) {
				if (cur && !cursor_blink) {
					from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row];
				}
				else {
					if (blink_rev)
						from = XEP80_Fonts_atari_fonts[char_set][font_index ^ XEP80_FONTS_REV_FONT_BIT][ch][font_row];
					else
						from = XEP80_Fonts_atari_fonts[char_set][font_index][ch][font_row];
				}
				for (font_col=0; font_col < XEP80_CHAR_WIDTH; font_col++) {
					if (font_blink && !cur) {
						if ((font_index & XEP80_FONTS_UNDER_FONT_BIT) && font_row == XEP80_FONTS_UNDER_ROW) {
							*to++ = *from++;
						}
						else {
							*to++ = blink;
							from++;
						}
					}
					else {
						*to++ = *from++;
					}
				}
				to += XEP80_SCRN_WIDTH - XEP80_CHAR_WIDTH;
			}
		}
    }
}

static void XEP80_BlitScreen(void)
{
    int screen_row, screen_col;

    for (screen_row = 0; screen_row < XEP80_HEIGHT; screen_row++) {
        for (screen_col = xscroll; screen_col < xscroll + XEP80_LINE_LEN;
             screen_col++) {
            XEP80_BlitChar(screen_col, screen_row, FALSE);
        }
    }
}

static void XEP80_BlitRows(int y_start, int y_end)
{
    int screen_row, screen_col;

    for (screen_row = y_start; screen_row <= y_end; screen_row++) {
        for (screen_col = xscroll; screen_col < xscroll + XEP80_LINE_LEN;
             screen_col++) {
            XEP80_BlitChar(screen_col, screen_row, FALSE);
        }
    }
}

static void XEP80_BlitGraphChar(x, y)
{
    int graph_col;
    UBYTE *to1,*to2;
    UBYTE ch;
    UBYTE on, off;

    if (inverse_mode) {
        on = XEP80_Fonts_offcolor;
        off = XEP80_Fonts_oncolor;
    }
    else {
        on = XEP80_Fonts_oncolor;
        off = XEP80_Fonts_offcolor;
    }
	
	if (y + XEP80_GRAPH_Y_OFFSET < XEP80_first_row)
		XEP80_first_row = y + XEP80_GRAPH_Y_OFFSET;
	if (y + XEP80_GRAPH_Y_OFFSET > XEP80_last_row)
		XEP80_last_row = y + XEP80_GRAPH_Y_OFFSET;

    ch = xep80_graph_data[y][x];

    to1 = &XEP80_screen_1[XEP80_SCRN_WIDTH * (y + XEP80_GRAPH_Y_OFFSET)
                          + x * 8 + XEP80_GRAPH_X_OFFSET];
    to2 = &XEP80_screen_2[XEP80_SCRN_WIDTH * (y + XEP80_GRAPH_Y_OFFSET)
                          + x * 8 + XEP80_GRAPH_X_OFFSET];

    for (graph_col=7; graph_col >= 0; graph_col--) {
        if (ch & (1<<graph_col)) {
            *to1++ = on;
            *to2++ = on;
        }
        else {
            *to1++ = off;
            *to2++ = off;
        }
    }
}

static void XEP80_BlitGraphScreen(void)
{
	int x, y;
	
	for (x=0; x<XEP80_GRAPH_WIDTH/8; x++)
		for (y=0; y<XEP80_GRAPH_HEIGHT; y++)
			XEP80_BlitGraphChar(x,y);
}

void XEP80_ChangeColors(void)
{
	XEP80_Fonts_InitFonts();
	if (graphics_mode)
		XEP80_BlitGraphScreen();
	else {
		XEP80_BlitScreen();
		XEP80_BlitChar(xcur, ycur, TRUE);
		}
}

void XEP80StateSave(void)
{
	StateSav_SaveINT(&XEP80_enabled, 1);
	if (XEP80_enabled) {
		StateSav_SaveINT(&XEP80_port, 1);
		StateSav_SaveINT(&Atari_xep80, 1);
		StateSav_SaveINT(&output_state, 1);
		StateSav_SaveINT(&output_word, 1);
		StateSav_SaveINT(&input_state, 1);
		StateSav_SaveINT(&input_word, 1);
		StateSav_SaveUWORD(input_queue, IN_QUEUE_SIZE);
		StateSav_SaveINT(&input_queue_read, 1);
		StateSav_SaveINT(&input_queue_write, 1);

		StateSav_SaveINT(&xcur, 1);
		StateSav_SaveINT(&xscroll, 1);
		StateSav_SaveINT(&ycur, 1);
		StateSav_SaveINT(&new_xcur, 1);
		StateSav_SaveINT(&new_ycur, 1);
		StateSav_SaveINT(&old_xcur, 1);
		StateSav_SaveINT(&old_ycur, 1);
		StateSav_SaveINT(&lmargin, 1);
		StateSav_SaveINT(&rmargin, 1);
		StateSav_SaveUBYTE(&attrib_a, 1);
		StateSav_SaveUBYTE(&attrib_b, 1);
		StateSav_SaveINT(&list_mode, 1);
		StateSav_SaveINT(&char_set, 1);
		StateSav_SaveINT(&cursor_on, 1);
		StateSav_SaveINT(&cursor_blink, 1);
		StateSav_SaveINT(&cursor_overwrite, 1);
		StateSav_SaveINT(&blink_reverse, 1);
		StateSav_SaveINT(&inverse_mode, 1);
		StateSav_SaveINT(&screen_output, 1);
		StateSav_SaveINT(&burst_mode, 1);
		StateSav_SaveINT(&graphics_mode, 1);
		StateSav_SaveINT(&pal_mode, 1);
		StateSav_SaveINT(&font_a_index, 1);
		StateSav_SaveINT(&font_a_double, 1);
		StateSav_SaveINT(&font_a_blank, 1);
		StateSav_SaveINT(&font_a_blink, 1);
		StateSav_SaveINT(&font_b_index, 1);
		StateSav_SaveINT(&font_b_double, 1);
		StateSav_SaveINT(&font_b_blank, 1);
		StateSav_SaveINT(&font_b_blink, 1);
		StateSav_SaveUBYTE(&xep80_data[0][0], XEP80_HEIGHT * XEP80_WIDTH);
		StateSav_SaveUBYTE(&xep80_graph_data[0][0], XEP80_GRAPH_HEIGHT*XEP80_GRAPH_WIDTH/8);
	}
}

void XEP80StateRead(void)
{
	int local_xep80_enabled = 0;
	int local_xep80 = 0;
	
	/* test for end of file */
	StateSav_ReadINT(&local_xep80_enabled, 1);
	if (local_xep80_enabled) {
		StateSav_ReadINT(&XEP80_port, 1);
		StateSav_ReadINT(&local_xep80, 1);
		StateSav_ReadINT(&output_state, 1);
		StateSav_ReadINT(&output_word, 1);
		StateSav_ReadINT(&input_state, 1);
		StateSav_ReadINT(&input_word, 1);
		StateSav_ReadUWORD(input_queue, IN_QUEUE_SIZE);
		StateSav_ReadINT(&input_queue_read, 1);
		StateSav_ReadINT(&input_queue_write, 1);

		StateSav_ReadINT(&xcur, 1);
		StateSav_ReadINT(&xscroll, 1);
		StateSav_ReadINT(&ycur, 1);
		StateSav_ReadINT(&new_xcur, 1);
		StateSav_ReadINT(&new_ycur, 1);
		StateSav_ReadINT(&old_xcur, 1);
		StateSav_ReadINT(&old_ycur, 1);
		StateSav_ReadINT(&lmargin, 1);
		StateSav_ReadINT(&rmargin, 1);
		StateSav_ReadUBYTE(&attrib_a, 1);
		StateSav_ReadUBYTE(&attrib_b, 1);
		StateSav_ReadINT(&list_mode, 1);
		StateSav_ReadINT(&char_set, 1);
		StateSav_ReadINT(&cursor_on, 1);
		StateSav_ReadINT(&cursor_blink, 1);
		StateSav_ReadINT(&cursor_overwrite, 1);
		StateSav_ReadINT(&blink_reverse, 1);
		StateSav_ReadINT(&inverse_mode, 1);
		StateSav_ReadINT(&screen_output, 1);
		StateSav_ReadINT(&burst_mode, 1);
		StateSav_ReadINT(&graphics_mode, 1);
		StateSav_ReadINT(&pal_mode, 1);
		StateSav_ReadINT(&font_a_index, 1);
		StateSav_ReadINT(&font_a_double, 1);
		StateSav_ReadINT(&font_a_blank, 1);
		StateSav_ReadINT(&font_a_blink, 1);
		StateSav_ReadINT(&font_b_index, 1);
		StateSav_ReadINT(&font_b_double, 1);
		StateSav_ReadINT(&font_b_blank, 1);
		StateSav_ReadINT(&font_b_blink, 1);
		StateSav_ReadUBYTE(&xep80_data[0][0], XEP80_HEIGHT * XEP80_WIDTH);
		StateSav_ReadUBYTE(&xep80_graph_data[0][0], XEP80_GRAPH_HEIGHT*XEP80_GRAPH_WIDTH/8);
		if (!XEP80_Fonts_inited)
			XEP80_Fonts_InitFonts();
		if (graphics_mode)
			XEP80_BlitGraphScreen();
		else {
			XEP80_BlitScreen();
			XEP80_BlitChar(xcur, ycur, TRUE);
			}
		XEP80_enabled = TRUE;
		/* not correct for SDL version */
#if 0
		if (Atari_xep80 != local_xep80)
			Atari_SwitchXep80();
#endif
		Atari_xep80 = local_xep80;
		}
	else {
		XEP80_enabled = FALSE;
		/* not correct for SDL version */
#if 0
		if (Atari_xep80)
			Atari_SwitchXep80();
#endif
		}
}

#endif /* XEP80 */

/*
vim:ts=4:sw=4:
*/

