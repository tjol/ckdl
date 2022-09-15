==========================
``libkdl``: the ckdl C API
==========================

.. highlight:: c

::

    #include <kdl/kdl.h>

Basic types
-----------

Strings
^^^^^^^

ckdl uses two distinct string types, one owned and one non-owned: if you have a
:c:type:`kdl_owned_string` object, you are responsible for managing the memory, while if you have a
:c:type:`kdl_str` object, you may read the string data for a limited time, but someone else (i.e.,
the library) is going to take care of freeing the memory. In that case, you must be aware of the
string's lifetime and only access it as long as the pointer is still valid. The owned/non-owned
distinction is akin to ``String`` vs ``str`` in Rust or ``std::u8string`` vs ``std::u8string_view``
in C++, though of course in C memory management is more manual than in those languages.

All string are UTF-8.

The ckdl API mostly exposes :c:type:`kdl_str` objects with a limited lifetime. To use the string
data in your program or library, you must normally copy the strings into a
:c:type:`kdl_owned_string` using :c:func:`kdl_clone_str`, or into your own string type.

.. c:type:: struct kdl_str kdl_str

    A non-owned ("borrowed") string.

    .. c:member:: char const *data

        A pointer to the actual string data.

        .. warning::

            The string is not, in general, null-terminated. It may contain null characters.

        This pointer may be ``NULL``, which signifies the absense of a value. An empty string is
        represented by a non-NULL pointer and a :c:member:`len` of zero. (In either case the pointer
        must not be dereferenced).

    .. c:member:: size_t len

        The length of the string in bytes (not Unicode characters)


.. c:type:: struct kdl_owned_string kdl_owned_string

    An owned string object.

    .. c:member:: char *data

        The pointer to the string data.

        .. note::

            The string is null-terminated, but may also contain null characters.

    .. c:member:: size_t len

        The length of the string in bytes (not Unicode characters), not including the terminating
        nul byte.

String handling functions
"""""""""""""""""""""""""

.. c:function:: kdl_str kdl_borrow_str(kdl_owned_string const *str)

    Create a "borrowed" :c:type:`kdl_str` from an owned string.

    :param str: The owned string to "borrow"
    :return: The string, borrowed

.. c:function:: kdl_str kdl_str_from_cstr(char const *s)

    Create a :c:type:`kdl_str` from a nul-terminated C string

    :param s: The C string to use
    :return: The string, as a :c:type:`kdl_str`

.. c:function:: kdl_owned_string kdl_clone_str(kdl_str const *s)

    Create an owned string from a borrowed string (think ``strdup()``).

    :param s: The string to duplicate
    :return: A string owned by the caller with the same content

.. c:function:: void kdl_free_string(kdl_owned_string *s)

    Free a string created by :c:func:`kdl_clone_str`. This function also replaces the data pointer
    by ``NULL``.

    :param s: The string to destroy


String escaping
"""""""""""""""

ckdl provides a couple of functions to parse and to generate backslash escape sequences as defined
by the KDL spec. For generating the escapes, there are a few options:

.. c:type:: enum kdl_escape_mode kdl_escape_mode

    .. c:enumerator:: KDL_ESCAPE_MINIMAL = 0

        Only escape what *must* be escaped: ``"`` and ``\``

    .. c:enumerator:: KDL_ESCAPE_CONTROL = 0x10

        Escape ASCII control characters

    .. c:enumerator:: KDL_ESCAPE_NEWLINE = 0x20

        Escape newline characters

    .. c:enumerator:: KDL_ESCAPE_TAB = 0x40

        Escape tabs

    .. c:enumerator:: KDL_ESCAPE_ASCII_MODE =0x170

        Escape all non-ASCII charscters

    .. c:enumerator:: KDL_ESCAPE_DEFAULT = KDL_ESCAPE_CONTROL | KDL_ESCAPE_NEWLINE | KDL_ESCAPE_TAB

        "Sensible" default: escape tabs, newlines, and other control characters, but leave most
        non-ASCII Unicode intact.

.. c:function:: kdl_owned_string kdl_escape(kdl_str const *s, kdl_escape_mode mode)

    Escape special characters in a string.

    :param s: The original string
    :param mode: How to escape
    :return: A string that could be surrounded by ``""`` in a KDL file

.. c:function:: kdl_owned_string kdl_unescape(kdl_str const *s)

    Resolve backslash escape sequences

    :param s: A string that might have been surrounded by ``""`` in a KDL file
    :return: The string with all backslash escapes replaced


KDL Values
^^^^^^^^^^

In a KDL document, an argument or property of a node has a value of one of four distinct data types:
null, boolean, number, or string, optionally paired with a type annotation which may be represented
as a string. In ckdl, a value (with a optional type annotation) is represented as:

.. c:type:: struct kdl_value kdl_value

    .. c:member:: kdl_type type

        The data type of the value

    .. c:member:: kdl_str type_annotation

        The type annotation, if any. The *lack* of a type annotation is represented by a ``NULL``
        pointer in the string.

    .. c:union:: @value_union

        .. c:member:: bool boolean

            If a boolean, the value ``true`` or ``false``

        .. c:member:: kdl_number number

            If a number, the value of the number

        .. c:member:: kdl_str string

            If a string, the value of the string

Note that the string value is borrrowed, not owned. The types are:

.. c:type:: enum kdl_type kdl_type

    .. c:enumerator:: KDL_TYPE_NULL
    .. c:enumerator:: KDL_TYPE_BOOLEAN
    .. c:enumerator:: KDL_TYPE_NUMBER
    .. c:enumerator:: KDL_TYPE_STRING

KDL has one "number" type which does not map cleanly onto one fundamental type in C, so numbers
require some special treatment:

.. c:type:: struct kdl_number kdl_number

    .. c:member:: kdl_number_type type

        Enum indicating how the number is stored

    .. c:union:: @number_union

        .. c:member:: long long integer

            The number represented as a signed integer (probably 64 bits)

        .. c:member:: double floating_point

            The number represented as a double-precision floating point number (probably 64 bits)

        .. c:member:: kdl_str string

            The number represented as a string (used when the number cannot be represented exactly
            in a either long long or an double).

.. c:type:: enum kdl_number_type kdl_number_type

    .. c:enumerator:: KDL_NUMBER_TYPE_INTEGER
    .. c:enumerator:: KDL_NUMBER_TYPE_FLOATING_POINT
    .. c:enumerator:: KDL_NUMBER_TYPE_STRING_ENCODED

.. _parser:

The ckdl Parser
---------------

The ckdl parser is an event-based "streaming" parser, inspired by `SAX`_. The workflow is, at the
highest level:

#. Create a parser object for a document
#. Read all events generated by the parser until EOF
#. Destroy the parser

Typically, you will want to build up some kind of document structure in your program while reading
the parsing events. The goal of this approach is to make it easy and efficient to use ckdl to load
KDL into structures native to some other programming language or framework.

.. _SAX: https://en.wikipedia.org/wiki/Simple_API_for_XML


.. _parse events:

Parse Events
^^^^^^^^^^^^

The events produced by the ckdl parser are:

.. c:type:: enum kdl_event kdl_event

    .. c:enumerator:: KDL_EVENT_EOF

        Regular end of document (do not continue reading)

    .. c:enumerator:: KDL_EVENT_PARSE_ERROR

        Parse error (do not continue reading)

    .. c:enumerator:: KDL_EVENT_START_NODE

        Start of a node

    .. c:enumerator:: KDL_EVENT_END_NODE

        End of a node. Every :c:enumerator:`KDL_EVENT_START_NODE` is followed, some time later,
        by a :c:enumerator:`KDL_EVENT_END_NODE` (barring a parse error)

    .. c:enumerator:: KDL_EVENT_ARGUMENT

        An argument to the most recently started node

    .. c:enumerator:: KDL_EVENT_PROPERTY

        A property of the most recently started node

    .. c:enumerator:: KDL_EVENT_COMMENT = 0x10000

        Normally not produced.

        If :c:enumerator:`KDL_EMIT_COMMENTS` is enabled, regular comments give a
        :c:enumerator:`KDL_EVENT_COMMENT` event, and any arguments, properties, or nodes that have
        been commented out using a slashdash (``/-``) are rendered as their original type ORed with
        :c:enumerator:`KDL_EVENT_COMMENT` (e.g., ``KDL_EVENT_COMMENT | KDL_EVENT_START_NODE``)


Each event is associated with certain event data:

.. c:type:: struct kdl_event_data kdl_event_data

    .. c:member:: kdl_event event

        The event type

    .. c:member:: kdl_str name

       The name of the node (with :c:enumerator:`KDL_EVENT_START_NODE`), or the name of the property
       (with :c:enumerator:`KDL_EVENT_PROPERTY`)

    .. c:member:: kdl_value value

        The value of the argument (with :c:enumerator:`KDL_EVENT_ARGUMENT`) or of the property
        (with :c:enumerator:`KDL_EVENT_PROPERTY`), including any potential type annotation.

        With :c:enumerator:`KDL_EVENT_START_NODE`, if the node has a type annotation, that
        annotation is encoded here: a node ``(type)name`` is represented as ``name="name"`` and
        ``value=(type)null``. The value itself is always ``null`` for a node.

To get a feel for what exact events are generated during parsing, you may want to use the
:ref:`ckdl-parse-events` tool.

Parser Objects
^^^^^^^^^^^^^^

You can create a parser either for a document that you have in the form of a UTF-8 string

.. c:function:: kdl_parser *kdl_create_string_parser(kdl_str doc, kdl_parse_option opt)

    :param doc: The KDL document text
    :param opt: Options for the parser
    :return: A :c:type:`kdl_parser` object ready to produce parse events

or from any other data source by supplying a ``read`` function

.. c:type:: size_t (*kdl_read_func)(void *user_data, char *buf, size_t bufsize)

    Pointer to a function that reads from some source, such as a file. Should return the actual number of
    bytes read, or zero on EOF.

.. c:function:: kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt)

    :param read_func: Function to use to read the KDL text
    :param user_data: opaque pointer to pass to ``read_func``
    :param opt: Options for the parser
    :return: A :c:type:`kdl_parser` object ready to produce parse events

You always interact with the parser through an otherwise opaque pointer

.. c:type:: struct _kdl_parser kdl_parser

    Opaque struct containing parser internals

which you must free once you're done with it, using

.. c:function:: void kdl_destroy_parser(kdl_parser *parser)

    :param parser: Parser to destroy

If you wish, you may configure the parser to emit comments in addition to "regular" events

.. c:type:: enum kdl_parse_option kdl_parse_option

    .. c:enumerator:: KDL_DEFAULTS

        By default, ignore all comments

    .. c:enumerator:: KDL_EMIT_COMMENTS

        Produce events for comments and events deleted using ``/-``

The parser object provides one method:

.. c:function:: kdl_event_data *kdl_parser_next_event(kdl_parser *parser)

    Get the next event in the document from a KDL parser

    :param parser: The parser
    :return: A pointer to a parse event structure. This pointer is valid until the next call to 
             :c:func:`kdl_parser_next_event` for this parser. The next call also invalidates all
             :c:type:`kdl_str` pointers which may be contained in the event data.


.. _emitter:

The ckdl Emitter
----------------

The ckdl emitter can help you produce nicely formatted KDL documents programmatically. Similarly
to :ref:`the parser <parser>`, the emitter operates in a streaming mode: after creating the emitter,
you feed nodes, arguments and properties to the emitter one-by-one, and they will be written in the
order you supply them.

Like the parser, the emitter supports two IO models: it can either write to an internal buffer and
give you a string at the end, or it can write its data on the fly by calling a writer function you
supply.

.. c:function:: kdl_emitter *kdl_create_buffering_emitter(kdl_emitter_options const *opt)

    Create an emitter than writes into an internal buffer. Read the buffer using
    :c:func:`kdl_get_emitter_buffer` after calling :c:func:`kdl_emit_end`.

    :param opt: Emitter configuration

.. c:type:: size_t (*kdl_write_func)(void *user_data, char const *data, size_t nbytes)

    Pointer to a function that writes to some destination, such as a file. It should return exactly
    ``nbytes``, or 0 in case of an error.

.. c:function:: kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, kdl_emitter_options const *opt)

    Create an emitter that writes by calling a user-supplied function

    :param write_func: Function to call for writing
    :param user_data: First argument of write_func
    :param opt: Emitter configuration

You will interact with the emitter through a pointer to an opaque :c:type:`kdl_emitter` structure.

.. c:type:: struct _kdl_emitter kdl_emitter

    Opaque structure containing emitter internals

Once you're finished, you must clean up and free the memory used by the emitter by calling
:c:func:`kdl_destroy_emitter`.

.. c:function:: void kdl_destroy_emitter(kdl_emitter *emitter)

    :param emitter: Emitter to destroy

The emitter supports some configuration, allowing you to specify some details of the generated KDL
text.

.. c:type:: struct kdl_emitter_options kdl_emitter_options

    .. c:member:: int indent

        Number of spaces to indent child nodes by (default: 4)

    .. c:member:: kdl_escape_mode escape_mode

        Configuration for :c:func:`kdl_escape`: which characters should be escaped in regular strings?
        (default: :c:enumerator:`KDL_ESCAPE_DEFAULT`)

    .. c:member:: kdl_identifier_emission_mode identifier_mode

        How should identifiers (i.e., node names, type annotations and property keys) be rendered?
        (default: :c:enumerator:`KDL_PREFER_BARE_IDENTIFIERS`)

    .. c:member::kdl_float_printing_options float_mode

        How exactly should doubles be formatted?

.. c:type:: enum kdl_identifier_emission_mode kdl_identifier_emission_mode

    .. c:enumerator:: KDL_PREFER_BARE_IDENTIFIERS

        Traditional: quote identifiers only if absolutely necessary

    .. c:enumerator:: KDL_QUOTE_ALL_IDENTIFIERS

        Express *all* identifiers as strings

    .. c:enumerator:: KDL_ASCII_IDENTIFIERS

        Allow only ASCII in bare identifiers

.. c:type:: struct kdl_float_printing_options kdl_float_printing_options

    .. c:member:: bool always_write_decimal_point

        Write ".0" if there would otherwise be no decimal point (default: false)

    .. c:member:: bool always_write_decimal_point_or_exponent

        Write ".0" if there would otherwise be neither a decimal point nor an exponent (i.e.,
        1.0 is written as "1.0", not "1", but 1.0e+6 is written as "1e6") (default: true)

    .. c:member:: bool capital_e

        Use ``E`` instead of ``e`` to introduce the exponent. (default: false)

    .. c:member:: bool exponent_plus

        Always write the exponent with a sign: ``1e+6`` instead of ``1e6`` (default: false)

    .. c:member:: bool plus

        Always write a sign in front of the number: ``+1.0`` instead of ``1.0`` (default: false)

    .. c:member:: int min_exponent

        The absolute power of 10 above which to use scientific notation (default: 4)

.. c:var:: extern const kdl_emitter_options KDL_DEFAULT_EMITTER_OPTIONS

    Default configuration for the emitter.

The emitter has a number of methods to write KDL nodes, arguments and properties:

.. c:function:: bool kdl_emit_node(kdl_emitter *emitter, kdl_str name)

    Write a node tag

.. c:function:: bool kdl_emit_node_with_type(kdl_emitter *emitter, kdl_str type, kdl_str name)

    Write a node tag including a type annotation

.. c:function:: bool kdl_emit_arg(kdl_emitter *emitter, kdl_value const *value)

    Write an argument for a node

.. c:function:: bool kdl_emit_property(kdl_emitter *emitter, kdl_str name, kdl_value const *value)

    Write a property for a node

.. c:function:: bool kdl_start_emitting_children(kdl_emitter *emitter)

    Start a list of children for the previous node (``{``)

.. c:function:: bool kdl_finish_emitting_children(kdl_emitter *emitter)

    End the list of children (``}``)

To end the KDL document, you must call one final method

.. c:function:: bool kdl_emit_end(kdl_emitter *emitter)

    Finish the document: write a final newline if required

To get the actual KDL document text (assuming the emitter was created with
:c:func:`kdl_create_buffering_emitter`), call:

.. c:function:: kdl_str kdl_get_emitter_buffer(kdl_emitter *emitter)

    Get the internal buffer of the emitter, containing the document generated so far.

