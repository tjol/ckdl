cmake_minimum_required(VERSION 3.18)
project(ckdl C CXX)

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
)

add_library(kdl ${KDL_C_SOURCES})
target_include_directories(kdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# for tests
add_library(kdl-internal-headers INTERFACE)
target_include_directories(kdl-internal-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(src/utils)

enable_testing()
add_subdirectory(tests)
