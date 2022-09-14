=======================
``ckdl`` Python Package
=======================

.. highlight:: shell-session

Build and install the ``ckdl`` Python Package from source using ``pip``::

    % pip install . # where "." is the path to the ckdl sources

or build a `wheel <PEP 427>`_ with

::

    % python3 setup.py bdist_wheel

or your favourite `PEP 517`_ compatible tool, such as `build`_.

.. note::

    As always, it is recommended to work in a `venv`_

.. _PEP 427: https://peps.python.org/pep-0427/
.. _PEP 517: https://peps.python.org/pep-0517/
.. _build: https://pypa-build.readthedocs.io/en/latest/
.. _venv: https://docs.python.org/3/library/venv.html

.. contents::

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
    ...     ckdl.Node("node1", args=["argument 1", 2, None], properties={"node1-prop": 0xff_ff}, children=[
    ...         ckdl.Node("child1"),
    ...         ckdl.Node("child2-type", "child2", child2_prop=True)
    ...     ]),
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

    >>>


API
^^^

.. highlight:: python3

.. py:module:: ckdl

.. py:currentmodule:: ckdl

The ``ckdl`` package is relatively simple. It provides one function

.. py:function:: parse(kdl_doc)

    Parse a KDL document

    :param kdl_doc: The KDL document to parse
    :type kdl_doc: str
    :rtype: Document

and three classes to represent KDL data:

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

        :type: str or NoneType

    .. py:attribute:: name

        :type: str

    .. py:attribute:: args

        :type: list

    .. py:attribute:: properties

        :type: dict

    .. py:attribute:: children

        :type: list of Node

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

    A KDL document, consisting of zero or more nodes.

    .. py:method:: dump(self)

        Serialize the document to KDL

    .. py:method:: __str__(self)

        See dump()

