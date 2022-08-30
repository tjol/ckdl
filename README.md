# ckdl - KDL reading and writing for C, C++

**ckdl** is a C library that implements reading and writing the
[KDL Document Language](https://github.com/kdl-org/kdl).

ckdl provides a simple “streaming“ API, inspired by SAX, instead of reading the
entire document into a data structure. While this may be inconvenient to work
with directly for most applications, the idea is to enable **bindings for
different languages** which can then read the document into a suitable data
structure relatively directly.

This repository currently contains language bindings for:

 * C++

### Status

ckdl is in the very early stages of development and not currently standard
compliant, though it should be able to read most documents.
