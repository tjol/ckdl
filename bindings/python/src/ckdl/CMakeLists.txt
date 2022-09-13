find_package(Cython REQUIRED)
find_package(PythonExtensions REQUIRED)

add_cython_target(ckdl_cython _ckdl.pyx C PY3)
add_library(_ckdl MODULE ${ckdl_cython})
target_link_libraries(_ckdl kdl)
python_extension_module(_ckdl)
install(TARGETS _ckdl LIBRARY DESTINATION "bindings/python/src/ckdl" COMPONENT ckdl_py EXCLUDE_FROM_ALL)

add_custom_target(install-ckdl-py
    ${CMAKE_COMMAND}
    -DCMAKE_INSTALL_COMPONENT=ckdl_py
    -P "${PROJECT_BINARY_DIR}/cmake_install.cmake"
    DEPENDS _ckdl)
