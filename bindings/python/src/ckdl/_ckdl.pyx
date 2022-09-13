from ._libkdl cimport *
from libc.limits cimport LLONG_MIN, LLONG_MAX

cdef str _kdl_str_to_py_str(const kdl_str* s):
    return s.data[:s.len].decode("utf-8")

cdef kdl_owned_string _py_str_to_kdl_str(str s):
    cdef kdl_str borrowed
    b = s.encode("utf-8")
    borrowed.data = b
    borrowed.len = len(b)
    return kdl_clone_str(&borrowed)

cdef class Value:
    """A KDL value with a type annotation"""
    cdef public str type_annotation
    cdef public object value

    def __init__(self, str type_annotation, value):
        self.type_annotation = type_annotation
        self.value = value

    def __repr__(self):
        return "<Value {}>".format(self)

    def __str__(self):
        return str(Document(Node("-", [self])))[2:].strip()

cdef _convert_kdl_value_no_type(const kdl_value* v):
    if v.type == KDL_TYPE_NULL:
        return None
    elif v.type == KDL_TYPE_BOOLEAN:
        return v.boolean
    elif v.type == KDL_TYPE_NUMBER:
        if v.number.type == KDL_NUMBER_TYPE_INTEGER:
            return v.number.integer
        elif v.number.type == KDL_NUMBER_TYPE_FLOATING_POINT:
            return v.number.floating_point
        elif v.number.type == KDL_NUMBER_TYPE_STRING_ENCODED:
            # FIXME - handle big numbers correctly
            return _kdl_str_to_py_str(&v.number.string)
    elif v.type == KDL_TYPE_STRING:
        return _kdl_str_to_py_str(&v.string)
    raise RuntimeError("Invalid kdl_value object!")

cdef _convert_kdl_value(const kdl_value* v):
    if v.type_annotation.data == NULL:
        # No type
        return _convert_kdl_value_no_type(v)
    else:
        return Value(_kdl_str_to_py_str(&v.type_annotation), _convert_kdl_value_no_type(v))


cdef class Node:
    """
    A KDL node, with its arguments, properties and children.

    The constructor supports a variety of signatures:

    >>> Node("tag")
    <Node tag>
    >>> Node("type-annotation", "tag")
    <Node (type-annotation)tag>
    >>> Node("type-annotation", "tag", "arg1", "arg2")
    <Node (type-annotation)tag; 2 args>
    >>> Node("tag", ["arg1", "arg2"])
    <Node tag; 2 args>
    >>> Node(None, "tag", 1, 2, 3)
    <Node tag; 3 args>
    >>> Node(None, "tag", 1, 2, 3, prop1=Value("f32", 1.2))
    <Node tag; 3 args; 1 property>
    >>> Node(None, "tag", "arg1", Node("child1"), Node("child2"), prop=1)
    <Node tag; 1 arg; 1 property; 2 children>
    >>> Node("tag", args=[1,2,3], properties={"key": "value"}, children=[Node("child")])
    <Node tag; 3 args; 1 property; 1 child>
    >>>
    """

    cdef public str type_annotation
    cdef public str name
    cdef public list args
    cdef public dict properties
    cdef public list children

    def __init__(self, *args, **kwargs):
        # Default constructor
        self.args = []
        self.properties = {}
        self.children = []
        if len(args) == 0 and len(kwargs) == 0:
            return

        if len(args) >= 2 and (args[0] is None or isinstance(args[0], str)) and isinstance(args[1], str):
            # type and name
            self.type_annotation = args[0]
            self.name = args[1]
            args = args[2:]
        else:
            # name only
            if len(args) < 1 or not isinstance(args[0], str):
                raise ValueError("Expected node name")
            self.name = args[0]
            args = args[1:]

        # parse args and children from remaining positionals
        if len(args) == 1 and isinstance(args[0], list):
            args = args[0]
        seen_nodes = False
        for obj in args:
            if isinstance(obj, Node):
                seen_nodes = True
                self.children.append(obj)
            elif not seen_nodes:
                self.args.append(obj)
            else:
                raise ValueError("Non-Node ({}) not allowed after first child".format(type(obj)))

        # parse keywords args - either properties or special arguments
        seen_special_kwarg = False
        if "args" in kwargs:
            seen_special_kwarg = True
            if self.args:
                raise ValueError("Args given both as positional args and as keyword arg")
            else:
                self.args = kwargs["args"]
                del kwargs["args"]
        if "properties" in kwargs:
            seen_special_kwarg = True
            self.properties = kwargs["properties"]
            del kwargs["properties"]
        if "children" in kwargs:
            seen_special_kwarg = True
            if self.children:
                raise ValueError("Children given both as positional args and as keyword arg")
            else:
                self.children = kwargs["children"]
                del kwargs["children"]

        if seen_special_kwarg and len(kwargs) != 0:
            raise ValueError("Unknown keyword arguments: {}".format(", ".join(kwargs.keys())))
        elif len(kwargs) != 0:
            self.properties = kwargs

    def __repr__(self):
        cdef str node_name_kdl = str(Node(self.type_annotation, self.name)).strip()
        cdef str node_details = ""
        if len(self.args) == 1:
            node_details += "; 1 arg".format(len(self.args))
        elif len(self.args) > 1:
            node_details += "; {} args".format(len(self.args))
        if len(self.properties) == 1:
            node_details += "; 1 property".format(len(self.properties))
        elif len(self.properties) > 1:
            node_details += "; {} properties".format(len(self.properties))
        if len(self.children) == 1:
            node_details += "; 1 child".format(len(self.children))
        elif len(self.children) > 1:
            node_details += "; {} children".format(len(self.children))
        return "<Node {}{}>".format(node_name_kdl, node_details)

    def __str__(self):
        return str(Document(self))

cdef kdl_value _make_kdl_value(value, kdl_owned_string* tmp_str_t, kdl_owned_string* tmp_str_v):
    cdef kdl_value result
    result.type_annotation.data = NULL
    result.type_annotation.len = 0
    if isinstance(value, Value):
        # We have a type annotation
        if tmp_str_t == NULL:
            raise RuntimeError("Nested Value objects are not allowed")
        tmp_str_t[0] = _py_str_to_kdl_str(value.type_annotation)
        result = _make_kdl_value(value.value, NULL, tmp_str_v)
        result.type_annotation = kdl_borrow_str(tmp_str_t)
    elif value is None:
        result.type = KDL_TYPE_NULL
    elif isinstance(value, bool):
        result.type = KDL_TYPE_BOOLEAN
        result.boolean = value
    elif isinstance(value, int):
        # does it fit in a long long?
        result.type = KDL_TYPE_NUMBER
        if LLONG_MIN <= value <= LLONG_MAX:
            result.number.type = KDL_NUMBER_TYPE_INTEGER
            result.number.integer = value
        else:
            result.number.type = KDL_NUMBER_TYPE_STRING_ENCODED
            tmp_str_v[0] = _py_str_to_kdl_str(str(value))
            result.number.string = kdl_borrow_str(tmp_str_v)
    elif isinstance(value, float):
        result.type = KDL_TYPE_NUMBER
        result.number.type = KDL_NUMBER_TYPE_FLOATING_POINT
        result.number.floating_point = value
    elif isinstance(value, str):
        tmp_str_v[0] = _py_str_to_kdl_str(value)
        result.type = KDL_TYPE_STRING
        result.string = kdl_borrow_str(tmp_str_v)
    else:
        raise TypeError(str(type(value)))

    return result

cdef bint _emit_arg(kdl_emitter *emitter, arg) except False:
    cdef kdl_owned_string type_str
    cdef kdl_owned_string val_str
    cdef kdl_value v
    type_str.data = NULL
    val_str.data = NULL

    v = _make_kdl_value(arg, &type_str, &val_str)
    kdl_emit_arg(emitter, &v)

    kdl_free_string(&type_str)
    kdl_free_string(&val_str)

    return True

cdef bint _emit_prop(kdl_emitter *emitter, key, value) except False:
    cdef kdl_owned_string name_str
    cdef kdl_owned_string type_str
    cdef kdl_owned_string val_str
    cdef kdl_value v
    name_str.data = NULL
    type_str.data = NULL
    val_str.data = NULL

    name_str = _py_str_to_kdl_str(key)
    v = _make_kdl_value(value, &type_str, &val_str)
    kdl_emit_property(emitter, kdl_borrow_str(&name_str), &v)

    kdl_free_string(&name_str)
    kdl_free_string(&type_str)
    kdl_free_string(&val_str)

    return True

cdef bint _emit_node(kdl_emitter *emitter, Node node) except False:
    cdef kdl_owned_string name
    cdef kdl_owned_string type_annotation

    name = _py_str_to_kdl_str(node.name)
    if node.type_annotation is not None:
        type_annotation = _py_str_to_kdl_str(node.type_annotation)
        kdl_emit_node_with_type(emitter, kdl_borrow_str(&type_annotation), kdl_borrow_str(&name))
        kdl_free_string(&type_annotation)
    else:
        kdl_emit_node(emitter, kdl_borrow_str(&name))

    kdl_free_string(&name)

    for arg in node.args:
        _emit_arg(emitter, arg)

    for key, value in node.properties.items():
        _emit_prop(emitter, key, value)

    if node.children:
        kdl_start_emitting_children(emitter)
        for child in node.children:
            _emit_node(emitter, child)
        kdl_finish_emitting_children(emitter)

    return True

cdef class Document:
    """
    A KDL document, consisting of zero or more nodes (Node objects).
    """

    cdef public list nodes

    def __init__(self, *args):
        if len(args) == 1 and isinstance(args[0], list):
            self.nodes = args[0]
        else:
            self.nodes = list(args)

    def __repr__(self):
        return "<Document; {} nodes>".format(len(self.nodes))

    def __iter__(self):
        return iter(self.nodes)

    def __len__(self):
        return len(self.nodes)

    def __getitem__(self, i):
        return self.nodes[i]

    def __str__(self):
        return self.dump()

    def dump(self):
        """Convert to KDL"""
        cdef kdl_emitter *emitter
        cdef kdl_emitter_options opts
        cdef kdl_str buf
        cdef str doc

        opts.indent = 4
        opts.escape_mode = KDL_ESCAPE_DEFAULT
        opts.identifier_mode = KDL_PREFER_BARE_IDENTIFIERS

        emitter = kdl_create_buffering_emitter(opts)

        for node in self.nodes:
            _emit_node(emitter, node)

        kdl_emit_end(emitter)
        buf = kdl_get_emitter_buffer(emitter)
        doc = _kdl_str_to_py_str(&buf)

        kdl_destroy_emitter(emitter)
        return doc


def parse(str kdl_text):
    """
    parse(kdl_text)

    Parse a KDL document (must be a str) and return a Document.
    """

    cdef kdl_event_data* ev
    cdef kdl_parser* parser
    cdef kdl_str kdl_doc
    cdef list root_node_list = []
    cdef list stack = []
    cdef list nodes = root_node_list
    cdef Node current_node = None

    byte_str = kdl_text.encode("utf-8")
    kdl_doc.data = byte_str
    kdl_doc.len = len(byte_str)
    parser = kdl_create_string_parser(kdl_doc, KDL_DEFAULTS)

    while True:
        ev = kdl_parser_next_event(parser)

        if ev.event == KDL_EVENT_EOF:
            kdl_destroy_parser(parser)
            return Document(root_node_list)
        elif ev.event == KDL_EVENT_PARSE_ERROR:
            raise RuntimeError(_kdl_str_to_py_str(&ev.value.string))
        elif ev.event == KDL_EVENT_START_NODE:
            current_node = Node()
            current_node.name = _kdl_str_to_py_str(&ev.name)
            if ev.value.type_annotation.data != NULL:
                current_node.type_annotation = _kdl_str_to_py_str(&ev.value.type_annotation)
            nodes.append(current_node)
            stack.append(current_node)
            nodes = current_node.children
        elif ev.event == KDL_EVENT_END_NODE:
            stack.pop()
            if len(stack) == 0:
                # at the top
                nodes = root_node_list
            else:
                # in a node
                nodes = stack[-1].children
        elif ev.event == KDL_EVENT_ARGUMENT:
            current_node.args.append(_convert_kdl_value(&ev.value))
        elif ev.event == KDL_EVENT_PROPERTY:
            current_node.properties[_kdl_str_to_py_str(&ev.name)] = _convert_kdl_value(&ev.value)
        else:
            raise RuntimeError("Unexpected event")