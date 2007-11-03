DOCS += manual
DESCRIPTION.manual = The library end user documentation
TARGETS.manual = manual$D
TOOLKIT.manual = DOXYGEN

SRC.manual$D = docs/manual-doc.conf include/lensfun/lensfun.h docs/manual-main.txt


DOCS += readme
DESCRIPTION.readme = Readme file and licenses

install-readme: README lgpl-3.0.txt gpl-3.0.txt
	@$(if $V,,@echo INSTALL $^ &&)$(call INSTALL,$^,$(CONF_DOCSDIR),0644)
