# Settings for building for the MacOS X target

.SUFFIXES: .dylib

# OS-dependent extensions

# Shared library filenames end in this
_SO = .dylib
# Shared library filename prefix
SO_ = lib
# Executables end in this
_EX =

# Flag to tell Darwin linker to set the library name:
GCC.LDFLAGS.SHARED = -dynamiclib -Wl,"-dylib_install_name,$1"
# Set to non-empty if platform supports versioned shared libraries
SO.VERSION = 1
