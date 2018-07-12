ADD_SUBDIRECTORY(lensfun)

# also build getopt on windows
IF(WIN32 AND BUILD_LENSTOOL)
  ADD_SUBDIRECTORY(getopt)
ENDIF()
