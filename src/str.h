#ifndef KDL_INTERNAL_STR_H_
#define KDL_INTERNAL_STR_H_

#include "kdl/common.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct _kdl_write_buffer {
    char* buf;
    size_t buf_len;
    size_t str_len;
};

typedef struct _kdl_write_buffer _kdl_write_buffer;

KDL_NODISCARD _kdl_write_buffer _kdl_new_write_buffer(size_t initial_size);
bool _kdl_buf_push_chars(_kdl_write_buffer* buf, char const* s, size_t count);
bool _kdl_buf_push_char(_kdl_write_buffer* buf, char c);
bool _kdl_buf_push_codepoint(_kdl_write_buffer* buf, uint32_t c);
KDL_NODISCARD kdl_owned_string _kdl_buf_to_string(_kdl_write_buffer* buf);
void _kdl_free_write_buffer(_kdl_write_buffer* buf);

// Escape special characters in a string according to KDLv1 string rules
KDL_NODISCARD kdl_owned_string kdl_escape_v1(kdl_str const* s, kdl_escape_mode mode);
// Resolve backslash escape sequences according to KDLv1 rules
KDL_NODISCARD kdl_owned_string kdl_unescape_v1(kdl_str const* s);

// Escape special characters in a string according to KDLv2 string rules
KDL_NODISCARD kdl_owned_string kdl_escape_v2(kdl_str const* s, kdl_escape_mode mode);
// Resolve backslash escape sequences according to KDLv2 rules for single-line strings
KDL_NODISCARD kdl_owned_string kdl_unescape_v2_single_line(kdl_str const* s);
// Resolve backslash escape sequences according to KDLv2 rules for multi-line strings
KDL_NODISCARD kdl_owned_string kdl_unescape_v2_multi_line(kdl_str const* s);

KDL_NODISCARD kdl_owned_string _kdl_dedent_multiline_string(kdl_str const* s);
KDL_NODISCARD kdl_owned_string _kdl_remove_escaped_whitespace(kdl_str const* s);
KDL_NODISCARD kdl_owned_string _kdl_resolve_escapes_v2(kdl_str const* s);
bool _kdl_str_contains_newline(kdl_str const* s);

#endif // KDL_INTERNAL_STR_H_
