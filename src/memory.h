#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <string.h>	/* memcpy, memset */

#include "atari.h" /* UBYTE, UWORD */
#include "config.h"

#define dGetByte(x)				(memory[x])
#define dPutByte(x, y)			(memory[x] = y)

#ifndef WORDS_BIGENDIAN
#ifdef UNALIGNED_LONG_OK
#define dGetWord(x)				(* (UWORD *) &memory[x])
#define dPutWord(x, y)			(* (UWORD *) &memory[x] = (y))
#define dGetWordAligned(x)		dGetWord(x)
#define dPutWordAligned(x, y)	dPutWord(x, y)
#else	/* UNALIGNED_LONG_OK */
#define dGetWord(x)				(memory[x] + (memory[(x) + 1] << 8))
#define dPutWord(x, y)			(memory[x] = (UBYTE) (y), memory[x + 1] = (UBYTE) ((y) >> 8))
/* faster versions of dGetWord and dPutWord for even addresses */
/* TODO: guarantee that memory is UWORD-aligned and use UWORD access */
#define dGetWordAligned(x)		dGetWord(x)
#define dPutWordAligned(x, y)	dPutWord(x, y)
#endif	/* UNALIGNED_LONG_OK */
#else	/* WORDS_BIGENDIAN */
/* can't do any word optimizations for big endian machines */
#define dGetWord(x)				(memory[x] + (memory[(x) + 1] << 8))
#define dPutWord(x, y)			(memory[x] = (UBYTE) (y), memory[x + 1] = (UBYTE) ((y) >> 8))
#define dGetWordAligned(x)		dGetWord(x)
#define dPutWordAligned(x, y)	dPutWord(x, y)
#endif	/* WORDS_BIGENDIAN */

#define dCopyFromMem(from, to, size)	memcpy(to, memory + (from), size)
#define dCopyToMem(from, to, size)		memcpy(memory + (to), from, size)
#define dFillMem(addr1, value, length)	memset(memory + (addr1), value, length)

extern UBYTE memory[65536];

#ifndef PAGED_ATTRIB

extern UBYTE attrib[65536];
#define RAM 0
#define ROM 1
#define HARDWARE 2
#define GetByte(addr)		(attrib[addr] == HARDWARE ? Atari800_GetByte(addr) : memory[addr])
#define PutByte(addr, byte)	 do { if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) Atari800_PutByte(addr, byte); } while (0)
#define SetRAM(addr1, addr2) memset(attrib + (addr1), RAM, (addr2) - (addr1) + 1)
#define SetROM(addr1, addr2) memset(attrib + (addr1), ROM, (addr2) - (addr1) + 1)
#define SetHARDWARE(addr1, addr2) memset(attrib + (addr1), HARDWARE, (addr2) - (addr1) + 1)

#else /* PAGED_ATTRIB */

typedef UBYTE (*rdfunc)(UWORD addr);
typedef void (*wrfunc)(UWORD addr, UBYTE value);

extern rdfunc readmap[256];
extern wrfunc writemap[256];
void ROM_PutByte(UWORD addr, UBYTE byte);
#define GetByte(addr)		(readmap[(addr) >> 8] ? (*readmap[(addr) >> 8])(addr) : memory[addr])
#define PutByte(addr,byte)	(writemap[(addr) >> 8] ? (*writemap[(addr) >> 8])(addr, byte) : (memory[addr] = byte))
#define SetRAM(addr1, addr2) do { \
		int i; \
		for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
			readmap[i] = NULL; \
			writemap[i] = NULL; \
		} \
	} while (0)
#define SetROM(addr1, addr2) do { \
		int i; \
		for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
			readmap[i] = NULL; \
			writemap[i] = ROM_PutByte; \
		} \
	} while (0)

#endif /* PAGED_ATTRIB */

extern int have_basic;
extern int cartA0BF_enabled;

void MEMORY_InitialiseMachine(void);
void MemStateSave(UBYTE SaveVerbose);
void MemStateRead(UBYTE SaveVerbose);
void CopyFromMem(UWORD from, UBYTE *to, int size);
void CopyToMem(const UBYTE *from, UWORD to, int size);
void MEMORY_HandlePORTB(UBYTE byte, UBYTE oldval);
void Cart809F_Disable(void);
void Cart809F_Enable(void);
void CartA0BF_Disable(void);
void CartA0BF_Enable(void);
#define CopyROM(addr1, addr2, src) memcpy(memory + (addr1), src, (addr2) - (addr1) + 1)
void get_charset(UBYTE *cs);

#endif /* _MEMORY_H_ */
