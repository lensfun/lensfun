# Rules for handling arbitrary data files

XFNAME.DATA = $1
MKDEPS.DATA = $1

# Install rules ($1 = module name, $2 = unexpanded target file,
# $3 = full target file name)
define MKIRULES.DATA

	$(if $V,,@echo INSTALL $3 to $(call .INSTDIR,$1,$2,DATA,$(CONF_DATADIR)) &&)\
	$$(call INSTALL,$3,$(call .INSTDIR,$1,$2,DATA,$(CONF_DATADIR)),$(call .INSTMODE,$1,$2,0644))
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.DATA
.PHONY: $1
endef
