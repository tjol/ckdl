#include "kdl/common.h"
#include "str.h"
#include "utf8.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// No buffering in Debug mode to find more bugs
#ifdef KDL_DEBUG
#  define MIN_BUFFER_SIZE 1
#  define BUFFER_SIZE_INCREMENT 1
#else
#  define BUFFER_SIZE_INCREMENT 1024
#endif

KDL_EXPORT extern inline kdl_str kdl_borrow_str(kdl_owned_string const *str);

kdl_str kdl_str_from_cstr(char const *s)
{
    return (kdl_str){ s, strlen(s) };
}

kdl_owned_string kdl_clone_str(kdl_str const *s)
{
    kdl_owned_string result;
    result.data = malloc(s->len + 1);
    if (result.data != NULL) {
        memcpy(result.data, s->data, s->len);
        result.data[s->len] = '\0';
        result.len = s->len;
    } else {
        result.len = 0;
    }
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

_kdl_write_buffer _kdl_new_write_buffer(size_t initial_size)
{
    return (_kdl_write_buffer){
        .buf = malloc(initial_size),
        .buf_len = initial_size,
        .str_len = 0
    };
}

void _kdl_free_write_buffer(_kdl_write_buffer *buf)
{
    free(buf->buf);
    buf->buf = NULL;
    buf->buf_len = 0;
    buf->str_len = 0;
}

bool _kdl_buf_push_chars(_kdl_write_buffer *buf, char const *s, size_t count)
{
    if (buf->buf_len - buf->str_len < count) {
        size_t increment = BUFFER_SIZE_INCREMENT >= count ? BUFFER_SIZE_INCREMENT : count;
        buf->buf_len += increment;
        buf->buf = reallocf(buf->buf, buf->buf_len);
        if (buf->buf == NULL) {
            return false;
        }
    }
    memcpy(buf->buf + buf->str_len, s, count);
    buf->str_len += count;
    return true;
}

bool _kdl_buf_push_char(_kdl_write_buffer *buf, char c)
{
    return _kdl_buf_push_chars(buf, &c, 1);
}

bool _kdl_buf_push_codepoint(_kdl_write_buffer *buf, uint32_t c)
{
    // 4 is the maximum length of a character in UTF-8
    if (buf->buf_len - buf->str_len < 4) {
        size_t increment = BUFFER_SIZE_INCREMENT >= 4 ? BUFFER_SIZE_INCREMENT : 4;
        buf->buf_len += increment;
        buf->buf = reallocf(buf->buf, buf->buf_len);
        if (buf->buf == NULL) {
            return false;
        }
    }
    size_t pushed = _kdl_push_codepoint(c, buf->buf + buf->str_len);
    if (pushed == 0) {
        return false;
    } else {
        buf->str_len += pushed;
        return true;
    }
}

kdl_owned_string _kdl_buf_to_string(_kdl_write_buffer *buf)
{
    kdl_owned_string s = { reallocf(buf->buf, buf->str_len + 1), buf->str_len };
    if (s.data == NULL) s.len = 0;
    buf->buf = NULL;
    buf->buf_len = 0;
    buf->str_len = 0;
    s.data[s.len] = '\0';
    return s;
}

kdl_owned_string kdl_escape(kdl_str const *s, kdl_escape_mode mode)
{
    kdl_owned_string result;
    kdl_str unescaped = *s;

    size_t orig_len = unescaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(2 * orig_len);
    if (buf.buf == NULL) goto esc_error;

    uint32_t c;

    while (true) {
        char const *orig_char = unescaped.data;
        switch (_kdl_pop_codepoint(&unescaped, &c)) {
        case KDL_UTF8_EOF:
            goto esc_eof;
        case KDL_UTF8_OK:
            break;
        default: // error
            goto esc_error;
        }

        if (c == 0x0A && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\n", 2)) goto esc_error;
        } else if (c == 0x0D && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\r", 2)) goto esc_error;
        } else if (c == 0x09  && (mode & KDL_ESCAPE_TAB)) {
            if (!_kdl_buf_push_chars(&buf, "\\t", 2)) goto esc_error;
        } else if (c == 0x5C) {
            if (!_kdl_buf_push_chars(&buf, "\\\\", 2)) goto esc_error;
        } else if (c == 0x22) {
            if (!_kdl_buf_push_chars(&buf, "\\\"", 2)) goto esc_error;
        } else if (c == 0x08 && (mode & KDL_ESCAPE_CONTROL)) {
            if (!_kdl_buf_push_chars(&buf, "\\b", 2)) goto esc_error;
        } else if (c == 0x0C && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\f", 2)) goto esc_error;
        } else if (
            ((mode & KDL_ESCAPE_CONTROL)
                && ((c < 0x20 && c != 0x0A && c != 0x0D && c != 0x09 && c != 0x0C)
                    || c == 0x7F /* DEL */))
            || ((mode & KDL_ESCAPE_NEWLINE)
                && (c == 0x85 || c == 0x2028 || c == 0x2029))
            || ((mode & KDL_ESCAPE_ASCII_MODE) == KDL_ESCAPE_ASCII_MODE
                && c >= 0x7f)) {
            // \u escape
            char u_esc_buf[11];
            int count = snprintf(u_esc_buf, 11, "\\u{%x}", (unsigned int)c);
            if (count < 0 || !_kdl_buf_push_chars(&buf, u_esc_buf, count)) {
                goto esc_error;
            }
        } else if (c > 0x10ffff) {
            // Not a valid character
            goto esc_error;
        } else {
            // keep the rest
            _kdl_buf_push_chars(&buf, orig_char, unescaped.data - orig_char);
        }
    }
esc_eof:
    return _kdl_buf_to_string(&buf);

esc_error:
    _kdl_free_write_buffer(&buf);
    result = (kdl_owned_string){ NULL, 0 };
    return result;
}

kdl_owned_string kdl_unescape(kdl_str const *s)
{
    kdl_owned_string result;
    kdl_str escaped = *s;
    uint32_t c;

    size_t orig_len = escaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(2 * orig_len);
    if (buf.buf == NULL) goto unesc_error;

    char const *p = s->data;
    char const *end = p + s->len;

    while (p != end) {
        if (*p == '\\') {
            // deal with the escape
            if (++p == end) goto unesc_error;
            switch (*p) {
            case 'n':
                _kdl_buf_push_char(&buf, '\n');
                ++p;
                break;
            case 'r':
                _kdl_buf_push_char(&buf, '\r');
                ++p;
                break;
            case 't':
                _kdl_buf_push_char(&buf, '\t');
                ++p;
                break;
            case '\\':
                _kdl_buf_push_char(&buf, '\\');
                ++p;
                break;
            case '/':
                _kdl_buf_push_char(&buf, '/');
                ++p;
                break;
            case '"':
                _kdl_buf_push_char(&buf, '\"');
                ++p;
                break;
            case 'b':
                _kdl_buf_push_char(&buf, '\b');
                ++p;
                break;
            case 'f':
                _kdl_buf_push_char(&buf, '\f');
                ++p;
                break;
            case 'u':
                // u should be followed by {
                if (++p == end || *(p++) != '{') goto unesc_error;
                // parse hex
                c = 0;
                for (;;++p) {
                    if (p == end) goto unesc_error;
                    else if (*p == '}') break;
                    else if (*p >= '0' && *p <= '9') c = (c << 4) + (*p - '0');
                    else if (*p >= 'a' && *p <= 'f') c = (c << 4) + (*p - 'a' + 0xa);
                    else if (*p >= 'A' && *p <= 'F') c = (c << 4) + (*p - 'A' + 0xa);
                    else goto unesc_error;
                }
                if (!_kdl_buf_push_codepoint(&buf, c)) goto unesc_error;
                ++p;
                break;
            default:
                // emit backslash instead
                _kdl_buf_push_char(&buf, '\\');
                // don't eat the character after the backslash
                break;
            }
        }

        char const *start = p;
        while (p != end && *p != '\\') ++p;
        // copy everything until the backslash
        if (!_kdl_buf_push_chars(&buf, start, (p - start))) goto unesc_error;
    }

    return _kdl_buf_to_string(&buf);

unesc_error:
    _kdl_free_write_buffer(&buf);
    result = (kdl_owned_string){ NULL, 0 };
    return result;
}
