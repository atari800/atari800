#include <string.h>	/* memset */

#include "config.h"

#ifdef PAGED_MEM
#define dGetByte(x) GetByte(x)
UBYTE GetByte(UWORD addr);
#define dPutByte(x,y) PutByte(x,y)
int PutByte(UWORD addr, UBYTE byte);
#else
#define dGetByte(x) (memory[x])
#define dPutByte(x,y) (memory[x] = y)
extern UBYTE memory[65536];
extern UBYTE attrib[65536];
#define	GetByte(addr)		((attrib[addr] == HARDWARE) ? Atari800_GetByte(addr) : memory[addr])
#define	PutByte(addr,byte)	if (attrib[addr] == RAM) memory[addr] = byte; else if (attrib[addr] == HARDWARE) Atari800_PutByte(addr,byte);
#define RAM 0
#define ROM 1
#define HARDWARE 2
#define SetRAM(addr1,addr2) memset(attrib+addr1,RAM,addr2-addr1+1)
#define SetROM(addr1,addr2) memset(attrib+addr1,ROM,addr2-addr1+1)
#define SetHARDWARE(addr1,addr2) memset(attrib+addr1,HARDWARE,addr2-addr1+1)
#endif
#define Poke(x,y) (dPutByte((x),(y)))

extern int have_basic;
extern int cartA0BF_enabled;

typedef int ATPtr;

void PatchOS(void);
int Initialise_AtariXL(void);
int Initialise_Atari5200(void);
int Initialise_EmuOS(void);
void ClearRAM(void);
void DisablePILL(void);
void EnablePILL(void);
int Initialise_AtariOSA(void);
int Initialise_AtariOSB(void);
void MemStateSave(UBYTE SaveVerbose);
void MemStateRead(UBYTE SaveVerbose);
void CopyFromMem(ATPtr from, UBYTE * to, int size);
void CopyToMem(UBYTE * from, ATPtr to, int size);
void PORTB_handler(UBYTE byte);
void Cart809F_Disable(void);
void Cart809F_Enable(void);
void CartA0BF_Disable(void);
void CartA0BF_Enable(void);
#define CopyROM(addr1,addr2,src) memcpy(memory+addr1,src,addr2-addr1+1)
void get_charset(char * cs);
