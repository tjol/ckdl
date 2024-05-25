#include <kdl/kdl.h>

#include "test_util.h"

#include <string.h>

static void test_tokenizer_strings(void)
{
    kdl_token token;

    char const* doc_v1 = "r#\"abc\"def\"#";
    char const* doc_v2 = "#\"abc\"def\"#";

    kdl_str k_doc_v1 = kdl_str_from_cstr(doc_v1);
    kdl_str k_doc_v2 = kdl_str_from_cstr(doc_v2);

    kdl_tokenizer* tok = kdl_create_string_tokenizer(k_doc_v1);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_RAW_STRING_V1);
    ASSERT(token.value.len == 7);
    ASSERT(memcmp(token.value.data, "abc\"def", 7) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);

    tok = kdl_create_string_tokenizer(k_doc_v2);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_RAW_STRING_V2);
    ASSERT(token.value.len == 7);
    ASSERT(memcmp(token.value.data, "abc\"def", 7) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);
}

static void test_tokenizer_identifiers(void)
{
    kdl_token token;

    kdl_str doc = kdl_str_from_cstr("a<b;a,b;a>b");

    kdl_tokenizer* tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WORD);
    ASSERT(token.value.len == 3);
    ASSERT(memcmp(token.value.data, "a<b", 3) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_SEMICOLON);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WORD);
    ASSERT(token.value.len == 3);
    ASSERT(memcmp(token.value.data, "a,b", 3) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_SEMICOLON);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WORD);
    ASSERT(token.value.len == 3);
    ASSERT(memcmp(token.value.data, "a>b", 3) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);
}

static void test_tokenizer_whitespace(void)
{
    kdl_token token;

    kdl_str doc = kdl_str_from_cstr("\x0b");

    kdl_tokenizer* tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WHITESPACE);
    ASSERT(token.value.len == 1);
    ASSERT(token.value.data[0] == 0xb);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);
}

static void test_tokenizer_bom(void)
{
    kdl_token token;

    kdl_str doc = kdl_str_from_cstr("\xEF\xBB\xBF ");
    kdl_tokenizer* tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WHITESPACE);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);

    doc = kdl_str_from_cstr(" \xEF\xBB\xBF");
    tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_WHITESPACE);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);
}

static void test_tokenizer_illegal_codepoints(void)
{
    kdl_token token;

    kdl_str doc = kdl_str_from_cstr("no\007de"); // BEL is illegal in identifiers
    kdl_tokenizer* tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);

    doc = kdl_str_from_cstr("\"this string contains a backspace \x08\"");
    tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);

    doc = kdl_str_from_cstr("// Not even comments can contain \x1B");
    tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);

    doc = kdl_str_from_cstr("/* multi-line comments also can't have \x15 */");
    tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);

    doc = kdl_str_from_cstr("###\" multi-line strings must't \x10 \"###");
    tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_ERROR);

    kdl_destroy_tokenizer(tok);
}

static void test_tokenizer_equals(void)
{
    kdl_token token;

    kdl_str doc = kdl_str_from_cstr("\xef\xbc\x9d"); // Fullwidth equals U+FF1D
    kdl_tokenizer* tok = kdl_create_string_tokenizer(doc);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_EQUALS);

    kdl_destroy_tokenizer(tok);
}

static void test_string_escapes(void)
{
    kdl_str s = kdl_str_from_cstr("\\s\\  \n\n\t  \\u{1b}");
    kdl_owned_string unesc = kdl_unescape_v2(&s);

    ASSERT(unesc.len == 2);
    ASSERT(memcmp(unesc.data, " \x1b", 2) == 0);

    kdl_str unesc_ = kdl_borrow_str(&unesc);
    kdl_owned_string reesc = kdl_escape_v2(&unesc_, KDL_ESCAPE_DEFAULT);

    ASSERT(reesc.len == 7);
    ASSERT(memcmp(reesc.data, " \\u{1b}", 7) == 0);

    kdl_free_string(&unesc);
    kdl_free_string(&reesc);
}

static void test_parser_v1_raw_string(void)
{
    kdl_str doc = kdl_str_from_cstr("r#\"a\"# ");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_VERSION_1);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "a", 1) == 0);

    kdl_destroy_parser(parser);

    parser = kdl_create_string_parser(doc, KDL_DETECT_VERSION);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "a", 1) == 0);

    kdl_destroy_parser(parser);

    parser = kdl_create_string_parser(doc, KDL_DEFAULTS);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "a", 1) == 0);

    kdl_destroy_parser(parser);

    // Test that V1 raw strings are illegal in V2
    parser = kdl_create_string_parser(doc, KDL_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_parser_v2_raw_string(void)
{
    kdl_str doc = kdl_str_from_cstr("#\"a\"#");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "a", 1) == 0);

    kdl_destroy_parser(parser);

    parser = kdl_create_string_parser(doc, KDL_DETECT_VERSION);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "a", 1) == 0);

    kdl_destroy_parser(parser);
}

void TEST_MAIN(void)
{
    run_test("Tokenizer: KDLv2 strings", &test_tokenizer_strings);
    run_test("Tokenizer: KDLv2 identifiers", &test_tokenizer_identifiers);
    run_test("Tokenizer: KDLv2 whitespace", &test_tokenizer_whitespace);
    run_test("Tokenizer: byte-order-mark", &test_tokenizer_bom);
    run_test("Tokenizer: illegal codepoints", &test_tokenizer_illegal_codepoints);
    run_test("Tokenizer: KDLv2 equals sign", &test_tokenizer_equals);
    run_test("Strings: KDLv2 escapes", &test_string_escapes);
    run_test("Parser: KDLv1 raw string", &test_parser_v1_raw_string);
    run_test("Parser: KDLv2 raw string", &test_parser_v2_raw_string);
}
