cmake_minimum_required(VERSION 3.18)
project(kdl-tests C CXX)

include(CTest)

# Download test data
if(NOT KDL_TEST_CASES_ROOT)
    message(STATUS "Downloading KDL test cases from GitHub")

    set(KDL_GIT_URL "https://github.com/kdl-org/kdl.git")
    set(KDL_TGZ_URL "https://github.com/kdl-org/kdl/archive/refs/heads/main.tar.gz")
    find_package(Git)

    if(Git_FOUND)
        set(CHECKOUT "${CMAKE_CURRENT_BINARY_DIR}/kdl")
        if(EXISTS "${CHECKOUT}/.git/HEAD")
            # already cloned, do a pull
            execute_process(COMMAND ${GIT_EXECUTABLE} -C "${CHECKOUT}" pull
                RESULT_VARIABLE GIT_PULL_RESULT)
            if(NOT GIT_PULL_RESULT EQUAL 0)
                message(WARNING "git pull failed")
            endif()
        else()
            execute_process(COMMAND ${GIT_EXECUTABLE} clone ${KDL_GIT_URL} "${CHECKOUT}"
                RESULT_VARIABLE GIT_CLONE_RESULT)
            if(NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "git clone failed")
            endif()
        endif()
        set(KDL_TEST_CASES_ROOT "${CHECKOUT}/tests/test_cases" CACHE PATH "kdl-org test cases location")
    else()
        set(TGZ_FILE ${CMAKE_CURRENT_BINARY_DIR}/kdl.tar.gz)
        if(EXISTS ${TGZ_FILE})
            message(STATUS "${TGZ_FILE} already exists")
        else()
            file(DOWNLOAD ${KDL_TGZ_URL} ${TGZ_FILE}.dl STATUS DOWNLOAD_STATUS)
            list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
            if(DOWNLOAD_STATUS_CODE EQUAL 0)
                file(RENAME ${TGZ_FILE}.dl ${TGZ_FILE})
            else()
                message(FATAL_ERROR "Download failed: ${DOWNLOAD_STATUS}")
            endif()
        endif()

        file(ARCHIVE_EXTRACT INPUT ${TGZ_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        set(KDL_TEST_CASES_ROOT "${CMAKE_CURRENT_BINARY_DIR}/kdl-main/tests/test_cases" CACHE PATH "kdl-org test cases location")
    endif()
endif()

add_library(test_util OBJECT test_util.c)
target_include_directories(test_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(utf8_test utf8_test.c)
target_link_libraries(utf8_test kdl kdl-internal-headers test_util)
add_test(utf8_test utf8_test)

add_executable(parser_test parser_test.c)
target_link_libraries(parser_test kdl test_util)
add_test(parser_test parser_test)

add_executable(emitter_test emitter_test.c)
target_link_libraries(emitter_test kdl test_util)
add_test(emitter_test emitter_test)

add_executable(example_doc_test example_doc_test.c)
target_link_libraries(example_doc_test kdl test_util ckdl-cat)
target_compile_definitions(example_doc_test PRIVATE "KDL_TEST_CASES_ROOT=\"${KDL_TEST_CASES_ROOT}\"")
add_test(NAME example_doc_test COMMAND "$<TARGET_FILE:example_doc_test>" "${KDL_TEST_CASES_ROOT}")
