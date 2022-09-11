cmake_minimum_required(VERSION 3.17)
project(kdlpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckTypeSize)
set(CMAKE_EXTRA_INCLUDE_FILES "string_view")
check_type_size(std::u8string_view U8STRING_VIEW LANGUAGE CXX)
unset(CMAKE_EXTRA_INCLUDE_FILES)
if(NOT HAVE_U8STRING_VIEW)
    message(WARNING "kdlpp requires a C++20 compiler with std::u8string_view")
    message(WARNING "Not building kdlpp")
    set(BUILD_KDLPP OFF CACHE BOOL "Build C++ binding (kdlpp)" FORCE)
else()
    set(BUILD_KDLPP ON CACHE BOOL "Build C++ binding (kdlpp)")
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(BUILD_KDLPP)

    set(KDLPP_CXX_SOURCES
        src/kdlpp.cpp
    )

    add_library(kdlpp ${KDLPP_CXX_SOURCES})
    target_include_directories(kdlpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(kdlpp PUBLIC kdl)
    target_compile_features(kdlpp PUBLIC cxx_std_20)

    if(BUILD_TESTS)
        add_subdirectory(tests)
    endif()

endif()
