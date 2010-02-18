# If there's no Doxygen installed, ignore generated docs
ifneq ($(DOXYGEN),)

# There's no "compilation" stage in doxygen
MKDEPS.DOXYGEN = $1
# Our target is a directory under $(OUT)docs with the name of module
XFNAME.DOXYGEN = $(addprefix $(OUT)docs/,$(subst $D,/,$(filter %$D,$1)))

# If macro DOXYGEN.$(MODULE) is defined, it will be invoked after doxygen.
# This is useful to copy additional LaTeX style files, pictures etc
# to target directory, or even invoking make to build LaTeX documentation.
# The arguments to the macro are:
# $1 - module name
# $2 - output directory
define BUILD.DOXYGEN
	$(call MKDIR,$2)
	sed -e 's,@CONF_VERSION@,$(CONF_VERSION),' \
	    -e 's,@OUT@,$2,' \
	    $1 | doxygen -
	$(if $(DOXYGEN.$3),$(call DOXYGEN.$3,$3,$2))
endef

# Linking rules ($1 = target, $2 = dependency list, $3 = module name,
# $4 = target name)
define MKLRULES.DOXYGEN
$1: $2
	$(if $V,,@echo BUILD.DOXYGEN $$@ &&)$$(call BUILD.DOXYGEN,$$(filter %.conf,$$^),$$@,$4)
endef

# Install rules ($1 = module name, $2 = unexpanded target file,
# $3 = full target file name)
define MKIRULES.DOXYGEN

	$(if $V,,@echo INSTALLDIR $3 to $(call .INSTDIR,$1,$2,DOC,$(CONF_DOCDIR)$1/) &&)\
	$$(call INSTALLDIR,$3,$(call .INSTDIR,$1,$2,DOC,$(CONF_DOCDIR)$1/),0644)
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.DOXYGEN
.PHONY: $1
endef

endif
