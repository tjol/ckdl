#ifndef KDL_PARSER_H_
#define KDL_PARSER_H_

#include "common.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Type of parser event
enum kdl_event {
    KDL_EVENT_EOF = 0,     // regular end of file
    KDL_EVENT_PARSE_ERROR, // parse error
    KDL_EVENT_START_NODE,  // start of a node (a child node, if the previous node has not yet ended)
    KDL_EVENT_END_NODE,    // end of a node
    KDL_EVENT_ARGUMENT,    // argument for the current node
    KDL_EVENT_PROPERTY,    // property for the current node
    // If KDL_EMIT_COMMENTS is specified:
    //  - on its own: a comment
    //  - ORed with another event type: a node/argument/property that has been commented out
    //    using a slashdash
    KDL_EVENT_COMMENT = 0x10000
};

// Parser configuration
enum kdl_parse_option {
    KDL_DEFAULTS = 0,      // Nothing special
    KDL_EMIT_COMMENTS = 1  // Emit comments (default: don't)
};

typedef enum kdl_event kdl_event;
typedef struct kdl_event_data kdl_event_data;
typedef enum kdl_parse_option kdl_parse_option;
typedef struct _kdl_parser kdl_parser;

// Full event structure
struct kdl_event_data {
    kdl_event event; // What is the event?
    kdl_str name;    // name of the node or property
    kdl_value value; // value including type annotation (for nodes: null with type annotation)
};

// Create a parser that reads from a string
KDL_NODISCARD KDL_EXPORT kdl_parser *kdl_create_string_parser(kdl_str doc, kdl_parse_option opt);
// Create a parser that reads data by calling a user-supplied function
KDL_NODISCARD KDL_EXPORT kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt);
// Destroy a parser
KDL_EXPORT void kdl_destroy_parser(kdl_parser *parser);

// Get the next parse event
// Returns a pointer to an event structure. The structure (including all strings it contains!) is
// invalidated on the next call.
KDL_EXPORT kdl_event_data *kdl_parser_next_event(kdl_parser *parser);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_PARSER_H_
