#ifndef KDL_PARSER_H_
#define KDL_PARSER_H_

#include "common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum _kdl_event {
    KDL_EVENT_EOF = 0,
    KDL_EVENT_PARSE_ERROR,
    KDL_EVENT_START_NODE,
    KDL_EVENT_END_NODE,
    KDL_EVENT_ARGUMENT,
    KDL_EVENT_PROPERTY,
    KDL_EVENT_COMMENT = 0x10000
};
typedef enum _kdl_event kdl_event;

enum _kdl_type {
    KDL_TYPE_NULL,
    KDL_TYPE_STRING,
    KDL_TYPE_NUMBER,
    KDL_TYPE_BOOLEAN
};
typedef enum _kdl_type kdl_type;

enum _kdl_number_type {
    KDL_NUMBER_TYPE_INTEGER,
    KDL_NUMBER_TYPE_FLOATING_POINT,
    KDL_NUMBER_TYPE_STRING_ENCODED
};
typedef enum _kdl_number_type kdl_number_type;

struct _kdl_number {
    kdl_number_type type;
    union {
        long long integer;
        double floating_point;
        kdl_str string;
    } value;
};
typedef struct _kdl_number kdl_number;

struct _kdl_value {
    kdl_type type;
    union {
        kdl_str string;
        kdl_number number;
        bool boolean;
    } value;
};
typedef struct _kdl_value kdl_value;

struct _kdl_event_data {
    kdl_event event;
    kdl_value value;
    kdl_str type_annotation;
    kdl_str property_key;
};
typedef struct _kdl_event_data kdl_event_data;

enum _kdl_parse_option {
    KDL_DEFAULTS = 0,
    KDL_EMIT_COMMENTS = 1,
};
typedef enum _kdl_parse_option kdl_parse_option;

typedef struct _kdl_parser kdl_parser;

KDL_NODISCARD kdl_parser *kdl_create_parser_for_string(kdl_str doc, kdl_parse_option opt);
KDL_NODISCARD kdl_parser *kdl_create_parser_for_stream(kdl_read_func read_func, void *user_data, kdl_parse_option opt);
void kdl_destroy_parser(kdl_parser *parser);

kdl_event_data *kdl_parser_next_event(kdl_parser *parser);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_PARSER_H_
