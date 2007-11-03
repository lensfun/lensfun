TESTS += trwxml
DESCRIPTION.trwxml = Test read/write XML functions
TARGETS.trwxml = trwxml$E
SRC.trwxml$E = $(wildcard tests/trwxml/*.cpp)

LIBS.trwxml = lensfun$L
SYSLIBS.trwxml = GLIB_20
