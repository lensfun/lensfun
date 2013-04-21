# This is a example submakefile for a example "helloworld" application

# Add this module to the list of test programs
TESTS += example
# Add a module description to the help text
DESCRIPTION.example = This is an example program
# Which targets are the result of building this module
TARGETS.example = example$E
# The list of source files for the first target
SRC.example$E = $(wildcard tests/example/*.c)

# You can use specific compiler flags for just this module or even object files
CXXFLAGS.example = -DTEST
# The libraries this application depends on
LIBS.example = lensfun$L
ifdef NEED_REGEX
LIBS.example += regex$L
endif
# Link this example against these system libraries (detected by configure)
SYSLIBS.example = GLIB_20
