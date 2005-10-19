CROSS=ppc-amigaos-

CC = $(CROSS)gcc
AR = $(CROSS)ar
RANLIB = $(CROSS)ranlib
STRIP = $(CROSS)strip
RM = rm -rf
MKDIR = mkdir -p

# -------------------------------------------------------------

TARGET = Atari800
VERSION = 2
CFLAGS = -O3 -I. -I.. -I"$(SDK_INCLUDE)" -I"$(AHI_INCLUDE)" -Wall -D__USE_CLIB2_TIMEVAL
LIBS = -lm

AMIGASRCS=\
	amiga.c \
	gui.c \
	support.c

EMUSRCS= \
	antic.c \
	atari.c \
	binload.c \
	cartridge.c \
	cassette.c \
	compfile.c \
	colours.c \
	cpu.c \
	cycle_map.c \
	devices.c \
	gtia.c \
	input.c \
	log.c \
	memory.c \
	monitor.c \
	mzpokeysnd.c \
	pia.c \
	pokey.c \
	pokeysnd.c \
	remez.c \
	rtime.c \
	screen.c \
	sio.c \
	statesav.c \
	ui.c \
	ui_basic.c \
	util.c


SRCS = $(AMIGASRCS) $(addprefix ../,$(EMUSRCS))

# -------------------------------------------------------------

OBJS = $(SRCS:%.c=ppc-amigaos-objs/amiga/%.o)
AOBJS = $(ASRCS:%.S=ppc-amigaos-objs/amiga/%.o)

all: dirs $(TARGET)

dirs:
	-$(MKDIR) ppc-amigaos-objs ppc-amigaos-objs/amiga

# Rules for building
$(TARGET): $(OBJS) $(AOBJS)
	$(CC) $(LINK) -o $@.debug $(OBJS) $(AOBJS) $(LIBS)
	$(STRIP) -R.comment -o $@ $@.debug
	cp Atari800 "/cygdrive/c/Dokumente und Einstellungen/sba/Eigene Dateien/AmigaOne/Atari800/Atari800"

ppc-amigaos-objs/amiga/%.o: %.S
	$(CC) -Wa,-mregnames $(AFLAGS) -I$/home/sba/amigaos4/include -c $< -o $@

ppc-amigaos-objs/amiga/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) ppc-amigaos-objs

.PHONY: revision
revision:
	bumprev $(VERSION) $(TARGET)
