CC = ppc-amigaos-gcc
AR = ppc-amigaos-ar
RANLIB = ppc-amigaos-ranlib
STRIP = ppc-amigaos-strip
RM = rm
MKDIR = mkdir

# -------------------------------------------------------------

TARGET = Atari800
CFLAGS = -I. -I.. -Wall
LIBS = -lm

AMIGASRCS=\
	amiga.c \
	gui.c \
	support.c

EMUSRCS= \
	antic.c \
	atari.c \
	ataripcx.c \
	binload.c \
	cartridge.c \
	cassette.c \
	compfile.c \
	colours.c \
	cpu.c \
	cycle_map.c \
	devices.c \
	diskled.c \
	gtia.c \
	input.c \
	list.c \
	log.c \
	memory.c \
	monitor.c \
	mzpokeysnd.c \
	pia.c \
	pokey.c \
	pokeysnd.c \
	prompts.c \
	remez.c \
	rt-config.c \
	rtime.c \
	sio.c \
	statesav.c \
	ui.c \
	ui_basic.c


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
	$(STRIP) --strip-all -o $@ $@.debug

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
