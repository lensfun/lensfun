ADD_EXECUTABLE(test_database test_database.cpp)
TARGET_LINK_LIBRARIES(test_database lensfun ${COMMON_LIBS})
ADD_TEST(Database test_database)

ADD_EXECUTABLE(test_modifier test_modifier.cpp)
TARGET_LINK_LIBRARIES(test_modifier lensfun ${COMMON_LIBS})
ADD_TEST(Modifier test_modifier)

ADD_EXECUTABLE(test_modifier_color test_modifier_color.cpp)
TARGET_LINK_LIBRARIES(test_modifier_color lensfun ${COMMON_LIBS})
ADD_TEST(Modifier_color test_modifier_color)
