prefix=@CMAKE_INSTALL_PREFIX@
bindir=@CMAKE_INSTALL_PREFIX@/@BINDIR@
libdir=@CMAKE_INSTALL_PREFIX@/@LIBDIR@
includedir=@CMAKE_INSTALL_PREFIX@/@INCLUDEDIR@
datadir=@CMAKE_INSTALL_PREFIX@/@DATADIR@
docdir=@CMAKE_INSTALL_PREFIX@/@DOCDIR@

Name: lensfun
Description: A photographic lens database and access library
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_MICRO@.@VERSION_BUGFIX@
Requires.private: glib-2.0
Libs: -L${libdir} -llensfun
Cflags: -I${includedir} -I${includedir}/lensfun
