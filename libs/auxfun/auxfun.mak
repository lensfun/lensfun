LIBS += auxfun
DESCRIPTION.auxfun = Auxiliary library for test programs
DIR.INCLUDE.C += :include/auxfun
TARGETS.auxfun = auxfun$L
SRC.auxfun$L := $(wildcard libs/auxfun/*.cpp)
SYSLIBS.auxfun$L = LIBPNG
