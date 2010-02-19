# Rules for building pkgconfig files from templates

ifdef CONF_LIBDIR
# Try to guess here if target supports pkgconfig
# - Any POSIX platform
# - Cross-compiling on a POSIX target for WINDOWS (mingw32)
ifneq ($(findstring /posix/,/$(TARGET)/)$(findstring /posix-windows/,/$(HOST)-$(TARGET)/),)

XFNAME.PKGCONFIG = $(addprefix $$(OUT),$1)
MKDEPS.PKGCONFIG = $1

define BUILD.PKGCONFIG
	sed -e "s,@CONF_PACKAGE@,$(CONF_PACKAGE),g" \
	    -e "s,@CONF_VERSION@,$(CONF_VERSION),g" \
	    -e "s,@CONF_PREFIX@,$(CONF_PREFIX),g" \
	    -e "s,@CONF_BINDIR@,$(CONF_BINDIR),g" \
	    -e "s,@CONF_SYSCONFDIR@,$(CONF_SYSCONFDIR),g" \
	    -e "s,@CONF_DATADIR@,$(CONF_DATADIR),g" \
	    -e "s,@CONF_LIBDIR@,$(CONF_LIBDIR),g" \
	    -e "s,@CONF_INCLUDEDIR@,$(CONF_INCLUDEDIR),g" \
	    -e "s,@CONF_DOCDIR@,$(CONF_DOCDIR),g" \
	    -e "s,@CONF_LIBEXECDIR@,$(CONF_LIBEXECDIR),g" \
	    $(PKGCONFIG.SED.$1) $(PKGCONFIG.SED.$2) \
	    $< >$@
endef

# Linking rules ($1 = target full filename, $2 = dependency list,
# $3 = module name, $4 = unexpanded target name)
define MKLRULES.PKGCONFIG
$1: $2
	$(if $V,,@echo BUILD.PKGCONFIG $$@ &&)$$(call BUILD.PKGCONFIG,$1,$3)
endef

# Install rules ($1 = module name, $2 = unexpanded target file,
# $3 = full target file name)
define MKIRULES.PKGCONFIG

	$(if $V,,@echo INSTALL $3 to $(call .INSTDIR,$1,$2,PKGCONFIG,$(CONF_LIBDIR)pkgconfig) &&)\
	$$(call INSTALL,$3,$(call .INSTDIR,$1,$2,PKGCONFIG,$(CONF_LIBDIR)pkgconfig),0644)
endef

# Dependency rules ($1 = dependency file, $2 = source file list,
# $3 = module name, $4 = target name)
define MKDRULES.PKGCONFIG
.PHONY: $1
endef

endif
endif
