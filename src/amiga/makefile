# If Kickstart isn't defined, we don't run on AmigaOS
ifndef Kickstart
CROSS_COMPILE = ppc-amigaos-
RM     = rm -R
MKDIR  = mkdir -p
else
RM = delete all
MKDIR = makedir
endif

CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib
STRIP = $(CROSS_COMPILE)strip

# -------------------------------------------------------------

TARGET = Atari800
VERSION = 2
#CFLAGS = -O3 -I$(AHI_INCLUDE) -I. -I.. -Wall -use-dynld -DSUPPORTS_PLATFORM_CONFIGURE -DSUPPORTS_PLATFORM_CONFIGSAVE 
CFLAGS = -O3 -I$(AHI_INCLUDE) -I. -I.. -Wall -DSUPPORTS_PLATFORM_CONFIGURE -DSUPPORTS_PLATFORM_CONFIGSAVE
#LIBS = -lm -lz -lpng -lAuto
LIBS = -lm -lauto

AMIGASRCS=\
	amiga.c \
	gui.c \
	support.c

EMUSRCS= \
	afile.c \
	antic.c \
	atari.c \
	binload.c \
	cartridge.c \
	cassette.c \
	compfile.c \
	cfg.c \
	colours.c \
	colours_external.c \
	colours_pal.c \
	colours_ntsc.c \
	cpu.c \
	devices.c \
	esc.c \
	gtia.c \
	log.c \
	memory.c \
	monitor.c \
	pia.c \
	pokey.c \
	rtime.c \
	sio.c \
	util.c \
	pbi_proto80.c \
	input.c \
	statesav.c \
	ui_basic.c \
	ui.c \
	screen.c \
	cycle_map.c \
	pbi_mio.c \
	pbi_bb.c \
	pbi_scsi.c \
	pokeysnd.c \
	mzpokeysnd.c \
	remez.c \
	sndsave.c \
	pbi_xld.c \
	votrax.c \
	votraxsnd.c \
	xep80.c \
	xep80_fonts.c \
	pbi.c


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
