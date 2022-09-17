=======================
``ckdl`` Python Package
=======================

.. highlight:: shell-session

Download and install the `ckdl <https://pypi.org/project/ckdl/>`_
Python Package using ``pip``::

    % pip install ckdl

You can also build and install from source using ``pip``::

    % pip install path/to/ckdl/source

or some other `PEP 517`_ compatible tool.

.. note::

    As always, it is recommended to work in a `venv`_

.. _PEP 517: https://peps.python.org/pep-0517/
.. _venv: https://docs.python.org/3/library/venv.html

The Python ``ckdl`` API
-----------------------

A Brief Tour
^^^^^^^^^^^^

Reading
"""""""

.. code-block:: pycon

    >>> import ckdl
    >>> kdl_txt = """
    ... node1 1 2 "three" (f64)4.0 {
    ...     (some-type)"first child"
    ...     child-#2 prop="val"
    ... }
    ... """
    >>> doc = ckdl.parse(kdl_txt)
    >>> doc
    <Document; 1 nodes>
    >>> doc.nodes
    [<Node node1; 4 args; 2 children>]
    >>> doc.nodes[0]
    <Node node1; 4 args; 2 children>
    >>> doc[0].type_annotation
    >>> doc[0].args
    [1, 2, 'three', <Value (f64)4>]
    >>> type(doc[0].args[0])
    <class 'int'>
    >>> type(doc[0].args[3])
    <class 'ckdl._ckdl.Value'>
    >>> doc[0].args[3]
    <Value (f64)4>
    >>> doc[0].args[3].type_annotation
    'f64'
    >>> doc[0].children
    [<Node (some-type)"first child">, <Node child-#2; 1 property>]
    >>> doc[0].children[0].type_annotation
    'some-type'
    >>> doc[0].children[1].properties
    {'prop': 'val'}

Writing
"""""""

.. code-block:: pycon

    >>> mydoc = ckdl.Document(
    ...     ckdl.Node("node1", args=["argument 1", 2, None],
    ...                        properties={"node1-prop": 0xff_ff},
    ...                        children=[
    ...                            ckdl.Node("child1"),
    ...                            ckdl.Node("child2-type", "child2", child2_prop=True)
    ...                        ]),
    ...     ckdl.Node(None, "node2", "arg1", "arg2", ckdl.Node("child3"), some_property="foo")
    ... )
    >>> print(str(mydoc))
    node1 "argument 1" 2 null node1-prop=65535 {
        child1
        (child2-type)child2 child2_prop=true
    }
    node2 "arg1" "arg2" some_property="foo" {
        child3
    }


API
^^^

.. highlight:: python3

.. py:module:: ckdl

.. py:currentmodule:: ckdl

The ``ckdl`` package is relatively simple. It provides one function to parse KDL,
three classes to represent data, and some classes to optionally configure the
emitter.

Parsing
"""""""

.. py:function:: parse(kdl_doc)

    Parse a KDL document

    :param kdl_doc: The KDL document to parse
    :type kdl_doc: str
    :rtype: Document
    :raises: :py:exc:`ParseError`

.. py:exception:: ParseError

    Thrown by :py:func:`parse` when the CKDL parser cannot parse the document (generally
    because it's ill-formed).

Data types
""""""""""

.. py:class:: Value(type_annotation : str, value)

    A KDL value with a type annotation.

    Values without a type annotation are represented as NoneType, bool, int, float, or str.

    .. py:attribute:: type_annotation

        The type annotation of the value

        :type: str

    .. py:attribute:: value

        The actual value

.. py:class:: Node

    A KDL node, with its arguments, properties and children

    .. py:attribute:: type_annotation

        Type annotation as str or NoneType

    .. py:attribute:: name

        Node name - str

    .. py:attribute:: args

        Node args - list

    .. py:attribute:: properties

        Node properties - dict

    .. py:attribute:: children

        Child nodes - list of :py:class:`Node`

    The Node constructor supports a number of different signatures.

    If the first two arguments are strings, or None and a string, they are interpreted as the
    type annotation and the node tag name. Then, either:

    * | ``Node([type_annotation,] name, *args, *children, **properties)``
      | the remaining positional arguments are all the node arguments, followed by the child nodes,
        and the keyword arguments are the properties, or
    * | ``Node([type_annotation,] name, [args, [children, ]] *, **properties)``
      | the next positional arguments are lists of all the arguments and children, and the keyword
        arguments are the properties, or
    * | ``Node([type_annotation,] name, [args=..., [children=..., ]] *, [properties=...])``
      | the properties are passed as a dict in the ``properties`` keyword argument, the arguments
        are passed as a list either in the ``args`` keyword argument, or the positional argument
        after the tag name, and the children are similarly passed as a list, either in the
        ``children`` keyword argument, or in the positional argument following the node arguments.


    Note that when the node arguments are given as positional arguments, and the first argument is a
    string, the type annotation cannot be omitted (``Node("name", "arg", 1)`` is ``(name)arg 1``, and
    ``Node(None, "name", "arg", 1)`` is ``name "arg" 1``, but ``Node("name", 1, 2)`` is ``name 1 2``).

.. py:class:: Document(nodes)

    A KDL document, consisting of zero or more nodes.\

    .. py:attribute:: nodes

        The top-level nodes in the document - list of :py:class:`Node`

    .. py:method:: dump(self[, opts : EmitterOptions])

        Serialize the document to KDL

        :param opts: (optional) Options for the ckdl emitter

    .. py:method:: __str__(self)

        See dump()

Emitter configuration
"""""""""""""""""""""

.. py:class:: EmitterOptions(*, indent=None, escape_mode=None, identifier_mode=None, float_mode=None)

    .. py:attribute:: indent

        Number of spaces to indent child nodes by (default: 4)

        :type: int

    .. py:attribute:: escape_mode

        Which characters should be escaped in regular strings?

        :type: EscapeMode

    .. py:attribute:: identifier_mode

        How should identifiers (i.e., node names, type annotations and property keys) be rendered?

        :type: IdentifierMode

    .. py:attribute:: float_mode

        How exactly should doubles be formatted?

        :type: FloatMode

.. py:class:: EscapeMode

    Enum

    .. py:attribute:: minimal
    .. py:attribute:: control
    .. py:attribute:: newline
    .. py:attribute:: tab
    .. py:attribute:: ascii_mode
    .. py:attribute:: default

.. py:class:: IdentifierMode

    Enum

    .. py:attribute:: prefer_bare_identifiers
    .. py:attribute:: quote_all_identifiers
    .. py:attribute:: ascii_identifiers


.. py:class:: FloatMode(*, always_write_decimal_point=None, always_write_decimal_point_or_exponent=None, capital_e=None, exponent_plus=None, plus=None, min_exponent=None)

    .. py:attribute:: always_write_decimal_point

        :type: bool

    .. py:attribute:: always_write_decimal_point_or_exponent

        :type: bool

    .. py:attribute:: capital_e

        :type: bool

    .. py:attribute:: exponent_plus

        :type: bool

    .. py:attribute:: plus

        :type: bool

    .. py:attribute:: min_exponent

        :type: int

