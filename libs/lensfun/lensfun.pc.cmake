prefix=@CMAKE_INSTALL_PREFIX@
bindir=@CMAKE_INSTALL_FULL_BINDIR@
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@
datadir=@CMAKE_INSTALL_FULL_DATADIR@
docdir=@CMAKE_INSTALL_FULL_DOCDIR@

Name: lensfun
Description: A photographic lens database and access library
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_MICRO@.@VERSION_BUGFIX@
Requires.private: glib-2.0
Libs: -L${libdir} -llensfun
Cflags: -I${includedir} -I${includedir}/lensfun
