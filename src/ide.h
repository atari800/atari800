#ifndef IDE_H_
#define IDE_H_

#include "atari.h"
#include <stdint.h>

extern int IDE_enabled;

int     IDE_Initialise(int *argc, char *argv[]);
uint8_t IDE_GetByte(uint16_t addr);
void    IDE_PutByte(uint16_t addr, uint8_t byte);

#endif
