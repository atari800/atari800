#include <time.h>

#include "atari.h"
#include "memory.h"
#include "log.h"

extern UBYTE *cart_image;
extern int cart_type;

static int rtime_state = 0;
				/* 0 = waiting for register # */
				/* 1 = got register #, waiting for hi nybble */
				/* 2 = got hi nybble, waiting for lo nybble */
static int rtime_tmp = 0; 
static int rtime_tmp2 = 0; 

static int regset[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int hex2bcd(int h)
{
	return(((h/10) << 4) | (h % 10));
}

int gettime(int p)
{
	time_t tt;
	struct tm *lt;

	tt = time(NULL);
	lt = localtime(&tt);

	switch (p) {
	case 5:
		return(hex2bcd(lt->tm_year));
	case 4:
		return(hex2bcd(lt->tm_mon + 1)); /* Months are zero-base indexed by RTIME */
	case 3:
		return(hex2bcd(lt->tm_mday));
	case 2:
		return(hex2bcd(lt->tm_hour));
	case 1:
		return(hex2bcd(lt->tm_min));
	case 0:
		return(hex2bcd(lt->tm_sec));
	case 6:
		return(hex2bcd(((lt->tm_wday+2)%7)+1));
	}
	return(0);
}

void fillarray()
{
	regset[0] = gettime(0); 	
	regset[1] = gettime(1); 	
	regset[2] = gettime(2); 	
	regset[3] = gettime(3); 	
	regset[4] = gettime(4); 	
	regset[5] = gettime(5); 	
	regset[6] = gettime(6); 	
}

UBYTE SuperCart_GetByte(UWORD addr)
{
	/* Aprint("d5xx read @ 0x%x", addr); */
	if (addr == 0xd5b8 || addr == 0xd5b9) {
		fillarray();
		switch(rtime_state) {
		case 0:
			/* Aprint("pretending rtime not busy, returning 0"); */
			return(0);
		case 1:
			if (rtime_tmp > 5)
			Aprint("returning %d as hi nybble of register %d",
				regset[rtime_tmp] >> 4, rtime_tmp);
			rtime_state = 2;
			return(regset[rtime_tmp] >> 4);
		case 2:
			if (rtime_tmp > 5)
			Aprint("returning %d as lo nybble of register %d",
				regset[rtime_tmp] & 0x0f, rtime_tmp);
			rtime_state = 0;
			return(regset[rtime_tmp] & 0x0f);
		}
	}
	return(0);
}

int SuperCart_PutByte(UWORD addr, UBYTE byte)
{
	/* Aprint("d5xx write: 0x%x @ 0x%x", byte, addr); */
	if (addr == 0xd5b8 || addr == 0xd5b9) {
		switch (rtime_state) {
		case 0:
			if (byte > 5)
			Aprint("setting active register to %d", byte);
			rtime_tmp = byte;
			rtime_state = 1;
			break;
		case 1:
			if (rtime_tmp > 5)
			Aprint("got %d as hi nybble of register %d",
				byte, rtime_tmp);	
			rtime_tmp2 = byte << 4;
			rtime_state = 2;
			break;
		case 2:
			if (rtime_tmp > 5)
			Aprint("got %d as lo nybble, register %d now = %d",
				byte, rtime_tmp, rtime_tmp2 | byte);
			rtime_tmp2 |= byte;
			regset[rtime_tmp] = rtime_tmp2;
			rtime_state = 0;
			break;
		}
		return FALSE;
	}

	supercart_handler(addr, byte);

	return FALSE;
}
