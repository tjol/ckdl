#include "str.h"
#include "compat.h"
#include "grammar.h"
#include "kdl/common.h"
#include "utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// No buffering in Debug mode to find more bugs
#ifdef KDL_DEBUG
#    define MIN_BUFFER_SIZE 1
#    define BUFFER_SIZE_INCREMENT 1
#else
#    define BUFFER_SIZE_INCREMENT 1024
#endif

KDL_EXPORT extern inline kdl_str kdl_borrow_str(kdl_owned_string const* str);

kdl_str kdl_str_from_cstr(char const* s) { return (kdl_str){s, strlen(s)}; }

kdl_owned_string kdl_clone_str(kdl_str const* s)
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

void kdl_free_string(kdl_owned_string* s)
{
    if (s->data != NULL) {
        free(s->data);
        s->data = NULL;
    }
    s->len = 0;
}

_kdl_write_buffer _kdl_new_write_buffer(size_t initial_size)
{
    return (_kdl_write_buffer){.buf = malloc(initial_size), .buf_len = initial_size, .str_len = 0};
}

void _kdl_free_write_buffer(_kdl_write_buffer* buf)
{
    free(buf->buf);
    buf->buf = NULL;
    buf->buf_len = 0;
    buf->str_len = 0;
}

bool _kdl_buf_push_chars(_kdl_write_buffer* buf, char const* s, size_t count)
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

bool _kdl_buf_push_char(_kdl_write_buffer* buf, char c) { return _kdl_buf_push_chars(buf, &c, 1); }

bool _kdl_buf_push_codepoint(_kdl_write_buffer* buf, uint32_t c)
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

kdl_owned_string _kdl_buf_to_string(_kdl_write_buffer* buf)
{
    kdl_owned_string s = {reallocf(buf->buf, buf->str_len + 1), buf->str_len};
    if (s.data == NULL) s.len = 0;
    buf->buf = NULL;
    buf->buf_len = 0;
    buf->str_len = 0;
    s.data[s.len] = '\0';
    return s;
}

kdl_owned_string kdl_escape(kdl_str const* s, kdl_escape_mode mode) { return kdl_escape_v1(s, mode); }

kdl_owned_string kdl_unescape(kdl_str const* s) { return kdl_unescape_v1(s); }

KDL_NODISCARD KDL_EXPORT kdl_owned_string kdl_escape_v(
    kdl_version version, kdl_str const* s, kdl_escape_mode mode)
{
    switch (version) {
    case KDL_VERSION_1:
        return kdl_escape_v1(s, mode);
    case KDL_VERSION_2:
        return kdl_escape_v2(s, mode);
    default:
        return (kdl_owned_string){NULL, 0};
    }
}

KDL_NODISCARD KDL_EXPORT kdl_owned_string kdl_unescape_v(kdl_version version, kdl_str const* s)
{
    switch (version) {
    case KDL_VERSION_1:
        return kdl_unescape_v1(s);
    case KDL_VERSION_2:
        return kdl_unescape_v2(s);
    default:
        return (kdl_owned_string){NULL, 0};
    }
}

kdl_owned_string kdl_escape_v1(kdl_str const* s, kdl_escape_mode mode)
{
    kdl_owned_string result;
    kdl_str unescaped = *s;

    size_t orig_len = unescaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(2 * orig_len);
    if (buf.buf == NULL) goto esc_error;

    uint32_t c;

    while (true) {
        char const* orig_char = unescaped.data;
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
        } else if (c == 0x09 && (mode & KDL_ESCAPE_TAB)) {
            if (!_kdl_buf_push_chars(&buf, "\\t", 2)) goto esc_error;
        } else if (c == 0x5C) {
            if (!_kdl_buf_push_chars(&buf, "\\\\", 2)) goto esc_error;
        } else if (c == 0x22) {
            if (!_kdl_buf_push_chars(&buf, "\\\"", 2)) goto esc_error;
        } else if (c == 0x08 && (mode & KDL_ESCAPE_CONTROL)) {
            if (!_kdl_buf_push_chars(&buf, "\\b", 2)) goto esc_error;
        } else if (c == 0x0C && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\f", 2)) goto esc_error;
        } else if (((mode & KDL_ESCAPE_CONTROL)
                       && ((c < 0x20 && c != 0x0A && c != 0x0D && c != 0x09 && c != 0x0C)
                           || c == 0x7F /* DEL */))
            || ((mode & KDL_ESCAPE_NEWLINE) && (c == 0x85 || c == 0x2028 || c == 0x2029))
            || ((mode & KDL_ESCAPE_ASCII_MODE) == KDL_ESCAPE_ASCII_MODE && c >= 0x7f)) {
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
    result = (kdl_owned_string){NULL, 0};
    return result;
}

kdl_owned_string kdl_unescape_v1(kdl_str const* s)
{
    kdl_owned_string result;
    kdl_str escaped = *s;
    uint32_t c;

    size_t orig_len = escaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(2 * orig_len);
    if (buf.buf == NULL) goto unesc_error;

    char const* p = s->data;
    char const* end = p + s->len;

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
                for (;; ++p) {
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
                // stray backslashes are not allowed
                goto unesc_error;
            }
        }

        char const* start = p;
        while (p != end && *p != '\\') ++p;
        // copy everything until the backslash
        if (!_kdl_buf_push_chars(&buf, start, (p - start))) goto unesc_error;
    }

    return _kdl_buf_to_string(&buf);

unesc_error:
    _kdl_free_write_buffer(&buf);
    result = (kdl_owned_string){NULL, 0};
    return result;
}

kdl_owned_string kdl_escape_v2(kdl_str const* s, kdl_escape_mode mode)
{
    kdl_owned_string result;
    kdl_str unescaped = *s;

    size_t orig_len = unescaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(2 * orig_len);
    if (buf.buf == NULL) goto esc_error;

    uint32_t c;

    while (true) {
        char const* orig_char = unescaped.data;
        switch (_kdl_pop_codepoint(&unescaped, &c)) {
        case KDL_UTF8_EOF:
            goto esc_eof;
        case KDL_UTF8_OK:
            break;
        default: // error
            goto esc_error;
        }

        if (c > 0x10ffff) {
            // Not a valid character
            goto esc_error;
        } else if (c == 0x0A && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\n", 2)) goto esc_error;
        } else if (c == 0x0D && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\r", 2)) goto esc_error;
        } else if (c == 0x09 && (mode & KDL_ESCAPE_TAB)) {
            if (!_kdl_buf_push_chars(&buf, "\\t", 2)) goto esc_error;
        } else if (c == 0x5C) {
            if (!_kdl_buf_push_chars(&buf, "\\\\", 2)) goto esc_error;
        } else if (c == 0x22) {
            if (!_kdl_buf_push_chars(&buf, "\\\"", 2)) goto esc_error;
        } else if (c == 0x08 && (mode & KDL_ESCAPE_CONTROL)) {
            if (!_kdl_buf_push_chars(&buf, "\\b", 2)) goto esc_error;
        } else if (c == 0x0C && (mode & KDL_ESCAPE_NEWLINE)) {
            if (!_kdl_buf_push_chars(&buf, "\\f", 2)) goto esc_error;
        } else if (_kdl_is_illegal_char(KDL_CHARACTER_SET_V2, c)
            || ((mode & KDL_ESCAPE_CONTROL) && c == 0x0B /* vertical tab */)
            || ((mode & KDL_ESCAPE_NEWLINE) && (c == 0x85 || c == 0x2028 || c == 0x2029))
            || ((mode & KDL_ESCAPE_ASCII_MODE) == KDL_ESCAPE_ASCII_MODE && c >= 0x7f)) {
            // \u escape
            char u_esc_buf[11];
            int count = snprintf(u_esc_buf, 11, "\\u{%x}", (unsigned int)c);
            if (count < 0 || !_kdl_buf_push_chars(&buf, u_esc_buf, count)) {
                goto esc_error;
            }
        } else {
            // keep the rest
            _kdl_buf_push_chars(&buf, orig_char, unescaped.data - orig_char);
        }
    }
esc_eof:
    return _kdl_buf_to_string(&buf);

esc_error:
    _kdl_free_write_buffer(&buf);
    result = (kdl_owned_string){NULL, 0};
    return result;
}

kdl_owned_string kdl_unescape_v2(kdl_str const* s)
{
    kdl_owned_string result;
    kdl_owned_string dedented = _kdl_dedent_multiline_string(s);
    kdl_str escaped = kdl_borrow_str(&dedented);

    size_t orig_len = escaped.len;
    _kdl_write_buffer buf = _kdl_new_write_buffer(orig_len);
    if (buf.buf == NULL) goto unesc_error;
    if (dedented.data == NULL) goto unesc_error;

    uint32_t c = 0;
    kdl_utf8_status status;

    while ((status = _kdl_pop_codepoint(&escaped, &c)) == KDL_UTF8_OK) {
        if (_kdl_is_illegal_char(KDL_CHARACTER_SET_V2, c)) {
            goto unesc_error;
        } else if (c == '\\') {
            if (_kdl_pop_codepoint(&escaped, &c) != KDL_UTF8_OK) goto unesc_error;

            switch (c) {
            case 'n':
                _kdl_buf_push_char(&buf, '\n');
                break;
            case 'r':
                _kdl_buf_push_char(&buf, '\r');
                break;
            case 't':
                _kdl_buf_push_char(&buf, '\t');
                break;
            case 's':
                _kdl_buf_push_char(&buf, ' ');
                break;
            case '\\':
                _kdl_buf_push_char(&buf, '\\');
                break;
            case '"':
                _kdl_buf_push_char(&buf, '\"');
                break;
            case 'b':
                _kdl_buf_push_char(&buf, '\b');
                break;
            case 'f':
                _kdl_buf_push_char(&buf, '\f');
                break;
            case 'u': {
                // u should be followed by {
                if (_kdl_pop_codepoint(&escaped, &c) != KDL_UTF8_OK || c != '{') goto unesc_error;

                uint32_t r = 0;
                while ((status = _kdl_pop_codepoint(&escaped, &c)) == KDL_UTF8_OK) {
                    // parse hex
                    if (c == '}') break;
                    else if (c >= '0' && c <= '9') r = (r << 4) + (c - '0');
                    else if (c >= 'a' && c <= 'f') r = (r << 4) + (c - 'a' + 0xa);
                    else if (c >= 'A' && c <= 'F') r = (r << 4) + (c - 'A' + 0xa);
                    else goto unesc_error;
                }
                if (status != KDL_UTF8_OK) goto unesc_error;
                if ((0xD800 <= r && r <= 0xDFFF) // only Unicode Scalar values are allowed in strings
                    || !_kdl_buf_push_codepoint(&buf, r))
                    goto unesc_error;
                break;
            }
            default:
                // See if this is a whitespace escape
                if (_kdl_is_whitespace(KDL_CHARACTER_SET_V2, c) || _kdl_is_newline(c)) {
                    kdl_str tail = escaped; // make a copy - we will advance too far
                    while ((status = _kdl_pop_codepoint(&tail, &c)) == KDL_UTF8_OK
                        && (_kdl_is_whitespace(KDL_CHARACTER_SET_V2, c) || _kdl_is_newline(c))) {
                        // skip this char
                        escaped = tail;
                    }
                    // if there is a UTF-8 error, this will be discovered on the
                    // next iteration of the outer loop
                    break;
                } else {
                    // Not whitespace - backslash is illegal here
                    goto unesc_error;
                }
            }
        } else {
            // Nothing special, copy the character
            _kdl_buf_push_codepoint(&buf, c);
        }
    }

    if (status == KDL_UTF8_EOF) {
        // ok
        kdl_free_string(&dedented);
        result = _kdl_buf_to_string(&buf);
        return result;
    } else {
        goto unesc_error;
    }

unesc_error:
    kdl_free_string(&dedented);
    _kdl_free_write_buffer(&buf);
    result = (kdl_owned_string){NULL, 0};
    return result;
}

kdl_owned_string _kdl_dedent_multiline_string(kdl_str const* s)
{
    kdl_owned_string result;

    uint32_t c;
    kdl_str orig = *s;
    kdl_utf8_status status;

    char* buf_dedented = NULL;

    // Normalize newlines first
    _kdl_write_buffer buf_norm_lf = _kdl_new_write_buffer(s->len);

    while ((status = _kdl_pop_codepoint(&orig, &c)) == KDL_UTF8_OK) {
        if (_kdl_is_newline(c)) {
            // Normalize CRLF
            if (c == '\r' && orig.len >= 1 && orig.data[0] == '\n') {
                ++orig.data;
                --orig.len;
            }
            // every newline becomes a line feed
            _kdl_buf_push_char(&buf_norm_lf, '\n');
        } else {
            _kdl_buf_push_codepoint(&buf_norm_lf, c);
        }
    }

    if (status != KDL_UTF8_EOF) {
        goto dedent_err;
    }

    kdl_str norm_lf = (kdl_str){.data = buf_norm_lf.buf, .len = buf_norm_lf.str_len};

    // What's after the final newline?
    char const* final_newline = NULL;
    for (char const* p = norm_lf.data + norm_lf.len - 1; p >= norm_lf.data; --p) {
        if (*p == '\n') {
            final_newline = p;
            break;
        }
    }

    if (final_newline == NULL) {
        // no newlines = no change
        return _kdl_buf_to_string(&buf_norm_lf);
    }

    kdl_str indent
        = (kdl_str){.data = final_newline + 1, .len = (norm_lf.data + norm_lf.len) - (final_newline + 1)};

    // Check that the indentation is all whitespace
    kdl_str indent_ = indent;
    while (_kdl_pop_codepoint(&indent_, &c) == KDL_UTF8_OK) {
        if (!_kdl_is_whitespace(KDL_CHARACTER_SET_V2, c)) {
            goto dedent_err;
        }
    }

    // The first character of the string MUST be a newline if there are any
    // newlines.
    if (norm_lf.data[0] != '\n') {
        goto dedent_err;
    }

    // Remove the whitespace from the beginning of all lines
    buf_dedented = malloc(norm_lf.len - 1);
    char* out = buf_dedented;
    char const* in = norm_lf.data; // skip initial LF
    char const* end = norm_lf.data + norm_lf.len;
    bool at_start = true;
    // copy the rest of the string
    if (norm_lf.len > 1) {
        while (in < end) {
            *out = *in;
            if (*in == '\n') {
                if (in + 1 < end && *(in + 1) == '\n') {
                    // double newline - ok
                } else {
                    // check indent
                    if (memcmp(in + 1, indent.data, indent.len) == 0) {
                        // skip indent
                        in += indent.len;
                    } else {
                        goto dedent_err;
                    }
                }
            }
            if (!at_start) {
                // Skip the initial newline => only advance the output pointer
                // if we're somewhere other than the initial newline
                ++out;
            }
            ++in;
            at_start = false;
        }
    }

    size_t len = out - buf_dedented;
    // Strip the final line feed
    if (len > 0 && buf_dedented[len - 1] == '\n') --len;
    buf_dedented = realloc(buf_dedented, len);
    return (kdl_owned_string){.data = buf_dedented, .len = len};

dedent_err:
    _kdl_free_write_buffer(&buf_norm_lf);
    if (buf_dedented != NULL) free(buf_dedented);
    result = (kdl_owned_string){NULL, 0};
    return result;
}
