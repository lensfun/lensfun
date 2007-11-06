DOCS += manual
DESCRIPTION.manual = The library end user documentation
TARGETS.manual = manual$D
TOOLKIT.manual = DOXYGEN
INSTALL.TARGETS += manual

SRC.manual$D = docs/manual-doc.conf include/lensfun/lensfun.h docs/manual-main.txt

#------------------------------------------------------------------------------#

DOCS += readme
DESCRIPTION.readme = Readme file and licenses
INSTALL.TARGETS += readme
SRC.readme = README docs/lgpl-3.0.txt docs/gpl-3.0.txt

install-readme: $(SRC.readme)
	@$(if $V,,@echo INSTALL $(SRC.readme) &&)$(call INSTALL,$(SRC.readme),$(CONF_DOCSDIR),0644)
