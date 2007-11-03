TESTS += tfun
DESCRIPTION.tfun = Test lensfun library by listing various things from database
TARGETS.tfun = tfun$E
SRC.tfun$E = $(wildcard tests/tfun/*.cpp)

LIBS.tfun = lensfun$L
SYSLIBS.tfun = GLIB_20
