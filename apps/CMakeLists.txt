IF(BUILD_LENSTOOL)
    IF(WIN32)
      INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/getopt)
    ENDIF()
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
    INCLUDE_DIRECTORIES(${PNG_INCLUDE_DIRS})
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
    ADD_EXECUTABLE(lenstool lenstool/lenstool.cpp lenstool/image.cpp)
    TARGET_LINK_LIBRARIES(lenstool lensfun ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
    INSTALL(TARGETS lenstool DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF()


IF(INSTALL_HELPER_SCRIPTS)
  INSTALL(PROGRAMS g-lensfun-update-data lensfun-add-adapter lensfun-update-data lensfun-convert-lcp
          DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(INSTALL_HELPER_SCRIPTS)


# Inspired by http://bloerg.net/2012/11/10/cmake-and-distutils.html

FIND_PROGRAM(PYTHON "python3")
IF(PYTHON)
    SET(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
    SET(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
    SET(DEPS_IN     "${CMAKE_CURRENT_SOURCE_DIR}/lensfun/__init__.py.in")
    SET(DEPS        "${CMAKE_CURRENT_BINARY_DIR}/lensfun/__init__.py")
    SET(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

    CONFIGURE_FILE(${SETUP_PY_IN} ${SETUP_PY})
    CONFIGURE_FILE(${DEPS_IN} ${DEPS})

    ADD_CUSTOM_COMMAND(OUTPUT ${OUTPUT}
                       COMMAND ${PYTHON} ${SETUP_PY} build
                       COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
                       DEPENDS ${DEPS_IN})

    ADD_CUSTOM_TARGET(python-package ALL DEPENDS ${OUTPUT})

    IF(NOT DEFINED SETUP_PY_INSTALL_PREFIX)
      SET(SETUP_PY_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    ENDIF()
    INSTALL(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --prefix=\$ENV{DESTDIR}${SETUP_PY_INSTALL_PREFIX})")
ENDIF(PYTHON)
