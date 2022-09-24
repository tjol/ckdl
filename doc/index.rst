=======================
ckdl - KDL library in C
=======================

**ckdl** is a C library that implements a parser and an emitter for the
`KDL Document Language`_.

ckdl provides a relatively low-level "streaming" API inspired by `SAX`_, meaning elements
of the document are provided to the application one by one on the fly, rather than reading
the entire document into some data structure. The application (or wrapper!) is then free
to load the information into a data structure of its choice.

The hope is that this approach makes ckdl well-suited as a basis for **wrappers in other
languages** that can interface with C, which would then supply their own data structures
appropriate to their particular runtime.

.. _KDL Document Language: https://github.com/kdl-org/kdl
.. _SAX: https://en.wikipedia.org/wiki/Simple_API_for_XML

The ckdl project also features simple "demo" bindings for the following languages:

* :doc:`Python 3 <ckdl-py>`
* :doc:`C++ 20 <kdlpp>`

Building ckdl
-------------

Get ckdl `from GitHub <https://github.com/tjol/ckdl>`_:

.. code-block:: shell

    git clone https://github.com/tjol/ckdl.git

ckdl is written in portable modern C, and it should ideally run anywhere you have a resonably
modern C compiler with at least a minimal standard library. The only really platform-dependent
code is in the test suite, which you don't have to build if you can't.

ckdl has been tested with:

* Linux (x86_64, x86, arm64, arm32v7l), with glibc and musl libc - going back as far as
  CentOS 6
* MacOS (arm64, x86_64)
* Windows 10 (x86_64, x86), using Microsoft Visual C++ and Mingw-w64
* FreeBSD 12 on x86_64
* NetBSD 5 on x86_64
* Illumos (OmniOS) on x86_64

To build ckdl, you will need a C compiler supporting C11 (like GCC 4.6+, Clang or Microsoft
Visual C++) and `CMake`_ version 3.8 or later. (This should be in your system package manager
if you're using Linux, BSD, or `Homebrew`_ on Mac)

.. _CMake: https://cmake.org
.. _Homebrew: https://brew.sh

To build in release mode, run, in the ckdl directory, on UNIX:

.. code-block:: shell-session

    % mkdir build
    % cd build
    % cmake ..
    % make

or on Windows:

.. code-block:: doscon

    > mkdir build
    > cd build
    > cmake ..
    > cmake --build . --config Release

The CMake scripts support a few options, including:

* ``-DBUILD_SHARED_LIBS=ON``: Build a shared library (so/dylib/DLL) instead of a static
  library
* ``-DBUILD_KDLPP=OFF``: Disable building the C++20 bindings
* ``-DBUILD_TESTS=OFF``: Disable building the test suite

To run the test suite, run ``make test`` or ``ctest`` in the build directory.

On UNIX, you also have the option of installing ckdl using ``make install`` if you really
want to, though most users will most likely just want to integrate the library into their
own build system. By default, this does not install the command-line utilities. To install
those, run ``cmake --install . --component ckdl-utils`` (possibly with the appropriate value
of the ``DESTDIR`` environment variable).

Dynamic vs static library
^^^^^^^^^^^^^^^^^^^^^^^^^

The ckdl C library ``libkdl`` supports both dynamic linking and static linking. Which option
you want to choose depends on your situation: When using C, C++, or a similar language, static
libraries can be easier to work with, while when using a dynamic runtime a dynamic library
may be your best bet (Python's ctypes and .NET's P/Invoke are examples of technologies that
as good as require the use of a dynamic library).

.. attention::

    On **Windows**, if you're using the static library, and not the DLL, you must define the
    macro ``KDL_STATIC_LIB`` before including the ckdl headers, or you'll get linking errors.

    On UNIX, this is not required, but it can't hurt in portable applications and libraries.


Contents
========

.. toctree::
    :maxdepth: 3

    ckdl-api
    ckdl-utils
    ckdl-py
    kdlpp

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
