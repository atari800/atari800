/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */

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

#define MIXBUFSIZE	0x800
#define WAVSIZE		0x100
int buffers = 0;

enum {SOUND_NONE, SOUND_DX, SOUND_WAV};

static int issound = SOUND_NONE;
static int dsprate = 22050;
static int snddelay = 40;	/* delay in milliseconds */
#ifdef STEREO
static int stereo = TRUE;
#else
static int stereo = FALSE;
#endif

static HANDLE event;
static HWAVEOUT wout;
static WAVEHDR *waves;

#ifdef DIRECTX
static int wavonly = FALSE;
static int sbufsize = 0;
static int samples = 0; 	/* #samples to be in play buffer */
static UBYTE mixbuf[MIXBUFSIZE];
static DWORD bufpos = 0;

static LPDIRECTSOUND lpDS = NULL;
static LPDIRECTSOUNDBUFFER pDSB = NULL;

static int initsound_dx(void)
{
  int i;
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
  dsBD.dwFlags =
    DSBCAPS_GETCURRENTPOSITION2
    | DSBCAPS_LOCSOFTWARE;
  dsBD.dwBufferBytes = 0x4000;
  dsBD.lpwfxFormat = &wfx;

  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = stereo ? 2 : 1;
  wfx.nChannels = 1;
  wfx.nSamplesPerSec = dsprate;
  wfx.nAvgBytesPerSec = dsprate * wfx.nChannels;
  wfx.nBlockAlign = stereo ? 2 : 1;
  wfx.wBitsPerSample = 8;
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
  UBYTE *p1, *p2;
  int s1, s2;
  int err;
  int i;

  if (issound != SOUND_DX)
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
	Sound_Continue();
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
  WAVEFORMATEX wf;
  MMRESULT err;

  event = CreateEvent(NULL, TRUE, FALSE, NULL);

  memset(&wf, 0, sizeof(wf));

  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = stereo ? 2 : 1;
  wf.nSamplesPerSec = dsprate;
  wf.nAvgBytesPerSec = dsprate * wf.nChannels;
  wf.nBlockAlign = 2;
  wf.wBitsPerSample = 8;
  wf.cbSize = 0;

  err = waveOutOpen(&wout, WAVE_MAPPER, &wf, (int)event, 0, CALLBACK_EVENT);
  if (err == WAVERR_BADFORMAT)
    {
      Aprint("wave output paremeters unsupported\n");
      exit(1);
    }
  if (err != MMSYSERR_NOERROR)
    {
      Aprint("cannot open wave output (%x)\n", err);
      exit(1);
    }

  buffers = ((wf.nAvgBytesPerSec * snddelay / 1000) >> 8) + 1;
  waves = malloc(buffers * sizeof(*waves));
  for (i = 0; i < buffers; i++)
    {
      memset(&waves[i], 0, sizeof (waves[i]));
      if (!(waves[i].lpData = (uint8 *)malloc(WAVSIZE)))
	{
	  Aprint("could not get wave buffer memory\n");
	  exit(1);
	}
      waves[i].dwBufferLength = WAVSIZE;
      err = waveOutPrepareHeader(wout, &waves[i], sizeof(waves[i]));
      if (err != MMSYSERR_NOERROR)
	{
	  Aprint("cannot prepare wave header (%x)\n", err);
	  exit(1);
	}
      waves[i].dwFlags |= WHDR_DONE;
    }

  Pokey_sound_init(FREQ_17_EXACT, dsprate, 1);

  issound = SOUND_WAV;
  return 0;
}

void Sound_Initialise(int *argc, char *argv[])
{
  int i, j;
  int usesound = TRUE;

  if (issound != SOUND_NONE)
    return;

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
#ifdef DIRECTX
      else if (strcmp(argv[i], "-wavonly") == 0)
	wavonly = TRUE;
#endif
      else
      {
	if (strcmp(argv[i], "-help") == 0)
	{
	  Aprint("\t-sound			enable sound\n"
		 "\t-nosound			disable sound\n"
#ifdef DIRECTX
		 "\t-wavonly			disable direct sound\n"
#endif
		 "\t-dsprate <rate>		set dsp rate\n"
		 "\t-snddelay <milliseconds>	set sound delay\n"
		);
	}
	argv[j++] = argv[i];
      }
    }
  *argc = j;

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
      Pokey_process(wh->lpData, wh->dwBufferLength);
      err = waveOutWrite(wout, wh, sizeof(*wh));
      if (err != MMSYSERR_NOERROR)
      {
	Aprint("cannot write wave output (%x)\n", err);
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


/*
$Log$
Revision 1.4  2001/04/17 05:32:33  knik
sound_continue moved outside dx conditional

Revision 1.3  2001/04/08 05:50:16  knik
standard wave output driver added; sound and directx conditional compile

Revision 1.2  2001/03/24 10:13:43  knik
-nosound option fixed

Revision 1.1  2001/03/18 07:56:48  knik
win32 port

*/
