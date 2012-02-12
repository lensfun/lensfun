# build lensfun library
SET(LENSFUN_SRC camera.cpp database.cpp lens.cpp 
                mount.cpp lensfunprv.h cpuid.cpp 
                mod-color-sse2.cpp mod-color.cpp 
                mod-coord-sse.cpp mod-coord.cpp
                mod-subpix.cpp modifier.cpp auxfun.cpp
                ../../include/lensfun/lensfun.h.in)
IF(WIN32)
  LIST(APPEND LENSFUN_SRC windows/auxfun.cpp)
ENDIF()

IF(BUILD_STATIC)
  ADD_LIBRARY(lensfun STATIC ${LENSFUN_SRC})
ELSE()
  ADD_LIBRARY(lensfun SHARED ${LENSFUN_SRC})
  SET_TARGET_PROPERTIES(lensfun PROPERTIES COMPILE_FLAGS -DCONF_SYMBOL_VISIBILITY)
ENDIF()
SET_TARGET_PROPERTIES(lensfun PROPERTIES SOVERSION "${VERSION_MAJOR}" VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_BUGFIX}")  

IF(NOT HAVE_REGEX_H)
  TARGET_LINK_LIBRARIES(lensfun tre_regex)
ENDIF()
TARGET_LINK_LIBRARIES(lensfun ${GLIB2_LIBRARIES})

INSTALL(TARGETS lensfun 
        RUNTIME DESTINATION ${BINDIR}
        LIBRARY DESTINATION ${LIBDIR}
        ARCHIVE DESTINATION ${LIBDIR})
