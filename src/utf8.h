#ifndef KDL_INTERNAL_UTF8_H_
#define KDL_INTERNAL_UTF8_H_

#include <stdint.h>
#include <stdbool.h>

#include "kdl/common.h"

enum kdl_utf8_status {
    KDL_UTF8_OK,
    KDL_UTF8_EOF,
    KDL_UTF8_INCOMPLETE,
    KDL_UTF8_DECODE_ERROR
};
typedef enum kdl_utf8_status kdl_utf8_status;

kdl_utf8_status _kdl_pop_codepoint(kdl_str *str, uint32_t *codepoint);
int _kdl_push_codepoint(uint32_t codepoint, char *buf);

#endif // KDL_INTERNAL_UTF8_H_
