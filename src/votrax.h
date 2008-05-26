#ifndef VOTRAX_H
#define VOTRAX_H

#define MAX_VOTRAXSC01 1
#include "atari.h"

#define INT16 SWORD
#define UINT16 UWORD
#define INLINE

typedef void (*VOTRAXSC01_BUSYCALLBACK)(int);

struct VOTRAXSC01interface
{
        int num;	/* total number of chips */
	VOTRAXSC01_BUSYCALLBACK BusyCallback[MAX_VOTRAXSC01];	/* callback function when busy signal changes */
};

int VOTRAXSC01_sh_start(void *sound_interface);
void VOTRAXSC01_sh_stop(void);

void votraxsc01_w(UBYTE data);
UBYTE votraxsc01_status_r(void);

void Votrax_Update(int num, INT16 *buffer, int length);
int votraxsc01_samples(int currentP, int nextP, int cursamples);

#endif
