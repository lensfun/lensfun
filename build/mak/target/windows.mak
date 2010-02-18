# Settings for building for the Windows target

.SUFFIXES: .exe .dll

# OS-dependent extensions

# Shared library filenames end in this
_SO = .dll
# Shared library filename prefix
SO_=
# Executables end in this
_EX = .exe

# How to link a shared library
GCC.LDFLAGS.SHARED = -shared -Wl,"--out-implib,$2$1.a"
