find_package(Cython REQUIRED)
find_package(PythonExtensions REQUIRED)

add_cython_target(ckdl_cython ckdl.pyx C PY3)
add_library(ckdl MODULE ${ckdl_cython})
target_link_libraries(ckdl kdl)
python_extension_module(ckdl)
install(TARGETS ckdl LIBRARY DESTINATION "." COMPONENT ckdl_py EXCLUDE_FROM_ALL)

add_custom_target(install-ckdl-py
    ${CMAKE_COMMAND}
    -DCMAKE_INSTALL_COMPONENT=ckdl_py
    -P "${PROJECT_BINARY_DIR}/cmake_install.cmake"
    DEPENDS ckdl)
