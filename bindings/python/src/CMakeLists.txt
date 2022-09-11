find_package(Cython REQUIRED)
find_package(PythonExtensions REQUIRED)

add_cython_target(ckdl_cython ckdl.pyx C PY3)
add_library(ckdl MODULE ${ckdl_cython})
target_link_libraries(ckdl kdl)
python_extension_module(ckdl)
install(TARGETS ckdl LIBRARY DESTINATION ".")
