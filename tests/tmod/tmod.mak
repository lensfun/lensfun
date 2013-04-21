TESTS += tmod
DESCRIPTION.tmod = Test image modifier functions
TARGETS.tmod = tmod$E
SRC.tmod$E = $(wildcard tests/tmod/*.cpp)

LIBS.tmod = lensfun$L auxfun$L
ifdef NEED_REGEX
LIBS.tmod += regex$L
endif
SYSLIBS.tmod = GLIB_20 LIBPNG
