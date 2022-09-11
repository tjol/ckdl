from skbuild import setup

setup(
    name="ckdl",
    packages=[],
    cmake_args=[
        "-DBUILD_TESTS:BOOL=OFF",
        "-DBUILD_KDLPP:BOOL=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON",
    ],
    cmake_install_target="install-ckdl-py",
)
