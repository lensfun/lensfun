# Microsoft Visual C compiler suite definitions

.SUFFIXES: .c .cpp .obj .lib

MSVC.CC ?= cl.exe -c
MSVC.CFLAGS = -nologo -W2 -Gs -MD -EHsc -D_M_IX86=1 -D_X86_=1 \
    $(MSVC.CFLAGS.$(MODE)) $(MSVC.CFLAGS.DEF) $(MSVC.CFLAGS.INC) $(CFLAGS)
# -D_M_AMD64=1 -D_AMD64_=1
MSVC.CFLAGS.INC = $(if $(DIR.INCLUDE.CXX),-I$(subst ;, -I,$(DIR.INCLUDE.CXX)))

MSVC.CFLAGS.release = -Oxys
MSVC.CFLAGS.debug = -D__DEBUG__ -Oi -Z7

MSVC.CXX ?= cl.exe -c
MSVC.CXXFLAGS = $(MSVC.CFLAGS) $(CXXFLAGS)

MSVC.LD ?= link.exe
# Default subsystem in "console"; you can override it with LDFLAGS.your_module
MSVC.LDFLAGS = -nologo -opt:ref -opt:icf -force:multiple -incremental:no \
    -fullbuild -opt:nowin98 -subsystem:console $(MSVC.LDFLAGS.$(MODE)) $(LDFLAGS)
MSVC.LDFLAGS.LIBS = $(LDLIBS) advapi32.lib user32.lib
MSVC.LDFLAGS.DLL = -dll $(MSVC.LDFLAGS)

MSVC.LDFLAGS.release = -release -debug:none
MSVC.LDFLAGS.debug = -debug

# Windows makes difference between "console" and "windows" apps
MSVC.LDFLAGS.console = -subsystem:console
MSVC.LDFLAGS.windows = -subsystem:windows

MSVC.LINKLIB = $(if $(findstring $L,$1),,$(if $(findstring /,$1),$1,-l$1))

MSVC.AR ?= link.exe -lib
MSVC.ARFLAGS = -nologo -machine:$(ARCH)

MAKEDEP ?= makedep
MSVC.MDEP = makedep -D_WIN32 -D_MSC_VER=6
MSVC.MDEPFLAGS = -c -a -p'$$(OUT)' -o.obj $(MSVC.CFLAGS.DEF) $(MSVC.CFLAGS.INC)

# Check for target architecture and don't allow compiling for unsupported archs
ifeq ($(findstring /$(ARCH)/,/x86/),)
XFNAME.MSVC = $(error Only the X86 architecture is currently supported by the MSVC toolkit!)
else
# Translate application/library pseudo-name into an actual file name
XFNAME.MSVC = $(addprefix $$(OUT),\
    $(patsubst %$E,%.exe,\
    $(patsubst %$D,%.dll,\
    $(patsubst %$L,%.lib,$1))))
endif

MKDEPS.MSVC = \
    $(patsubst %.c,%.obj,\
    $(patsubst %.cpp,%.obj,\
    $(call MKDEPS.DEFAULT,$1)))

COMPILE.MSVC.CXX  = $(MSVC.CXX) -Fo$@ $(strip $(MSVC.CXXFLAGS) $1) $<
COMPILE.MSVC.CC   = $(MSVC.CC) -Fo$@ $(strip $(MSVC.CFLAGS) $1) $<

# Compilation rules ($1 = source file list, $2 = source file directories,
# $3 = module name, $4 = target name)
define MKCRULES.MSVC
$(if $(filter %.c,$1),$(foreach z,$2,
$(addsuffix %.obj,$(addprefix $$(OUT),$z)): $(addsuffix %.c,$z)
	$(if $V,,@echo COMPILE.MSVC.CC $$< &&)$$(call COMPILE.MSVC.CC,$(CFLAGS.$3) $(CFLAGS.$4) $(call .SYSLIBS,CFLAGS,$3,$4))))
$(if $(filter %.cpp,$1),$(foreach z,$2,
$(addsuffix %.obj,$(addprefix $$(OUT),$z)): $(addsuffix %.cpp,$z)
	$(if $V,,@echo COMPILE.MSVC.CXX $$< &&)$$(call COMPILE.MSVC.CXX,$(CXXFLAGS.$3) $(CXXFLAGS.$4) $(call .SYSLIBS,CXXFLAGS,$3,$4))))
endef

LINK.MSVC.AR = $(MSVC.AR) $(MSVC.ARFLAGS) -out:$@ $^
LINK.MSVC.EXEC = $(MSVC.LD) -out:$@ $(MSVC.LDFLAGS) $1 $^ $(MSVC.LDFLAGS.LIBS) $2
LINK.MSVC.DLL = $(MSVC.LD) -out:$@ $(MSVC.LDFLAGS.DLL) $1 $(filter-out %.def,$^) \
    $(if $(filter %.def,$^),-def:$(filter %.def,$^)) $(MSVC.LDFLAGS.LIBS) $2

# Linking rules ($1 = target, $2 = dependency list, $3 = module name,
# $4 = target name)
define MKLRULES.MSVC
$(if $(filter %.lib,$1),\
$(filter %.lib,$1): $2
	$(if $V,,@echo LINK.MSVC.AR $$@ &&)$$(LINK.MSVC.AR))
$(if $(filter-out %.lib,$1),$(filter-out %.lib %.nls,$1): $2
	$(if $V,,@echo LINK.MSVC.EXEC $$@ &&)$$(call LINK.MSVC.EXEC,$(LDFLAGS.$3) $(LDFLAGS.$4) $(call .SYSLIBS,LDLIBS,$3,$4),$(foreach z,$(LIBS.$3) $(LIBS.$4),$(call MSVC.LINKLIB,$z))))
endef

define MAKEDEP.MSVC
	$(call RM,$@)
	$(if $(filter %.c,$^),$(MSVC.MDEP) $(strip $(MSVC.MDEPFLAGS) $(filter -D%,$1) $(filter -I%,$1)) -f $@ $(filter %.c,$^))
	$(if $(filter %.cpp,$^),$(MSVC.MDEP) $(strip $(MSVC.MDEPFLAGS) $(filter -D%,$2) $(filter -I%,$2)) -f $@ $(filter %.cpp,$^))
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.MSVC
$1: $(MAKEDEP_DEP) $2
	$(if $V,,@echo MAKEDEP.MSVC $$@ &&)$$(call MAKEDEP.MSVC,$(subst $(COMMA),$$(COMMA),$(CFLAGS.$3) $(CFLAGS.$4) $(CFLAGS)),$(subst $(COMMA),$$(COMMA),-D__cplusplus $(CXXFLAGS.$3) $(CXXFLAGS.$4) $(CXXFLAGS)))
endef
