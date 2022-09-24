cmake_minimum_required(VERSION 3.12)
project(kdlpp-tests CXX)

add_executable(kdlpp_test kdlpp_test.cpp)
target_link_libraries(kdlpp_test kdlpp test_util)
add_test(kdlpp_test kdlpp_test)

if (WIN32 AND BUILD_SHARED_LIBS)
    # Copy kdl.dll to the test folder so that the tests work
    add_custom_command(TARGET kdlpp_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kdl> $<TARGET_FILE_DIR:kdlpp_test>)
endif()
