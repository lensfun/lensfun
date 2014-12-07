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
