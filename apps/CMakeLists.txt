IF(BUILD_LENSTOOL)
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
    INCLUDE_DIRECTORIES(${PNG_INCLUDE_DIRS})
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
    ADD_EXECUTABLE(lenstool lenstool/lenstool.cpp lenstool/image.cpp)
    TARGET_COMPILE_DEFINITIONS(lenstool PRIVATE -Dauxfun_EXPORTS)
    TARGET_LINK_LIBRARIES(lenstool lensfun ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
    IF(WIN32)
      TARGET_LINK_LIBRARIES(lenstool getopt)
    ENDIF()
    INSTALL(TARGETS lenstool DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF()


IF(INSTALL_HELPER_SCRIPTS)
  INSTALL(PROGRAMS g-lensfun-update-data lensfun-add-adapter lensfun-update-data lensfun-convert-lcp
          DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(INSTALL_HELPER_SCRIPTS)


# Inspired by http://bloerg.net/2012/11/10/cmake-and-distutils.html
IF(INSTALL_PYTHON_MODULE)
    FIND_PACKAGE (Python3 COMPONENTS Interpreter REQUIRED)
    # Windows has a dummy python.exe in the PATH which opens the Microsoft Store, so check if Python is real.
    EXECUTE_PROCESS(COMMAND ${Python3_EXECUTABLE} --version RESULT_VARIABLE PY_RESULT OUTPUT_QUIET)
    IF (NOT PY_RESULT EQUAL 0)
      MESSAGE(FATAL_ERROR "Python3 not found, it might be a dummy python.exe")
    ENDIF()

    SET(PYPROJECT_TOML_IN "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml.in")
    SET(PYPROJECT_TOML    "${CMAKE_CURRENT_BINARY_DIR}/pyproject.toml")
    SET(DEPS_IN           "${CMAKE_CURRENT_SOURCE_DIR}/lensfun/__init__.py.in")
    SET(DEPS              "${CMAKE_CURRENT_BINARY_DIR}/lensfun/__init__.py")
    SET(OUTPUT            "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

    FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build/")
    FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" PY_PACKAGE_DIR)
    CONFIGURE_FILE(${PYPROJECT_TOML_IN} ${PYPROJECT_TOML})
    CONFIGURE_FILE(${DEPS_IN} ${DEPS})

    ADD_CUSTOM_COMMAND(OUTPUT ${OUTPUT}
                       COMMAND ${Python3_EXECUTABLE} -m build ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
                       DEPENDS ${PYPROJECT_TOML_IN} ${DEPS_IN})

    ADD_CUSTOM_TARGET(python-package ALL DEPENDS ${OUTPUT})

    IF(NOT DEFINED PY_INSTALL_PREFIX)
      SET(PY_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    ENDIF()
    INSTALL(CODE "execute_process(COMMAND ${Python3_EXECUTABLE} -m pip install --prefix=\$ENV{DESTDIR}${PY_INSTALL_PREFIX} ${CMAKE_CURRENT_BINARY_DIR})")
ENDIF()
