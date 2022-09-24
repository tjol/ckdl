#include "utf8.h"

#include "test_util.h"

#include <string.h>
#include <stdlib.h>

static char ASCII_IN[] = "abcd1234";
static uint32_t ASCII_OUT[] = {0x61, 0x62, 0x63, 0x64, 0x31, 0x32, 0x33, 0x34};

static char ASCII_CONTROL_IN[] = " \n\0\t\a";
static uint32_t ASCII_CONTROL_OUT[] = {0x20, 0xa, 0x0, 0x9, 0x7};

static char LATIN1_IN[] = "\xc3\xa5ngstr\xc3\xb6m";
static uint32_t LATIN1_OUT[] = {0xe5, 0x6e, 0x67, 0x73, 0x74, 0x72, 0xf6, 0x6d};

static char RUSSIAN_IN[] = "\xd0\x94. \xd0\x98. \xd0\x9c\xd0\xb5\xd0\xbd\xd0\xb4\xd0\xb5\xd0\xbb\xd0\xb5\xd0\xb5\xd0\xb2";
static uint32_t RUSSIAN_OUT[] = {0x414, 0x2e, 0x20, 0x418, 0x2e, 0x20, 0x41c, 0x435, 0x43d, 0x434, 0x435, 0x43b, 0x435, 0x435, 0x432};

static char EMOJI_IN[] = "\xf0\x9f\x90\xb1\xf0\x9f\xa7\xb6";
static uint32_t EMOJI_OUT[] = {0x1f431, 0x1f9f6};

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
        ASSERT(KDL_UTF8_OK == _kdl_pop_codepoint(&s, &codepoint));
        ASSERT(codepoint == out_ptr[i]);
        buf_end += _kdl_push_codepoint(codepoint, buf_end);
    }
    ASSERT(KDL_UTF8_EOF == _kdl_pop_codepoint(&s, &codepoint));
    ASSERT(buf_end == buf + in_len);
    ASSERT(*buf_end == (char)(-1));
    ASSERT(0 == memcmp(buf, in_ptr, in_len));
    free(buf);
}

static void test_utf8_ascii(void)
{
    ASSERT_UTF8_STRING(ASCII_IN, ASCII_OUT);
    ASSERT_UTF8_STRING(ASCII_CONTROL_IN, ASCII_CONTROL_OUT);
}

static void test_utf8_bmp(void)
{
    ASSERT_UTF8_STRING(LATIN1_IN, LATIN1_OUT);
    ASSERT_UTF8_STRING(RUSSIAN_IN, RUSSIAN_OUT);
}

static void test_utf8_emoji(void)
{
    ASSERT_UTF8_STRING(EMOJI_IN, EMOJI_OUT);
}

static void test_utf8_invalid(void)
{
    uint32_t codepoint = 0;
    size_t count = sizeof(INVALID) / sizeof(char const*);
    for (size_t i = 0; i < count; ++i) {
        kdl_str s = {INVALID[i], strlen(INVALID[i])};
        ASSERT(KDL_UTF8_DECODE_ERROR == _kdl_pop_codepoint(&s, &codepoint));
    }
}

static void test_utf8_incomplete(void)
{
    uint32_t codepoint = 0;
    size_t count = sizeof(INCOMPLETE) / sizeof(char const*);
    for (size_t i = 0; i < count; ++i) {
        kdl_str s = {INCOMPLETE[i], strlen(INCOMPLETE[i])};
        ASSERT(KDL_UTF8_INCOMPLETE == _kdl_pop_codepoint(&s, &codepoint));
    }
}

void TEST_MAIN(void)
{
    run_test("UTF8: ASCII", &test_utf8_ascii);
    run_test("UTF8: BMP", &test_utf8_bmp);
    run_test("UTF8: Emoji", &test_utf8_emoji);
    run_test("UTF8: Invalid", &test_utf8_invalid);
    run_test("UTF8: Incomplete",&test_utf8_incomplete);
}
