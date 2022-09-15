#ifndef KDL_EMITTER_H_
#define KDL_EMITTER_H_

#include "common.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Formatting options for a kdl_emitter: How should identifiers be serialized
enum kdl_identifier_emission_mode {
    KDL_PREFER_BARE_IDENTIFIERS, // Traditional: quote identifiers only if absolutely necessary
    KDL_QUOTE_ALL_IDENTIFIERS,   // Express *all* identifiers as strings
    KDL_ASCII_IDENTIFIERS        // Use only ASCII
};

typedef enum kdl_identifier_emission_mode kdl_identifier_emission_mode;
typedef struct kdl_emitter_options kdl_emitter_options;
typedef struct kdl_float_printing_options kdl_float_printing_options;
typedef struct _kdl_emitter kdl_emitter;

// Formatting options for floating-point numbers
struct kdl_float_printing_options {
    bool always_write_decimal_point;
    bool always_write_decimal_point_or_exponent;
    bool capital_e;
    bool exponent_plus;
    bool plus;
    int min_exponent;
};

// Formatting options for a kdl_emitter
struct kdl_emitter_options {
    int indent; // Number of spaces to indent child nodes by
    kdl_escape_mode escape_mode; // How to escape strings
    kdl_identifier_emission_mode identifier_mode; // How to quote identifiers
    kdl_float_printing_options float_mode; // How to print floating point numbers
};

KDL_EXPORT extern const kdl_emitter_options KDL_DEFAULT_EMITTER_OPTIONS;

// Create an emitter than writes into an internal buffer
KDL_NODISCARD KDL_EXPORT kdl_emitter *kdl_create_buffering_emitter(kdl_emitter_options const *opt);
// Create an emitter that writes by calling a user-supplied function
KDL_NODISCARD KDL_EXPORT kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, kdl_emitter_options const *opt);

// Destroy an emitter
KDL_EXPORT void kdl_destroy_emitter(kdl_emitter *emitter);

// Write a node tag
KDL_EXPORT bool kdl_emit_node(kdl_emitter *emitter, kdl_str name);
// Write a node tag including a type annotation
KDL_EXPORT bool kdl_emit_node_with_type(kdl_emitter *emitter, kdl_str type, kdl_str name);
// Write an argument for a node
KDL_EXPORT bool kdl_emit_arg(kdl_emitter *emitter, kdl_value const *value);
// Write a property for a node
KDL_EXPORT bool kdl_emit_property(kdl_emitter *emitter, kdl_str name, kdl_value const *value);
// Start a list of children for the previous node ('{')
KDL_EXPORT bool kdl_start_emitting_children(kdl_emitter *emitter);
// End the list of children ('}')
KDL_EXPORT bool kdl_finish_emitting_children(kdl_emitter *emitter);
// Finish - write a final newline if required
KDL_EXPORT bool kdl_emit_end(kdl_emitter *emitter);

// Get a reference to the current emitter buffer
// This string is invalidated on any call to kdl_emit_*
KDL_EXPORT kdl_str kdl_get_emitter_buffer(kdl_emitter *emitter);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_EMITTER_H_
