# ckdl - KDL reading and writing for C, Python, C++

**ckdl** is a C (C11) library that implements reading and writing the
[KDL Document Language](https://github.com/kdl-org/kdl).

ckdl provides a simple “streaming“ API, inspired by SAX, instead of reading the
entire document into a data structure. While this may be inconvenient to work
with directly for most applications, the idea is to enable **bindings for
different languages** which can then read the document into a suitable data
structure relatively directly.

This repository currently contains language bindings for:

 * Python
 * C++20

The C and C++ parts are built with CMake.

To install the Python bindings, run

    pip install ckdl

For more details about how to build and use ckdl, check out the
[documentation on RTD](https://ckdl.readthedocs.io/en/latest/index.html) or
under `doc/` in this repository.

### Status

ckdl passes all test cases in the KDL test suite except for one where a number is
formatted a little differently.

It's reasonable to suspect that ckdl is *close* to being standard-compliant, but
of course it's always possible there are bugs affecting some finer details.
