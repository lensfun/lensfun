# Additional rules for compiling .asm files to gcc .o format

ifeq ($(GCC.ASSEMBLER),NASM)

GCC.NASM ?= nasm
ifeq ($(TARGET),win32)
GCC.NASMFLAGS = -f win32 -DEXTERNC_UNDERSCORE
else
GCC.NASMFLAGS = -f elf
endif
GCC.NASMFLAGS += \
    $(GCC.NASMFLAGS.$(MODE)) $(GCC.NASMFLAGS.DEF) $(GCC.NASMFLAGS.INC) $(NASMFLAGS)
GCC.NASMFLAGS.DEF = $(NASMFLAGS.DEF)
GCC.NASMFLAGS.INC = $(if $(DIR.INCLUDE.NASM),-I$(subst ;, -I,$(DIR.INCLUDE.NASM)))
GCC.NASMFLAGS.SHARED ?= -DPIC

GCC.NASMFLAGS.release = -O2
GCC.NASMFLAGS.debug = -D__DEBUG__ -g

COMPILE.GCC.NASM = $(GCC.NASM) -o $@ $(strip $(GCC.NASMFLAGS) $1) $<

# Compilation rules ($1 = source file list, $2 = source file directories,
# $3 = module name, $4 = target name)
define MKCRULES.GCC.NASM
$(if $(filter %.asm,$1),$(foreach z,$2,
$(addsuffix $(if $(SHARED.$4),%.lo,%.o),$(addprefix $$(OUT),$z)): $(addsuffix %.asm,$z)
	$(if $V,,@echo COMPILE.GCC.NASM$(if $(SHARED.$4),.SHARED) $$< &&)$$(call COMPILE.GCC.NASM,$(NASMFLAGS.$3) $(NASMFLAGS.$4) $(if $(SHARED.$4),$(GCC.NASMFLAGS.SHARED)))))
endef

GCC.EXTRA.MKCRULES += $(MKCRULES.GCC.NASM)

endif
