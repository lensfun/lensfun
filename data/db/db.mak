DATA += lensdb
DESCRIPTION.lensdb = Lens database files

install-lensdb: $(wildcard data/db/*.xml)
	@$(if $V,,@echo INSTALL $^ &&)$(call INSTALL,$^,$(CONF_DATADIR),0644)
