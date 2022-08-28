#include "kdl/common.h"
#include "kdl_utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE_INCREMENT 512

extern inline kdl_str kdl_borrow_str(kdl_owned_string const *str);


kdl_owned_string kdl_clone_str(kdl_str const *s)
{
    kdl_owned_string result;
    result.data = malloc(s->len + 1);
    memcpy(result.data, s->data, s->len);
    result.data[s->len + 1] = '\0';
    result.len = s->len;
    return result;
}


void kdl_free_string(kdl_owned_string *s)
{
    if (s->data != NULL) {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
}

static inline bool push_chars(char **buf, size_t *buf_len, size_t *str_len, char const *s, size_t count)
{
    while (*buf_len - *str_len < count) {
        *buf_len += BUFFER_SIZE_INCREMENT;
        *buf = realloc(*buf, *buf_len);
        if (*buf == NULL) return false;
    }
    memcpy(*buf + *str_len, s, count);
    *str_len += count;
    return true;
}

static inline bool push_char(char **buf, size_t *buf_len, size_t *str_len, char c)
{
    return push_chars(buf, buf_len, str_len, &c, 1);
}


kdl_owned_string kdl_escape(kdl_str const *s)
{
    kdl_owned_string result;
    kdl_str unescaped = *s;

    size_t orig_len = unescaped.len;
    size_t buf_len = 2 * orig_len;
    size_t str_len = 0;
    char *buf = malloc(buf_len);
    if (buf == NULL) goto esc_error;

    uint32_t c;

    while (true) {
        switch (_kdl_pop_codepoint(&unescaped, &c)) {
        case KDL_UTF8_EOF:
            goto esc_eof;
        case KDL_UTF8_OK:
            break;
        default: // error
            goto esc_error;
        }

        if (c == 0x0A) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\n", 2)) goto esc_error;
        } else if (c == 0x0D) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\r", 2)) goto esc_error;
        } else if (c == 0x09) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\t", 2)) goto esc_error;
        } else if (c == 0x5C) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\\\", 2)) goto esc_error;
        } else if (c == 0x22) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\\"", 2)) goto esc_error;
        } else if (c == 0x08) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\b", 2)) goto esc_error;
        } else if (c == 0x0C) {
            if (!push_chars(&buf, &buf_len, &str_len, "\\f", 2)) goto esc_error;
        } else if (c >= 0x20 && c < 0x7f) {
            // Keep printable ASCII
            push_char(&buf, &buf_len, &str_len, c);
        } else if (c > 0x10ffff) {
            // Not a valid character
            goto esc_error;
        } else {
            // \u escape
            char u_esc_buf[11];
            int count = snprintf(u_esc_buf, 11, "\\u{%x}", (unsigned int)c);
            if (count < 0 || !push_chars(&buf, &buf_len, &str_len, u_esc_buf, count)) {
                goto esc_error;
            }
        }
    }
esc_eof:
    buf = realloc(buf, str_len + 1);
    if (buf == NULL) goto esc_error;
    buf[str_len + 1] = '\0';

    result = (kdl_owned_string){ buf, str_len };
    return result;

esc_error:
    if (buf != NULL) free(buf);
    result = (kdl_owned_string){ NULL, 0 };
    return result;
}
