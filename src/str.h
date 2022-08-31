#ifndef KDL_INTERNAL_STR_H_
#define KDL_INTERNAL_STR_H_

#include "kdl/common.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct _kdl_write_buffer {
    char *buf;
    size_t buf_len;
    size_t str_len;
};

typedef struct _kdl_write_buffer _kdl_write_buffer;

_kdl_write_buffer _kdl_new_write_buffer(size_t initial_size);
bool _kdl_buf_push_chars(_kdl_write_buffer *buf, char const *s, size_t count);
bool _kdl_buf_push_char(_kdl_write_buffer *buf, char c);
bool _kdl_buf_push_codepoint(_kdl_write_buffer *buf, uint32_t c);
kdl_owned_string _kdl_buf_to_string(_kdl_write_buffer *buf);
void _kdl_free_write_buffer(_kdl_write_buffer *buf);

#endif // KDL_INTERNAL_STR_H_
