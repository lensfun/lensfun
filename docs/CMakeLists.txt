# find doxygen
FIND_PACKAGE(Doxygen REQUIRED)

# set some files/path for later user
# we don't use path doc because path doc is used by cmake itself because
# of target doc
SET(CMAKE_DOC_OUT ${CMAKE_BINARY_DIR}/doc_doxygen)
set(DOXYFILE "${CMAKE_CURRENT_BINARY_DIR}/doxyfile")

# create doxyfile
CONFIGURE_FILE(doxyfile.in.cmake ${DOXYFILE} @ONLY)

# now build documentation
ADD_CUSTOM_TARGET(doc
  ALL
  COMMAND "${DOXYGEN_EXECUTABLE}"
  DEPENDS "${DOXYFILE}" manual-main.txt ../include/lensfun/lensfun.h.in
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Building documentation with doxygen"
)

# install documentation  
INSTALL(DIRECTORY ${CMAKE_DOC_OUT}/ DESTINATION ${DOCDIR})
