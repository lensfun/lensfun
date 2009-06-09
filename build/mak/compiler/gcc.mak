# GNU compiler suite definitions

.SUFFIXES: .c .cpp .o .a .pc .pc.in

GCC.CC ?= gcc -c
GCC.CFLAGS = -pipe -Wall \
    $(GCC.CFLAGS.$(MODE)) $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC) $(CFLAGS)
GCC.CFLAGS.DEF = $(CFLAGS.DEF)
GCC.CFLAGS.INC = $(if $(DIR.INCLUDE.CXX),-I$(subst ;, -I,$(DIR.INCLUDE.CXX)))
GCC.CFLAGS.SHARED ?= -fPIC

GCC.CFLAGS.release = -s -O3 -fomit-frame-pointer -funroll-loops
GCC.CFLAGS.debug = -D__DEBUG__ -g

GCC.CXX ?= g++ -c
GCC.CXXFLAGS = $(GCC.CFLAGS) $(CXXFLAGS)
GCC.CXXFLAGS.SHARED = $(GCC.CFLAGS.SHARED)

GCC.CPP = gcc -E
GCC.CPPFLAGS = -pipe -x c-header $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC)

GCC.LD ?= g++
GCC.LDFLAGS = $(GCC.LDFLAGS.$(MODE))
GCC.LDFLAGS.LIBS = $(LDLIBS) -lm
ifeq ($(TARGET),mac)
GCC.LDFLAGS.SHARED ?= $(GCC.CFLAGS.SHARED) -dynamiclib
else
GCC.LDFLAGS.SHARED ?= $(GCC.CFLAGS.SHARED) -shared
endif

GCC.LDFLAGS.release = -s
GCC.LDFLAGS.debug = -gdwarf-2 -g3

GCC.LINKLIB = $(if $(findstring $L,$1),,$(if $(findstring /,$1),$1,-l$1))

MAKEDEP ?= makedep
GCC.MDEP = $(MAKEDEP)
GCC.MDEPFLAGS = -c -a -p'$$(OUT)' $(GCC.CFLAGS.DEF) $(GCC.CFLAGS.INC) $(MDEPFLAGS)

GCC.AR = ar
GCC.ARFLAGS = crs

# Translate application/library pseudo-name into an actual file name
XFNAME.GCC = $(addprefix $$(OUT),\
    $(patsubst %$E,%,\
    $(if $(findstring $L,$1),\
        $(if $(SHARED.$1),\
            $(addprefix lib,$(patsubst %$L,%$(XSO),$1)),\
            $(addprefix lib,$(patsubst %$L,%.a,$1))\
        ),\
    $1)\
))

MKDEPS.GCC = \
    $(patsubst %.c,%.o,\
    $(patsubst %.cpp,%.o,\
    $(call MKDEPS.DEFAULT,$1)))

COMPILE.GCC.CXX  = $(GCC.CXX) -o $@ $(strip $(GCC.CXXFLAGS) $1) $<
COMPILE.GCC.CC   = $(GCC.CC) -o $@ $(strip $(GCC.CFLAGS) $1) $<

# Compilation rules ($1 = source file list, $2 = source file directories,
# $3 = module name, $4 = target name)
define MKCRULES.GCC
$(if $(filter %.c,$1),$(foreach z,$2,
$(addsuffix %.o,$(addprefix $$(OUT),$z)): $(addsuffix %.c,$z)
	$(if $V,,@echo COMPILE.GCC.CC $$< &&)$$(call COMPILE.GCC.CC,$(CFLAGS.$3) $(CFLAGS.$4) $(if $(SHARED.$4),$(GCC.CFLAGS.SHARED)) $(call .SYSLIBS,CFLAGS,$3,$4))))
$(if $(filter %.cpp,$1),$(foreach z,$2,
$(addsuffix %.o,$(addprefix $$(OUT),$z)): $(addsuffix %.cpp,$z)
	$(if $V,,@echo COMPILE.GCC.CXX $$< &&)$$(call COMPILE.GCC.CXX,$(CXXFLAGS.$3) $(CXXFLAGS.$4) $(if $(SHARED.$4),$(GCC.CXXFLAGS.SHARED)) $(call .SYSLIBS,CXXFLAGS,$3,$4))))
endef

LINK.GCC.AR = $(GCC.AR) $(GCC.ARFLAGS) $@ $^
LINK.GCC.EXEC = $(GCC.LD) -o $@ $(GCC.LDFLAGS) $(LDFLAGS) $1 $^ $(GCC.LDFLAGS.LIBS) $(LDFLAGS.LIBS) $2
define LINK.GCC.SO
	$(GCC.LD) -o $@.$(SHARED.$3) -Wl,"-soname=$(notdir $@).$(basename $(basename $(SHARED.$3)))" $(GCC.LDFLAGS.SHARED) $(GCC.LDFLAGS) $(LDFLAGS) $1 $^ $(GCC.LDFLAGS.LIBS) $(LDFLAGS.LIBS) $2
	ln -fs $(notdir $@.$(SHARED.$3)) $@.$(basename $(basename $(SHARED.$3)))
	ln -fs $(notdir $@.$(basename $(basename $(SHARED.$3)))) $@
endef

# Linking rules ($1 = target full filename, $2 = dependency list,
# $3 = module name, $4 = unexpanded target name)
define MKLRULES.GCC
$1: $2\
$(if $(findstring $L,$4),\
$(if $(SHARED.$4),
	$(if $V,,@echo LINK.GCC.SO $$@ &&)$$(call LINK.GCC.SO,$(LDFLAGS.$3) $(LDFLAGS.$4) $(call .SYSLIBS,LDLIBS,$3,$4),$(foreach z,$(LIBS.$3) $(LIBS.$4),$(call GCC.LINKLIB,$z)),$4),
	$(if $V,,@echo LINK.GCC.AR $$@ &&)$$(LINK.GCC.AR)))\
$(if $(findstring $E,$4),
	$(if $V,,@echo LINK.GCC.EXEC $$@ &&)$$(call LINK.GCC.EXEC,$(LDFLAGS.$3) $(LDFLAGS.$4) $(call .SYSLIBS,LDLIBS,$3,$4),$(foreach z,$(LIBS.$3) $(LIBS.$4),$(call GCC.LINKLIB,$z))))
endef

# Install rules ($1 = module name, $2 = unexpanded target file,
# $3 = full target file name)
define MKIRULES.GCC
$(if $(findstring $L,$2),\
$(foreach _,$3 $(if $(SHARED.$2),$3.$(SHARED.$2) $3.$(basename $(basename $(SHARED.$2)))),
	$(if $V,,@echo INSTALL $_ to $(call .INSTDIR,$1,$2,$(CONF_LIBDIR)) &&)\
	$$(call INSTALL,$_,$(call .INSTDIR,$1,$2,$(CONF_LIBDIR)),$(if $(SHARED.$2),0755,0644))))\
$(if $(findstring $E,$2),
	$(if $V,,@echo INSTALL $3 to $(call .INSTDIR,$1,$2,$(CONF_BINDIR)) &&)\
	$$(call INSTALL,$3,$(call .INSTDIR,$1,$2,$(CONF_BINDIR)),0755))\
$(if $(INSTALL.HEADERS.$2),
	$(if $V,,@echo INSTALL $(INSTALL.HEADERS.$2) to $(call .INSTDIR,$1,$2,$(CONF_INCLUDEDIR)) &&)\
	$$(call INSTALL,$(INSTALL.HEADERS.$2),$(call .INSTDIR,$1,$2,$(CONF_INCLUDEDIR)),0644))
endef

define MAKEDEP.GCC
	$(call RM,$@)
	$(if $(filter %.c,$^),$(GCC.MDEP) $(strip $(GCC.MDEPFLAGS) $(filter -D%,$1) $(filter -I%,$1)) -f $@ $(filter %.c,$^))
	$(if $(filter %.cpp,$^),$(GCC.MDEP) $(strip $(GCC.MDEPFLAGS) $(filter -D%,$2) $(filter -I%,$2)) -f $@ $(filter %.cpp,$^))
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.GCC
$1: $(MAKEDEP_DEP) $2
	$(if $V,,@echo MAKEDEP.GCC $$@ &&)$$(call MAKEDEP.GCC,$(subst $(COMMA),$$(COMMA),$(CFLAGS.$3) $(CFLAGS.$4) $(CFLAGS)),$(subst $(COMMA),$$(COMMA),-D__cplusplus $(CXXFLAGS.$3) $(CXXFLAGS.$4) $(CXXFLAGS)))
endef
