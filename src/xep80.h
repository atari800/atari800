#ifndef XEP80_H_
#define XEP80_H_

#include "config.h"
#include "atari.h"

#define XEP80_WIDTH			256
#define XEP80_HEIGHT		25
#define XEP80_CHAR_WIDTH	7
#define XEP80_CHAR_HEIGHT	11
#define XEP80_GRAPH_WIDTH   320
#define XEP80_GRAPH_HEIGHT  240
#define XEP80_LINE_LEN		80
#define XEP80_SCRN_WIDTH	(XEP80_LINE_LEN * XEP80_CHAR_WIDTH)
#define XEP80_SCRN_HEIGHT	(XEP80_HEIGHT * XEP80_CHAR_HEIGHT)

#define XEP80_ATARI_EOL			0x9b

extern int XEP80_enabled;
extern int XEP80_port;

extern UBYTE XEP80_screen_1[XEP80_SCRN_WIDTH*XEP80_SCRN_HEIGHT];
extern UBYTE XEP80_screen_2[XEP80_SCRN_WIDTH*XEP80_SCRN_HEIGHT];

UBYTE XEP80_GetBit(void);
void XEP80_PutBit(UBYTE byte);
void XEP80_ChangeColors(void);
void XEP80_StateSave(void);
void XEP80_StateRead(void);
int XEP80_Initialise(int *argc, char *argv[]);

#endif /* XEP80_H_ */
