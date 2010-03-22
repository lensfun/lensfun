# Settings for building on a Windows host

# Use cmd.exe as shell
SHELL := $(if $(ComSpec),$(ComSpec),$(COMSPEC))

# The SAY function must print its arguments to the terminal
SAY = @echo $(subst |,!,$1)
# This function should be able to create a multi-level directory
MKDIR = mkdir $1
# A function to remove one or more files
RM = $(foreach x,$1,del $(subst /,\,$x))
# Recursively remove files from directory $1 by mask $2
RRM = -del /q /s $(subst /,\,$1$2)

SINGLE.INSTALL=echo Installing files on Windows is not supported
SINGLE.INSTALLDIR=echo Installing directories on Windows is not supported
