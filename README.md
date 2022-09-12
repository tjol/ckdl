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

### Status

ckdl is in an early stage of development. It is probably standard-compliant,
and passes all test cases in the KDL test suite other than those that depend
on the exact string representation of floating-point numbers or integers
&gt; 64 bits.
