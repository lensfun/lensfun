# Settings for building on the POSIX platform

SHELL := /bin/sh

# $1 - file, $2 - target dir, $3 - mode
define SINGLE.INSTALL
	if [ ! -d $2 ]; then install -m 0755 -d $2; fi
	if [ -L $1 ]; then cp -P $1 $2$(notdir $1); else install -m $3 $1 $2$(notdir $1); fi
endef

# $1 - directory, $2 - target dir, $3 - mode for files
define SINGLE.INSTALLDIR
	if [ ! -d $2 ]; then install -m 0755 -d $2; else rm -rf $2$(notdir $(1:/=)); fi
	cp -drP $1 $2$(notdir $(1:/=))
	find $2$(notdir $(1:/=)) -type f -print0 | xargs -0 chmod $3
endef
