########################
#
# VGMPlay Makefile
# (for GNU Make 3.81)
#
########################

CC = gcc
CPP = g++
CCFLAGS = -O3
MAINFLAGS = -DCONSOLE_MODE -DADDITIONAL_FORMATS
EMUFLAGS = -DENABLE_ALL_CORES

SRC = .
OBJ = obj
EMUSRC = $(SRC)/chips
EMUOBJ = $(OBJ)/chips

OBJDIRS = $(OBJ) \
	$(EMUOBJ)
MAINOBJS = \
	$(OBJ)/VGMPlay.o \
	$(OBJ)/VGMPlay_AddFmts.o \
	$(OBJ)/VGMPlayUI.o \
	$(OBJ)/Stream.o \
	$(OBJ)/ChipMapper.o
EMUOBJS = \
	$(EMUOBJ)/262intf.o \
	$(EMUOBJ)/2151intf.o \
	$(EMUOBJ)/2203intf.o \
	$(EMUOBJ)/2413intf.o \
	$(EMUOBJ)/2608intf.o \
	$(EMUOBJ)/2610intf.o \
	$(EMUOBJ)/2612intf.o \
	$(EMUOBJ)/3526intf.o \
	$(EMUOBJ)/3812intf.o \
	$(EMUOBJ)/8950intf.o \
	$(EMUOBJ)/adlibemu_opl2.o \
	$(EMUOBJ)/adlibemu_opl3.o \
	$(EMUOBJ)/ay8910.o \
	$(EMUOBJ)/c140.o \
	$(EMUOBJ)/c6280.o \
	$(EMUOBJ)/dac_control.o \
	$(EMUOBJ)/emu2413.o \
	$(EMUOBJ)/fm2612.o \
	$(EMUOBJ)/fm.o \
	$(EMUOBJ)/fmopl.o \
	$(EMUOBJ)/gb.o \
	$(EMUOBJ)/k051649.o \
	$(EMUOBJ)/k053260.o \
	$(EMUOBJ)/k054539.o \
	$(EMUOBJ)/multipcm.o \
	$(EMUOBJ)/nes_apu.o \
	$(EMUOBJ)/okim6258.o \
	$(EMUOBJ)/okim6295.o \
	$(EMUOBJ)/panning.o \
	$(EMUOBJ)/pokey.o \
	$(EMUOBJ)/pwm.o \
	$(EMUOBJ)/qsound.o \
	$(EMUOBJ)/rf5c68.o \
	$(EMUOBJ)/segapcm.o \
	$(EMUOBJ)/scd_pcm.o \
	$(EMUOBJ)/sn76489.o \
	$(EMUOBJ)/sn76496.o \
	$(EMUOBJ)/sn76496_opl.o \
	$(EMUOBJ)/sn764intf.o \
	$(EMUOBJ)/upd7759.o \
	$(EMUOBJ)/ym2151.o \
	$(EMUOBJ)/ym2413.o \
	$(EMUOBJ)/ym2413hd.o \
	$(EMUOBJ)/ym2413_opl.o \
	$(EMUOBJ)/ym2612.o \
	$(EMUOBJ)/ymdeltat.o \
	$(EMUOBJ)/ymf262.o \
	$(EMUOBJ)/ymf271.o \
	$(EMUOBJ)/ymf278b.o \
	$(EMUOBJ)/ymz280b.o

VGMPlay:	$(OBJDIRS) $(MAINOBJS) $(EMUOBJS)
	$(CC) $(MAINOBJS) $(EMUOBJS) -lpthread -lm -lrt -Wl,-lz -o VGMPlay

# compile the main c-files
$(OBJ)/%.o:	$(SRC)/%.c
	$(CC) $(CCFLAGS) $(MAINFLAGS) -c $< -o $@

# compile the chip-emulator c-files
$(EMUOBJ)/%.o:	$(EMUSRC)/%.c
	$(CC) $(CCFLAGS) $(EMUFLAGS) -c $< -o $@

$(OBJDIRS):
	mkdir $(OBJDIRS)

clean:
	rm -f $(MAINOBJS) $(EMUOBJS)
