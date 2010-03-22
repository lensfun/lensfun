# Settings for building for the POSIX target

.SUFFIXES: .so

# OS-dependent extensions

# Shared library filenames end in this
_SO = .so
# Shared library filename prefix
SO_ = lib
# Executables end in this
_EX =

# How to tell the linker to set shared library name
GCC.LDFLAGS.SHARED = -shared -Wl,"-soname=$1"
# Set to non-empty if platform supports versioned shared libraries
SO.VERSION = 1
