#ifndef KDL_INTERNAL_UTF8_H_
#define KDL_INTERNAL_UTF8_H_

#include <stdint.h>
#include <stdbool.h>

#include "kdl/common.h"

enum _kdl_utf8_status {
    KDL_UTF8_OK,
    KDL_UTF8_EOF,
    KDL_UTF8_INCOMPLETE,
    KDL_UTF8_DECODE_ERROR
};
typedef enum _kdl_utf8_status kdl_utf8_status;

kdl_utf8_status _kdl_pop_codepoint(kdl_str *str, uint32_t *codepoint);

#endif // KDL_INTERNAL_UTF8_H_
