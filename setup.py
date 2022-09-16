from skbuild import setup

setup(
    name="ckdl",
    version="0.1a1",
    description="KDL parser and writer",
    author="Thomas Jollans",
    url="https://github.com/tjol/ckdl.git",
    classifiers = [
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3"
    ],
    license = { "file": "COPYING" },
    package_dir={"": "bindings/python/src"},
    packages=["ckdl"],
    cmake_args=[
        "-DBUILD_TESTS:BOOL=OFF",
        "-DBUILD_KDLPP:BOOL=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON",
    ],
    cmake_install_target="install-ckdl-py",
)
