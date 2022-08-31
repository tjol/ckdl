cmake_minimum_required(VERSION 3.18)
project(kdlpp-tests CXX)

include(CTest)

add_executable(kdlpp_test kdlpp_test.cpp)
target_link_libraries(kdlpp_test kdlpp test_util)
add_test(kdlpp_test kdlpp_test)
