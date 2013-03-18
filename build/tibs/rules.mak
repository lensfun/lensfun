# The Instant Build System: core
# This makefile does the following:
#  - Includes any submakefiles (.mak) it can find
#  - For every module defined in submakefiles build and evaluate a number
#    of makefile rules to build, install and clean them

DIR.TIBS ?= /usr/share/tibs

# Give sane defaults to installation directories, if not defined by user
CONF_PREFIX ?= /usr/local/
CONF_BINDIR ?= $(CONF_PREFIX)bin/
CONF_SYSCONFDIR ?= $(CONF_PREFIX)etc/
CONF_DATADIR ?= $(CONF_PREFIX)share/$(CONF_PACKAGE)/
CONF_LIBDIR ?= $(CONF_PREFIX)lib$(if $(findstring /$(ARCH)/,/x86_64/),64)/
CONF_INCLUDEDIR ?= $(CONF_PREFIX)include/
CONF_DOCDIR ?= $(CONF_PREFIX)share/doc/$(CONF_PACKAGE)-$(CONF_VERSION)/
CONF_LIBEXECDIR ?= $(CONF_PREFIX)libexec/$(CONF_PACKAGE)/

.PHONY: default help all dep clean dist distclean cleandep cleangen install
default: showhelp

# Forget all the built-in exotic suffixes
.SUFFIXES:
# Declare only actually used suffixes
.SUFFIXES: .dep

# Default groups of build targets
GROUPS := LIBS APPS DRIVERS TOOLS TESTS DATA DOCS $(GROUPS)
LIBS.dir = libs
LIBS.desc = Project libraries
APPS.dir = apps
APPS.desc = Application programs
TOOLS.dir = tools
TOOLS.desc = Miscelaneous tools
TESTS.dir = tests
TESTS.desc = Small programs for testing
DATA.dir = data
DATA.desc = Project data files
DOCS.dir = docs
DOCS.desc = Documentation files
DRIVERS.dir = drivers
DRIVERS.desc = Drivers

# A newline
define NL


endef
# A comma
COMMA=,
# A empty
EMPTY=
# A space
SPACE=$(EMPTY) $(EMPTY)
# Useful macro to strip extension from a file
NOSUFFIX=$(subst $(suffix $1),,$1)
# This function should print its argument to the terminal
SAY = @echo '$(subst ','\'',$1)'
# Create a possibly multi-level directory
MKDIR = mkdir -p $1
# How to remove one or more files without stupid questions
RM = rm -f $1
# Recursively remove files by mask $2 in directory $1
RRM = find $1 -name $2 | xargs rm -rf
# How to remove a directory recursively
RMDIR = rm -rf $1
# How to copy a file
CP = cp $1 $2
# How to set the last modification date on a file to current
TOUCH = touch $1
# Check if version is valid (e.g. matches NUMBER.NUMBER(.NUMBER(.NUMBER)?)?) on Unices, not valid on others
VALID_VERSION = $(if $1,$(if $(findstring /$(TARGET)/,/posix/mac/),$(if $(shell echo $1 | sed -e 's/[0-9 ]\+\.[0-9 ]\+\(\.[0-9 ]\+\(\.[0-9 ]\+\)\?\+\)\?//'),,T)))
# How to install one or several files
INSTALL = $(foreach _,$1,$(call SINGLE.INSTALL,$_,$2,$3)$(NL))
# How to install one or several directories
INSTALLDIR = $(foreach _,$1,$(call SINGLE.INSTALLDIR,$_,$2,$3)$(NL))
# Use DESTDIR as an alias for INSTALL_PREFIX 
INSTALL_PREFIX ?= $(DESTDIR)

# Pseudo-file-extensions (for cross-platform filenames)
# For executable files:
E = @exe@
# Library files (either static or dynamic, depends on SHARED.$(libname))
L = @lib@
# Pseudo-extension for documentation modules
D = @doc@

# Base directory for output files
OUTBASE=out
# The actual directo+ry for output files
OUT = $(OUTBASE)/$(TARGET).$(ARCH)/$(MODE)/
# A list of directories for output files
OUTDIRS := $(OUT)

# Default include directories
DIR.INCLUDE.C ?= include
DIR.INCLUDE.NASM ?= include/asm

# Check if MODE has an allowed value
ifeq ($(findstring /$(MODE)/,/release/debug/),)
$(error Incorrect value for the MODE variable: $(MODE))
endif

# Include host-dependent definitions
SUBMAKEFILES += $(wildcard $(DIR.TIBS)/host/$(HOST).mak)
# Include target-dependent definitions
SUBMAKEFILES += $(wildcard $(DIR.TIBS)/target/$(TARGET).mak)

# Expand a target name into an actual file name ($1 - source file, $2 - target)
MKDEPS.DEFAULT = $(addprefix $$(OUT),$1)
# Call the appropiate MKDEPS macro ($1 - toolkit, $2 - source file, $3 - target)
MKDEPS = $(call MKDEPS.$(if $(MKDEPS.$1),$1,DEFAULT),$2,$3)
# Includue the definitions for all known compilers
SUBMAKEFILES += $(wildcard $(DIR.TIBS)/compiler/*.mak)
# And also submakefiles for all modules
SUBMAKEFILES += $(foreach x,$(GROUPS),$(wildcard $($x.dir)/*.mak $($x.dir)/*/*.mak))
# Now actually parse the submakefiles
include $(SUBMAKEFILES)

# Separator line -- $-, that is :-)
-=------------------------------------------------------------------------

# Now disable all projects, which require non-existent SYSLIBS
CHECK.SYSLIBS = $(if $(strip $(foreach y,$(foreach x,$1 $(TARGETS.$1),$(SYSLIBS.$x)),$(if $(HAVE_$y),,F))),,1)
CHECK.PACKAGE = $(if $(strip $(call CHECK.SYSLIBS,$1)),$1)
$(foreach x,$(GROUPS),$(eval $x=$(foreach y,$x,$(foreach z,$($y),$(call CHECK.PACKAGE,$z)))))

# Show help for one group of targets:
# $1 - Group base name,
define SAYHELP
  $(if $(firstword $($1)),
	$(call SAY,$($1.dir) - $($1.desc))
	$(foreach y,$($1),$(call SAY,    $y - $(DESCRIPTION.$y))$(NL))
  )
endef

define GENHELP
	$(call SAY,dep - Generate all dependency files)
	$(call SAY,all - Build the whole project)
	$(call SAY,clean - Clean all object$(COMMA) executable and dependency files)
	$(call SAY,distclean - Clean everything not from distribution archive (BEWARE!))
	$(call SAY,cleandep - Clean all dependency files)
	$(if $(GENFILES),$(call SAY,cleangen - Clean all autogenerated source files))
	$(call SAY,install - Build and install anything installable from this project)
	$(foreach x,$(filter-out $(INSTALL.EXCLUDE),$(INSTALL.TARGETS)),\
		$(if $(DESCRIPTION.$x),\
			$(call SAY,    install-$x - Install $(DESCRIPTION.$x))$(NL)))
endef

# Show help by default
showhelp:
	$(call SAY,$-)
	$(call SAY,You must choose one of the following targets for building:)
	$(call SAY,$-)
	$(GENHELP)
	$(foreach x,$(GROUPS),$(call SAYHELP,$x))
	$(call SAY,$-)

# Generate dependency files
dep depend:
ifneq ($(AUTODEP),1)
	@$(MAKE) --no-print-directory dep AUTODEP=1
else
	$(call SAY,done)
endif

# Make the 'all' target which builds all groups
all: $(foreach x,$(GROUPS),$($x.dir))
# Make separate targets to build whole groups
$(foreach x,$(GROUPS),$(eval $($x.dir): $($x)))
# Make separate targets to install whole groups
$(foreach x,$(GROUPS),$(eval install-$($x.dir): install-$($x)))

# The name of the toolkit to build target $2 from module $1
.TKNAME = $(or $(TOOLKIT.$2),$(TOOLKIT.$1),$(TOOLKIT))
# Extract just the unique directory names from a list of source files
.DIRLIST = $(filter $(firstword $(dir $1))%,$(sort $(dir $1)))
# Return the given $1 flags (CFLAGS/CXXFLAGS/LDLIBS) for the module $2/target $3
.SYSLIBS = $(foreach 4,$(SYSLIBS.$2) $(SYSLIBS.$3),$(subst $(COMMA),$$(COMMA),$($1.$4)))
# Return installation directory for module $1 target $2, type $3, default value $4.
# Installation directory may be overriden by defining a variable named either
# INSTDIR.$(type).$(target) or INSTDIR.$(type).$(module) or INSTDIR.$(target)
# or INSTDIR.$(module) (listed in decreasing priority order) depending on your needs.
# 'type' can be one of "BIN", "LIB", "INCLUDE", "DOC", "PKGCONFIG" and a few others
# (look in the .mak file for the respective toolkit).
.INSTDIR = $(INSTALL_PREFIX)$(or $(INSTDIR.$3.$2),$(INSTDIR.$3.$1),$(INSTDIR.$2),$(INSTDIR.$1),$4)
# Return the target file mode for module $1 target $2, default value $3
.INSTMODE = $(or $(INSTMODE.$1),$(INSTMODE.$2),$3)

# Okay, here goes the horror part :)
# $1 - module name
define MODULE.BUILDRULES
.PHONY: $1
$1: outdirs $(foreach 2,$(TARGETS.$1),$(call XFNAME.$(.TKNAME),$2))
$(foreach 2,$(TARGETS.$1),$(if $(SRC.$2),
ifeq ($(AUTODEP),1)
$(call MKDRULES.$(.TKNAME),$(OUT)deps/$2.dep,$(OUT)deps/.dir $(SRC.$1) $(SRC.$2),$1,$2)
endif
-include $(OUT)deps/$2.dep
DEPS.$2 = $(call MKDEPS,$(.TKNAME),$(SRC.$1) $(SRC.$2),$2) $(foreach 3,$(filter %$L,$(LIBS.$1) $(LIBS.$2)),$(call XFNAME.$(.TKNAME),$3))
OUTDIRS += $$(dir $$(DEPS.$2))
$(call MKCRULES.$(.TKNAME),$(SRC.$1) $(SRC.$2),$(call .DIRLIST,$(SRC.$1) $(SRC.$2)),$1,$2)
$(call MKLRULES.$(.TKNAME),$(call XFNAME.$(.TKNAME),$2),$$(DEPS.$2),$1,$2)
))
endef

# Evaluate build rules for all modules
$(eval $(foreach x,$(foreach y,$(GROUPS) AUX,$($y)),$(call MODULE.BUILDRULES,$x)))

# Generate the installation rules for given module
# $1 - module name
define MODULE.INSTRULES
.PHONY: install-$1
install: install-$1
install-$1: outdirs $(foreach 2,$(TARGETS.$1),$(call XFNAME.$(.TKNAME),$2))\
$(foreach 2,$(TARGETS.$1),$(call MKIRULES.$(.TKNAME),$1,$2,$(call XFNAME.$(.TKNAME),$2)))

endef

# Evaluate install rules for installable modules
$(eval $(foreach x,$(filter-out $(INSTALL.EXCLUDE),$(INSTALL.TARGETS)),$(call MODULE.INSTRULES,$x)))

# Debug target to look at autogenerated rules
showrules::
	@echo -e '$(subst $(NL),\n,$(foreach x,$(foreach y,$(GROUPS) AUX,$($y)),$(call MODULE.BUILDRULES,$x)))'
	@echo -e '$(subst $(NL),\n,$(foreach x,$(filter-out $(INSTALL.EXCLUDE),$(INSTALL.TARGETS)),$(call MODULE.INSTRULES,$x)))'

OUTDIRS := $(sort $(OUTDIRS))

# Clean the currently configured output directory and autogenerated files
clean:: cleangen
	$(call RMDIR,$(OUT))

# Clean all autogenerated source files
cleangen::
	$(if $(GENFILES),$(call RM,$(GENFILES)))

# Clean all dependency files
cleandep::
	$(call RMDIR,$(OUT)deps/)

# How to create all output directories at once
outdirs: $(OUTDIRS)
$(OUTDIRS):
	$(call MKDIR,$@)

# This flag file is required so that .dep file do not depend on the $(OUT)deps/
# directory, but on this file. If they depend on the directory, all .dep files
# will be re-created every time the timestamp on the deps directory change
# (that is, every time at least one .dep file is re-created all other files
# are rebuilt as well).
$(OUT)deps/.dir:
	$(call MKDIR,$(OUT)deps)
	$(call TOUCH,$@)

# Build a distribution archive
DIST = $(CONF_PACKAGE)-$(CONF_VERSION).tar.bz2
DISTEXTRA += $(wildcard GNUmakefile configure)
# Take care that $($x.dir) are targets, so we'll append a /
DISTFILES = $(wildcard $(foreach x,$(GROUPS),$($x.dir)/)) $(DISTEXTRA)
DISTEXCLUDEPATH=$(if $(patsubst %/,,$1),! -path '$1',! -path '$(1:/=)' ! -path '$1*')

dist: $(DIST)

$(DIST): $(DISTFILES)
	$(call MKDIR,$(@:.tar.bz2=))
	find $^ -type f ! -path '*/.svn*' $(foreach x,$(GENFILES),$(call DISTEXCLUDEPATH,$x)) -print0 | \
	xargs -0 cp -a --parents --target-directory=$(@:.tar.bz2=)
	tar cjf $@ $(@:.tar.bz2=) --no-xattrs --no-anchored --numeric-owner --owner 0 --group 0
	$(call RMDIR,$(@:.tar.bz2=))

# Clean all non-distribution files
distclean: clean $(DISTFILES)
	$(call RMDIR,$(OUTBASE))
	find * ! -path . ! -path '*/.svn*' \
		$(foreach x,$(DISTFILES),$(call DISTEXCLUDEPATH,$x)) \
		-print0 | xargs -0 rm -rf
