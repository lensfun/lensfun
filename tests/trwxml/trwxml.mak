TESTS += trwxml
DESCRIPTION.trwxml = Test read/write XML functions
TARGETS.trwxml = trwxml$E
SRC.trwxml$E = $(wildcard tests/trwxml/*.cpp)

LIBS.trwxml = lensfun$L
ifdef NEED_REGEX
LIBS.trwxml += regex$L
endif
SYSLIBS.trwxml = GLIB_20
