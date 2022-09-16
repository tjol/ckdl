==================================
``kdlpp``: C++20 bindings for ckdl
==================================

``kdlpp`` is a simple C++20 wrapper around ``libkdl`` that interprets the raw parser events
and build up a KDL documemnt object model using C++ the classes :cpp:class:`kdl::Document`,
:cpp:class:`kdl::Node`, :cpp:class:`kdl::Value`, and :cpp:class:`kdl::Number`.

``kdlpp`` is built together with ``libkdl`` by default, assuming you have a sufficiently
recent C++ compiler. It makes use of ``u8string``  and ``u8string_view``, which were introduced
in C++20, for correct UTF-8 support.

The KDL C++20 API
-----------------

A Brief Tour
^^^^^^^^^^^^

Reading
"""""""

.. literalinclude:: ../bindings/cpp/tests/kdlpp_test.cpp
    :language: cpp
    :start-after: begin kdlpp reading demo
    :end-before: end kdlpp reading demo
    :dedent:


Writing
"""""""

.. literalinclude:: ../bindings/cpp/tests/kdlpp_test.cpp
    :language: cpp
    :start-after: begin kdlpp writing demo
    :end-before: end kdlpp writing demo
    :dedent:

API
^^^

.. code-block:: cpp

    #include <kdlpp.h>

For the actual API, please consult `the header file, kdlpp.h <https://github.com/tjol/ckdl/blob/main/bindings/cpp/include/kdlpp.h>`_, directly.
