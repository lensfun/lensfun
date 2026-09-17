# require at least cmake 3.12
CMAKE_MINIMUM_REQUIRED(VERSION 3.12 FATAL_ERROR)

# path for helper modules
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/cmake/modules")
GET_FILENAME_COMPONENT(SOURCE_BASE_DIR ${CMAKE_SOURCE_DIR} PATH CACHE)

#  global setup
project(lensfun C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
ELSE()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wformat-security -pedantic -Wextra -fexceptions")
ENDIF()

# version
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 3)
SET(VERSION_MICRO 99)
SET(VERSION_BUGFIX 0)
SET(VERSION_API 2)
# This is the version of the *shipped* database, which is not necessarily the
# highest version number supported
SET(LENSFUN_DB_VERSION 2)

# check if some include are available
INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES(endian.h HAVE_ENDIAN_H)

# set include directories
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/lensfun)

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "[XxIi][0-9]?86|[Aa][Mm][Dd]64")
  SET(X86_ON ON)
else()
  SET(X86_ON OFF)
ENDIF()

# options controlling the build process
OPTION(BUILD_STATIC "Build static library" OFF)
OPTION(BUILD_WITH_MSVC_STATIC_RUNTIME "Build using static MSVC runtime" OFF)
OPTION(BUILD_TESTS "Build test suite" OFF)
OPTION(BUILD_LENSTOOL "Build the lenstool (requires libpng)" OFF)
OPTION(BUILD_FOR_SSE "Build with support for SSE" ${X86_ON})
OPTION(BUILD_FOR_SSE2 "Build with support for SSE2" ${X86_ON})
OPTION(BUILD_DOC "Build documentation with doxygen" OFF)
OPTION(INSTALL_PYTHON_MODULE "Install Python module for the helper scripts" ON)
OPTION(INSTALL_HELPER_SCRIPTS "Install various helper scripts" ON)

IF(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: Debug, Release"
       FORCE
  )
ENDIF()

# place all .exe and .dll files in same folder to avoid issues with tests
IF(WIN32)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
ENDIF()

# set local install prefix on windows
IF(WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Install prefix for Lensfun" FORCE)
ENDIF()

# overwrite some folders before GNUInstallDirs is loaded
# get standard GNU install directories for the current platform
INCLUDE(GNUInstallDirs)
IF(NOT DEFINED CMAKE_INSTALL_DATAROOTDIR)
  SET(CMAKE_INSTALL_DATAROOTDIR "share" CACHE PATH "read-only architecture-independent data root (share)")
ENDIF()

IF(BUILD_FOR_SSE)
  SET(VECTORIZATION_SSE 1)
  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    SET(VECTORIZATION_SSE_FLAGS "-msse")
  ENDIF()
ENDIF()
IF(BUILD_FOR_SSE2)
  SET(VECTORIZATION_SSE2 1)
  IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    SET(VECTORIZATION_SSE2_FLAGS "-msse2")
  ENDIF()
ENDIF()

IF(WIN32)
  # base path for searching for glib on windows
  IF(NOT GLIB2_BASE_DIR)
    SET(GLIB2_BASE_DIR "${SOURCE_BASE_DIR}/glib-2.28.1" CACHE STRING "Base path of glib2 dir." FORCE)
  ENDIF()
  # install glib dll on windows next to lensfun.dll
  IF (GLIB2_DLL)
    INSTALL(FILES ${GLIB2_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF()
ENDIF()


IF (BUILD_TESTS)
  # automatic tests need at least glib version 2.40
  # NOTE: must be one of the macros listed in https://developer.gnome.org/glib/stable/glib-Version-Information.html
  SET(LENSFUN_GLIB_REQUIREMENT_MACRO "GLIB_VERSION_2_40")
  FIND_PACKAGE(GLIB2 REQUIRED)
ELSE()
  SET(LENSFUN_GLIB_REQUIREMENT_MACRO "GLIB_VERSION_2_26")
  FIND_PACKAGE(GLIB2 REQUIRED)
ENDIF()

INCLUDE_DIRECTORIES(SYSTEM ${GLIB2_INCLUDE_DIRS})
LINK_DIRECTORIES(${GLIB2_LIBRARY_DIRS})


IF(BUILD_STATIC)
  ADD_DEFINITIONS(-DCONF_LENSFUN_STATIC)
ENDIF()

# create config.h file
CONFIGURE_FILE(include/lensfun/config.h.in.cmake ${CMAKE_BINARY_DIR}/config.h)
# create lensfun.h file
CONFIGURE_FILE(include/lensfun/lensfun.h.in ${CMAKE_BINARY_DIR}/lensfun.h @ONLY)

# include libs
ADD_SUBDIRECTORY(libs)

# build the test suite
IF(BUILD_TESTS)
    enable_testing()
    ADD_SUBDIRECTORY(tests)
ENDIF()

# apps subdir
IF(BUILD_LENSTOOL)
    FIND_PACKAGE(PNG REQUIRED)
ENDIF()
ADD_SUBDIRECTORY(apps)

# install include files
INSTALL(FILES ${CMAKE_BINARY_DIR}/lensfun.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lensfun)

# install db files
FILE(GLOB DATABASE_FILES data/db/*.xml)

INSTALL(FILES ${DATABASE_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/lensfun/version_${LENSFUN_DB_VERSION})
INSTALL(FILES data/db/timestamp.txt DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/lensfun/version_${LENSFUN_DB_VERSION})

FILE(GLOB DATABASE_DTD data/db/*.dtd data/db/*.xsd)
INSTALL(FILES ${DATABASE_DTD} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/lensfun/version_${LENSFUN_DB_VERSION})

# build documentation
IF(BUILD_DOC)
  ADD_SUBDIRECTORY(docs)
ENDIF()

IF(WIN32 AND NOT BUILD_STATIC)
  FIND_FILE(GLIB2_DLL
            NAMES glib-2.0-0.dll glib-2.dll glib-2-vs9.dll
            PATHS "${GLIB2_BASE_DIR}/bin"
            NO_SYSTEM_ENVIRONMENT_PATH)
  INSTALL(FILES ${GLIB2_DLL} DESTINATION ${CMAKE_INSTALL_LIBDIR})
ENDIF()

# print some information
MESSAGE(STATUS "-------------------------------------------------------------------------------------")
MESSAGE(STATUS "Building Lensfun ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_BUGFIX}")
MESSAGE(STATUS "-------------------------------------------------------------------------------------")
IF(BUILD_STATIC)
  MESSAGE(STATUS "Build as static library")
ELSE()
  MESSAGE(STATUS "Build as dynamic/shared library")
ENDIF()
IF (NOT CMAKE_CONFIGURATION_TYPES)
  MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
ENDIF()
MESSAGE(STATUS "Build doc: ${BUILD_DOC}")
MESSAGE(STATUS "Build test suite: ${BUILD_TESTS}")
MESSAGE(STATUS "Build lenstool: ${BUILD_LENSTOOL}")
MESSAGE(STATUS "Build with support for SSE: ${BUILD_FOR_SSE}")
MESSAGE(STATUS "Build with support for SSE2: ${BUILD_FOR_SSE2}")
MESSAGE(STATUS "Install helper scripts: ${INSTALL_HELPER_SCRIPTS}")
MESSAGE(STATUS "\nInstall prefix: ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "\nUsing: ")
MESSAGE(STATUS "GLIB2 include dir: ${GLIB2_INCLUDE_DIRS}")
MESSAGE(STATUS "GLIB2 library: ${GLIB2_LIBRARIES}")
IF(GLIB2_DLL)
MESSAGE(STATUS "GLIB2 DLL: ${GLIB2_DLL}")
ENDIF()
IF(BUILD_LENSTOOL)
MESSAGE(STATUS "libpng include dir: ${PNG_INCLUDE_DIRS}")
MESSAGE(STATUS "libpng lib: ${PNG_LIBRARIES}")
MESSAGE(STATUS "zlib include dir: ${ZLIB_INCLUDE_DIRS}")
MESSAGE(STATUS "zlib lib: ${ZLIB_LIBRARIES}")
ENDIF()

# Package
# This needs some testing, in particular for right package naming for different
# package managers and distributions. Debian works fine.
SET(CPACK_PACKAGE_NAME "lensfun")
SET(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${VERSION_MICRO}.${VERSION_BUGFIX}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${V_MAJOR}.${V_MINOR}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_BUGFIX}")
SET(CPACK_SOURCE_GENERATOR "TGZ;TBZ2")
SET(CPACK_PACKAGE_CONTACT "zap@homelink.ru")
SET(CPACK_SET_DESTDIR "ON")
IF(BUILD_AUXFUN)
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libglib2.0-0 (>=2.0.0), libpng (>=1.0), zlib (>=1.0)")
ELSE()
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libglib2.0-0 (>=2.0.0)")
ENDIF()

IF(CPACK_BINARY_DEB)
  # workaround to set correct debian package name
  SET(CPACK_DEBIAN_PACKAGE_NAME "liblensfun${VERSION_API}")
  STRING(TOLOWER "${CPACK_DEBIAN_PACKAGE_NAME}" CPACK_PACKAGE_NAME_LOWERCASE)
  FIND_PROGRAM(DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
  if(DPKG_PROGRAM)
      EXECUTE_PROCESS(
          COMMAND ${DPKG_PROGRAM} --print-architecture
          OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
          OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME_LOWERCASE}_${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_BUGFIX}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  else(DPKG_PROGRAM)
      SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME_LOWERCASE}_${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}.${VERSION_BUGFIX}_${CMAKE_SYSTEM_NAME}")
  endif(DPKG_PROGRAM)
ENDIF(CPACK_BINARY_DEB)

INCLUDE(CPack)

if(CMAKE_BUILD_TYPE STREQUAL "Coverage" AND CMAKE_COMPILER_IS_GNUCXX)
  include(CodeCoverage)
  setup_target_for_coverage(coverage ctest coverage)
endif(CMAKE_BUILD_TYPE STREQUAL "Coverage" AND CMAKE_COMPILER_IS_GNUCXX)
