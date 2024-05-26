cmake_minimum_required(VERSION 3.8)
project(kdl-tests C)

include(CTest)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)

# strdup is hidden behind a feature macro in glibc - check which one we need
include(CheckSymbolExists)
check_symbol_exists(strdup "string.h" HAVE_STRDUP)
if(NOT HAVE_STRDUP)
    message(STATUS "Trying -D_DEFAULT_SOURCE")
    set(CMAKE_REQUIRED_DEFINITIONS -D_DEFAULT_SOURCE)
    check_symbol_exists(strdup "string.h" HAVE_STRDUP_DEFAULT_SOURCE)
    if(HAVE_STRDUP_DEFAULT_SOURCE)
        add_definitions(-D_DEFAULT_SOURCE)
    else()
        # Older glibc versions (e.g. CentOS 7) don't have _DEFAULT_SOURCE
        # and require both _BSD_SOURCE and _POSIX_C_SOURCE for differnt
        # functions (_BSD_SOURCE has been deprecated since)
        message(STATUS "Trying -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L")
        set(CMAKE_REQUIRED_DEFINITIONS -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L)
        check_symbol_exists(strdup "string.h" HAVE_STRDUP_BSD_SOURCE)
        if(HAVE_STRDUP_BSD_SOURCE)
            add_definitions(-D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L)
        else()
            message(FATAL_ERROR "strdup function not found")
        endif()
    endif()
endif()

# Windows doesn't have unistd.h
check_symbol_exists(chdir "unistd.h" CHDIR_IN_UNISTD_H)
if(CHDIR_IN_UNISTD_H)
    add_definitions(-DCHDIR_IN_UNISTD_H)
else()
    check_symbol_exists(chdir "direct.h" CHDIR_IN_DIRECT_H)
    if(CHDIR_IN_DIRECT_H)
        add_definitions(-DCHDIR_IN_DIRECT_H)
    else()
        message(FATAL_ERROR "Could not find chdir()")
    endif()
endif()

# Check for dirent (POSIX) and related features
check_symbol_exists(readdir "dirent.h" HAVE_DIRENT LANGUAGE C)
if(HAVE_DIRENT)
    set(USE_DIRENT ON)

    # Check for fstatat() (modern POSIX, but may be behind a feature test macro)
    # Note: Illumos (OmniOS r151038 LTS) has a bug where you can't include
    #       sys/stat.h or fcntl.h on their own (hence the stdlib.h here)
    check_symbol_exists(fstatat "stdlib.h;sys/stat.h" HAVE_FSTATAT)
    if(NOT HAVE_FSTATAT)
        # It may be behind _ATFILE_SOURCE (e.g. on Illumos)
        message(STATUS "Trying -D_ATFILE_SOURCE")
        list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_ATFILE_SOURCE)
        check_symbol_exists(fstatat "stdlib.h;sys/stat.h"  HAVE_FSTATAT_ATFILE)
        if(HAVE_FSTATAT_ATFILE)
            add_definitions(-D_ATFILE_SOURCE)
        else()
            set(USE_DIRENT OFF)
        endif()
    endif()

endif()

if(USE_DIRENT)
    add_definitions(-DHAVE_DIRENT)

    # Check for optional BSD dirent attributes

    # File type field (Linux, BSD incl. macOS)
    CHECK_STRUCT_HAS_MEMBER("struct dirent" d_type "dirent.h" HAVE_DIRENT_D_TYPE LANGUAGE C)
    if(HAVE_DIRENT_D_TYPE)
        add_definitions(-DHAVE_DIRENT_D_TYPE)
    endif()
    # Name length field (BSD incl. macOS)
    CHECK_STRUCT_HAS_MEMBER("struct dirent" d_namlen dirent.h HAVE_DIRENT_D_NAMLEN LANGUAGE C)
    if(HAVE_DIRENT_D_NAMLEN)
        add_definitions(-DHAVE_DIRENT_D_NAMLEN)
    endif()
else()
    check_symbol_exists(FindFirstFileA "windows.h" HAVE_WIN32_FILE_API)
    if(HAVE_WIN32_FILE_API)
        add_definitions(-DHAVE_WIN32_FILE_API)
    else()
        message(FATAL_ERROR "No usable filesystem API found, cannot build tests!")
    endif()
endif()

add_library(test_util STATIC test_util.c fs_util.c)
target_compile_options(test_util PUBLIC ${KDL_COMPILE_OPTIONS})
target_include_directories(test_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(utf8_test utf8_test.c)
target_link_libraries(utf8_test kdl kdl-utf8 test_util)
add_test(utf8_test utf8_test)

add_executable(parser_test parser_test.c)
target_link_libraries(parser_test kdl test_util)
add_test(parser_test parser_test)

add_executable(emitter_test emitter_test.c)
target_link_libraries(emitter_test kdl test_util)
add_test(emitter_test emitter_test)

add_executable(kdlv2_test kdlv2_test.c)
target_link_libraries(kdlv2_test kdl test_util)
add_test(kdlv2_test kdlv2_test)

#################################################
# Upstream test suite for KDL version 1.0.0
####
set(KDL_TEST_CASES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/test_documents/upstream/1.0.0)
# Ignore some tests which require unsupported number representations
set(FUZZY_KDL_TESTS_LIST_V1
    no_decimal_exponent.kdl # float representation not consistent with other test cases
)
string(REPLACE ";" ":" FUZZY_KDL_TESTS_V1 "${FUZZY_KDL_TESTS_LIST_V1}")

add_executable(example_doc_test_v1 example_doc_test.c)
target_link_libraries(example_doc_test_v1 kdl test_util ckdl-cat)
target_compile_definitions(example_doc_test_v1 PRIVATE
    "KDL_TEST_CASES_ROOT=\"${KDL_TEST_CASES_ROOT}\""
    "FUZZY_KDL_TESTS=\"${FUZZY_KDL_TESTS_V1}\""
    "KDL_VERSION=KDL_VERSION_1")
add_test(NAME example_doc_test_v1 COMMAND "$<TARGET_FILE:example_doc_test_v1>" "${KDL_TEST_CASES_ROOT}")
#################################################
# Upstream test suite for KDL version 2.0.0
####
set(KDL_TEST_CASES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/test_documents/upstream/2.0.0.draft)
# Ignore some tests which require unsupported number representations
set(FUZZY_KDL_TESTS_LIST_V2
    no_decimal_exponent.kdl # float representation not consistent with other test cases
)
string(REPLACE ";" ":" FUZZY_KDL_TESTS_V2 "${FUZZY_KDL_TESTS_LIST_V2}")

add_executable(example_doc_test_v2 example_doc_test.c)
target_link_libraries(example_doc_test_v2 kdl test_util ckdl-cat)
target_compile_definitions(example_doc_test_v2 PRIVATE
    "KDL_TEST_CASES_ROOT=\"${KDL_TEST_CASES_ROOT}\""
    "FUZZY_KDL_TESTS=\"${FUZZY_KDL_TESTS_V2}\""
    "KDL_VERSION=KDL_VERSION_2")
# add_test(NAME example_doc_test_v2 COMMAND "$<TARGET_FILE:example_doc_test_v2>" "${KDL_TEST_CASES_ROOT}")
#################################################

if (WIN32 AND BUILD_SHARED_LIBS)
    # Copy kdl.dll to the test folder so that the tests work
    add_custom_command(TARGET example_doc_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kdl> $<TARGET_FILE_DIR:example_doc_test>)
endif()
