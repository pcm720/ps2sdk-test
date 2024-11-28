#.SILENT:
ELF_BASE_NAME := test

EE_BIN = $(ELF_BASE_NAME)_unc.elf
EE_BIN_PKD = $(ELF_BASE_NAME).elf
EE_BIN_DEBUG := $(ELF_BASE_NAME)-debug_unc.elf
EE_BIN_DEBUG_PKD := $(ELF_BASE_NAME)-debug.elf

EE_OBJS = main.o
IRX_FILES += fileXio.irx iomanX.irx ps2dev9.irx bdm.irx bdmfs_fatfs.irx ata_bd.irx


EE_LIBS = -ldebug -lfileXio -lpatches -lgskit -ldmakit -lgskit_toolkit
EE_CFLAGS := -mno-gpopt -G0 -DGIT_VERSION="\"${GIT_VERSION}\""

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = ./

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_INCS := -Iinclude -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s

BIN2C = $(PS2SDK)/bin/bin2c

.PHONY: all clean

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	rm -rf $(EE_BIN) $(EE_BIN_PKD) $(EE_BIN_DEBUG) $(EE_BIN_DEBUG_PKD) $(EE_ASM_DIR) $(EE_OBJS_DIR)

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx
	
$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
