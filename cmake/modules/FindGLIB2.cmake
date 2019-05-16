if(NOT MSVC)
    include(FindPkgConfig)
    pkg_search_module(GLIB2 glib-2.0)
elseif(ENV{VCPKG_ROOT})
    find_package(unofficial-glib CONFIG REQUIRED)
    if(unofficial-glib_FOUND)
        set(GLIB2_FOUND "${unofficial-glib_FOUND}")
    endif()
endif()

if(WIN32 AND NOT BUILD_STATIC)
    find_file(GLIB2_DLL
            NAMES glib-2.dll glib-2-vs9.dll libglib-2.0-0.dll
            PATHS ${GLIB2_LIBRARY_DIRS}/../bin
            PATH_SUFFIXES glib bin
            DOC "Glib2 dll")
endif()

if(NOT GLIB2_FOUND OR NOT PKG_CONFIG_FOUND)
    find_path(GLIB2_GLIB2CONFIG_INCLUDE_PATH
        NAMES glibconfig.h
        PATHS   /usr/local/lib
                /usr/lib
                /usr/lib64
                /opt/local/lib
                ${GLIB2_BASE_DIR}/lib
                ${GLIB2_BASE_DIR}/include
                ${CMAKE_LIBRARY_PATH}
        PATH_SUFFIXES include glib-2.0/include)

    find_path(GLIB2_INCLUDE_DIRS
        NAMES glib.h
        PATHS   /usr/local/include
                /usr/include
                /opt/local/include
                ${GLIB2_BASE_DIR}/include
        PATH_SUFFIXES include gtk-2.0 glib-2.0 glib20)

    find_library(GLIB2_LIBRARIES
        NAMES  glib-2.0 glib20 glib
        PATHS   /usr/local/lib
                /usr/lib
                /usr/lib64
                /opt/local/lib
                ${GLIB2_BASE_DIR}/lib
        PATH_SUFFIXES lib glib)

    if(GLIB2_GLIB2CONFIG_INCLUDE_PATH AND GLIB2_INCLUDE_DIRS AND GLIB2_LIBRARIES)
        set(GLIB2_INCLUDE_DIRS ${GLIB2_GLIB2CONFIG_INCLUDE_PATH} ${GLIB2_INCLUDE_DIRS})
        set(GLIB2_LIBRARIES ${GLIB2_LIBRARIES})
        set(GLIB2_FOUND 1)
    else()
        set(GLIB2_INCLUDE_DIRS)
        set(GLIB2_LIBRARIES)
        set(GLIB2_FOUND 0)
    endif()
endif()

#INCLUDE( FindPackageHandleStandardArgs)
#FIND_PACKAGE_HANDLE_STANDARD_ARGS( GLIB2 DEFAULT_MSG GLIB2_LIBRARIES GLIB2_GLIB2CONFIG_INCLUDE_PATH GLIB2_GLIB2_INCLUDE_PATH)

if(NOT GLIB2_FOUND AND GLIB2_FIND_REQUIRED)
        MESSAGE(FATAL_ERROR "Could not find glib2")
endif()