@echo This program creates a bb.bin image
@echo from bb_40_4f.bin, bb_50_5f.bin and bb_a0_bf.bin
@echo for use with the Atari800 5200 emulator.
copy /b bb_40_4f.bin + bb_50_5f.bin + bb_a0_bf.bin bb.bin
