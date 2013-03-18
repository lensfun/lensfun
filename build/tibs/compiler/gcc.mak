# GNU compiler suite definitions

.SUFFIXES: .c .cpp .o .lo .a .pc .pc.in

GCC.CC ?= gcc -c
GCC.CFLAGS = -pipe -Wall \
    $(GCC.CFLAGS.$(MODE)) $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC) $(CFLAGS)
GCC.CFLAGS.DEF = $(CFLAGS.DEF)
GCC.CFLAGS.INC = $(if $(DIR.INCLUDE.C),-I$(subst :, -I,$(DIR.INCLUDE.C)))
ifneq ($(TARGET),windows)
GCC.CFLAGS.SHARED ?= -fPIC
endif

GCC.CFLAGS.release = -s -O3 -fomit-frame-pointer -funroll-loops
GCC.CFLAGS.debug = -D__DEBUG__ -g

ifeq ($(ARCH),arm)
# ARM gcc is known to generate wrong code with -O3
GCC.CFLAGS.release := $(subst -O3,-O2,$(GCC.CFLAGS.release))
endif

GCC.CXX ?= g++ -c
GCC.CXXFLAGS = $(GCC.CFLAGS) $(CXXFLAGS)
GCC.CXXFLAGS.SHARED = $(GCC.CFLAGS.SHARED)

GCC.CPP = gcc -E
GCC.CPPFLAGS = -pipe -x c-header $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC)

GCC.LD ?= g++
GCC.LDFLAGS = $(GCC.LDFLAGS.$(MODE))
GCC.LDFLAGS.LIBS = $(LDLIBS) -lm

GCC.LDFLAGS.debug = -gdwarf-2 -g3

GCC.LINKLIB = $(if $(findstring $L,$1),,$(if $(findstring /,$1),$1,-l$1))

GCC.MDEP = $(or $(MAKEDEP),makedep)
GCC.MDEPFLAGS = -c -a -p'$$(OUT)' $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC) $(MDEPFLAGS)

GCC.AR ?= ar
GCC.ARFLAGS = crs

# Translate application/library pseudo-name into an actual file name
XFNAME.GCC = $(addprefix $$(OUT),\
    $(patsubst %$E,%$(_EX),\
    $(if $(findstring $L,$1),\
        $(if $(SHARED.$1),\
            $(addprefix $(SO_),$(patsubst %$L,%$(_SO),$1)),\
            $(addprefix lib,$(patsubst %$L,%.a,$1))\
        ),\
    $1)\
))

MKDEPS.GCC = \
    $(patsubst %.c,$(if $(SHARED.$2),%.lo,%.o),\
    $(patsubst %.cpp,$(if $(SHARED.$2),%.lo,%.o),\
    $(patsubst %.asm,$(if $(SHARED.$2),%.lo,%.o),\
    $(patsubst %.S,$(if $(SHARED.$2),%.lo,%.o),\
    $(call MKDEPS.DEFAULT,$1)))))

COMPILE.GCC.CXX  = $(GCC.CXX) -o $@ $(strip $(GCC.CXXFLAGS) $1) $<
COMPILE.GCC.CC   = $(GCC.CC) -o $@ $(strip $(GCC.CFLAGS) $1) $<

# Compilation rules ($1 = source file list, $2 = source file directories,
# $3 = module name, $4 = target name)
define MKCRULES.GCC
$(if $(filter %.c,$1),$(foreach z,$2,
$(addsuffix $(if $(SHARED.$4),%.lo,%.o),$(addprefix $$(OUT),$z)): $(addsuffix %.c,$z)
	$(if $V,,@echo COMPILE.GCC.CC$(if $(SHARED.$4),.SHARED) $$< &&)$$(call COMPILE.GCC.CC,$(CFLAGS.$3) $(CFLAGS.$4) $(if $(SHARED.$4),$(GCC.CFLAGS.SHARED)) $(call .SYSLIBS,CFLAGS,$3,$4))))
$(if $(filter %.cpp,$1),$(foreach z,$2,
$(addsuffix $(if $(SHARED.$4),%.lo,%.o),$(addprefix $$(OUT),$z)): $(addsuffix %.cpp,$z)
	$(if $V,,@echo COMPILE.GCC.CXX$(if $(SHARED.$4),.SHARED) $$< &&)$$(call COMPILE.GCC.CXX,$(CXXFLAGS.$3) $(CXXFLAGS.$4) $(if $(SHARED.$4),$(GCC.CXXFLAGS.SHARED)) $(call .SYSLIBS,CFLAGS,$3,$4))))
$(GCC.EXTRA.MKCRULES)
endef

LINK.GCC.AR = $(GCC.AR) $(GCC.ARFLAGS) $@ $^
LINK.GCC.EXEC = $(GCC.LD) -o $@ $(GCC.LDFLAGS) $(LDFLAGS) $1 $^ $(GCC.LDFLAGS.LIBS) $(LDFLAGS.LIBS) $2
define LINK.GCC.SO.VER
	$(GCC.LD) -o $@.$(SHARED.$3) $(call GCC.LDFLAGS.SHARED,$(notdir $@).$(basename $(basename $(basename $(SHARED.$3)))),$(dir $@)) $(GCC.LDFLAGS) $(LDFLAGS) $1 $^ $(GCC.LDFLAGS.LIBS) $(LDFLAGS.LIBS) $2
	ln -fs $(notdir $@.$(SHARED.$3)) $@.$(basename $(basename $(basename $(SHARED.$3))))
	ln -fs $(notdir $@.$(basename $(basename $(basename $(SHARED.$3))))) $@
endef
define LINK.GCC.SO.NOVER
	$(GCC.LD) -o $@ $(call GCC.LDFLAGS.SHARED,$(notdir $@),$(dir $@)) $(GCC.LDFLAGS) $(LDFLAGS) $1 $^ $(GCC.LDFLAGS.LIBS) $(LDFLAGS.LIBS) $2
endef
# If SHARED.$3 is a valid version number and target platform supports versioned
# shared libraries, invoke LINK.GCC.SO.VER.
# Otherwise create a non-versioned variant of the shared library
define LINK.GCC.SO
$(call LINK.GCC.SO.$(if $(call VALID_VERSION,$(SHARED.$3)),,NO)VER,$1,$2,$3)
endef

# Linking rules ($1 = target full filename, $2 = dependency list,
# $3 = module name, $4 = unexpanded target name)
define MKLRULES.GCC
$1: $2\
$(if $(findstring $L,$4),\
$(if $(SHARED.$4),
	$(if $V,,@echo LINK.GCC.SHARED $$@ &&)$$(call LINK.GCC.SO,$(subst $(COMMA),$$(COMMA),$(LDFLAGS.$3) $(LDFLAGS.$4)),$(call .SYSLIBS,LDLIBS,$3,$4) $(foreach z,$(LIBS.$3) $(LIBS.$4),$(call GCC.LINKLIB,$z)),$4),
	$(if $V,,@echo LINK.GCC.AR $$@ &&)$$(LINK.GCC.AR)))\
$(if $(findstring $E,$4),
	$(if $V,,@echo LINK.GCC.EXEC $$@ &&)$$(call LINK.GCC.EXEC,$(subst $(COMMA),$$(COMMA),$(LDFLAGS.$3) $(LDFLAGS.$4)),$(call .SYSLIBS,LDLIBS,$3,$4) $(foreach z,$(LIBS.$3) $(LIBS.$4),$(call GCC.LINKLIB,$z))))
$(GCC.EXTRA.MKLRULES)
endef

# Install rules ($1 = module name, $2 = unexpanded target file,
# $3 = full target file name)
define MKIRULES.GCC
$(if $(findstring $L,$2),\
$(foreach _,$3 $(if $(call VALID_VERSION,$(SHARED.$2)),$3.$(basename $(basename $(basename $(SHARED.$2)))) $3.$(SHARED.$2)),
	$(if $V,,@echo INSTALL $_ to $(call .INSTDIR,$1,$2,LIB,$(CONF_LIBDIR)) &&)\
	$$(call INSTALL,$_,$(call .INSTDIR,$1,$2,LIB,$(CONF_LIBDIR)),$(call .INSTMODE,$1,$2,$(if $(SHARED.$2),0755,0644)))))\
$(if $(findstring $E,$2),
	$(if $V,,@echo INSTALL $3 to $(call .INSTDIR,$1,$2,BIN,$(CONF_BINDIR)) &&)\
	$$(call INSTALL,$3,$(call .INSTDIR,$1,$2,BIN,$(CONF_BINDIR)),$(call .INSTMODE,$1,$2,0755)))\
$(if $(INSTALL.INCLUDE.$2),
	$(if $V,,@echo INSTALL $(INSTALL.INCLUDE.$2) to $(call .INSTDIR,$1,$2,INCLUDE,$(CONF_INCLUDEDIR)) &&)\
	$$(call INSTALL,$(INSTALL.INCLUDE.$2),$(call .INSTDIR,$1,$2,INCLUDE,$(CONF_INCLUDEDIR)),$(call .INSTMODE,$1,$2,0644)))
endef

define MAKEDEP.GCC
	$(call RM,$@)
	$(if $(filter %.c,$^),$(GCC.MDEP) $(strip $(GCC.MDEPFLAGS) $(filter -D%,$1) $(filter -I%,$1)) -o$3 -f $@ $(filter %.c,$^))
	$(if $(filter %.cpp,$^),$(GCC.MDEP) $(strip $(GCC.MDEPFLAGS) $(filter -D%,$2) $(filter -I%,$2)) -o$3 -f $@ $(filter %.cpp,$^))
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.GCC
$1: $(MAKEDEP_DEP) $2
	$(if $V,,@echo MAKEDEP.GCC $$@ &&)$$(call MAKEDEP.GCC,$(subst $(COMMA),$$(COMMA),$(CFLAGS.$3) $(CFLAGS.$4) $(CFLAGS)),$(subst $(COMMA),$$(COMMA),-D__cplusplus $(CXXFLAGS.$3) $(CXXFLAGS.$4) $(CXXFLAGS)),$(if $(SHARED.$4),.lo,.o))
$(GCC.EXTRA.MKDRULES)
endef
