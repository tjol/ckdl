# kdl tests

include(CTest)

add_library(test_util OBJECT test_util.c)

add_executable(utf8_test utf8_test.c)
target_link_libraries(utf8_test kdl kdl-internal-headers test_util)
add_test(utf8_test utf8_test)

add_executable(parser_test parser_test.c)
target_link_libraries(parser_test kdl test_util)
add_test(parser_test parser_test)

add_executable(emitter_test emitter_test.c)
target_link_libraries(emitter_test kdl test_util)
add_test(emitter_test emitter_test)
