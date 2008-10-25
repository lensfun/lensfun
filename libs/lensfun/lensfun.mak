LIBS += lensfun
DESCRIPTION.lensfun = Photographic lens database library
DIR.INCLUDE.CXX += ;include/lensfun
TARGETS.lensfun = lensfun$L
SRC.lensfun$L := $(wildcard libs/lensfun/*.cpp libs/lensfun/$(TARGET)/*.cpp)
SHARED.lensfun$L = $(if $(SHAREDLIBS),$(CONF_VERSION))
LIBS.lensfun = $(TARGETS.regex)
SYSLIBS.lensfun = GLIB_20
INSTALL.TARGETS += lensfun
INSTALL.HEADERS.lensfun$L = include/lensfun/lensfun.h
# This is needed for regex.c
CFLAGS.lensfun$L = -DHAVE_CONFIG_H

# For Posix systems we also will build the pkgconfig file
ifeq ($(TARGET),posix)
AUX += lensfun-pc
TOOLKIT.lensfun-pc = PKGCONFIG
TARGETS.lensfun-pc = lensfun.pc
SRC.lensfun.pc := $(wildcard libs/lensfun/*.pc.in) config.mak
INSTALL.TARGETS += lensfun-pc
endif
