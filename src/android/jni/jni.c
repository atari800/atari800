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
#include <string.h>

#include "log.h"
#include "atari.h"
#include "input.h"
#include "afile.h"
#include "screen.h"
#include "cpu.h"
#include "antic.h"
#include "../../memory.h"	/* override system header */
#include "sio.h"
#include "sysrom.h"
#include "akey.h"
#include "devices.h"

#include "graphics.h"
#include "androidinput.h"

/* exports/imports */
int *ovl_texpix;
int ovl_texw;
int ovl_texh;
extern void SoundThread_Update(void *buf, int offs, int len);
extern void Android_SoundInit(int rate, int bit16, int hq);

struct audiothread {
	UBYTE *sndbuf;
	jbyteArray sndarray;
};
static pthread_key_t audiothread_data;

static char devb_url[512];

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

static void JNICALL NativeClearDevB(JNIEnv *env, jobject this)
{
	dev_b_status.ready = FALSE;
	memset(devb_url, 0, sizeof(devb_url));
}

static jstring JNICALL NativeInit(JNIEnv *env, jobject this)
{
	int ac = 1;
	char av = '\0';
	char *avp = &av;

	pthread_key_create(&audiothread_data, NULL);
	pthread_setspecific(audiothread_data, NULL);

	NativeClearDevB(env, this);

	Atari800_Initialise(&ac, &avp);

	return (*env)->NewStringUTF(env, Atari800_TITLE);
}

static jobjectArray JNICALL NativeGetDrvFnames(JNIEnv *env, jobject this)
{
	jobjectArray arr;
	int i;
	char tmp[FILENAME_MAX + 3], fname[FILENAME_MAX];
	jstring str;

	arr = (*env)->NewObjectArray(env, 4, (*env)->FindClass(env, "java/lang/String"), NULL);
	for (i = 0; i < 4; i++) {
		Util_splitpath(SIO_filename[i], NULL, fname);
		sprintf(tmp, "D%d:%s", i + 1, fname);
		str = (*env)->NewStringUTF(env, tmp);
		(*env)->SetObjectArrayElement(env, arr, i, str);
		(*env)->DeleteLocalRef(env, str);
	}

	return arr;
}

static void JNICALL NativeUnmountAll(JNIEnv *env, jobject this)
{
	int i;

	for (i = 1; i <= 4; i++)
		SIO_DisableDrive(i);
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

static jboolean JNICALL NativeSaveState(JNIEnv *env, jobject this, jstring fname)
{
	const jbyte *fname_utf = NULL;
	int ret;

	fname_utf = (*env)->GetStringUTFChars(env, fname, NULL);
	ret = StateSav_SaveAtariState(fname_utf, "wb", TRUE);
	Log_print("Saved state %s with return %d", fname_utf, ret);
	(*env)->ReleaseStringUTFChars(env, fname, fname_utf);
	return ret;
}

static void JNICALL NativeRunAtariProgram(JNIEnv *env, jobject this, jstring img, jint drv,
										  jint reboot)
{
	const jbyte *img_utf = NULL;

	if (reboot) {
		NativeUnmountAll(env, this);
		CARTRIDGE_Remove();
	}
	img_utf = (*env)->GetStringUTFChars(env, img, NULL);
	if (!AFILE_OpenFile(img_utf, reboot, drv, FALSE))
		Log_print("Cannot start image: %s", img_utf);
	else
		CPU_cim_encountered = FALSE;

	(*env)->ReleaseStringUTFChars(env, img, img_utf);
}

static void JNICALL NativeExit(JNIEnv *env, jobject this)
{
	Atari800_Exit(FALSE);
}

static jint JNICALL NativeRunFrame(JNIEnv *env, jobject this)
{
	static int old_cim = FALSE;
	int ret = 0;

	do {
		INPUT_key_code = PLATFORM_Keyboard();

		if (!CPU_cim_encountered)
			Atari800_Frame();
		else
			Atari800_display_screen = TRUE;

		if (Atari800_display_screen || CPU_cim_encountered)
			PLATFORM_DisplayScreen();

		if (!old_cim && CPU_cim_encountered)
			ret = 1;

		old_cim = CPU_cim_encountered;
	} while (!Atari800_display_screen);

	if (dev_b_status.ready && devb_url[0] == '\0')
		if (strlen(dev_b_status.url)) {
			strncpy(devb_url, dev_b_status.url, sizeof(devb_url));
			Log_print("Received b: device URL: %s", devb_url);
			ret |= 2;
		} else
			Log_print("Device b: signalled with zero-length url");

	return ret;
}

static void JNICALL NativeSoundInit(JNIEnv *env, jobject this, jint size)
{
	jclass cls;
	jfieldID fid;
	jintArray arr;
	struct audiothread *at;

	Log_print("Audio init with buffer size %d", size);

	if (pthread_getspecific(audiothread_data))
		Log_print("Audiothread data already allocated for current thread");
	at = (struct audiothread *) malloc(sizeof(struct audiothread));

	cls = (*env)->GetObjectClass(env, this);
	fid = (*env)->GetFieldID(env, cls, "_buffer", "[B");
	arr = (*env)->GetObjectField(env, this, fid);
	at->sndarray = (*env)->NewGlobalRef(env, arr);

	at->sndbuf = malloc(size);
	if (!at->sndbuf)	Log_print("Cannot allocate memory for sound buffer");

	pthread_setspecific(audiothread_data, at);
}

static void JNICALL NativeSoundUpdate(JNIEnv *env, jobject this, jint offset, jint length)
{
	struct audiothread *at;

	if ( !(at = (struct audiothread *) pthread_getspecific(audiothread_data)) )
		return;
	SoundThread_Update(at->sndbuf, offset, length);
	(*env)->SetByteArrayRegion(env, at->sndarray, offset, length, at->sndbuf + offset);
}

static void JNICALL NativeSoundExit(JNIEnv *env, jobject this)
{
	struct audiothread *at;

	Log_print("Audio exit");

	if ( !(at = (struct audiothread *) pthread_getspecific(audiothread_data)) )
		return;

	(*env)->DeleteGlobalRef(env, at->sndarray);
	if (at->sndbuf)
		free(at->sndbuf);

	free(at);
	pthread_setspecific(audiothread_data, NULL);
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
								  int artifact, int frameskip, jboolean collisions, int crophoriz,
								  int cropvert)
{
	Android_Aspect = aspect;
	Android_Bilinear = bilinear;
	ANTIC_artif_mode = artifact;
	ANTIC_UpdateArtifacting();
	if (frameskip == 0) {
		Atari800_refresh_rate = 1;
		Atari800_auto_frameskip = TRUE;
	} else {
		Atari800_auto_frameskip = FALSE;
		Atari800_refresh_rate = frameskip;
	}
	Atari800_collisions_in_skipped_frames = collisions;
	Android_CropScreen[0] = (SCANLINE_LEN - crophoriz) / 2;
	Android_CropScreen[2] = crophoriz;
	Android_CropScreen[1] = SCREEN_HEIGHT - (SCREEN_HEIGHT - cropvert) / 2;
	Android_CropScreen[3] = -cropvert;
	Screen_visible_x1 = SCANLINE_START + Android_CropScreen[0];
	Screen_visible_x2 = Screen_visible_x1 + crophoriz;
	Screen_visible_y1 = SCREEN_HEIGHT - Android_CropScreen[1];
	Screen_visible_y2 = Screen_visible_y1 + cropvert;
}

static jboolean JNICALL NativePrefMachine(JNIEnv *env, jobject this, int nummac, jboolean ntsc)
{
	struct tSysConfig {
		int type;
		int ram;
	};
	static const struct tSysConfig machine[] = {
		{ Atari800_MACHINE_800, 16 },
		{ Atari800_MACHINE_800, 48 },
		{ Atari800_MACHINE_800, 52 },
		{ Atari800_MACHINE_800, 16 },
		{ Atari800_MACHINE_800, 48 },
		{ Atari800_MACHINE_800, 52 },
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

	Atari800_SetMachineType(machine[nummac].type);
	MEMORY_ram_size = machine[nummac].ram;
	/* Temporary hack to allow choosing OS rev. A/B and XL/XE features.
	   Delete after adding proper support for choosing system settings. */
	if (nummac < 3)
		SYSROM_os_versions[Atari800_MACHINE_800] = ntsc ? SYSROM_A_NTSC : SYSROM_A_PAL;
	else if (nummac >= 3 && nummac < 6)
		SYSROM_os_versions[Atari800_MACHINE_800] = ntsc ? SYSROM_AUTO : SYSROM_B_NTSC;
	else if (Atari800_machine_type == Atari800_MACHINE_XLXE) {
		Atari800_builtin_basic = TRUE;
		Atari800_keyboard_leds = FALSE;
		Atari800_f_keys = FALSE;
		Atari800_jumper = FALSE;
		Atari800_builtin_game = FALSE;
		Atari800_keyboard_detached = FALSE;
	}
	/* End of hack */

	Atari800_SetTVMode(ntsc ? Atari800_TV_NTSC : Atari800_TV_PAL);
	CPU_cim_encountered = FALSE;
	return Atari800_InitialiseMachine();
}

static void JNICALL NativePrefEmulation(JNIEnv *env, jobject this, jboolean basic, jboolean speed,
										jboolean disk, jboolean sector, jboolean browser)
{
	Atari800_disable_basic = basic;
	Screen_show_atari_speed = speed;
	Screen_show_disk_led = disk;
	Screen_show_sector_counter = sector;
	Devices_enable_b_patch = browser;
	Devices_UpdatePatches();
}

static void JNICALL NativePrefSoftjoy(JNIEnv *env, jobject this, jboolean softjoy, int up, int down,
									  int left, int right, int fire, int derotkeys, jobjectArray actions)
{
	int i;
	jobject obj;
	const char *str;
	char *sep;
	UBYTE act, akey;

	Android_SoftjoyEnable = softjoy;
	softjoymap[SOFTJOY_UP][0] = up;
	softjoymap[SOFTJOY_DOWN][0] = down;
	softjoymap[SOFTJOY_LEFT][0] = left;
	softjoymap[SOFTJOY_RIGHT][0] = right;
	softjoymap[SOFTJOY_FIRE][0] = fire;
	Android_DerotateKeys = derotkeys;

	for (i = 0; i < SOFTJOY_MAXACTIONS; i++) {
		obj = (*env)->GetObjectArrayElement(env, actions, i);
		str = (*env)->GetStringUTFChars(env, obj, NULL);
		sep = strchr(str, ',');
		act = ACTION_NONE;
		akey = AKEY_NONE;
		if (sep) {
			act = atoi(str);
			akey = atoi(sep + 1);
		}
		softjoymap[SOFTJOY_ACTIONBASE + i][0] = act;
		softjoymap[SOFTJOY_ACTIONBASE + i][1] = akey;
		(*env)->ReleaseStringUTFChars(env, obj, str);
		(*env)->DeleteLocalRef(env, obj);
	}
}

static void JNICALL NativePrefJoy(JNIEnv *env, jobject this, jboolean visible, int size, int opacity,
								  jboolean righth, int deadband, jboolean midx, int anchor, int anchorx,
								  int anchory, int grace, jboolean paddle, jboolean plandef)
{
	AndroidInput_JoyOvl.ovl_visible = visible;
	AndroidInput_JoyOvl.areaopacityset = 0.01f * opacity;
	Android_Joyscale = 0.01f * size;
	Android_Joyleft = !righth;
	Android_Splitpct = 0.01f * midx;
	AndroidInput_JoyOvl.deadarea = 0.01f * deadband;
	AndroidInput_JoyOvl.gracearea = 0.02f * grace;
	AndroidInput_JoyOvl.anchor = anchor;
	if (anchor) {
		AndroidInput_JoyOvl.joyarea.l = anchorx;
		AndroidInput_JoyOvl.joyarea.t = anchory;
	}
	Android_Paddle = paddle;
	INPUT_mouse_mode = paddle ? INPUT_MOUSE_PAD : INPUT_MOUSE_OFF;
	Android_PlanetaryDefense = FALSE;
	Android_ReversePddle = 0;
	if (plandef) {
		INPUT_mouse_mode = INPUT_MOUSE_PAD;
		Android_Splitpct = 1.0f;
		AndroidInput_JoyOvl.ovl_visible = FALSE;
		Android_PlanetaryDefense = TRUE;
		Android_Paddle = TRUE;
		Android_ReversePddle = 3;
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
	SYSROM_FindInDir(utf, FALSE);
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

static jstring JNICALL NativeGetURL(JNIEnv *env, jobject this)
{
	return (*env)->NewStringUTF(env, dev_b_status.url);
}

jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
	JNINativeMethod main_methods[] = {
		{ "NativeExit",				"()V",								NativeExit			  },
		{ "NativeRunAtariProgram",	"(Ljava/lang/String;II)V",			NativeRunAtariProgram },
		{ "NativePrefGfx",			"(IZIIZII)V",						NativePrefGfx		  },
		{ "NativePrefMachine",		"(IZ)Z",							NativePrefMachine	  },
		{ "NativePrefEmulation",	"(ZZZZZ)V",							NativePrefEmulation	  },
		{ "NativePrefSoftjoy",		"(ZIIIIII[Ljava/lang/String;)V",	NativePrefSoftjoy	  },
		{ "NativePrefJoy",			"(ZIIZIIZIIIZZ)V",					NativePrefJoy		  },
		{ "NativePrefSound",		"(IZZ)V",							NativePrefSound		  },
		{ "NativeSetROMPath",		"(Ljava/lang/String;)Z",			NativeSetROMPath	  },
		{ "NativeGetJoypos",		"()Ljava/lang/String;",				NativeGetJoypos		  },
		{ "NativeInit",				"()Ljava/lang/String;",				NativeInit			  },
		{ "NativeGetURL",			"()Ljava/lang/String;",				NativeGetURL		  },
		{ "NativeClearDevB",		"()V",								NativeClearDevB		  },
	};
	JNINativeMethod view_methods[] = {
		{ "NativeTouch", 			"(IIIIII)V", 						NativeTouch			  },
		{ "NativeKey",				"(II)V",							NativeKey			  },
	};
	JNINativeMethod snd_methods[] = {
		{ "NativeSoundInit",		"(I)V",								NativeSoundInit		  },
		{ "NativeSoundUpdate",		"(II)V",							NativeSoundUpdate	  },
		{ "NativeSoundExit",		"()V",								NativeSoundExit		  },
	};
	JNINativeMethod render_methods[] = {
		{ "NativeRunFrame",			"()I",								NativeRunFrame		  },
		{ "NativeGetOverlays",		"()V",								NativeGetOverlays	  },
		{ "NativeResize",			"(II)V",							NativeResize		  },
	};
	JNINativeMethod fsel_methods[] = {
		{ "NativeIsDisk",			"(Ljava/lang/String;)Z",			NativeIsDisk		  },
		{ "NativeRunAtariProgram",	"(Ljava/lang/String;II)V",			NativeRunAtariProgram },
		{ "NativeGetDrvFnames",		"()[Ljava/lang/String;",			NativeGetDrvFnames	  },
		{ "NativeUnmountAll",		"()V",								NativeUnmountAll	  },
	};
	JNINativeMethod pref_methods[] = {
		{ "NativeSaveState",		"(Ljava/lang/String;)Z",			NativeSaveState		  },
	};
	JNIEnv *env;
	jclass cls;

	if ((*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_2))
		return JNI_ERR;

	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/MainActivity");
	(*env)->RegisterNatives(env, cls, main_methods, sizeof(main_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/A800view");
	(*env)->RegisterNatives(env, cls, view_methods, sizeof(view_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/AudioThread");
	(*env)->RegisterNatives(env, cls, snd_methods, sizeof(snd_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/A800Renderer");
	(*env)->RegisterNatives(env, cls, render_methods, sizeof(render_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/FileSelector");
	(*env)->RegisterNatives(env, cls, fsel_methods, sizeof(fsel_methods)/sizeof(JNINativeMethod));
	cls = (*env)->FindClass(env, "name/nick/jubanka/colleen/Preferences");
	(*env)->RegisterNatives(env, cls, pref_methods, sizeof(pref_methods)/sizeof(JNINativeMethod));

	return JNI_VERSION_1_2;
}
