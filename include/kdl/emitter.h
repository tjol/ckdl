#ifndef KDL_EMITTER_H_
#define KDL_EMITTER_H_

#include "common.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum kdl_identifier_emission_mode {
    KDL_PREFER_BARE_IDENTIFIERS,
    KDL_QUOTE_ALL_IDENTIFIERS,
    KDL_ASCII_IDENTIFIERS
};

typedef enum kdl_identifier_emission_mode kdl_identifier_emission_mode;
typedef struct kdl_emitter_options kdl_emitter_options;
typedef struct _kdl_emitter kdl_emitter;

struct kdl_emitter_options {
    int indent;
    kdl_escape_mode escape_mode;
    kdl_identifier_emission_mode identifier_mode;
};

KDL_NODISCARD kdl_emitter *kdl_create_buffering_emitter(kdl_emitter_options opt);
KDL_NODISCARD kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, kdl_emitter_options opt);

void kdl_destroy_emitter(kdl_emitter *emitter);

bool kdl_emit_node(kdl_emitter *emitter, kdl_str name);
bool kdl_emit_node_with_type(kdl_emitter *emitter, kdl_str type, kdl_str name);
bool kdl_emit_arg(kdl_emitter *emitter, kdl_value const *value);
bool kdl_emit_property(kdl_emitter *emitter, kdl_str name, kdl_value const *value);
bool kdl_start_emitting_children(kdl_emitter *emitter);
bool kdl_finish_emitting_children(kdl_emitter *emitter);
bool kdl_emit_end(kdl_emitter *emitter);

kdl_str kdl_get_emitter_buffer(kdl_emitter *emitter);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_EMITTER_H_
