cmake_minimum_required(VERSION 3.18)
project(kdl-tests C CXX)

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

add_executable(kdlpp_test kdlpp_test.cpp)
target_link_libraries(kdlpp_test kdlpp test_util)
add_test(kdlpp_test kdlpp_test)
