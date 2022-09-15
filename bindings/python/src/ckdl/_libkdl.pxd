cdef extern from "kdl/common.h":
    ctypedef enum kdl_escape_mode:
        KDL_ESCAPE_MINIMAL = 0,
        KDL_ESCAPE_CONTROL = 0x10,
        KDL_ESCAPE_NEWLINE = 0x20,
        KDL_ESCAPE_TAB = 0x40,
        KDL_ESCAPE_ASCII_MODE =0x170,
        KDL_ESCAPE_DEFAULT = KDL_ESCAPE_CONTROL | KDL_ESCAPE_NEWLINE | KDL_ESCAPE_TAB

    ctypedef size_t (*kdl_read_func)(void *user_data, char *buf, size_t bufsize);
    ctypedef size_t (*kdl_write_func)(void *user_data, const char *data, size_t nbytes);

    ctypedef struct kdl_str:
        const char *data
        size_t len

    ctypedef struct kdl_owned_string:
        char *data
        size_t len

    cdef kdl_str kdl_borrow_str(const kdl_owned_string *s)

    cdef kdl_str kdl_str_from_cstr(const char *s)
    cdef kdl_owned_string kdl_clone_str(const kdl_str *s)
    cdef void kdl_free_string(kdl_owned_string *s)

    cdef kdl_owned_string kdl_escape(const kdl_str *s, kdl_escape_mode mode)
    cdef kdl_owned_string kdl_unescape(const kdl_str *s)

cdef extern from "kdl/value.h":
    ctypedef enum kdl_type:
        KDL_TYPE_NULL,
        KDL_TYPE_BOOLEAN,
        KDL_TYPE_NUMBER,
        KDL_TYPE_STRING

    ctypedef enum kdl_number_type:
        KDL_NUMBER_TYPE_INTEGER,
        KDL_NUMBER_TYPE_FLOATING_POINT,
        KDL_NUMBER_TYPE_STRING_ENCODED

    ctypedef struct kdl_number:
        kdl_number_type type
        long long integer
        double floating_point
        kdl_str string

    ctypedef struct kdl_value:
        kdl_type type
        kdl_str type_annotation
        bint boolean
        kdl_number number
        kdl_str string

cdef extern from "kdl/parser.h":
    ctypedef enum kdl_event:
        KDL_EVENT_EOF = 0,
        KDL_EVENT_PARSE_ERROR,
        KDL_EVENT_START_NODE,
        KDL_EVENT_END_NODE,
        KDL_EVENT_ARGUMENT,
        KDL_EVENT_PROPERTY,
        KDL_EVENT_COMMENT = 0x10000

    ctypedef enum kdl_parse_option:
        KDL_DEFAULTS = 0,
        KDL_EMIT_COMMENTS = 1,

    ctypedef struct kdl_event_data:
        kdl_event event
        kdl_str name
        kdl_value value

    ctypedef struct kdl_parser:
        pass

    cdef kdl_parser *kdl_create_string_parser(kdl_str doc, kdl_parse_option opt)
    cdef kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt)
    cdef void kdl_destroy_parser(kdl_parser *parser)

    cdef kdl_event_data *kdl_parser_next_event(kdl_parser *parser)

cdef extern from "kdl/emitter.h":
    ctypedef enum kdl_identifier_emission_mode:
        KDL_PREFER_BARE_IDENTIFIERS,
        KDL_QUOTE_ALL_IDENTIFIERS,
        KDL_ASCII_IDENTIFIERS

    ctypedef struct kdl_float_printing_options:
        bint always_write_decimal_point
        bint always_write_decimal_point_or_exponent
        bint capital_e
        bint exponent_plus
        bint plus
        int min_exponent

    ctypedef struct kdl_emitter_options:
        int indent
        kdl_escape_mode escape_mode
        kdl_identifier_emission_mode identifier_mode
        kdl_float_printing_options float_mode

    cdef kdl_emitter_options KDL_DEFAULT_EMITTER_OPTIONS

    ctypedef struct kdl_emitter:
        pass

    cdef kdl_emitter *kdl_create_buffering_emitter(const kdl_emitter_options *opt)
    cdef kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, const kdl_emitter_options *opt)

    cdef void kdl_destroy_emitter(kdl_emitter *emitter)

    cdef bint kdl_emit_node(kdl_emitter *emitter, kdl_str name)
    cdef bint kdl_emit_node_with_type(kdl_emitter *emitter, kdl_str type, kdl_str name)
    cdef bint kdl_emit_arg(kdl_emitter *emitter, const kdl_value *value)
    cdef bint kdl_emit_property(kdl_emitter *emitter, kdl_str name, const kdl_value *value)
    cdef bint kdl_start_emitting_children(kdl_emitter *emitter)
    cdef bint kdl_finish_emitting_children(kdl_emitter *emitter)
    cdef bint kdl_emit_end(kdl_emitter *emitter)

    cdef kdl_str kdl_get_emitter_buffer(kdl_emitter *emitter)
