# require at least cmake 2.8
CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR )

# path for helper modules
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/cmake/modules")
GET_FILENAME_COMPONENT(SOURCE_BASE_DIR ${CMAKE_SOURCE_DIR} PATH CACHE)

#  global setup
project(lensfun C CXX)

# version
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 3)
SET(VERSION_MICRO 2)
SET(VERSION_BUGFIX 0)
SET(VERSION_API 1)
# This is the version of the *shipped* database, which is not necessarily the
# highest version number supported
SET(LENSFUN_DB_VERSION 1)

# check if some include are available
INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES(regex.h HAVE_REGEX_H)
CHECK_INCLUDE_FILES(endian.h HAVE_ENDIAN_H)

# set include directories
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/lensfun)
IF(NOT HAVE_REGEX_H)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/regex)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/regex)
ENDIF()

# options controlling the build process
OPTION(BUILD_STATIC "Build static library" OFF)
OPTION(BUILD_TESTS "Build test suite" OFF)
OPTION(BUILD_LENSTOOL "Build the lenstool (requires libpng)" OFF)
OPTION(BUILD_FOR_SSE "Build with support for SSE" ON)
OPTION(BUILD_FOR_SSE2 "Build with support for SSE2" ON)
OPTION(BUILD_DOC "Build documentation with doxygen" OFF)
OPTION(INSTALL_HELPER_SCRIPTS "Install various helper scripts" ON)

IF(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: Debug, Release"
       FORCE
  )
ENDIF()

# set local install prefix on windows
IF(WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)  
  SET(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Install prefix for Lensfun" FORCE)    
ENDIF()
# get standard GNU install directories for the current platform
INCLUDE(GNUInstallDirs)
IF(NOT DEFINED CMAKE_INSTALL_DATAROOTDIR)
  SET(CMAKE_INSTALL_DATAROOTDIR "share" CACHE PATH "read-only architecture-independent data root (share)")
ENDIF()
IF (WIN32)
    SET(LENSFUN_WINDOWS_DATADIR ${CMAKE_INSTALL_DATAROOTDIR}/lensfun CACHE PATH "Directory for library data files. This should be relative to the location where the lensfun.dll will be installed.")
ENDIF()

IF(BUILD_FOR_SSE)
  SET(VECTORIZATION_SSE 1)
  IF(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse")
  ENDIF()
ENDIF()
IF(BUILD_FOR_SSE2)
  SET(VECTORIZATION_SSE2 1)
  IF(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
  ENDIF()
ENDIF()

IF(WIN32)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/getopt)
  # base path for searching for glib on windows
  IF(NOT GLIB2_BASE_DIR)
    SET(GLIB2_BASE_DIR "${SOURCE_BASE_DIR}/glib-2.28.1" CACHE STRING "Base path of glib2 dir." FORCE)
  ENDIF()
  # install glib dll on windows next to lensfun.dll
  IF (GLIB2_DLL)
    INSTALL(FILES ${GLIB2_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF()
ENDIF()

# find dependencies
# NOTE: must be one of the macros listed in https://developer.gnome.org/glib/stable/glib-Version-Information.html
SET(LENSFUN_GLIB_REQUIREMENT_MACRO "GLIB_VERSION_2_40")
FIND_PACKAGE(GLIB2 REQUIRED 2.40)

INCLUDE_DIRECTORIES(SYSTEM ${GLIB2_INCLUDE_DIRS})


IF(BUILD_STATIC)
  ADD_DEFINITIONS(-DCONF_LENSFUN_STATIC)
ENDIF()


# some windows and MSVC specific settings
IF(WIN32)
  SET(PLATFORM_WINDOWS 1)  
  
  IF (MSVC)
    IF(BUILD_STATIC)
      # compile everything for the static C runtime
      FOREACH(TYPE C CXX)
        # makefiles use blank configuration
        FOREACH(CFG "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
          SET(NEW_FLAGS "${CMAKE_${TYPE}_FLAGS${CFG}}")
          # fix up static libc flags
          STRING(REPLACE "/MD" "/MT" NEW_FLAGS "${NEW_FLAGS}")
          # *FORCE* to override whats already placed into the cache
          SET(CMAKE_${TYPE}_FLAGS${CFG} "${NEW_FLAGS}" CACHE STRING
            "CMAKE_${TYPE}_FLAGS${CFG} (overwritten to ensure static build)" FORCE)
        ENDFOREACH(CFG)
      ENDFOREACH(TYPE)
      # link only with the static C runtime !! THIS IS A MUST !!
      SET(NEW_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
      # ensure that the flags are not duplicaated on subsequent runs
      STRING(REPLACE " /NODEFAULTLIB:\"MSVCRT.lib\" /NODEFAULTLIB:\"MSVCRTd.lib\"" "" NEW_FLAGS "${NEW_FLAGS}")
      # note that flag names (NODEFAULTLIB) etc.) are case sensitive
      SET(CMAKE_EXE_LINKER_FLAGS
          "${NEW_FLAGS} /NODEFAULTLIB:\"MSVCRT.lib\" /NODEFAULTLIB:\"MSVCRTd.lib\""
          CACHE STRING "for MSVC" FORCE
      )
    ENDIF()
  ENDIF(MSVC)
ENDIF(WIN32)

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
ADD_SUBDIRECTORY(apps)

# install include files
INSTALL(FILES ${CMAKE_BINARY_DIR}/lensfun.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lensfun)
IF(NOT HAVE_REGEX_H)
  INSTALL(FILES include/regex/regex.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/regex)
ENDIF()

# install db files
FILE(GLOB DATABASE_FILES data/db/*.xml)
INSTALL(FILES ${DATABASE_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/lensfun/version_${LENSFUN_DB_VERSION})
INSTALL(FILES data/db/timestamp.txt DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/lensfun/version_${LENSFUN_DB_VERSION})

# build documentation
IF(BUILD_DOC)
  ADD_SUBDIRECTORY(docs)
ENDIF()

IF(WIN32 AND NOT BUILD_STATIC)
  FIND_FILE(GLIB2_DLL 
            NAMES glib-2.dll glib-2-vs9.dll
            PATHS "${GLIB2_BASE_DIR}/bin"
            NO_SYSTEM_ENVIRONMENT_PATH)
  INSTALL(FILES ${GLIB2_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR})
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
MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
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
IF(BUILD_APPS)
MESSAGE(STATUS "libpng include dir: ${PNG_INCLUDE_DIR}")
MESSAGE(STATUS "libpng lib: ${PNG_LIBRARY}")
MESSAGE(STATUS "zlib include dir: ${ZLIB_INCLUDE_DIR}")
MESSAGE(STATUS "zlib lib: ${ZLIB_LIBRARY}")
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
