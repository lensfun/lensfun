prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
bindir=@LENSFUN_PC_BINDIR@
libdir=@LENSFUN_PC_LIBDIR@
includedir=@LENSFUN_PC_INCLUDEDIR@
datadir=@LENSFUN_PC_DATADIR@
docdir=@LENSFUN_PC_DOCDIR@

Name: lensfun
Description: A photographic lens database and access library
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_MICRO@.@VERSION_BUGFIX@
Requires.private: glib-2.0
Libs: -L${libdir} -llensfun
Cflags: -I${includedir} -I${includedir}/lensfun
