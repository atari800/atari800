/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#define DIRECTSOUND_VERSION 0x0500

#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "sound.h"
#include "main.h"
#include "pokeysnd.h"
#include "atari.h"
#include "log.h"

#define MIXBUFSIZE     0x800

static LPDIRECTSOUND lpDS = NULL;
static LPDIRECTSOUNDBUFFER pDSB = NULL;

static int issound = FALSE;
static int usesound = TRUE;
static int sbufsize = 0;
static int samples = 0; 	/* #samples to be in play buffer */
static int dsprate = 22050;
static int snddelay = 40;	/* delay in milliseconds */
static UBYTE mixbuf[MIXBUFSIZE];
static DWORD bufpos = 0;

int initsound(int *argc, char *argv[])
{
  int i, j;
  int err;
  DSBUFFERDESC dsBD =
  {0};
  WAVEFORMATEX wfx;
  DSBCAPS bc;

  LPVOID pMem1, pMem2;
  DWORD dwSize1, dwSize2;

  if (issound)
    return 0;

  for (i = j = 1; i < *argc; i++)
    {
      if (strcmp(argv[i], "-sound") == 0)
	usesound = TRUE;
      else if (strcmp(argv[i], "-nosound") == 0)
	usesound = FALSE;
      else if (strcmp(argv[i], "-dsprate") == 0)
	sscanf(argv[++i], "%d", &dsprate);
      else if (strcmp(argv[i], "-snddelay") == 0)
	sscanf(argv[++i], "%d", &snddelay);
      else
      {
	if (strcmp(argv[i], "-help") == 0)
	{
	  Aprint("\t-sound		      enable sound");
	  Aprint("\t-nosound		      disable sound");
	  Aprint("\t-dsprate <rate>	      set dsp rate");
	  Aprint("\t-snddelay <milliseconds>  set sound delay");
	}
	argv[j++] = argv[i];
      }
    }
  *argc = j;

  if (!usesound)
    return 1;

  if ((err = DirectSoundCreate(NULL, &lpDS, NULL)) < 0)
    return err;
  if ((err = IDirectSound_SetCooperativeLevel(lpDS, hWndMain,
					      DSSCL_EXCLUSIVE)) < 0)
    goto end;

  dsBD.dwSize = sizeof(dsBD);
  dsBD.dwFlags =
    DSBCAPS_GETCURRENTPOSITION2
    | DSBCAPS_LOCSOFTWARE;
  dsBD.dwBufferBytes = 0x4000;
  dsBD.lpwfxFormat = &wfx;

  wfx.wFormatTag = 1;
  wfx.nChannels = 1;
#if 0
  wfx.nSamplesPerSec = 22050;
  wfx.nAvgBytesPerSec = 44100;
  wfx.nBlockAlign = 2;
  wfx.wBitsPerSample = 16;
#else
  wfx.nSamplesPerSec = dsprate;
  wfx.nAvgBytesPerSec = dsprate;
  wfx.nBlockAlign = 1;
  wfx.wBitsPerSample = 8;
#endif
  wfx.cbSize = 0;
  if ((err = IDirectSound_CreateSoundBuffer(lpDS, &dsBD, &pDSB, NULL)) < 0)
    goto end;

  bc.dwSize = sizeof(bc);
  IDirectSoundBuffer_GetCaps(pDSB, &bc);
  sbufsize = bc.dwBufferBytes;

  if ((err = IDirectSoundBuffer_Lock(pDSB, 0, sbufsize,
			       &pMem1, &dwSize1, &pMem2, &dwSize2,
			       DSBLOCK_FROMWRITECURSOR)) < 0)
    goto end;

  for (i = 0; i < dwSize1 >> 1; i++)
    *((short *) pMem1 + i) = 0;
  for (i = 0; i < dwSize2 >> 1; i++)
    *((short *) pMem2 + i) = 0;
  IDirectSoundBuffer_Unlock(pDSB, pMem1, dwSize1, pMem2, dwSize2);

  IDirectSoundBuffer_Play(pDSB, 0, 0, DSBPLAY_LOOPING);

  Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);

  samples = dsprate * snddelay / 1000;

  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &bufpos);

  issound = 1;
  return 0;

end:
  MessageBox(hWndMain, "DirectSound Init FAILED", myname, MB_OK);
  uninitsound();
  return err;
}

void uninitsound(void)
{
  issound = 0;

  if (lpDS)
    {
      if (pDSB)
	{
	  IDirectSoundBuffer_Stop(pDSB);
	  IDirectSoundBuffer_Release(pDSB);
	  pDSB = NULL;
	}
      IDirectSound_Release(lpDS);
      lpDS = NULL;
    }
}

void sndrestore(void)
{
  if (!pDSB)
    return;
  IDirectSoundBuffer_Restore(pDSB);
  IDirectSoundBuffer_Play(pDSB, 0, 0, DSBPLAY_LOOPING);
  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &bufpos);
}

void sndhandler(void)
{
  DWORD wc;
  int d1;
  LPVOID pMem1, pMem2;
  DWORD dwSize1, dwSize2;
  UBYTE *p1, *p2;
  int s1, s2;
  int err;
  int i;

  if (!issound)
    return;

  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &wc);

  d1 = (wc - bufpos);
  if (abs(d1) > (sbufsize >> 1))
    {
      if (d1 < 0)
	d1 += sbufsize;
      else
	d1 -= sbufsize;
    }
  if (d1 < -samples)		/* there is more than necessary bytes filled? */
    return;

  d1 = samples + d1;		/* samples to fill */
  if (d1 > (sizeof(mixbuf) / sizeof(mixbuf[0])))
    {
      d1 = (sizeof(mixbuf) / sizeof(mixbuf[0]));
    }

  if ((err = IDirectSoundBuffer_Lock(pDSB,
				     bufpos,
				     d1,
				     &pMem1, &dwSize1,
				     &pMem2, &dwSize2,
				     0)) < 0)
    {
      if (err == DSERR_BUFFERLOST)
	sndrestore();
      return;
    }

  s1 = dwSize1;
  s2 = dwSize2;
  p1 = pMem1;
  p2 = pMem2;
  bufpos += (s1 + s2);
  if (bufpos >= sbufsize)
    bufpos -= sbufsize;

  i = s1 + s2;

  Pokey_process(mixbuf, i);

  i = (int) mixbuf;
  if (s1)
    {
      for (; s1; s1--)
	{
	  *p1++ = *(((UBYTE *) i)++);
	}
    }
  if (s2)
    {
      for (; s2; s2--)
	{
	  *p2++ = *(((UBYTE *) i)++);
	}
    }

  IDirectSoundBuffer_Unlock(pDSB, pMem1, dwSize1, pMem2, dwSize2);

  return;
}

void Sound_Pause(void)
{
  if (!pDSB)
    return;
  IDirectSoundBuffer_Stop(pDSB);
}

void Sound_Continue(void)
{
  sndrestore();
}

/*
$Log$
Revision 1.2  2001/03/24 10:13:43  knik
-nosound option fixed

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
