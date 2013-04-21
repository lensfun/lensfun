ifdef NEED_REGEX

LIBS += regex
DESCRIPTION.regex = The TRE regular expression library
DIR.INCLUDE.C += :include/regex
TARGETS.regex = regex$L
SRC.regex$L := $(wildcard libs/regex/*.c)
CFLAGS.regex$L = -DHAVE_SYS_TYPES_H -DTRE_REGEX_T_FIELD=value \
    -DTRE_VERSION="0" -Dinline=
INSTALL.TARGETS += regex

ifeq ($(TOOLKIT),GCC)
CFLAGS.regex$L += -Wno-unused-function
endif

endif
