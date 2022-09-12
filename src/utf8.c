#include <stdbool.h>

#include "utf8.h"

inline static bool _is_utf8_continuation(int c)
{
    return (c & 0xc0) == 0x80;
}

kdl_utf8_status _kdl_pop_codepoint(kdl_str *str, uint32_t *codepoint)
{
    if (str->len < 1) {
        return KDL_UTF8_EOF;
    }

    char const* s = str->data;
    // Check high bit
    if ((s[0] & 0x80) == 0) {
        // ASCII
        *codepoint = (uint32_t)*s;
        ++str->data;
        --str->len;
        return KDL_UTF8_OK;
    } else if ((s[0] & 0xe0) == 0xc0) {
        // 2-byte sequence
        if (str->len < 2) {
            // Incomplete UTF-8 sequence
            return KDL_UTF8_INCOMPLETE;
        } else if (!_is_utf8_continuation(s[1])) {
            // Invalid UTF-8 sequence
            return KDL_UTF8_DECODE_ERROR;
        }
        *codepoint = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
        str->data += 2;
        str->len -= 2;
        return KDL_UTF8_OK;
    } else if ((s[0] & 0xf0) == 0xe0) {
        // 3-byte sequence
        if (str->len < 3) {
            // Incomplete UTF-8 sequence
            return KDL_UTF8_INCOMPLETE;
        } else if (!_is_utf8_continuation(s[1]) || !_is_utf8_continuation(s[2])) {
            // Invalid UTF-8 sequence
            return KDL_UTF8_DECODE_ERROR;
        }
        *codepoint = ((s[0] & 0xf) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
        str->data += 3;
        str->len -= 3;
        return KDL_UTF8_OK;
    } else if ((s[0] & 0xf8) == 0xf0) {
        // 4-byte sequence
        if (str->len < 4) {
            // Incomplete UTF-8 sequence
            return KDL_UTF8_INCOMPLETE;
        } else if (!_is_utf8_continuation(s[1]) || !_is_utf8_continuation(s[2]) || !_is_utf8_continuation(s[3])) {
            // Invalid UTF-8 sequence
            return KDL_UTF8_DECODE_ERROR;
        }
        *codepoint = ((s[0] & 0x7) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
        str->data += 4;
        str->len -= 4;
        return KDL_UTF8_OK;
    } else {
        return KDL_UTF8_DECODE_ERROR;
    }
}

int _kdl_push_codepoint(uint32_t codepoint, char *buf)
{
    char *p = buf;
    if (codepoint <= 0x7f) {
        // ASCII
        *(p++) = (char)codepoint;
        return (int)(p - buf);
    } else if (codepoint <= 0x7ff) {
        // fits in two bytes
        *(p++) = 0xc0 | (0x1f & (codepoint >> 6));
        *(p++) = 0x80 | (0x3f & codepoint);
        return (int)(p - buf);
    } else if (codepoint <= 0xffff) {
        // fits in three bytes
        *(p++) = 0xe0 | (0x0f & (codepoint >> 12));
        *(p++) = 0x80 | (0x3f & (codepoint >> 6));
        *(p++) = 0x80 | (0x3f & codepoint);
        return (int)(p - buf);
    } else if (codepoint <= 0x10ffff) {
        // fits in four bytes
        *(p++) = 0xf0 | (0x07 & (codepoint >> 18));
        *(p++) = 0x80 | (0x3f & (codepoint >> 12));
        *(p++) = 0x80 | (0x3f & (codepoint >> 6));
        *(p++) = 0x80 | (0x3f & codepoint);
        return (int)(p - buf);
    } else {
        // value too big
        return 0;
    }
}
