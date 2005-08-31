#ifndef _CARTRIDGE_H_
#define _CARTRIDGE_H_

#include "config.h"
#include "atari.h"

#define CART_NONE		0
#define CART_STD_8		1
#define CART_STD_16		2
#define CART_OSS_16		3
#define CART_5200_32	4
#define CART_DB_32		5
#define CART_5200_EE_16	6
#define CART_5200_40	7
#define CART_WILL_64	8
#define CART_EXP_64		9
#define CART_DIAMOND_64	10
#define CART_SDX_64		11
#define CART_XEGS_32	12
#define CART_XEGS_64	13
#define CART_XEGS_128	14
#define CART_OSS2_16	15
#define CART_5200_NS_16	16
#define CART_ATRAX_128	17
#define CART_BBSB_40	18
#define CART_5200_8		19
#define CART_5200_4		20
#define CART_RIGHT_8	21
#define CART_WILL_32	22
#define CART_XEGS_256	23
#define CART_XEGS_512	24
#define CART_XEGS_1024	25
#define CART_MEGA_16	26
#define CART_MEGA_32	27
#define CART_MEGA_64	28
#define CART_MEGA_128	29
#define CART_MEGA_256	30
#define CART_MEGA_512	31
#define CART_MEGA_1024	32
#define CART_SWXEGS_32	33
#define CART_SWXEGS_64	34
#define CART_SWXEGS_128	35
#define CART_SWXEGS_256	36
#define CART_SWXEGS_512	37
#define CART_SWXEGS_1024 38
#define CART_PHOENIX_8	39
#define CART_BLIZZARD_16 40
#define CART_ATMAX_128	41
#define CART_ATMAX_1024 42
#define CART_LAST_SUPPORTED 42

#define CART_MAX_SIZE	(1024 * 1024)
extern int cart_kb[CART_LAST_SUPPORTED + 1];
extern int cart_type;

int CART_IsFor5200(int type);
int CART_Checksum(const UBYTE *image, int nbytes);

#define CART_CANT_OPEN		-1	/* Can't open cartridge image file */
#define CART_BAD_FORMAT		-2	/* Unknown cartridge format */
#define CART_BAD_CHECKSUM	-3	/* Warning: bad CART checksum */
int CART_Insert(const char *filename);

void CART_Remove(void);

void CART_Start(void);
UBYTE CART_GetByte(UWORD addr);
void CART_PutByte(UWORD addr, UBYTE byte);
void CART_BountyBob1(UWORD addr);
void CART_BountyBob2(UWORD addr);
#ifdef PAGED_ATTRIB
UBYTE BountyBob1_GetByte(UWORD addr);
UBYTE BountyBob2_GetByte(UWORD addr);
void BountyBob1_PutByte(UWORD addr, UBYTE value);
void BountyBob2_PutByte(UWORD addr, UBYTE value);
#endif

#endif /* _CARTRIDGE_H_ */
