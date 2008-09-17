# Makefile for Microsoft NMAKE + CL
# Invoke with:
# nmake /f win32\msc\Makefile
# from the "src" directory.

CC = cl
RC = rc

DEFS =
LIBS = \
	libpng.lib zlib.lib \
	ddraw.lib dinput.lib dsound.lib dxguid.lib gdi32.lib \
	user32.lib winmm.lib
TARGET = atari800.exe

CFLAGS = /nologo /O2 /GS- /W3 /wd4996 /I win32\msc /I .
LDFLAGS = /nologo

OBJS = \
	afile.obj \
	antic.obj \
	atari.obj \
	binload.obj \
	cartridge.obj \
	cassette.obj \
	cfg.obj \
	colours.obj \
	compfile.obj \
	cpu.obj \
	cycle_map.obj \
	devices.obj \
	esc.obj \
	gtia.obj \
	input.obj \
	log.obj \
	memory.obj \
	monitor.obj \
	mzpokeysnd.obj \
	pbi.obj \
	pbi_xld.obj \
	pia.obj \
	pokey.obj \
	pokeysnd.obj \
	remez.obj \
	rtime.obj \
	screen.obj \
	sio.obj \
	sndsave.obj \
	statesav.obj \
	ui.obj \
	ui_basic.obj \
	util.obj \
	votrax.obj \
	win32\atari_win32.obj \
	win32\joystick.obj \
	win32\keyboard.obj \
	win32\main.obj \
	win32\screen_win32.obj \
	win32\sound.obj \
	win32\atari.res

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) /Fe$(TARGET) $(LDFLAGS) $(OBJS) $(LIBS)

# the default .c.obj rule would place the object files in the src directory
{win32\}.c{win32\}.obj:
	$(CC) $(CFLAGS) /c /Fo$@ $**

clean:
	del *.obj
	del win32\*.obj
	del win32\atari.res
	del $(TARGET)
