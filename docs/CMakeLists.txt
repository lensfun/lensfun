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
INSTALL(DIRECTORY ${CMAKE_DOC_OUT}/ DESTINATION ${CMAKE_INSTALL_DOCDIR})

# create and install man pages
FIND_PROGRAM(RST2MAN_EXECUTABLE NAMES rst2man rst2man.py rst2man2 rst2man2.py)
ADD_CUSTOM_TARGET(man ALL)
ADD_CUSTOM_COMMAND(
  TARGET man
  COMMAND ${RST2MAN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/man/g-lensfun-update-data.1.rst > ${CMAKE_CURRENT_BINARY_DIR}/g-lensfun-update-data.1
  COMMAND ${RST2MAN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/man/lensfun-update-data.1.rst > ${CMAKE_CURRENT_BINARY_DIR}/lensfun-update-data.1
  COMMAND ${RST2MAN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/man/lensfun-add-adapter.1.rst > ${CMAKE_CURRENT_BINARY_DIR}/lensfun-add-adapter.1
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

INSTALL(FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/g-lensfun-update-data.1 
    ${CMAKE_CURRENT_BINARY_DIR}/lensfun-update-data.1 
    ${CMAKE_CURRENT_BINARY_DIR}/lensfun-add-adapter.1 
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)
