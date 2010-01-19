/*
 * sound.c - Win32 port specific code
 *
 * Copyright (C) 2000 Krzysztof Nikiel
 * Copyright (C) 2000-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#ifdef SOUND

#include <windows.h>
#ifdef DIRECTX
# define DIRECTSOUND_VERSION 0x0500
# include <dsound.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "sound.h"
#include "main.h"
#include "pokeysnd.h"
#include "atari.h"
#include "log.h"

#define MIXBUFSIZE	0x1000
#define WAVSHIFT	9
#define WAVSIZE		(1 << WAVSHIFT)
int buffers = 0;

enum {SOUND_NONE, SOUND_DX, SOUND_WAV};

static int usesound = TRUE;
static int issound = SOUND_NONE;
static int dsprate = 44100;
static int snddelay = 40;	/* delay in milliseconds */
static int snddelaywav = 100;
static int bit16 = FALSE;

static HANDLE event;
static HWAVEOUT wout;
static WAVEHDR *waves;

#ifdef DIRECTX
static int wavonly = FALSE;
static DWORD sbufsize = 0;
static int samples = 0; 	/* #samples to be in play buffer */
static UBYTE mixbuf[MIXBUFSIZE];
static DWORD bufpos = 0;

static LPDIRECTSOUND lpDS = NULL;
static LPDIRECTSOUNDBUFFER pDSB = NULL;

static int initsound_dx(void)
{
  DWORD i;
  int err;
  DSBUFFERDESC dsBD =
  {0};
  WAVEFORMATEX wfx;
  DSBCAPS bc;

  LPVOID pMem1, pMem2;
  DWORD dwSize1, dwSize2;

  if (issound != SOUND_NONE)
    return 0;

  if ((err = DirectSoundCreate(NULL, &lpDS, NULL)) < 0)
    return err;
  if ((err = IDirectSound_SetCooperativeLevel(lpDS, hWndMain,
					      DSSCL_EXCLUSIVE)) < 0)
    goto end;

  dsBD.dwSize = sizeof(dsBD);
  dsBD.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
  dsBD.dwBufferBytes = 0x4000;
  dsBD.lpwfxFormat = &wfx;

  wfx.wFormatTag = WAVE_FORMAT_PCM;
#ifdef STEREO_SOUND
  wfx.nChannels = POKEYSND_stereo_enabled ? 2 : 1;
#else
  wfx.nChannels = 1;
#endif /* STEREO_SOUND */
  wfx.nSamplesPerSec = dsprate;
  wfx.nAvgBytesPerSec = dsprate * wfx.nChannels * (bit16 ? 2 : 1);
  wfx.nBlockAlign = wfx.nChannels * (bit16 ? 2 : 1);
  wfx.wBitsPerSample = bit16 ? 16 : 8;
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

  POKEYSND_Init(POKEYSND_FREQ_17_EXACT, (UWORD) dsprate, (UBYTE) wfx.nChannels, (bit16 ? POKEYSND_BIT16 : 0));

  samples = dsprate * snddelay / 1000;

  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &bufpos);

  issound = SOUND_DX;
  return 0;

end:
  MessageBox(hWndMain, "DirectSound Init FAILED", myname, MB_OK);
  Sound_Exit();
  return err;
}

static void uninitsound_dx(void)
{
  if (issound != SOUND_DX)
    return;

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
  issound = SOUND_NONE;
}

static void sound_update_dx(void)
{
  DWORD wc;
  int d1;
  LPVOID pMem1, pMem2;
  DWORD dwSize1, dwSize2;
  signed short *p1s;
  signed short *p2s;
  UBYTE *p1b;
  UBYTE *p2b;
  signed short *pbufs;
  UBYTE *pbufb;
  int s1, s2;
  int err;
  int i;
  int samplesize = bit16 ? 2 : 1;

#ifdef STEREO_SOUND
  if (POKEYSND_stereo_enabled) samplesize *= 2;
#endif

  if (issound != SOUND_DX)
    return;

  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &wc);

  d1 = (wc - bufpos);
	if ((DWORD) abs(d1) > (sbufsize >> 1)) {
		if (d1 < 0)
			d1 += sbufsize;
		else
			d1 -= sbufsize;
	}
  if (d1 < (-samples * samplesize)) // there is more than necessary bytes filled?
    return;

  d1 = (samples * samplesize) + d1; // bytes to fill
  d1 = (d1 / samplesize) * samplesize; //round to a sample pair

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
	Sound_Continue();
      return;
    }

  s1 = dwSize1;
  s2 = dwSize2;
  p1s = (signed short *)pMem1;
  p2s = (signed short *)pMem2;
  p1b = (UBYTE *)pMem1;
  p2b = (UBYTE *)pMem2;
  bufpos += (s1 + s2);
  if (bufpos >= sbufsize)
    bufpos -= sbufsize;

  if (bit16)
  {
    s1 /= 2;
    s2 /= 2;
  }
  i = s1 + s2;

  POKEYSND_Process(mixbuf, i);

  pbufs = (signed short *)mixbuf;
  pbufb = (UBYTE *)mixbuf;
  if (s1)
    {
      if (bit16)
      for (; s1; s1--)
	  *p1s++ = *pbufs++;
      else
	for (; s1; s1--)
	  *p1b++ = *pbufb++;
    }
  if (s2)
    {
      if (bit16)
      for (; s2; s2--)
	  *p2s++ = *pbufs++;
      else
	for (; s2; s2--)
	  *p2b++ = *pbufb++;
    }

  IDirectSoundBuffer_Unlock(pDSB, pMem1, dwSize1, pMem2, dwSize2);

  return;
}
#endif /* DIRECTX */

static void uninitsound_wav(void)
{
  int i;
  MMRESULT err;

  if (issound != SOUND_WAV)
    return;

l0:
  for (i = 0; i < buffers; i++)
    {
      if (!(waves[i].dwFlags & WHDR_DONE))
	{
	  WaitForSingleObject(event, 5000);
	  ResetEvent(event);
	  goto l0;
	}
    }

  waveOutReset (wout);

  for (i = 0; i < buffers; i++)
    {
      err = waveOutUnprepareHeader(wout, &waves[i], sizeof (waves[i]));
      if (err != MMSYSERR_NOERROR)
	{
	  fprintf(stderr, "warning: cannot unprepare wave header (%x)\n", err);
	}
      free(waves[i].lpData);
    }
  free(waves);

  waveOutClose(wout);
  CloseHandle(event);

  issound = SOUND_NONE;
}

static int initsound_wav(void)
{
  int i;
  WAVEFORMATEX wfx;
  MMRESULT err;

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  memset(&wfx, 0, sizeof(wfx));

  wfx.wFormatTag = WAVE_FORMAT_PCM;
#ifdef STEREO_SOUND
  wfx.nChannels = POKEYSND_stereo_enabled ? 2 : 1;
#else
  wfx.nChannels = 1;
#endif /* STEREO_SOUND */
  wfx.nSamplesPerSec = dsprate;
  wfx.nAvgBytesPerSec = dsprate * wfx.nChannels * (bit16 ? 2 : 1);
  wfx.nBlockAlign = wfx.nChannels * (bit16 ? 2 : 1);
  wfx.wBitsPerSample = bit16 ? 16 : 8;
  wfx.cbSize = 0;

  err = waveOutOpen(&wout, WAVE_MAPPER, &wfx, (int)event, 0, CALLBACK_EVENT);
  if (err == WAVERR_BADFORMAT)
    {
      Log_print("wave output parameters unsupported\n");
      exit(1);
    }
  if (err != MMSYSERR_NOERROR)
    {
      Log_print("cannot open wave output (%x)\n", err);
      exit(1);
    }

  buffers = ((wfx.nAvgBytesPerSec * snddelaywav / 1000) >> WAVSHIFT) + 1;
  waves = malloc(buffers * sizeof(*waves));
  for (i = 0; i < buffers; i++)
    {
      memset(&waves[i], 0, sizeof (waves[i]));
      if (!(waves[i].lpData = (UBYTE *)malloc(WAVSIZE)))
	{
	  Log_print("could not get wave buffer memory\n");
	  exit(1);
	}
      waves[i].dwBufferLength = WAVSIZE;
      err = waveOutPrepareHeader(wout, &waves[i], sizeof(waves[i]));
      if (err != MMSYSERR_NOERROR)
	{
	  Log_print("cannot prepare wave header (%x)\n", err);
	  exit(1);
	}
      waves[i].dwFlags |= WHDR_DONE;
    }

  POKEYSND_Init(POKEYSND_FREQ_17_EXACT, (UWORD) dsprate, (UBYTE) wfx.nChannels, (bit16 ? POKEYSND_BIT16 : 0));

  issound = SOUND_WAV;
  return 0;
}

void Sound_Initialise(int *argc, char *argv[])
{
  int i, j;
  int help = FALSE;

  if (issound != SOUND_NONE)
    return;

  for (i = j = 1; i < *argc; i++)
    {
      if (strcmp(argv[i], "-sound") == 0)
	usesound = TRUE;
      else if (strcmp(argv[i], "-nosound") == 0)
	usesound = FALSE;
      else if (strcmp(argv[i], "-audio16") == 0)
	bit16 = TRUE;
      else if (strcmp(argv[i], "-dsprate") == 0)
	sscanf(argv[++i], "%d", &dsprate);
      else if (strcmp(argv[i], "-snddelay") == 0)
      {
	sscanf(argv[++i], "%d", &snddelay);
        snddelaywav = snddelay;
      }
      else if (strcmp(argv[i], "-quality") == 0) {
	int quality;
	sscanf(argv[++i], "%d", &quality);
	if (quality > 1) {
	  POKEYSND_SetMzQuality(quality - 1);
	  POKEYSND_enable_new_pokey = 1;
	}
	else
	  POKEYSND_enable_new_pokey = 0;
      }
#ifdef DIRECTX
      else if (strcmp(argv[i], "-wavonly") == 0)
	wavonly = TRUE;
#endif
      else
      {
	if (strcmp(argv[i], "-help") == 0)
	{
	  Log_print("\t-sound           Enable sound\n"
		 "\t-nosound         Disable sound\n"
#ifdef DIRECTX
		 "\t-wavonly         Disable direct sound\n"
#endif
		 "\t-dsprate <rate>  Set dsp rate\n"
		 "\t-snddelay <ms>   Set sound delay\n"
		 "\t-audio16         Use 16 bit mixing\n"
		 "\t-quality <level> Set sound quality"
		);
	  help = TRUE;
	}
	argv[j++] = argv[i];
      }
    }
  *argc = j;

  if (help)
    return;

  if (!usesound)
    return;

#ifdef DIRECTX
  if (!wavonly)
  {
    i = initsound_dx();
    if (!i)
      return;
  }
#endif
  initsound_wav();
  return;
}

void Sound_Reinit(void)
{
  if (usesound)
  {
  Sound_Exit();
#ifdef DIRECTX
  if (!wavonly)
  {
    int i = initsound_dx();
    if (!i)
      return;
  }
#endif
  initsound_wav();
  }
  return;
}

void Sound_Exit(void)
{
#ifdef DIRECTX
  if (issound == SOUND_DX)
    uninitsound_dx();
#endif
  if (issound == SOUND_WAV)
    uninitsound_wav();

  issound = SOUND_NONE;
}

static WAVEHDR *getwave(void)
{
  int i;

  for (i = 0; i < buffers; i++)
    {
      if (waves[i].dwFlags & WHDR_DONE)
	{
	  waves[i].dwFlags &= ~WHDR_DONE;
	  return &waves[i];
	}
    }

  return NULL;
}

void Sound_Update(void)
{
  MMRESULT err;
  WAVEHDR *wh;

  switch (issound)
  {
  case SOUND_WAV:
    while ((wh = getwave()))
    {
      POKEYSND_Process(wh->lpData, wh->dwBufferLength >> (bit16 ? 1 : 0));
      err = waveOutWrite(wout, wh, sizeof(*wh));
      if (err != MMSYSERR_NOERROR)
      {
	Log_print("cannot write wave output (%x)\n", err);
	return;
      }
    }
    break;
#ifdef DIRECTX
  case SOUND_DX:
    sound_update_dx();
#endif
  }
}

void Sound_Pause(void)
{
#ifdef DIRECTX
  if (issound != SOUND_DX)
    return;
  if (!pDSB)
    return;
  IDirectSoundBuffer_Stop(pDSB);
#endif
}

void Sound_Continue(void)
{
#ifdef DIRECTX
  if (issound != SOUND_DX)
    return;
  if (!pDSB)
    return;
  IDirectSoundBuffer_Restore(pDSB);
  IDirectSoundBuffer_Play(pDSB, 0, 0, DSBPLAY_LOOPING);
  IDirectSoundBuffer_GetCurrentPosition(pDSB, 0, &bufpos);
#endif
}

#endif	/* SOUND */
