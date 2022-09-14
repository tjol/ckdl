=====================
ckdl utility programs
=====================

ckdl comes with a few small utility programs that might be usedful during development, either of
ckdl itself, or of libraries and applications using ckdl. They are unlikely to be useful outside
of pretty low-level development.

In the build directory, the utilities may be found unter ``src/utils``.

ckdl-cat
--------

The ``ckdl-cat`` utility parses a KDL file and emits a reformatted copy of that same file.

.. code-block:: shell-session

    % cat test.kdl
    "a" 1 "2
    3" {;(r##"#type"##)child}; --b {};;
    % ckdl-cat test.kdl
    a 1 "2\n3" {
        (#type)child
    }
    --b


.. _ckdl-parse-events:

ckdl-parse-events
-----------------

The ``ckdl-parse-events`` tool reads a KDL file using the :ref:`ckdl parser <parser>`, and writes the
:ref:`events <parse events>` the parser would generate for this KDL document to standard output.

.. code-block:: shell-session

    % cat test.kdl
    node1 "arg1" 0x10 prop=(type)r#""value""#
    node2
    % ckdl-parse-events test.kdl
    KDL_EVENT_START_NODE name="node1" value=null
    KDL_EVENT_ARGUMENT value="arg1"
    KDL_EVENT_ARGUMENT value=16
    KDL_EVENT_PROPERTY name="prop" value=(type)"\"value\""
    KDL_EVENT_END_NODE value=null
    KDL_EVENT_START_NODE name="node2" value=null
    KDL_EVENT_END_NODE value=null
    KDL_EVENT_EOF value=null


ckdl-tokenize
-------------

Really only useful for hacking on ckdl itself, the ``ckdl-tokenize`` program outputs the result of
the tokenization stage of parsing a KDL document.

.. code-block:: shell-session

    % cat test.kdl
    node1   ("type")1234 \ // comment
    ;
    % ckdl-tokenize test.kdl
    KDL_TOKEN_WORD "node1"
    KDL_TOKEN_WHITESPACE "   "
    KDL_TOKEN_START_TYPE "("
    KDL_TOKEN_STRING "type"
    KDL_TOKEN_END_TYPE ")"
    KDL_TOKEN_WORD "1234"
    KDL_TOKEN_WHITESPACE " "
    KDL_TOKEN_LINE_CONTINUATION "\\"
    KDL_TOKEN_WHITESPACE " "
    KDL_TOKEN_SINGLE_LINE_COMMENT "// comment"
    KDL_TOKEN_NEWLINE "\n"
    KDL_TOKEN_SEMICOLON ""
    KDL_TOKEN_NEWLINE "\n"

