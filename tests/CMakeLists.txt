ADD_EXECUTABLE(test_database test_database.cpp)
TARGET_LINK_LIBRARIES(test_database lensfun ${COMMON_LIBS})
ADD_TEST(Database test_database)

ADD_EXECUTABLE(test_modifier test_modifier.cpp)
TARGET_LINK_LIBRARIES(test_modifier lensfun ${COMMON_LIBS})
ADD_TEST(Modifier test_modifier)

find_package(OpenMP)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

ADD_EXECUTABLE(test_modifier_color test_modifier_color.cpp)
TARGET_LINK_LIBRARIES(test_modifier_color lensfun ${COMMON_LIBS})
ADD_TEST(Modifier_color test_modifier_color)

ADD_EXECUTABLE(test_modifier_subpix test_modifier_subpix.cpp)
TARGET_LINK_LIBRARIES(test_modifier_subpix lensfun ${COMMON_LIBS})
ADD_TEST(Modifier_subpix test_modifier_subpix)

ADD_EXECUTABLE(test_modifier_coord_scale test_modifier_coord_scale.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_scale lensfun ${COMMON_LIBS})
ADD_TEST(Modifier_coord_scale test_modifier_coord_scale)

ADD_EXECUTABLE(test_modifier_coord_distortion test_modifier_coord_distortion.cpp)
TARGET_LINK_LIBRARIES(test_modifier_coord_distortion lensfun ${COMMON_LIBS})
ADD_TEST(Modifier_coord_distortion test_modifier_coord_distortion)
