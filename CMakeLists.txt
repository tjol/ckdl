cmake_minimum_required(VERSION 3.7)
project(ckdl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_definitions(-D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE)
set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE)

# Check if we need _DEFAULT_SOURCE (e.g. GNU/Linux)
include(CheckSymbolExists)
check_symbol_exists(strdup "string.h" HAVE_STRDUP)
if(NOT HAVE_STRDUP)
    message(FATAL_ERROR "strdup function not found")
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
