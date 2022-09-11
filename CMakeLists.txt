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
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(KDL_C_SOURCES
    src/utf8.c
    src/str.c
    src/tokenizer.c
    src/parser.c
    src/emitter.c
)

add_library(kdl ${KDL_C_SOURCES})
target_include_directories(kdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# for tests
add_library(kdl-internal-headers INTERFACE)
target_include_directories(kdl-internal-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(src/utils)

if(CMAKE_VERSION VERSION_LESS 3.17)
    message(WARNING "CMake 3.17 is required for the C++20 bindings, not building kdlpp")
else()
    add_subdirectory(bindings/cpp)
endif()

enable_testing()
add_subdirectory(tests)
