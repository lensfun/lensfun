TESTS += tfun
DESCRIPTION.tfun = Test lensfun library by listing various things from database
TARGETS.tfun = tfun$E
SRC.tfun$E = $(wildcard tests/tfun/*.cpp)

LIBS.tfun = lensfun$L
ifdef NEED_REGEX
LIBS.tfun += regex$L
endif
SYSLIBS.tfun = GLIB_20
