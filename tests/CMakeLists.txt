# kdl tests

include(CTest)

add_library(test_util OBJECT test_util.c)

add_executable(utf8_test utf8_test.c)
target_link_libraries(utf8_test kdl kdl-internal-headers test_util)
add_test(utf8_test utf8_test)
