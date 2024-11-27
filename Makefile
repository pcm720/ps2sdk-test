#.SILENT:

GIT_VERSION := $(shell git describe --always --dirty --tags --exclude nightly)

ELF_BASE_NAME := nhddl-$(GIT_VERSION)

EE_BIN = $(ELF_BASE_NAME)_unc.elf
EE_BIN_PKD = $(ELF_BASE_NAME).elf
EE_BIN_DEBUG := $(ELF_BASE_NAME)-debug_unc.elf
EE_BIN_DEBUG_PKD := $(ELF_BASE_NAME)-debug.elf

EE_OBJS = main.o module_init.o common.o iso.o history.o options.o gui.o pad.o launcher.o iso_cache.o iso_title_id.o devices.o
IRX_FILES += sio2man.irx mcman.irx mcserv.irx fileXio.irx iomanX.irx freepad.irx
RES_FILES += icon_A.sys icon_C.sys icon_J.sys
ELF_FILES += loader.elf

EE_LIBS = -ldebug -lfileXio -lpatches -lgskit -ldmakit -lgskit_toolkit -lpng -lz -ltiff -lpad -lmc
EE_CFLAGS := -mno-gpopt -G0 -DGIT_VERSION="\"${GIT_VERSION}\""

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(RES_FILES:.sys=_sys.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_INCS := -Iinclude -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s

BIN2C = $(PS2SDK)/bin/bin2c

.PHONY: all clean .FORCE

.FORCE:

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	$(MAKE) -C loader clean
	rm -rf $(EE_BIN) $(EE_BIN_PKD) $(EE_BIN_DEBUG) $(EE_BIN_DEBUG_PKD) $(EE_ASM_DIR) $(EE_OBJS_DIR)

# ELF loader
loader/loader.elf: loader
	$(MAKE) -C $<

%loader_elf.c: loader/loader.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=loader/%)loader.elf $@ $(*:$(EE_SRC_DIR)%=%)loader_elf

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

# Resource files
%_sys.c:
	$(BIN2C) res/$(*:$(EE_SRC_DIR)%=%).sys $@ $(*:$(EE_SRC_DIR)%=%)_sys

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

ifeq ($(NEEDS_REBUILD),1)
# If rebuild flag is set, add .FORCE to force full rebuild
$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c .FORCE | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
else
$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
endif

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
