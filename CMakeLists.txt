cmake_minimum_required(VERSION 3.18)
project(ckdl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Check if we need _DEFAULT_SOURCE (e.g. GNU/Linux)
include(CheckSymbolExists)
check_symbol_exists(strdup "string.h" HAVE_STRDUP)
if(NOT HAVE_STRDUP)
    set(CMAKE_REQUIRED_DEFINITIONS -D_DEFAULT_SOURCE)
    check_symbol_exists(strdup "string.h" HAVE_STRDUP_GNU)
    if(NOT HAVE_STRDUP_GNU)
        message(FATAL_ERROR "strdup function not found")
    else()
        add_compile_definitions(_DEFAULT_SOURCE)
    endif()
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
add_subdirectory(bindings/cpp)

enable_testing()
add_subdirectory(tests)
