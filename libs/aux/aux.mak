LIBS += aux
DESCRIPTION.aux = Auxiliary library for test programs
NO-INSTALL += aux
DIR.INCLUDE.CXX += ;include/aux
TARGETS.aux = aux$L
SRC.aux$L := $(wildcard libs/aux/*.cpp)
SYSLIBS.aux$L = LIBPNG
