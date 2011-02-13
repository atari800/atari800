/*
 * jni.c - native functions exported to java
 *
 * Copyright (C) 2010 Kostas Nakos
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
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

#include <stddef.h>
#include <pthread.h>
#include <jni.h>

#include "log.h"
#include "atari.h"
#include "input.h"
#include "afile.h"
#include "screen.h"
#include "cpu.h"
#include "antic.h"
#include "../../memory.h"	/* override system header */

#include "platform.h"
#include "graphics.h"
#include "androidinput.h"

//#define USE_SOUND_MUTEX
#undef USE_SOUND_MUTEX

/* exported functions/parameters */
int *ovl_texpix;
int ovl_texw;
int ovl_texh;

#ifdef USE_SOUND_MUTEX
static pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static jbyteArray sndarray;
static UBYTE *sndbuf = NULL;

extern void SoundThread_Update(void *buf, int offs, int len);
extern void Android_SoundInit(int rate, int bit16, int hq);

static void JNICALL NativeGetOverlays(JNIEnv *env, jobject this)
{
	jclass cls;
	jfieldID fid;
	jintArray arr;
	jboolean cp;

	cls = (*env)->GetObjectClass(env, this);

	fid = (*env)->GetFieldID(env, cls, "OVL_TEXW", "I");
	ovl_texw = (*env)->GetIntField(env, this, fid);

	fid = (*env)->GetFieldID(env, cls, "OVL_TEXH", "I");
	ovl_texh = (*env)->GetIntField(env, this, fid);

	ovl_texpix = malloc(ovl_texw * ovl_texh * sizeof(int));
	if (ovl_texpix == NULL) Log_print("Cannot allocate memory for overlays");

	fid = (*env)->GetFieldID(env, cls, "_pix", "[I");
	arr = (*env)->GetObjectField(env, this, fid);
	(*env)->GetIntArrayRegion(env, arr, 0, ovl_texw * ovl_texh, ovl_texpix);

	Log_print("Overlays texture initialized: %dx%d", ovl_texw, ovl_texh);
}

static void JNICALL NativeResize(JNIEnv *env, jobject this, jint w, jint h)
{
	Log_print("Screen resize: %dx%d", w, h);
	Android_ScreenW = w;
	Android_ScreenH = h;
	Android_SplitCalc();
	Android_InitGraphics();
}

static jstring JNICALL NativeInit(JNIEnv *env, jobject this)
{
	int ac = 1;
	char av = '\0';
	char *avp = &av;

	Atari800_Initialise(&ac, &avp);

	return (*env)->NewStringUTF(env, Atari800_TITLE);
}

static void JNICALL NativeRunAtariProgram(JNIEnv *env, jobject this, jstring img, jint reboot)
{
	const jbyte *img_utf = NULL;

	img_utf = (*env)->GetStringUTFChars(env, img, NULL);
	if (!AFILE_OpenFile(img_utf, reboot, 1, FALSE))
		Log_print("Cannot start image: %s", img_utf);
	else
		CPU_cim_encountered = FALSE;
	(*env)->ReleaseStringUTFChars(env, img, img_utf);
}

static jboolean JNICALL NativeIsDisk(JNIEnv *env, jobject this, jstring img)
{
	const jbyte *img_utf = NULL;
	int type;

	img_utf = (*env)->GetStringUTFChars(env, img, NULL);
	type = AFILE_DetectFileType(img_utf);
	(*env)->ReleaseStringUTFChars(env, img, img_utf);
	switch (type) {
	case AFILE_ATR:
	case AFILE_ATX:
	case AFILE_XFD:
	case AFILE_ATR_GZ:
	case AFILE_XFD_GZ:
	case AFILE_DCM:
	case AFILE_PRO:
		return JNI_TRUE;
	default:
		return JNI_FALSE;
	}
}

static void JNICALL NativeExit(JNIEnv *env, jobject this)
{
	Atari800_Exit(FALSE);
}

static jboolean JNICALL NativeRunFrame(JNIEnv *env, jobject this)
{
	static int old_cim = FALSE;
	int ret = FALSE;

	do {
		INPUT_key_code = PLATFORM_Keyboard();

		/* guard emulation */
#ifdef USE_SOUND_MUTEX
		pthread_mutex_lock(&sound_mutex);
#endif
		if (!CPU_cim_encountered)
			Atari800_Frame();
		else
			Atari800_display_screen = TRUE;
#ifdef USE_SOUND_MUTEX
		pthread_mutex_unlock(&sound_mutex);
#endif

		if (Atari800_display_screen || CPU_cim_encountered)
			PLATFORM_DisplayScreen();

		if (!old_cim && CPU_cim_encountered)
			ret = TRUE;

		old_cim = CPU_cim_encountered;
	} while (!Atari800_display_screen);

	return ret;
}

static void JNICALL NativeSoundInit(JNIEnv *env, jobject this, jint size)
{
	jclass cls;
	jfieldID fid;
	jintArray arr;

	cls = (*env)->GetObjectClass(env, this);
	fid = (*env)->GetFieldID(env, cls, "_buffer", "[B");
	arr = (*env)->GetObjectField(env, this, fid);
	sndarray = (*env)->NewGlobalRef(env, arr);

	sndbuf = malloc(size);
	if (sndbuf == NULL) Log_print("Cannot allocate memory for sound buffer");
}

static void JNICALL NativeSoundUpdate(JNIEnv *env, jobject this, jint offset, jint length)
{
	/* guard sound generation */
#ifdef USE_SOUND_MUTEX
	pthread_mutex_lock(&sound_mutex);
#endif
	SoundThread_Update(sndbuf, offset, length);
#ifdef USE_SOUND_MUTEX
	pthread_mutex_unlock(&sound_mutex);
#endif
	(*env)->SetByteArrayRegion(env, sndarray, offset, length, sndbuf + offset);
}

static void JNICALL NativeSoundExit(JNIEnv *env, jobject this)
{
	(*env)->DeleteGlobalRef(env, sndarray);
	if (sndbuf) {
		free(sndbuf);
		sndbuf = NULL;
	}
}

static void JNICALL NativeKey(JNIEnv *env, jobject this, int k, int s)
{
	Android_KeyEvent(k, s);
}

static void JNICALL NativeTouch(JNIEnv *env, jobject this, int x1, int y1, int s1,
														   int x2, int y2, int s2)
{
	Android_TouchEvent(x1, y1, s1, x2, y2, s2);
}


static void JNICALL NativePrefGfx(JNIEnv *env, jobject this, int aspect, jboolean bilinear,
									int artifact, int frameskip, jboolean collisions)
{
	Android_Aspect = aspect;
	Android_Bilinear = bilinear;
	ANTIC_artif_mode = artifact;
	ANTIC_UpdateArtifacting();
	Atari800_refresh_rate = frameskip + 1;
	Atari800_collisions_in_skipped_frames = collisions;
}

static jboolean JNICALL NativePrefMachine(JNIEnv *env, jobject this, int nummac)
{
	struct tSysConfig {
		int type;
		int ram;
	};
	static const struct tSysConfig machine[] = {
		{ Atari800_MACHINE_OSA, 16 },
		{ Atari800_MACHINE_OSA, 48 },
		{ Atari800_MACHINE_OSA, 52 },
		{ Atari800_MACHINE_OSB, 16 },
		{ Atari800_MACHINE_OSB, 48 },
		{ Atari800_MACHINE_OSB, 52 },
		{ Atari800_MACHINE_XLXE, 16 },
		{ Atari800_MACHINE_XLXE, 64 },
		{ Atari800_MACHINE_XLXE, 128 },
		{ Atari800_MACHINE_XLXE, 192 },
		{ Atari800_MACHINE_XLXE, MEMORY_RAM_320_RAMBO },
		{ Atari800_MACHINE_XLXE, MEMORY_RAM_320_COMPY_SHOP },
		{ Atari800_MACHINE_XLXE, 576 },
		{ Atari800_MACHINE_XLXE, 1088 },
		{ Atari800_MACHINE_5200, 16 }
	};

	Atari800_machine_type = machine[nummac].type;
	MEMORY_ram_size = machine[nummac].ram;
	return Atari800_InitialiseMachine();
}

static void JNICALL NativePrefEmulation(JNIEnv *env, jobject this, jboolean basic, jboolean speed,
										jboolean disk, jboolean sector)
{
	Atari800_disable_basic = basic;
	Screen_show_atari_speed = speed;
	Screen_show_disk_led = disk;
	Screen_show_sector_counter = sector;
}

static void JNICALL NativePrefSoftjoy(JNIEnv *env, jobject this, jboolean softjoy, int up, int down,
										int left, int right, int fire)
{
	Android_SoftjoyEnable = softjoy;
	softjoymap[SOFTJOY_UP][0] = up;
	softjoymap[SOFTJOY_DOWN][0] = down;
	softjoymap[SOFTJOY_LEFT][0] = left;
	softjoymap[SOFTJOY_RIGHT][0] = right;
	softjoymap[SOFTJOY_FIRE][0] = fire;
}

static void JNICALL NativePrefOvl(JNIEnv *env, jobject this, jboolean visible, int size, int opacity,
					jboolean righth, int deadband, jboolean midx, int anchor, int anchorx, int anchory)
{
	AndroidInput_JoyOvl.ovl_visible = visible;
	AndroidInput_JoyOvl.areaopacityset = 0.01f * opacity;
	Android_Joyscale = 0.01f * size;
	Android_Joyleft = !righth;
	Android_Splitpct = 0.01f * midx;
	AndroidInput_JoyOvl.deadarea = 0.01f * deadband;
	AndroidInput_JoyOvl.anchor = anchor;
	if (anchor) {
		AndroidInput_JoyOvl.joyarea.l = anchorx;
		AndroidInput_JoyOvl.joyarea.t = anchory;
	}

	Android_SplitCalc();
	Joyovl_Scale();
	Joy_Reposition();
}

static void JNICALL NativePrefSound(JNIEnv *env, jobject this, int mixrate, jboolean sound16bit,
									jboolean hqpokey)
{
	Android_SoundInit(mixrate, sound16bit, hqpokey);
}

static jboolean JNICALL NativeSetROMPath(JNIEnv *env, jobject this, jstring path)
{
	const jbyte *utf = NULL;
	jboolean ret = JNI_FALSE;

	utf = (*env)->GetStringUTFChars(env, path, NULL);
	ret |= chdir(utf);
	ret |= Atari800_InitialiseMachine();
	(*env)->ReleaseStringUTFChars(env, path, utf);

	return ret;
}

static jstring JNICALL NativeGetJoypos(JNIEnv *env, jobject this)
{
	char tmp[16];
	sprintf(tmp, "%d %d", AndroidInput_JoyOvl.joyarea.l, AndroidInput_JoyOvl.joyarea.t);
	return (*env)->NewStringUTF(env, tmp);
}

jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
	JNINativeMethod main_methods[] = {
		{ "NativeExit",				"()V",						NativeExit			  },
		{ "NativeRunAtariProgram",	"(Ljava/lang/String;I)V",	NativeRunAtariProgram },
		{ "NativePrefGfx",			"(IZIIZ)V",					NativePrefGfx		  },
		{ "NativePrefMachine",		"(I)Z",						NativePrefMachine	  },
		{ "NativePrefEmulation",	"(ZZZZ)V",					NativePrefEmulation	  },
		{ "NativePrefSoftjoy",		"(ZIIIII)V",				NativePrefSoftjoy	  },
		{ "NativePrefOvl",			"(ZIIZIIZII)V",				NativePrefOvl		  },
		{ "NativePrefSound",		"(IZZ)V",					NativePrefSound		  },
		{ "NativeSetROMPath",		"(Ljava/lang/String;)Z",	NativeSetROMPath	  },
		{ "NativeGetJoypos",		"()Ljava/lang/String;",		NativeGetJoypos		  },
		{ "NativeInit",				"()Ljava/lang/String;",		NativeInit			  },
	};
	JNINativeMethod view_methods[] = {
		{ "NativeTouch", 			"(IIIIII)V", 				NativeTouch			  },
		{ "NativeKey",				"(II)V",					NativeKey			  },
	};
	JNINativeMethod snd_methods[] = {
		{ "NativeSoundInit",		"(I)V",						NativeSoundInit		  },
		{ "NativeSoundUpdate",		"(II)V",					NativeSoundUpdate	  },
		{ "NativeSoundExit",		"()V",						NativeSoundExit		  },
	};
	JNINativeMethod render_methods[] = {
		{ "NativeRunFrame",			"()Z",						NativeRunFrame		  },
		{ "NativeGetOverlays",		"()V",						NativeGetOverlays	  },
		{ "NativeResize",			"(II)V",					NativeResize		  },
	};
	JNINativeMethod fsel_methods[] = {
		{ "NativeIsDisk",			"(Ljava/lang/String;)Z",	NativeIsDisk		  },
	};
	JNIEnv *env;
	jclass cls;

	if ((*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_2))
		return JNI_ERR;

	cls = (*env)->FindClass(env, "name/nick/jubanka/atari800/MainActivity");
	(*env)->RegisterNatives(env, cls, main_methods, sizeof(main_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/atari800/A800view");
	(*env)->RegisterNatives(env, cls, view_methods, sizeof(view_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/atari800/AudioThread");
	(*env)->RegisterNatives(env, cls, snd_methods, sizeof(snd_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/atari800/A800Renderer");
	(*env)->RegisterNatives(env, cls, render_methods, sizeof(render_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/atari800/FileSelector");
	(*env)->RegisterNatives(env, cls, fsel_methods, sizeof(fsel_methods)/sizeof(JNINativeMethod));

	return JNI_VERSION_1_2;
}
