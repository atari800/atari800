#include "config.h"

#define dGetByte(x) GetByte(x)

#ifdef INLINE_GETBYTE
typedef UBYTE(*rdfunc)(UWORD addr);
extern rdfunc readmap[256];
extern inline UBYTE GetByte(UWORD addr)
{
	return ((*readmap[addr>>8])(addr));
}
#else
UBYTE GetByte(UWORD addr);
#endif

#define dPutByte(x,y) PutByte(x,y)
int PutByte(UWORD addr, UBYTE byte);
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
int Initialise_Monty(void);
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
