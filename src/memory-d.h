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

typedef int ATPtr;

int Insert_8K_ROM(char *filename);
int Insert_16K_ROM(char *filename);
int Insert_OSS_ROM(char *filename);
int Insert_DB_ROM(char *filename);
int Insert_32K_5200ROM(char *filename);
int Remove_ROM(void);
int Insert_Cartridge(char *filename);
void RestoreSIO( void );
void Coldstart(void);
int Initialise_AtariXL(void);
int Initialise_Atari5200(void);
int Initialise_EmuOS(void);
void ClearRAM(void);
int bounty_bob1(UWORD addr);
int bounty_bob2(UWORD addr);
void DisablePILL(void);
void EnablePILL(void);
int Initialise_AtariOSA(void);
int Initialise_AtariOSB(void);
void MemStateSave(UBYTE SaveVerbose);
void MemStateRead(UBYTE SaveVerbose);
void CopyFromMem(ATPtr from, UBYTE * to, int size);
void CopyToMem(UBYTE * from, ATPtr to, int size);
void PORTB_handler(UBYTE byte);
void supercart_handler(UWORD addr, UBYTE byte);
void get_charset(char * cs);
