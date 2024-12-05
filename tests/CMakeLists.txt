### Deprecated API tests
ADD_SUBDIRECTORY(deprecated)

find_package(OpenMP)

IF (OpenMP_FOUND)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
ENDIF()

ADD_EXECUTABLE(test_auxfun test_auxfun.cpp)
TARGET_LINK_LIBRARIES(test_auxfun lensfun ${COMMON_LIBS})
ADD_TEST(NAME Auxiliary WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND test_auxfun)

ADD_EXECUTABLE(test_database test_database.cpp)
TARGET_LINK_LIBRARIES(test_database lensfun ${COMMON_LIBS})
ADD_TEST(NAME Database WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND test_database)

ADD_EXECUTABLE(test_lens test_lens.cpp)
TARGET_LINK_LIBRARIES(test_lens lensfun ${COMMON_LIBS})
ADD_TEST(NAME Lens COMMAND test_lens)

ADD_EXECUTABLE(test_mount_c_api test_mount_c_api.cpp)
TARGET_LINK_LIBRARIES(test_mount_c_api lensfun ${COMMON_LIBS})
ADD_TEST(NAME Mount_c_api COMMAND test_mount_c_api)

ADD_EXECUTABLE(test_modifier test_modifier.cpp)
TARGET_LINK_LIBRARIES(test_modifier lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier COMMAND test_modifier)

IF(X86_ON)
  ADD_EXECUTABLE(test_modifier_performance test_modifier_performance.cpp)
  TARGET_LINK_LIBRARIES(test_modifier_performance lensfun ${COMMON_LIBS})
  ADD_TEST(NAME Modifier_performance WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND test_modifier_performance)
ENDIF()

ADD_EXECUTABLE(test_modifier_color test_modifier_color.cpp)
TARGET_LINK_LIBRARIES(test_modifier_color lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_color COMMAND test_modifier_color)

ADD_EXECUTABLE(test_modifier_subpix test_modifier_subpix.cpp)
TARGET_LINK_LIBRARIES(test_modifier_subpix lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_subpix COMMAND test_modifier_subpix)

ADD_EXECUTABLE(test_modifier_coord_scale test_modifier_coord_scale.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_scale lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_scale COMMAND test_modifier_coord_scale)

ADD_EXECUTABLE(test_modifier_coord_perspective_correction test_modifier_coord_perspective_correction.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_perspective_correction lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_perspective_correction COMMAND test_modifier_coord_perspective_correction)

ADD_EXECUTABLE(test_modifier_coord_distortion test_modifier_coord_distortion.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_distortion lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_distortion COMMAND test_modifier_coord_distortion)

ADD_EXECUTABLE(test_modifier_coord_geometry test_modifier_coord_geometry.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_geometry lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_geometry COMMAND test_modifier_coord_geometry)

ADD_EXECUTABLE(test_modifier_coord_tiny_image test_modifier_coord_tiny_image.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_tiny_image lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_tiny_image COMMAND test_modifier_coord_tiny_image)

ADD_EXECUTABLE(test_modifier_coord_centering test_modifier_coord_centering.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_centering lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_coord_centering COMMAND test_modifier_coord_centering)

ADD_EXECUTABLE(test_modifier_regression test_modifier_regression.cpp)
TARGET_LINK_LIBRARIES(test_modifier_regression lensfun ${COMMON_LIBS})
ADD_TEST(NAME Modifier_regression WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND test_modifier_regression)

ADD_EXECUTABLE(test_lffuzzystrcmp test_lffuzzystrcmp.cpp)
TARGET_LINK_LIBRARIES(test_lffuzzystrcmp lensfun ${COMMON_LIBS})
ADD_TEST(NAME test_lffuzzystrcmp COMMAND test_lffuzzystrcmp)

FIND_PACKAGE(Python3 COMPONENTS Interpreter REQUIRED)
ADD_TEST(NAME Database_integrity COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_database/check_database.py ${CMAKE_SOURCE_DIR}/data/db)
