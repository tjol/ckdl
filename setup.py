from skbuild import setup

setup(
    name="ckdl",
    package_dir={"": "bindings/python/src"},
    packages=["ckdl"],
    cmake_args=[
        "-DBUILD_TESTS:BOOL=OFF",
        "-DBUILD_KDLPP:BOOL=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON",
    ],
    cmake_install_target="install-ckdl-py",
)
