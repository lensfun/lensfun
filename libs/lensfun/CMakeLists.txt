# build Lensfun library
SET(LENSFUN_SRC camera.cpp database.cpp lens.cpp 
                mount.cpp lensfunprv.h cpuid.cpp 
                mod-color-sse.cpp mod-color-sse2.cpp mod-color.cpp
                mod-coord-sse.cpp mod-coord.cpp mod-pc.cpp
                mod-subpix.cpp modifier.cpp auxfun.cpp
                legacy/legacy-modifier.cpp legacy/legacy-mod-coord.cpp legacy/legacy-mod-coord-sse.cpp
                legacy/legacy-lens.cpp legacy/legacy-auxfun.cpp legacy/legacy-mod-subpix.cpp
                legacy/legacy-cpuid.cpp legacy/legacy-mod-color.cpp
                legacy/legacy-mod-color-sse.cpp legacy/legacy-mod-color-sse2.cpp
                ../../include/lensfun/lensfun.h.in)

SET_SOURCE_FILES_PROPERTIES(mod-color-sse.cpp mod-coord-sse.cpp
  PROPERTIES COMPILE_FLAGS "${VECTORIZATION_SSE_FLAGS}")
SET_SOURCE_FILES_PROPERTIES(mod-color-sse2.cpp
  PROPERTIES COMPILE_FLAGS "${VECTORIZATION_SSE2_FLAGS}")

IF(BUILD_STATIC)
  ADD_LIBRARY(lensfun STATIC ${LENSFUN_SRC})
ELSE()
  ADD_LIBRARY(lensfun SHARED ${LENSFUN_SRC})
  SET_TARGET_PROPERTIES(lensfun PROPERTIES COMPILE_FLAGS -DCONF_SYMBOL_VISIBILITY)
ENDIF()
SET_TARGET_PROPERTIES(lensfun PROPERTIES SOVERSION "${VERSION_API}" VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}")

# legacy stuff
IF(NOT HAVE_REGEX_H)
  TARGET_LINK_LIBRARIES(lensfun tre_regex)
ENDIF()

TARGET_LINK_LIBRARIES(lensfun ${GLIB2_LIBRARIES})

INSTALL(TARGETS lensfun 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# pkgconfig support
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/lensfun.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/lensfun.pc @ONLY)
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/lensfun.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )

