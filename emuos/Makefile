# Altirra - Atari 800/800XL emulator
# Kernel ROM replacement
# Copyright (C) 2008-2020 Avery Lee
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

MADS=mads
OUT=build

CDIR=../src/roms/

SRC_BIOS_SHARED=\
 src/atarifont.inc \
 src/boot.s \
 src/cassette.s \
 src/cio.inc \
 src/cio.s \
 src/disk.s \
 src/editor.s \
 src/hardware.inc \
 src/init.s \
 src/interrupt.s \
 src/irq.s \
 src/kerneldb.inc \
 src/keyboard.s \
 src/keytable.s \
 src/mathpack.s \
 src/misc.s \
 src/printer.s \
 src/screenext.s \
 src/screen.s \
 src/sio.inc \
 src/sio.s \
 src/vbi.s \
 src/version.inc \

SRC_BIOS_800=\
 src/blackboard.s \

SRC_BIOS_XL=\
 src/atariifont.inc \
 src/bootscreen.s \
 src/driveimage.inc \
 src/pbi.s \
 src/phandler.s \
 src/selftestentry.s \

SRC_BASIC=\
 src_basic/data.s \
 src_basic/error.s \
 src_basic/evaluator.s \
 src_basic/exec.s \
 src_basic/functions.s \
 src_basic/io.s \
 src_basic/list.s \
 src_basic/math.s \
 src_basic/memory.s \
 src_basic/parser.s \
 src_basic/parserbytecode.s \
 src_basic/statements.s \
 src_basic/system.inc \
 src_basic/tokens.inc \
 src_basic/util.s \
 src_basic/variables.s

all:\
 $(CDIR)/altirraos_xl.c \
 $(CDIR)/altirraos_800.c \
 $(CDIR)/altirra_basic.c

# Rules to assemble ROMs
$(OUT)/altirraos_xl.rom: main.xasm $(SRC_BIOS_XL) $(SRC_BIOS_SHARED) | $(OUT)
	$(MADS) -d:_KERNEL_XLXE=1 -s -p -i:src -i:fnt -b:49152 -l:$(@:.rom=.lst) -t:$(@:.rom=.lab) -o:$@ $<

$(OUT)/altirraos_800.rom: main.xasm $(SRC_BIOS_800) $(SRC_BIOS_SHARED) | $(OUT)
	$(MADS) -d:_KERNEL_XLXE=0 -s -p -i:src -i:fnt -b:55296 -l:$(@:.rom=.lst) -t:$(@:.rom=.lab) -o:$@ $<

$(OUT)/altirra_basic.rom: src_basic/atbasic.s $(SRC_BASIC) | $(OUT)
	$(MADS) -d:CART=1 -s -p -i:src_basic -b:40960 -l:$(@:.rom=.lst) -t:$(@:.rom=.lab) -o:$@ $<

# Rules to convert ROM into HEX file
$(OUT)/%.hex: $(OUT)/%.rom
	hexdump  -ve '"\t" 8/1 "0x%02x," "\n"' < $< | sed -e '$$ s/,$$//' > $@

# Join the HEX file with the headers to get the C source files
$(CDIR)/%.c: %_head.in $(OUT)/%.hex template_tail.in
	cat $^ > $@

$(OUT):
	mkdir -p $(OUT)

clean:
	-rm -f $(OUT)/altirra_basic.lab
	-rm -f $(OUT)/altirra_basic.lst
	-rm -f $(OUT)/altirra_basic.rom
	-rm -f $(OUT)/altirraos_800.lab
	-rm -f $(OUT)/altirraos_800.lst
	-rm -f $(OUT)/altirraos_800.rom
	-rm -f $(OUT)/altirraos_xl.lab
	-rm -f $(OUT)/altirraos_xl.lst
	-rm -f $(OUT)/altirraos_xl.rom
	-rmdir $(OUT)

