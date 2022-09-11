if(NOT SKBUILD)
    message(STATUS "Not running in scikit-build context - not building Python bindings")
    return()
endif()

add_subdirectory(src)
