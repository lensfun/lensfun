DOCS += manual
DESCRIPTION.manual = The library end user documentation
TARGETS.manual = manual$D
TOOLKIT.manual = DOXYGEN
INSTALL.TARGETS += manual

SRC.manual$D = docs/manual-doc.conf include/lensfun/lensfun.h docs/manual-main.txt

#------------------------------------------------------------------------------#

DOCS += readme
DESCRIPTION.readme = Readme file and licenses
TARGETS.readme = README docs/lgpl-3.0.txt docs/gpl-3.0.txt docs/cc-by-sa-3.0.txt
TOOLKIT.readme = DATA
INSTALL.TARGETS += readme
INSTDIR.readme = $(CONF_DOCDIR)
