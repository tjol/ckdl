# CMake 3.12 introduced cxx_std_20
cmake_minimum_required(VERSION 3.12)
project(kdlpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	# compiling with GCC or Clang requires us to add the -std=c++20 flag in some scenarios
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
endif()

include(CheckTypeSize)
set(CMAKE_EXTRA_INCLUDE_FILES "string_view")
check_type_size(std::u8string_view U8STRING_VIEW LANGUAGE CXX)
unset(CMAKE_EXTRA_INCLUDE_FILES)

if(HAVE_U8STRING_VIEW)
    include(CheckCXXSourceCompiles)
    # Check for C++20 concepts
    check_cxx_source_compiles("
        template<typename T> concept foo = true;
        int main() { return 0; }"
        HAVE_CONCEPTS)
    check_cxx_source_compiles("
        struct A {
            int i;
            bool operator==(const A& other) const = default;
        };
        int main() { return 0; }"
        HAVE_DEFAULT_COMPARISONS)
endif()

if(NOT HAVE_U8STRING_VIEW OR NOT HAVE_CONCEPTS OR NOT HAVE_DEFAULT_COMPARISONS)
    message(WARNING "kdlpp requires a C++20 compiler")
    message(WARNING "Not building kdlpp")
    set(BUILD_KDLPP OFF CACHE BOOL "Build C++ binding (kdlpp)" FORCE)
else()
    set(BUILD_KDLPP ON CACHE BOOL "Build C++ binding (kdlpp)")
endif()

if(BUILD_KDLPP)

    set(KDLPP_CXX_SOURCES
        src/kdlpp.cpp
    )

    add_library(kdlpp ${KDLPP_CXX_SOURCES})

    target_compile_options(kdlpp PRIVATE ${KDL_COMPILE_OPTIONS})
    target_compile_definitions(kdlpp PRIVATE BUILDING_KDLPP=1)

    target_include_directories(kdlpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(kdlpp PUBLIC kdl)
    target_compile_features(kdlpp PUBLIC cxx_std_20)

    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(kdlpp PUBLIC KDLPP_STATIC_LIB=1)
    endif()

    install(TARGETS kdlpp COMPONENT libkdlpp)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/kdlpp.h TYPE INCLUDE COMPONENT libkdlpp)

    if(BUILD_TESTS)
        add_subdirectory(tests)
    endif()

endif()
