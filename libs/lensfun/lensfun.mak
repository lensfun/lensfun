LIBS += lensfun
DESCRIPTION.lensfun = Photographic lens database library
DIR.INCLUDE.CXX += ;include/lensfun
TARGETS.lensfun = lensfun$L
SRC.lensfun$L := $(wildcard libs/lensfun/*.cpp)
SHARED.lensfun$L = $(if $(SHAREDLIBS),$(CONF_VERSION))
SYSLIBS.lensfun = GLIB_20
