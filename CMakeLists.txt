# CMake 3.8 makes it possible to test which feature selection macros (like
# _BSD_SOURCE) are required in combination with the selected C standard settings
# (see https://cmake.org/cmake/help/latest/policy/CMP0067.html)
#
# Ubuntu 18.04 LTS "bionic" ships with 3.10, Debian bullseye has 3.17 and RHEL 8
# has 3.20.
# Users of older systems have to install a newer version of CMake.
cmake_minimum_required(VERSION 3.8)
project(ckdl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    string(REGEX REPLACE "/W3" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    add_compile_options(/wd4996 /wd5105)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # try GCC-like options
    include(CheckCCompilerFlag)
    set(warning_flags Wall Wextra Wpedantic)
    if(BUILD_SHARED_LIBS)
        list(APPEND warning_flags fvisibility=hidden)
    endif()
    foreach(flag ${warning_flags})
        check_c_compiler_flag("-${flag}" "COMPILER_FLAG_${flag}")
        if(${COMPILER_FLAG_${flag}})
            add_compile_options("-${flag}")
        endif()
    endforeach()
endif()

set(KDL_C_SOURCES
    src/str.c
    src/tokenizer.c
    src/parser.c
    src/emitter.c
)

set(KDL_UTF8_C_SOURCES
    src/utf8.c
)

add_library(kdl-utf8 STATIC ${KDL_UTF8_C_SOURCES})
target_include_directories(kdl-utf8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(kdl ${KDL_C_SOURCES})
target_link_libraries(kdl kdl-utf8)
target_include_directories(kdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(kdl PRIVATE -DBUILDING_KDL=1)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(kdl PUBLIC -DKDL_STATIC_LIB=1)
endif()

add_subdirectory(src/utils)

set(BUILD_TESTS ON CACHE BOOL "Build tests")

if(BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CMAKE_VERSION VERSION_LESS 3.17)
    message(WARNING "CMake 3.17 is required for the C++20 bindings, not building kdlpp")
else()
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        add_subdirectory(bindings/cpp)
    else()
        message(WARNING "No C++ compiler found, not building kdlpp\n"
            "If you have a C++ compiler, try setting the CXX environment variable "
            "or the CMAKE_CXX_COMPILER CMake cache variable to the full path of "
            "the compiler.")
    endif()
endif()
