#ifndef KDL_PARSER_H_
#define KDL_PARSER_H_

#include "common.h"
#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum kdl_event {
    KDL_EVENT_EOF = 0,
    KDL_EVENT_PARSE_ERROR,
    KDL_EVENT_START_NODE,
    KDL_EVENT_END_NODE,
    KDL_EVENT_ARGUMENT,
    KDL_EVENT_PROPERTY,
    KDL_EVENT_COMMENT = 0x10000
};

enum kdl_parse_option {
    KDL_DEFAULTS = 0,
    KDL_EMIT_COMMENTS = 1,
};

typedef enum kdl_event kdl_event;
typedef struct kdl_event_data kdl_event_data;
typedef enum kdl_parse_option kdl_parse_option;
typedef struct _kdl_parser kdl_parser;

struct kdl_event_data {
    kdl_event event;
    kdl_str name;
    kdl_value value;
};

KDL_NODISCARD kdl_parser *kdl_create_string_parser(kdl_str doc, kdl_parse_option opt);
KDL_NODISCARD kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt);
void kdl_destroy_parser(kdl_parser *parser);

kdl_event_data *kdl_parser_next_event(kdl_parser *parser);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_PARSER_H_
