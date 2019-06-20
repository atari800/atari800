AUTOMAKE_OPTIONS = subdir-objects

if CONFIGURE_TARGET_LIBATARI800
TARGET_BASE_NAME = libatari800.a
else
TARGET_BASE_NAME = atari800
endif
TARGET = $(TARGET_BASE_NAME)

bin_PROGRAMS = 
noinst_PROGRAMS =

man1dir = $(mandir)/man1

AM_CPPFLAGS = -I.

SUFFIXES = .c .cpp .s .o .h .asm

doc_DATA = 

if CONFIGURE_TARGET_LIBATARI800
lib_LIBRARIES = libatari800.a
include_HEADERS = libatari800/libatari800.h
libatari800_a_SOURCES = \
	libatari800/libatari800.h \
	libatari800/main.c libatari800/main.h \
	libatari800/init.c libatari800/init.h \
	libatari800/exit.c \
	libatari800/input.c libatari800/input.h \
	libatari800/video.c libatari800/video.h \
	libatari800/statesav.c libatari800/statesav.h \
	libatari800/sound.c libatari800/sound.h
noinst_PROGRAMS += libatari800_test guess_settings
libatari800_test_SOURCES = libatari800/libatari800_test.c
libatari800_test_CFLAGS = -Ilibatari800
libatari800_test_LDADD = libatari800.a
guess_settings_SOURCES = libatari800/guess_settings.c
guess_settings_CFLAGS = -Ilibatari800
guess_settings_LDADD = libatari800.a
else
if CONFIGURE_HOST_JAVANVM
all-local:: $(TARGET_BASE_NAME).jar
JAVA = java
JAVAC = javac
else
if CONFIGURE_TARGET_ANDROID
all-local:: android
else
bin_PROGRAMS += atari800
endif
endif
endif


atari800_SOURCES = \
	platform.h \
	pcjoy.h \
	akey.h \
	afile.c afile.h \
	antic.c antic.h \
	atari.c atari.h \
	binload.c binload.h \
	cartridge.c cartridge.h \
	cassette.c cassette.h \
	compfile.c compfile.h \
	cfg.c cfg.h \
	cpu.c cpu.h \
	crc32.c crc32.h \
	devices.c devices.h \
	esc.c esc.h \
	gtia.c gtia.h \
	img_tape.c img_tape.h \
	log.c log.h \
	memory.c memory.h \
	monitor.c monitor.h \
	pbi.c pbi.h \
	pia.c pia.h \
	pokey.c pokey.h \
	roms/altirra_5200_os.c roms/altirra_5200_os.h \
	rtime.c rtime.h \
	sio.c sio.h \
	sysrom.c sysrom.h \
	util.c util.h
atari800_LDADD =

if A8_USE_SDL
atari800_SOURCES += sdl/init.c sdl/init.h
if CONFIGURE_HOST_WIN
atari800_SOURCES += win32/SDL_win32_main.c
endif
endif

if WITH_SOUND
atari800_SOURCES += \
	pokeysnd.c pokeysnd.h \
	mzpokeysnd.c mzpokeysnd.h \
	remez.c remez.h \
	sndsave.c sndsave.h
endif
if WITH_SOUND_SDL
atari800_SOURCES += sound.c sound.h sdl/sound.c
endif
if WITH_SOUND_FALCON
atari800_SOURCES += sound.c falcon/sound.c
endif
if WITH_SOUND_JAVANVM
atari800_SOURCES += sound.c javanvm/sound.c
endif
if WITH_SOUND_DOSSB
atari800_SOURCES += sound.c dos/sound_dos.c dos/dos_sb.c dos/dos_sb.h
endif
if WITH_SOUND_OSS
atari800_SOURCES += sound.c sound_oss.c
endif
if WITH_SOUND_LIBATARI800
atari800_SOURCES += sound.c libatari800/sound.c libatari800/sound.h
endif
if WANT_POKEYREC
atari800_SOURCES += pokeyrec.c pokeyrec.h
endif



if CONFIGURE_TARGET_FALCON
atari800_SOURCES += \
	falcon/main.c falcon/c2p_uni.asm falcon/c2p_unid.asm falcon/videl.asm falcon/ikbd.asm \
	falcon/res.h falcon/xcb.h falcon/jclkcook.h
endif

if CONFIGURE_TARGET_PS2
atari800_SOURCES += atari_ps2.c
atari800_LDADD += ps2kbd.s usbd.s audsrv.s
endif

if CONFIGURE_TARGET_RPI
atari800_SOURCES += atari_rpi.c gles2/video.c sdl/main.c sdl/input.c sdl/input.h
endif

if CONFIGURE_TARGET_WINDX
noinst_LIBRARIES = libwin32.a
libwin32_a_SOURCES = \
	win32/atari_win32.c win32/atari_win32.h \
	win32/screen_win32.c win32/screen_win32.h \
	win32/render_direct3d.c win32/render_direct3d.h \
	win32/render_directdraw.c win32/render_directdraw.h \
	win32/render_gdi.c win32/render_gdi.h \
	win32/render_gdiplus.c win32/render_gdiplus.h \
	win32/main.c win32/main.h win32/main_menu.h \
	win32/keyboard.c win32/keyboard.h \
	win32/joystick.c win32/joystick.h
if WITH_SOUND_WIN
libwin32_a_SOURCES += win32/sound.c
endif
atari800_LDADD += libwin32.a win32/atari.ro
# A special rule for win32 to not compile with -ansi -pedantic
libwin32_a_CFLAGS = $(CFLAGS_NOANSI)
endif

if CONFIGURE_TARGET_X11
atari800_SOURCES += atari_x11.c
endif

if WITH_VIDEO_JAVANVM
atari800_SOURCES += javanvm/main.c javanvm/javanvm.h javanvm/video.c javanvm/video.h javanvm/input.c javanvm/input.h
endif

if WITH_VIDEO_SDL
atari800_SOURCES += \
	videomode.c videomode.h \
	sdl/main.c \
	sdl/video.c sdl/video.h \
	sdl/video_sw.c sdl/video_sw.h \
	sdl/input.c sdl/input.h \
	sdl/palette.c sdl/palette.h
atari800_SOURCES += pbi_proto80.c pbi_proto80.h af80.c af80.h bit3.c bit3.h
endif

if WITH_VIDEO_DOSVGA
atari800_SOURCES += dos/atari_vga.c dos/vga_gfx.c dos/vga_gfx.h dos/vga_asm.s dos/dos_ints.h
endif

if WITH_VIDEO_CURSES
atari800_SOURCES += atari_curses.c
endif

if WITH_VIDEO_NO
atari800_SOURCES += atari_basic.c
else
# These objects are not compiled when --with-video=no
atari800_SOURCES += input.c input.h statesav.c statesav.h
if !WITH_VIDEO_LIBATARI800
atari800_SOURCES += ui_basic.c ui_basic.h ui.c ui.h
endif
if !WANT_CURSES_BASIC
# These objects are not compiled when --with-video=no or --enable-cursesbasic=no
atari800_SOURCES += \
	artifact.c artifact.h \
	colours.c colours.h \
	colours_ntsc.c colours_ntsc.h \
	colours_pal.c colours_pal.h \
	colours_external.c colours_external.h \
	screen.c screen.h
if WANT_NEW_CYCLE_EXACT
atari800_SOURCES += cycle_map.c cycle_map.h
endif
endif
endif

if WANT_EMUOS_ALTIRRA
atari800_SOURCES += \
	roms/altirraos_800.c roms/altirraos_800.h \
	roms/altirraos_xl.c roms/altirraos_xl.h \
	roms/altirra_basic.c roms/altirra_basic.h
endif

if WANT_PBI_MIO
atari800_SOURCES += pbi_mio.c pbi_mio.h
endif
if WANT_PBI_BB
atari800_SOURCES += pbi_bb.c pbi_bb.h
endif
if WANT_PBI_MIO_OR_BB
atari800_SOURCES += pbi_scsi.c pbi_scsi.h
endif
if WANT_PBI_XLD
if WITH_SOUND
atari800_SOURCES += pbi_xld.c pbi_xld.h
endif
endif
if WANT_VOICEBOX
if WITH_SOUND
atari800_SOURCES += voicebox.c voicebox.h
endif
endif
if WANT_PBI_XLD_OR_VOICEBOX
if WITH_SOUND
atari800_SOURCES += votrax.c votrax.h votraxsnd.c votraxsnd.h
endif
endif
if WANT_IDE
atari800_SOURCES += ide.c ide.h ide_internal.h
endif
if WITH_OPENGL
atari800_SOURCES += sdl/video_gl.c sdl/video_gl.h
endif
if WANT_FALCON_CPUASM
atari800_SOURCES += falcon/cpu_m68k.asm
endif
if WANT_XEP80_EMULATION
atari800_SOURCES += xep80.c xep80.h xep80_fonts.c xep80_fonts.h
endif
if WANT_NTSC_FILTER
atari800_SOURCES += \
	filter_ntsc.c filter_ntsc.h \
	atari_ntsc/atari_ntsc.c atari_ntsc/atari_ntsc.h \
	atari_ntsc/atari_ntsc_config.h atari_ntsc/atari_ntsc_impl.h
endif
if WANT_PAL_BLENDING
atari800_SOURCES += pal_blending.c pal_blending.h
endif
if WANT_R_IO_DEVICE
atari800_SOURCES += rdevice.c rdevice.h
endif


if CONFIGURE_TARGET_LIBATARI800
libatari800_a_SOURCES += $(atari800_SOURCES)
endif


# A special rule for SDL_win32_main.c to suppress warnings since this file is
# from SDL and should not have to be modified
win32/SDL_win32_main.o: win32/SDL_win32_main.c
	$(CC) -c -o $@ $(AM_CPPFLAGS) $(CFLAGS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(DEFS) -Wno-missing-declarations -Wno-missing-prototypes $<

ide.o: ide.c ide.h ide_internal.h
	$(CC) -c -o $@ $(AM_CPPFLAGS) $(CFLAGS_NOANSI) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(DEFS) -Wno-missing-declarations -Wno-missing-prototypes $<

win32/atari.ro: win32/atari.rc
	$(WINDRES) -o $@ --output-format coff --target pe-i386 --define WIN32 --define __MINGW32__ $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $< 

# some special rules for some win32 source files which are actually C++
win32/main.o: win32/main.c
	$(CC) -x c++ -c -o $@ $(AM_CPPFLAGS) $(CFLAGS_NOANSI) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(DEFS) -Wno-missing-declarations -Wno-missing-prototypes $<
win32/render_gdiplus.o: win32/render_gdiplus.c
	$(CC) -x c++ -c -o $@ $(AM_CPPFLAGS) $(CFLAGS_NOANSI) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(DEFS) -Wno-missing-declarations -Wno-missing-prototypes $<
win32/render_direct3d.o: win32/render_direct3d.c
	$(CC) -x c++ -c -o $@ $(AM_CPPFLAGS) $(CFLAGS_NOANSI) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(DEFS) -Wno-missing-declarations -Wno-missing-prototypes $<



.asm.o:
	$(CCAS) $(CCASFLAGS) -o $@ $<

if CONFIGURE_HOST_JAVANVM
$(TARGET_BASE_NAME).class: javanvm/$(TARGET_BASE_NAME).java | $(TARGET_BASE_NAME)_runtime.class
	$(JAVAC) -d . $(JAVACFLAGS) $<
RUNTIME = _runtime
else
RUNTIME =
endif


$(TARGET_BASE_NAME)$(RUNTIME).class: $(TARGET)
#To compile using java bytecode directly:
#	$(JAVA) $(JAVAFLAGS) org.ibex.nestedvm.Compiler -o UnixRuntime -outfile $@ $(TARGET_BASE_NAME)$(RUNTIME) $(TARGET)
	$(JAVA) $(JAVAFLAGS) org.ibex.nestedvm.Compiler -o UnixRuntime -outformat javasource -outfile $(TARGET_BASE_NAME)$(RUNTIME).java $(TARGET_BASE_NAME)$(RUNTIME) $(TARGET)
	$(JAVAC) -d . $(JAVACFLAGS) -J-Xmx256m $(TARGET_BASE_NAME)$(RUNTIME).java
##Also, -o UnixRuntime fixes directory browsing but requires /c:/-style paths

$(TARGET_BASE_NAME).jar: $(TARGET_BASE_NAME).class $(TARGET_BASE_NAME)$(RUNTIME).class
	printf "Manifest-Version: 1.0\nMain-Class: $(TARGET_BASE_NAME)\nClass-Path: unix_runtime.jar\n" > .manifest
	jar cfm $(TARGET_BASE_NAME).jar .manifest *.class

# Allow parallel execution in sub-make with '+'
android:
	+ ndk-build -C android
	ant -f android/build.xml debug
.PHONY: android

CLEANFILES = *.o *.a *.class .manifest $(TARGET) $(TARGET_BASE_NAME).jar $(TARGET_BASE_NAME)_runtime.java core *.bak *~
CLEANFILES += roms/*.o roms/*.bak roms/*~
CLEANFILES += dos/*.o dos/*.bak dos/*~
CLEANFILES += falcon/*.o falcon/*.bak falcon/*~
CLEANFILES += sdl/*.o sdl/*.bak sdl/*~
CLEANFILES += win32/*.o win32/*.ro win32/*.bak win32/*~
CLEANFILES += javanvm/*.o javanvm/*.bak javanvm/*~
CLEANFILES += atari_ntsc/*.o atari_ntsc/*.bak atari_ntsc/*~
CLEANFILES += android/libs android/obj android/bin android/gen
CLEANFILES += libatari800/*.o libatari800/*.bak libatari800/*~

DISTCLEANFILES = Makefile configure config.log config.status config.h android/jni/Android.mk
DISTCLEANFILES += autom4te.cache

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(man1dir)
	$(INSTALL_DATA) $(srcdir)/atari800.man ${DESTDIR}${man1dir}/atari800.1

uninstall-local:
	$(RM) ${DESTDIR}${man1dir}/atari800.1*

readme.html: $(TARGET)
	./$(TARGET) -help </dev/null | $(top_srcdir)/util/usage2html.pl \
		$(top_srcdir)/DOC/readme.html.in $(top_srcdir)/DOC/USAGE $(srcdir)/atari.h > $@

doc: readme.html

EXTRA_DIST = $(doc_DATA) atari800.man
EXTRA_DIST += joycfg.c mkimg.c
EXTRA_DIST += win32/atari.rc win32/atari1.ico
EXTRA_DIST += vtxsmpls.inc
EXTRA_DIST += javanvm/UnixRuntime.patch javanvm/atari800.java
EXTRA_DIST += macosx
EXTRA_DIST += wince
EXTRA_DIST += win32/msc
EXTRA_DIST += amiga
EXTRA_DIST += \
	android/AndroidManifest.xml \
	android/build.xml \
	android/project.properties \
	android/jni/Android.mk.in \
	android/jni/Application.mk \
	android/jni/keys.inc \
	android/jni/androidinput.c android/jni/androidinput.h \
	android/jni/graphics.c android/jni/graphics.h \
	android/jni/jni.c \
	android/jni/platform.c \
	android/jni/sound.c \
	android/res/ \
	android/src/A800Renderer.java \
	android/src/A800view.java \
	android/src/AudioThread.java \
	android/src/FileSelector.java \
	android/src/KeymapPreference.java \
	android/src/MainActivity.java \
	android/src/Preferences.java \
	android/src/SliderPreference.java \
	$(empty)
