# ckdl - KDL reading and writing for C, Python, C++

**ckdl** is a C library that implements reading and writing the
[KDL Document Language](https://github.com/kdl-org/kdl).

ckdl provides a simple “streaming“ API, inspired by SAX, instead of reading the
entire document into a data structure. While this may be inconvenient to work
with directly for most applications, the idea is to enable **bindings for
different languages** which can then read the document into a suitable data
structure relatively directly.

This repository currently contains language bindings for:

 * Python
 * C++20

For details about how to build and use ckdl, check out the
[documentation on RTD](https://ckdl.readthedocs.io/en/latest/index.html) or
under `doc/` in this repository.

### Status

ckdl is likely standard-compliant, and passes all test cases in the KDL test
suite other than those affected by these known issues:

 * Numbers that are not representable in a 64-bit signed integer (`long long`) or in a
   `double` are not parsed at all, but are validated and passed through as strings
   in whatever format they are in. It may be better to convert them to decimal
   (which is also what the test suite expects)
 * Floating-point numbers are formatted using the C `printf` facility, which, while
   correct, does not produce the shortest decimal representation.
