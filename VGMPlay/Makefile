########################
#
# VGMPlay Makefile
# (for GNU Make 3.81)
#
########################

# TODO: Make this look less horrible.

# (notice that you can just pass these as arguments when running make)

# set to 1 if you build on Windows using MinGW.
ifndef WINDOWS
WINDOWS = 0
endif

# set to 1 if you build on Mac OSX
ifndef MACOSX
MACOSX = 0
endif

ifeq ($(WINDOWS), 1)
# no libAO for Windows
USE_LIBAO = 0
endif
ifeq ($(MACOSX), 1)
# Mac OSX requires libAO
USE_LIBAO = 1
endif

# disable Hardware OPL Support [enabled by default]
ifndef DISABLE_HWOPL_SUPPORT
DISABLE_HWOPL_SUPPORT = 1
endif

# set to 1 if you want to use libao instead of OSS for sound streaming under Linux [enabled by default]
ifndef USE_LIBAO
USE_LIBAO = 1
endif

# set to 1 for debug builds
ifndef DEBUG
DEBUG = 0
endif

# set to 1 for swprintf fix on older MinGW versions
ifndef OLD_SWPRINTF
OLD_SWPRINTF = 0
endif

# Byte Order
#   0 = undefined (use endian-safe code, might be slightly slower)
#   1 = Little Endian (Intel) [default]
#   2 = Big Endian (Motorola)
ifndef BYTE_ORDER
BYTE_ORDER = 1
endif



CC = gcc
ifndef PREFIX
PREFIX = /usr/local
#PREFIX = $(HOME)/.local
endif
MANPREFIX = $(PREFIX)/share/man

# -- VGMPlay specific Compile Flags --
MAINFLAGS := -DCONSOLE_MODE -DADDITIONAL_FORMATS -DSET_CONSOLE_TITLE
ifeq ($(WINDOWS), 1)
# MinGW defines __WINDOWS__, Visual Studio defines WIN32
MAINFLAGS += -DWIN32
endif
ifeq ($(OLD_SWPRINTF), 1)
MAINFLAGS += -DOLD_SWPRINTF
endif
ifeq ($(USE_LIBAO), 1)
MAINFLAGS += -DUSE_LIBAO
endif
EMUFLAGS := -DENABLE_ALL_CORES

# -- Byte Order Optimizations --
ifeq ($(BYTE_ORDER), 1)
# Intel Byte Order
MAINFLAGS += -DVGM_LITTLE_ENDIAN
EMUFLAGS += -DVGM_LITTLE_ENDIAN
else
ifeq ($(BYTE_ORDER), 2)
# Motorola Byte Order
MAINFLAGS += -DVGM_BIG_ENDIAN
EMUFLAGS += -DVGM_BIG_ENDIAN
else
# undefined byte order
endif
endif

ifeq ($(DISABLE_HWOPL_SUPPORT), 1)
MAINFLAGS += -DDISABLE_HW_SUPPORT
endif


# -- General Compile Flags --
ifeq ($(DEBUG), 0)
CFLAGS := -O3 -g0 $(CFLAGS)
else
CFLAGS := -g $(CFLAGS)
endif
#CFLAGS += -Wall
CFLAGS += -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable
# libm (math library) and libz (zlib)
LDFLAGS := -lm -lz $(LDFLAGS)

ifeq ($(WINDOWS), 1)
# for Windows, add kernel32 and winmm (Multimedia APIs)
LDFLAGS += -lkernel32 -lwinmm
else	# if UNIX
ifeq ($(MACOSX), 1)
# You might want to include additional paths using -I/some/path here,
# in case some libraries (like libao) can't be found.
#CFLAGS += -I/some/path
else
# for Linux, add librt (clock stuff) and libpthread (threads)
LDFLAGS += -lrt -lpthread -pthread
endif
MAINFLAGS += -pthread -DSHARE_PREFIX=\"$(PREFIX)\"
endif

ifeq ($(USE_LIBAO), 1)
LDFLAGS += -lao
endif

# add Library Path, if defined
ifdef LD_LIBRARY_PATH
LDFLAGS += -L $(LD_LIBRARY_PATH)
endif

SRC = .
OBJ = obj
EMUSRC = $(SRC)/chips
EMUOBJ = $(OBJ)/chips

OBJDIRS = \
	$(OBJ) \
	$(EMUOBJ)
MAINOBJS = \
	$(OBJ)/VGMPlay.o \
	$(OBJ)/VGMPlay_AddFmts.o \
	$(OBJ)/Stream.o \
	$(OBJ)/ChipMapper.o
ifeq ($(WINDOWS), 1)
ifeq ($(DISABLE_HWOPL_SUPPORT), 0)
MAINOBJS += $(OBJ)/pt_ioctl.o
endif
endif
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
	$(EMUOBJ)/ay_intf.o \
	$(EMUOBJ)/c140.o \
	$(EMUOBJ)/c352.o \
	$(EMUOBJ)/c6280.o \
	$(EMUOBJ)/c6280intf.o \
	$(EMUOBJ)/dac_control.o \
	$(EMUOBJ)/es5503.o \
	$(EMUOBJ)/es5506.o \
	$(EMUOBJ)/emu2149.o \
	$(EMUOBJ)/emu2413.o \
	$(EMUOBJ)/fm2612.o \
	$(EMUOBJ)/fm.o \
	$(EMUOBJ)/fmopl.o \
	$(EMUOBJ)/gb.o \
	$(EMUOBJ)/iremga20.o \
	$(EMUOBJ)/k051649.o \
	$(EMUOBJ)/k053260.o \
	$(EMUOBJ)/k054539.o \
	$(EMUOBJ)/multipcm.o \
	$(EMUOBJ)/nes_apu.o \
	$(EMUOBJ)/nes_intf.o \
	$(EMUOBJ)/np_nes_apu.o \
	$(EMUOBJ)/np_nes_dmc.o \
	$(EMUOBJ)/np_nes_fds.o \
	$(EMUOBJ)/okim6258.o \
	$(EMUOBJ)/okim6295.o \
	$(EMUOBJ)/Ootake_PSG.o \
	$(EMUOBJ)/panning.o \
	$(EMUOBJ)/pokey.o \
	$(EMUOBJ)/pwm.o \
	$(EMUOBJ)/qsound_ctr.o \
	$(EMUOBJ)/qsound_mame.o \
	$(EMUOBJ)/qsound_intf.o \
	$(EMUOBJ)/rf5c68.o \
	$(EMUOBJ)/saa1099.o \
	$(EMUOBJ)/segapcm.o \
	$(EMUOBJ)/scd_pcm.o \
	$(EMUOBJ)/scsp.o \
	$(EMUOBJ)/scspdsp.o \
	$(EMUOBJ)/sn76489.o \
	$(EMUOBJ)/sn76496.o \
	$(EMUOBJ)/sn764intf.o \
	$(EMUOBJ)/upd7759.o \
	$(EMUOBJ)/vsu.o \
	$(EMUOBJ)/ws_audio.o \
	$(EMUOBJ)/x1_010.o \
	$(EMUOBJ)/ym2151.o \
	$(EMUOBJ)/ym2413.o \
	$(EMUOBJ)/ym2612.o \
	$(EMUOBJ)/ym3438.o \
	$(EMUOBJ)/ymdeltat.o \
	$(EMUOBJ)/ymf262.o \
	$(EMUOBJ)/ymf271.o \
	$(EMUOBJ)/ymf278b.o \
	$(EMUOBJ)/ymz280b.o \
	$(EMUOBJ)/ay8910_opl.o \
	$(EMUOBJ)/sn76496_opl.o \
	$(EMUOBJ)/ym2413hd.o \
	$(EMUOBJ)/ym2413_opl.o
VGMPLAY_OBJS = \
	$(OBJ)/VGMPlayUI.o
VGM2PCM_OBJS = \
	$(OBJ)/vgm2pcm.o
VGM2WAV_OBJS = \
	$(OBJ)/vgm2wav.o
EXTRA_OBJS = $(VGMPLAY_OBJS) $(VGM2PCM_OBJS) $(VGM2WAV_OBJS)


all:	vgmplay vgm2pcm vgm2wav

vgmplay:	$(EMUOBJS) $(MAINOBJS) $(VGMPLAY_OBJS)
	@echo Linking vgmplay ...
	@$(CC) $(VGMPLAY_OBJS) $(MAINOBJS) $(EMUOBJS) $(LDFLAGS) -o vgmplay
	@echo Done.

vgm2pcm:	$(EMUOBJS) $(MAINOBJS) $(VGM2PCM_OBJS)
	@echo Linking vgm2pcm ...
	@$(CC) $(VGM2PCM_OBJS) $(MAINOBJS) $(EMUOBJS) $(LDFLAGS) -o vgm2pcm
	@echo Done.

vgm2wav:	$(EMUOBJS) $(MAINOBJS) $(VGM2WAV_OBJS)
	@echo Linking vgm2wav ...
	@$(CC) $(VGM2WAV_OBJS) $(MAINOBJS) $(EMUOBJS) $(LDFLAGS) -o vgm2wav
	@echo Done.

# compile the chip-emulator c-files
$(EMUOBJ)/%.o:	$(EMUSRC)/%.c
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(EMUFLAGS) -c $< -o $@

# compile the main c-files
$(OBJ)/%.o:	$(SRC)/%.c
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(MAINFLAGS) -c $< -o $@

clean:
	@echo Deleting object files ...
	@rm -f $(MAINOBJS) $(EMUOBJS) $(EXTRA_OBJS)
	@echo Deleting executable files ...
	@rm -f vgmplay vgm2pcm vgm2wav
	@echo Done.

# Thanks to ZekeSulastin and nextvolume for the install and uninstall routines.
install:	vgmplay
	install -m 755 vgmplay $(DESTDIR)$(PREFIX)/bin/vgmplay
	install -m 755 vgm2pcm $(DESTDIR)$(PREFIX)/bin/vgm2pcm
	install -m 755 vgm2wav $(DESTDIR)$(PREFIX)/bin/vgm2wav
	mkdir -m 755 -p $(DESTDIR)$(MANPREFIX)/man1
	install -m 644 vgmplay.1 $(DESTDIR)$(MANPREFIX)/man1/vgmplay.1
	mkdir -m 755 -p $(DESTDIR)$(PREFIX)/share/vgmplay
	install -m 644 VGMPlay.ini $(DESTDIR)$(PREFIX)/share/vgmplay/vgmplay.ini

# install ROMs
rom_install:
	install -m 644 yrw801.rom $(DESTDIR)$(PREFIX)/share/vgmplay/yrw801.rom


# Install the "vgm-player" wrapper
play_install:	install
	install -m 755 vgm-player $(DESTDIR)$(PREFIX)/bin/vgm-player

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/vgmplay
	rm -f $(DESTDIR)$(PREFIX)/bin/vgm2pcm
	rm -f $(DESTDIR)$(PREFIX)/bin/vgm2wav
	rm -f $(DESTDIR)$(PREFIX)/bin/vgm-player
	rm -f $(DESTDIR)$(MANPREFIX)/man1/vgmplay.1
	rm -rf $(DESTDIR)$(PREFIX)/share/vgmplay

.PHONY: all clean install uninstall
