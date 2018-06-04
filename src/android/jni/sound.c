/*
 * sound.c - android sound
 *
 * Copyright (C) 2014 Kostas Nakos
 * Copyright (C) 2014 Atari800 development team (see DOC/CREDITS)
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "pokeysnd.h"
#include "log.h"

static int at_sixteenbit = 0;
static int snd_bufsizems = 0;
static int snd_mixrate   = 0;

#define OSL_BUFSIZE_MS 10
static void *osl_handle;
int Android_osl_sound;
static int osl_disable = 0;
static int osl_bufnum = 0;
static int osl_bufszbytes = 0;
static UBYTE *osl_soundbuf = NULL;
static UBYTE **osl_soundbufptr = NULL;
static ULONG osl_lastplayedindex = 0;
static volatile ULONG osl_lastindex = 0;
static int osl_nextbufindex = 0;

static SLObjectItf  osl_engine = NULL,
					osl_mixer = NULL,
					osl_player = NULL;
static SLEngineItf  osl_engineif = NULL;
static SLPlayItf 	osl_playif = NULL;
static SLAndroidSimpleBufferQueueItf osl_bufqif = NULL;

static SLresult SLAPIENTRY (*osl_slCreateEngine) (
	SLObjectItf             *pEngine,
	SLuint32                numOptions,
	const SLEngineOption    *pEngineOptions,
	SLuint32                numInterfaces,
	const SLInterfaceID     *pInterfaceIds,
	const SLboolean         * pInterfaceRequired
) = NULL; 
static SLAPIENTRY const SLInterfaceID *osl_SL_IID_VOLUME;
static SLAPIENTRY const SLInterfaceID *osl_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
static SLAPIENTRY const SLInterfaceID *osl_SL_IID_ENGINE;
static SLAPIENTRY const SLInterfaceID *osl_SL_IID_PLAY;

#define CHECK_OSL(command, errmsg)								\
	if ( (res = command) != SL_RESULT_SUCCESS ) {				\
		Log_print("ERROR: OSL: Cannot " errmsg " (%08X)", res);	\
		return FALSE;											\
	} else														\
		(void) 0
#define GET_SYMBOL(var, sname)										\
	var = dlsym(osl_handle, sname);									\
	errstr = dlerror();												\
	if (errstr) {													\
		Log_print("ERROR: Cannot resolve " sname ": %s", errstr);	\
		return FALSE;												\
	} else															\
		(void) 0

void Sound_Continue(void);

/* Legacy AudioThread functions */

void Android_SoundInit(int rate, int bufsizems, int bit16, int hq, int disableOSL)
{
	Log_print("SoundInit for android initializing with %dHz, %d bufsize, OSL %s",
				rate, bufsizems, (disableOSL) ? "off" : "on");
	POKEYSND_bienias_fix = 0;
	POKEYSND_enable_new_pokey = hq;
	at_sixteenbit = bit16;
	snd_bufsizems = bufsizems;
	snd_mixrate = rate;
	osl_bufnum = snd_bufsizems / OSL_BUFSIZE_MS;
	osl_bufszbytes = OSL_BUFSIZE_MS * (at_sixteenbit ? 2 : 1) * snd_mixrate / 1000;
	osl_disable = disableOSL;
	if (disableOSL)
		Android_osl_sound = FALSE;
	Log_print("Initializing POKEY");
	POKEYSND_Init(POKEYSND_FREQ_17_EXACT, rate, 1, bit16 ? POKEYSND_BIT16 : 0);
	Log_print("POKEY init done");
}

void SoundThread_Update(void *buf, int offs, int len)
{
	POKEYSND_Process(buf + offs, len >> at_sixteenbit);
}


/* Native sound helpers */

static int OSL_grab_functions(void)
{
	const char *errstr;

	dlerror();

	GET_SYMBOL(osl_slCreateEngine, "slCreateEngine");
	GET_SYMBOL(osl_SL_IID_VOLUME, "SL_IID_VOLUME");
	GET_SYMBOL(osl_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE");
	GET_SYMBOL(osl_SL_IID_ENGINE, "SL_IID_ENGINE");
	GET_SYMBOL(osl_SL_IID_PLAY, "SL_IID_PLAY");

	return TRUE;
}

static int OSL_load(void)
{
	osl_handle = dlopen("libOpenSLES.so", RTLD_LAZY);
	if (! osl_handle) {
		Log_print("Cannot dlopen OSL");
		return FALSE;
	}
	if ( OSL_grab_functions() ) {
			Log_print("Open SL ES found and bound");
			return TRUE;
		}
	return FALSE;
}

void OSL_process(SLAndroidSimpleBufferQueueItf bufqif, void *ctx)
{
	SLAndroidSimpleBufferQueueState st;

	if ( (*bufqif)->GetState(bufqif, &st) != SL_RESULT_SUCCESS ) {
		Log_print("ERROR: Cannot get queue state");
		return;
	}
	osl_lastindex = st.index;
}

static int OSL_init(void)
{
	SLresult res;

	const SLInterfaceID mixids[] = { *osl_SL_IID_VOLUME };
	const SLboolean mixreq[] = { SL_BOOLEAN_FALSE };

	SLDataLocator_AndroidSimpleBufferQueue dlsbq = {
		.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		.numBuffers  = osl_bufnum
	};
	SLDataFormat_PCM dfpcm = {
		.formatType		= SL_DATAFORMAT_PCM,
		.numChannels	= 1,
		.samplesPerSec	= (snd_mixrate == 44100) ? SL_SAMPLINGRATE_44_1  :
						  (snd_mixrate == 22050) ? SL_SAMPLINGRATE_22_05 :
						  (snd_mixrate == 11025) ? SL_SAMPLINGRATE_11_025:
						  0,
		.bitsPerSample	= (at_sixteenbit) ? SL_PCMSAMPLEFORMAT_FIXED_16 : SL_PCMSAMPLEFORMAT_FIXED_8,
		.containerSize	= (at_sixteenbit) ? SL_PCMSAMPLEFORMAT_FIXED_16 : SL_PCMSAMPLEFORMAT_FIXED_8,
		.channelMask	= SL_SPEAKER_FRONT_CENTER,
		.endianness		= SL_BYTEORDER_LITTLEENDIAN
	};
	SLDataSource dsrc = {
		.pLocator = &dlsbq,
		.pFormat  = &dfpcm
	};
	SLDataLocator_OutputMix dlmix = {
		.locatorType = SL_DATALOCATOR_OUTPUTMIX,
		.outputMix	 = NULL
	};
	SLDataSink dsnk = {
		.pLocator = &dlmix,
		.pFormat  = NULL
	};
	const SLInterfaceID apids[] = { *osl_SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
	const SLboolean apreq[] = { SL_BOOLEAN_TRUE };

	if (dfpcm.samplesPerSec == 0) {
		Log_print("ERROR: Incorrect mixrate %d", snd_mixrate);
		return FALSE;
	}
	/* engine */
	CHECK_OSL( osl_slCreateEngine(&osl_engine, 0, NULL, 0, NULL, NULL), "create engine");
	CHECK_OSL( (*osl_engine)->Realize(osl_engine, SL_BOOLEAN_FALSE), "realize engine");
	CHECK_OSL( (*osl_engine)->GetInterface(osl_engine, *osl_SL_IID_ENGINE, &osl_engineif), "get engine i/f");

	/* mixer */
	CHECK_OSL( (*osl_engineif)->CreateOutputMix(osl_engineif, &osl_mixer, 1, mixids, mixreq), "create mixer");
	CHECK_OSL( (*osl_mixer)->Realize(osl_mixer, SL_BOOLEAN_FALSE), "realize mixer");
	dlmix.outputMix = osl_mixer;

	/* player */
	CHECK_OSL( (*osl_engineif)->CreateAudioPlayer(osl_engineif, &osl_player, &dsrc, &dsnk, 1, apids, apreq), "create player");
	CHECK_OSL( (*osl_player)->Realize(osl_player, SL_BOOLEAN_FALSE), "realize player");
	CHECK_OSL( (*osl_player)->GetInterface(osl_player, *osl_SL_IID_PLAY, &osl_playif), "get player i/f");
	CHECK_OSL( (*osl_player)->GetInterface(osl_player, *osl_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &osl_bufqif), "get buffer queue i/f");

	/* register callback */
	CHECK_OSL( (*osl_bufqif)->RegisterCallback(osl_bufqif, OSL_process, NULL), "register callback");

	Log_print("OpenSL ES initialized successfully");
	return TRUE;
}

static void OSL_stop_playback(void)
{
	SLuint32 playstate;

	(*osl_playif)->SetPlayState(osl_playif, SL_PLAYSTATE_STOPPED);
	Log_print("Waiting for OSL player to finish");
	do {
		usleep(50000);
		(*osl_playif)->GetPlayState(osl_playif, &playstate);
	} while (playstate != SL_PLAYSTATE_STOPPED);
	Log_print("Playback finished");
}

static void OSL_teardown(void)
{
	if (osl_playif) {
		OSL_stop_playback();
		(*osl_player)->Destroy(osl_player);
		osl_player = NULL;
		osl_playif = NULL;
		osl_bufqif = NULL;
	}

	if (osl_mixer) {
		(*osl_mixer)->Destroy(osl_mixer);
		osl_mixer = NULL;
	}

	if (osl_engine) {
		(*osl_engine)->Destroy(osl_engine);
		osl_engine = NULL;
		osl_engineif = NULL;
	}
	Log_print("OpenSL teardown complete");
}

static void OSL_buf_free(void)
{
	if (osl_soundbufptr) {
		free(osl_soundbufptr);
		osl_soundbufptr = NULL;
	}
	if (osl_soundbuf) {
		free(osl_soundbuf);
		osl_soundbuf = NULL;
	}
}

static int OSL_buf_alloc(void)
{
	UBYTE *ptr;
	int i;

	if (osl_soundbuf || osl_soundbufptr) {
		Log_print("WARNING: Sound buffers already allocated. Freeing.");
		OSL_buf_free();
	}
	if (! ( osl_soundbufptr = (UBYTE **) malloc(osl_bufnum * sizeof(void *)) ) ) {
		Log_print("ERROR: Cannot allocate sound buffer pointers (%d)", osl_bufnum);
		return FALSE;
	}
	if (! ( osl_soundbuf = (UBYTE *) malloc(osl_bufnum * osl_bufszbytes) ) ) {
		Log_print("ERROR: Cannot allocate sound buffer for %d buffers of %d bytes each", osl_bufnum, osl_bufszbytes);
		return FALSE;
	}
	memset(osl_soundbuf, 0, osl_bufnum * osl_bufszbytes);
	for (ptr = osl_soundbuf, i = 0; i < osl_bufnum; i++, ptr += osl_bufszbytes)
		osl_soundbufptr[i] = ptr;

	Log_print("Allocated OK sound buffer for %d buffers %dms each, %d bits at %dHz",
			  osl_bufnum, OSL_BUFSIZE_MS, (at_sixteenbit) ? 16 : 8, snd_mixrate);
	return TRUE;
}

static int OSL_start_playback(void)
{
	SLresult res;
	int i;

	Sound_Continue();

	for (i = 0; i < osl_bufnum; i++) {
		if (! osl_bufqif)	return FALSE;
		CHECK_OSL( (*osl_bufqif)->Enqueue(osl_bufqif, osl_soundbufptr[i], osl_bufszbytes), "enqueue init buffer");
	}

	osl_nextbufindex = 0;
	osl_lastindex = osl_lastplayedindex = 0;

	Log_print("Buffer queue bootstrap OK");

	return TRUE;
}

/* Platform interface. Used only with SL sound */

void Sound_Exit(void)
{
	Log_print("Sound exit");

	OSL_teardown();
	OSL_buf_free();
	if (osl_handle) {
		dlclose(osl_handle);
		osl_handle = NULL;
	}
	Android_osl_sound = FALSE;
}

int Sound_Initialise(int *argc, char *argv[])
{
	Android_osl_sound = TRUE;

	if (
			  osl_disable ||
			! OSL_load() ||
			! OSL_init() ||
			! OSL_buf_alloc() ||
			! OSL_start_playback()
		)
	{
			Android_osl_sound = FALSE;
			Sound_Exit();
			Log_print("Using legacy AudioThread");
	} else
		Log_print("Using OpenSL ES sound");

	return 1;
}

void Sound_Update(void)
{
	SLresult res;

	while (osl_lastplayedindex < osl_lastindex) {
		if (! osl_soundbufptr)	return;
		POKEYSND_Process(osl_soundbufptr[osl_nextbufindex], osl_bufszbytes >> at_sixteenbit);
		if (! osl_bufqif)		return;
		res = (*osl_bufqif)->Enqueue(osl_bufqif, osl_soundbufptr[osl_nextbufindex], osl_bufszbytes);
		if (res != SL_RESULT_SUCCESS) {
			Log_print("Cannot enqueue buffer #%d (%08X)", osl_nextbufindex, res);
			return;
		}
		osl_nextbufindex = (osl_nextbufindex + 1) % osl_bufnum;
		osl_lastplayedindex++;
	}
}

void Sound_Pause(void)
{
	Log_print("OSL pausing sound");
	if (! osl_playif)	return;
	(*osl_playif)->SetPlayState(osl_playif, SL_PLAYSTATE_PAUSED);
}

void Sound_Continue(void)
{
	Log_print("OSL resuming sound");
	if (! osl_playif)	return;
	(*osl_playif)->SetPlayState(osl_playif, SL_PLAYSTATE_PLAYING);
}
