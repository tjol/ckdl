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

set(KDL_COMPILE_OPTIONS)

if(MSVC)
    string(REGEX REPLACE "/W3" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    list(APPEND KDL_COMPILE_OPTIONS /wd4200 /wd4996)
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
            list(APPEND KDL_COMPILE_OPTIONS "-${flag}")
        endif()
    endforeach()
endif()

# Check for math library
include(CheckSymbolExists)
add_library(math INTERFACE)
check_symbol_exists(floor "math.h" HAVE_FLOOR)
if (NOT HAVE_FLOOR)
    message(STATUS "Trying to link with libm")
    set(CMAKE_REQUIRED_LIBRARIES m)
    check_symbol_exists(floor "math.h" HAVE_FLOOR_LIBM)
    if (NOT HAVE_FLOOR_LIBM)
        message(FATAL_ERROR "floor() missing (could not find C math library)")
    else()
        target_link_libraries(math INTERFACE m)
    endif()
endif()

# Check for anonymous unions (C11 feature)
include(CheckCSourceCompiles)
check_c_source_compiles("
struct s {
    int i;
    union {
        int j;
        long k;
    };
};
int main()
{
    struct s s = { .i = 1, .k = 0L };
    return 0;
}
" HAVE_C11_ANONYMOUS_UNIONS)

if (NOT HAVE_C11_ANONYMOUS_UNIONS)
    message(FATAL_ERROR "C compiler does not support C11 anonymous unions. "
        "Please use a newer C compiler with support for the C11 standard.")
endif()

# Check for platform-specific functions we might use
check_symbol_exists(reallocf "stdlib.h" HAVE_REALLOCF)

set(KDL_C_SOURCES
    src/bigint.c
    src/compat.c
    src/emitter.c
    src/parser.c
    src/str.c
    src/tokenizer.c
)

set(KDL_UTF8_C_SOURCES
    src/utf8.c
)

add_library(kdl-utf8 STATIC ${KDL_UTF8_C_SOURCES})
target_compile_options(kdl-utf8 PRIVATE ${KDL_COMPILE_OPTIONS})
target_include_directories(kdl-utf8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(kdl ${KDL_C_SOURCES})
target_compile_options(kdl PRIVATE ${KDL_COMPILE_OPTIONS})
target_link_libraries(kdl PRIVATE kdl-utf8 math)
target_include_directories(kdl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(kdl PRIVATE BUILDING_KDL=1 $<$<CONFIG:DEBUG>:KDL_DEBUG>)
if(HAVE_REALLOCF)
    target_compile_definitions(kdl PRIVATE -DHAVE_REALLOCF)
endif()

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(kdl PUBLIC -DKDL_STATIC_LIB=1)
    target_compile_definitions(kdl-utf8 PUBLIC -DKDL_STATIC_LIB=1)
endif()

include(GNUInstallDirs)
install(TARGETS kdl DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libkdl)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/kdl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT libkdl)

add_subdirectory(src/utils)

set(BUILD_TESTS ON CACHE BOOL "Build tests")

if(BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CMAKE_VERSION VERSION_LESS 3.12)
    message(WARNING "CMake 3.12 is required for the C++20 bindings, not building kdlpp")
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
add_subdirectory(bindings/python)
