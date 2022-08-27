# kdl tests

message(CHECK_START "Looking for Criterion")
list(APPEND CMAKE_MESSAGE_INDENT "  ")

# try to find Criterion
find_package(Criterion QUIET)
if(TARGET Criterion::criterion)
    set(CRITERION_TARGET Criterion::criterion)
elseif(Criterion_FOUND)
    message(WARNING "Criterion found but target Criterion::criterion does not exist")
endif()

if(NOT CRITERION_TARGET)
    message(STATUS "Trying pkg-config")
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(Criterion IMPORTED_TARGET criterion)
        if(TARGET PkgConfig::Criterion)
            set(CRITERION_TARGET PkgConfig::Criterion)
        endif()
    endif()
endif()

# Do we have Criterion?
if(NOT CRITERION_TARGET)
    # find libcriterion.so
    message(CHECK_START "Checking for -lcriterion")
    find_library(CRITERION criterion)
    if(NOT ${CRITERION} STREQUAL CRITERION-NOTFOUND)
        message(CHECK_PASS "found ${CRITERION}")

        set(CMAKE_REQUIRED_LIBRARIES ${CRITERION})
        include(CheckIncludeFile)
        check_include_file(criterion/criterion.h CRITERION_H_FOUND)
        if(CRITERION_H_FOUND)
            add_library(_criterion INTERFACE IMPORTED)
            target_link_libraries(_criterion PUBLIC ${CRITERION})
            set(CRITERION_TARGET _criterion)
        endif()
    else()
        message(CHECK_FAIL "not found")
    endif()
endif()

# Do we have Criterion *now*?
list(POP_BACK CMAKE_MESSAGE_INDENT)
if(NOT CRITERION_TARGET)
    message(CHECK_FAIL "not found")
    message(WARNING "Criterion not found, not building tests")
    return()
else()
    message(CHECK_PASS "ok")
endif()

include(CTest)

add_executable(utf8_test utf8_test.c)
target_link_libraries(utf8_test kdl kdl-internal-headers ${CRITERION_TARGET})
add_test(utf8_test utf8_test)
