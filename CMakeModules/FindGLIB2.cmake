
INCLUDE(FindPkgConfig)
PKG_SEARCH_MODULE( GLIB2 REQUIRED glib-2.0 )

IF ( NOT GLIB2_FOUND AND NOT PKG_CONFIG_FOUND )
    FIND_PATH(GLIB2_GLIB2CONFIG_INCLUDE_PATH
        NAMES glibconfig.h
        PATHS
          /usr/local/lib
          /usr/lib
          /usr/lib64
          /opt/local/lib
          ${CMAKE_LIBRARY_PATH}
          ${GLIB22_BASE_DIR}/lib
        PATH_SUFFIXES glib-2.0/include
    )


    FIND_PATH(GLIB2_INCLUDE_DIRS
        NAMES glib.h
        PATHS
            /usr/local/include
            /usr/include
            /opt/local/include
            ${GLIB22_BASE_DIR}/include
        PATH_SUFFIXES gtk-2.0 glib-2.0 glib20 
    )

    FIND_LIBRARY(GLIB2_LIBRARIES
        NAMES  glib-2.0 glib20 glib
        PATHS  
            /usr/local/lib
            /usr/lib
            /usr/lib64
            /opt/local/lib
            ${GLIB22_BASE_DIR}/lib
    )
    
    IF(GLIB2_GLIB2CONFIG_INCLUDE_PATH AND GLIB2_INCLUDE_DIRS AND GLIB2_LIBRARIES)
        SET( GLIB2_INCLUDE_DIRS  ${GLIB2_GLIB2CONFIG_INCLUDE_PATH} ${GLIB2_INCLUDE_DIRS} )
        SET( GLIB2_LIBRARIES ${GLIB2_LIBRARIES} )
        SET( GLIB2_FOUND 1)
    ELSE()
        SET( GLIB2_INCLUDE_DIRS )
        SET( GLIB2_LIBRARIES )
        SET( GLIB2_FOUND 0)
    ENDIF()    
    
ENDIF ( NOT GLIB2_FOUND AND NOT PKG_CONFIG_FOUND )

#INCLUDE( FindPackageHandleStandardArgs )
#FIND_PACKAGE_HANDLE_STANDARD_ARGS( GLIB22 DEFAULT_MSG GLIB22_GLIB2_LIBRARY GLIB22_GLIB2CONFIG_INCLUDE_PATH GLIB22_GLIB2_INCLUDE_PATH )

IF (NOT GLIB2_FOUND AND GLIB2_FIND_REQUIRED)
        MESSAGE(FATAL_ERROR "Could not find glib2")
ENDIF()


