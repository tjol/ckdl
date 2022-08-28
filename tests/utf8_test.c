#include "kdl_utf8.h"

#include <criterion/criterion.h>
#include <string.h>

static char ASCII_IN[] = "abcd1234";
static uint32_t ASCII_OUT[] = {0x61U, 0x62U, 0x63U, 0x64U, 0x31U, 0x32U, 0x33U, 0x34U};

static char ASCII_CONTROL_IN[] = " \n\0\t\a";
static uint32_t ASCII_CONTROL_OUT[] = {0x20U, 0xaU, 0x0U, 0x9U, 0x7U};

static char LATIN1_IN[] = "\xc3\xa5ngstr\xc3\xb6m";
static uint32_t LATIN1_OUT[] = {0xe5U, 0x6eU, 0x67U, 0x73U, 0x74U, 0x72U, 0xf6U, 0x6dU};

static char RUSSIAN_IN[] = "\xd0\x94. \xd0\x98. \xd0\x9c\xd0\xb5\xd0\xbd\xd0\xb4\xd0\xb5\xd0\xbb\xd0\xb5\xd0\xb5\xd0\xb2";
static uint32_t RUSSIAN_OUT[] = {0x414U, 0x2eU, 0x20U, 0x418U, 0x2eU, 0x20U, 0x41cU, 0x435U, 0x43dU, 0x434U, 0x435U, 0x43bU, 0x435U, 0x435U, 0x432U};

static char EMOJI_IN[] = "\xf0\x9f\x90\xb1\xf0\x9f\xa7\xb6";
static uint32_t EMOJI_OUT[] = {0x1f431U, 0x1f9f6U};

// Examples with characters missing or incorrect high bits
static char const *INVALID[] = {
    "\xc3\xe4",
    "\xc3\x24",
    "\xe8\x0f\xaf",
    "\xe8\x8f\x2f",
    "\xe8\xcf\xaf",
    "\xe8\x8f\xef",
    "\xf0\x1f\x92\xa9",
    "\xf0\x9f\xd2\xa9",
    "\xf0\x9f\x92\xe9"
};

static char const *INCOMPLETE[] = {
    "\xc3",
    "\xe8",
    "\xe8\x8f",
    "\xf0\x9f\x92"
};


#define ASSERT_UTF8_STRING(in,out) assert_utf8_string(in, sizeof(in)/sizeof(char) - 1, out, sizeof(out)/sizeof(uint32_t))

void assert_utf8_string(char const *in_ptr, size_t in_len, uint32_t const *out_ptr, size_t out_len)
{
    uint32_t codepoint;
    kdl_str s = {in_ptr, in_len};
    char *buf = malloc(in_len + 1);
    buf[in_len] = -1;
    char *buf_end = buf;

    for (size_t i = 0; i < out_len; ++i)
    {
        cr_assert(KDL_UTF8_OK == _kdl_pop_codepoint(&s, &codepoint));
        cr_assert(codepoint == out_ptr[i]);
        buf_end += _kdl_push_codepoint(codepoint, buf_end);
    }
    cr_assert(KDL_UTF8_EOF == _kdl_pop_codepoint(&s, &codepoint));
    cr_assert(buf_end == buf + in_len);
    cr_assert(*buf_end == -1);
    cr_assert(0 == memcmp(buf, in_ptr, in_len));
}

Test(utf8, ascii)
{
    ASSERT_UTF8_STRING(ASCII_IN, ASCII_OUT);
    ASSERT_UTF8_STRING(ASCII_CONTROL_IN, ASCII_CONTROL_OUT);
}

Test(utf8, bmp)
{
    ASSERT_UTF8_STRING(LATIN1_IN, LATIN1_OUT);
    ASSERT_UTF8_STRING(RUSSIAN_IN, RUSSIAN_OUT);
}

Test(utf8, emoji)
{
    ASSERT_UTF8_STRING(EMOJI_IN, EMOJI_OUT);
}

Test(utf8, invalid)
{
    uint32_t codepoint = 0;
    size_t count = sizeof(INVALID) / sizeof(char const*);
    for (size_t i = 0; i < count; ++i) {
        kdl_str s = {INVALID[i], strlen(INVALID[i])};
        cr_expect(KDL_UTF8_DECODE_ERROR == _kdl_pop_codepoint(&s, &codepoint));
    }
}

Test(utf8, incomplete)
{
    uint32_t codepoint = 0;
    size_t count = sizeof(INCOMPLETE) / sizeof(char const*);
    for (size_t i = 0; i < count; ++i) {
        kdl_str s = {INCOMPLETE[i], strlen(INCOMPLETE[i])};
        cr_expect(KDL_UTF8_INCOMPLETE == _kdl_pop_codepoint(&s, &codepoint));
    }
}
