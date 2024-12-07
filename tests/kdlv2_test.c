#include <kdl/kdl.h>

#include "test_util.h"

#include <string.h>

static void test_tokenizer_raw_strings(void)
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

static void test_tokenizer_multiline_strings(void)
{
    kdl_token token;

    char const* doc1 = "\"\"\"\nabc\"\n\"\"\"";
    char const* doc2 = "##\"\"\"\nabc\"\"\"#\n\"\"\"##";

    kdl_str k_doc1 = kdl_str_from_cstr(doc1);
    kdl_str k_doc2 = kdl_str_from_cstr(doc2);

    kdl_tokenizer* tok = kdl_create_string_tokenizer(k_doc1);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_MULTILINE_STRING);
    ASSERT(token.value.len == 6);
    ASSERT(memcmp(token.value.data, "\nabc\"\n", 6) == 0);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_EOF);

    kdl_destroy_tokenizer(tok);

    tok = kdl_create_string_tokenizer(k_doc2);

    ASSERT(kdl_pop_token(tok, &token) == KDL_TOKENIZER_OK);
    ASSERT(token.type == KDL_TOKEN_RAW_MULTILINE_STRING);
    ASSERT(token.value.len == 9);
    ASSERT(memcmp(token.value.data, "\nabc\"\"\"#\n", 9) == 0);

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
    kdl_str s = kdl_str_from_cstr("\\s\\  \t  \\u{1b}\\\x0b    ");
    kdl_owned_string unesc = kdl_unescape_v(KDL_VERSION_2, &s);

    ASSERT(unesc.len == 2);
    ASSERT(memcmp(unesc.data, " \x1b", 2) == 0);

    kdl_str unesc_ = kdl_borrow_str(&unesc);
    kdl_owned_string reesc = kdl_escape_v(KDL_VERSION_2, &unesc_, KDL_ESCAPE_DEFAULT);

    ASSERT(reesc.len == 7);
    ASSERT(memcmp(reesc.data, " \\u{1b}", 7) == 0);

    kdl_free_string(&unesc);
    kdl_free_string(&reesc);
}

static void test_multiline_strings(void)
{
    kdl_str expected = kdl_str_from_cstr("test\n\nstring");
    kdl_str escaped_variants[] = {
        kdl_str_from_cstr("test\\n\\nstring"),
        kdl_str_from_cstr("\ntest\\n\\nstring\n"),
        kdl_str_from_cstr("\r\ntest\r\rstring\n"),
        kdl_str_from_cstr("\n \t test\f \t \f \t string\n \t "),
        kdl_str_from_cstr("\n  te\\\n\n  st\n\n  string\n  "),
    };
    int n_escaped_variants = sizeof(escaped_variants) / sizeof(escaped_variants[0]);

    for (int i = 0; i < n_escaped_variants; ++i) {
        kdl_owned_string result = kdl_unescape_v(KDL_VERSION_2, &escaped_variants[i]);
        ASSERT(result.len == expected.len);
        ASSERT(memcmp(result.data, expected.data, expected.len) == 0);
        kdl_free_string(&result);
    }

    kdl_str invalid_strings[] = {
        kdl_str_from_cstr("\na\n  "),         // indent missing at start
        kdl_str_from_cstr("\n  \r\t\ta\n  "), // indent wrong in middle
    };
    int n_invalid_strings = sizeof(invalid_strings) / sizeof(invalid_strings[0]);

    for (int i = 0; i < n_invalid_strings; ++i) {
        kdl_owned_string result = kdl_unescape_v(KDL_VERSION_2, &invalid_strings[i]);
        ASSERT(result.data == NULL);
    }

    kdl_str edge_cases[][2] = {
        {kdl_str_from_cstr("\n\t"),              kdl_str_from_cstr("")       }, // empty
        {kdl_str_from_cstr("\n"),                kdl_str_from_cstr("")       }, // empty
        {kdl_str_from_cstr("\n\n  hello\n  "),   kdl_str_from_cstr("\nhello")}, // double newline at start
        {kdl_str_from_cstr("\n  \\\n     \n  "), kdl_str_from_cstr("")       }, // escaped newline within
        {kdl_str_from_cstr("\n  \n     \n  "),   kdl_str_from_cstr("\n   ")  }, // whitespace only
    };
    int n_edge_cases = sizeof(edge_cases) / sizeof(edge_cases[0]);

    for (int i = 0; i < n_edge_cases; ++i) {
        kdl_str const* input = &edge_cases[i][0];
        kdl_str const* output = &edge_cases[i][1];
        kdl_owned_string result = kdl_unescape_v(KDL_VERSION_2, input);
        ASSERT(result.len == output->len);
        ASSERT(memcmp(result.data, output->data, output->len) == 0);
        kdl_free_string(&result);
    }
}

static void test_parser_v1_raw_string(void)
{
    kdl_str doc = kdl_str_from_cstr("r#\"a\"# ");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_READ_VERSION_1);

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
    parser = kdl_create_string_parser(doc, KDL_READ_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_parser_v2_raw_string(void)
{
    kdl_str doc = kdl_str_from_cstr("#\"a\"#");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_READ_VERSION_2);

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

static void test_parser_hashtag_null(void)
{
    kdl_str doc = kdl_str_from_cstr("a #null #true #false");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_READ_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_BOOLEAN);
    ASSERT(ev->value.boolean == true);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_BOOLEAN);
    ASSERT(ev->value.boolean == false);

    kdl_destroy_parser(parser);
}

static void test_parser_hashless_syntax_error(void)
{
    kdl_str doc = kdl_str_from_cstr("a null");
    kdl_event_data* ev;

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_READ_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_parser_hashtag_null_is_v2(void)
{
    kdl_event_data* ev;
    kdl_str doc = kdl_str_from_cstr("a #null true");

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_DETECT_VERSION);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);

    doc = kdl_str_from_cstr("a null #true");

    parser = kdl_create_string_parser(doc, KDL_DETECT_VERSION);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_parser_kdlv2_whitespace(void)
{
    kdl_event_data* ev;
    kdl_str doc = kdl_str_from_cstr("( a ) n p = 1");

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_DETECT_VERSION);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(ev->name.len == 1);
    ASSERT(ev->name.data[0] == 'n');
    ASSERT(ev->value.type_annotation.len == 1);
    ASSERT(ev->value.type_annotation.data[0] == 'a');

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PROPERTY);
    ASSERT(ev->name.len == 1);
    ASSERT(ev->name.data[0] == 'p');
    ASSERT(ev->value.type == KDL_TYPE_NUMBER);

    kdl_destroy_parser(parser);
}

static void test_parser_comment_in_property(void)
{
    kdl_event_data* ev;
    kdl_str doc = kdl_str_from_cstr("node key /* equals sign coming up */ = \\ \n value");

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_EMIT_COMMENTS | KDL_READ_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_COMMENT);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PROPERTY);
    ASSERT(ev->name.len == 3);
    ASSERT(memcmp(ev->name.data, "key", 3) == 0);
    ASSERT(ev->value.type == KDL_TYPE_STRING);
    ASSERT(ev->value.string.len == 5);
    ASSERT(memcmp(ev->value.string.data, "value", 5) == 0);

    kdl_destroy_parser(parser);
}


static void test_parser_comment_in_type(void)
{
    kdl_event_data* ev;
    kdl_str doc = kdl_str_from_cstr("(/* */ t1) node (t2 /* */)arg");

    kdl_parser* parser = kdl_create_string_parser(doc, KDL_EMIT_COMMENTS | KDL_READ_VERSION_2);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_COMMENT);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(ev->value.type_annotation.len == 2);
    ASSERT(memcmp(ev->value.type_annotation.data, "t1", 2) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_COMMENT);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->name.data == NULL);
    ASSERT(ev->value.type == KDL_TYPE_STRING);
    ASSERT(ev->value.string.len == 3);
    ASSERT(memcmp(ev->value.string.data, "arg", 3) == 0);
    ASSERT(ev->value.type_annotation.len == 2);
    ASSERT(memcmp(ev->value.type_annotation.data, "t2", 2) == 0);

    kdl_destroy_parser(parser);
}


void TEST_MAIN(void)
{
    run_test("Tokenizer: KDLv2 raw strings", &test_tokenizer_raw_strings);
    run_test("Tokenizer: KDLv2 multi-line strings", &test_tokenizer_multiline_strings);
    run_test("Tokenizer: KDLv2 identifiers", &test_tokenizer_identifiers);
    run_test("Tokenizer: KDLv2 whitespace", &test_tokenizer_whitespace);
    run_test("Tokenizer: byte-order-mark", &test_tokenizer_bom);
    run_test("Tokenizer: illegal codepoints", &test_tokenizer_illegal_codepoints);
    run_test("Tokenizer: KDLv2 equals sign", &test_tokenizer_equals);
    run_test("Strings: KDLv2 escapes", &test_string_escapes);
    run_test("Strings: KDLv2 multi-line strings", &test_multiline_strings);
    run_test("Parser: KDLv1 raw string", &test_parser_v1_raw_string);
    run_test("Parser: KDLv2 raw string", &test_parser_v2_raw_string);
    run_test("Parser: KDLv2 #null", &test_parser_hashtag_null);
    run_test("Parser: null is a syntax error in KDLv2", &test_parser_hashless_syntax_error);
    run_test("Parser: #null means we're in v2, etc.", &test_parser_hashtag_null_is_v2);
    run_test("Parser: whitespace in new place", &test_parser_kdlv2_whitespace);
    run_test("Parser: comment in property definition", &test_parser_comment_in_property);
    run_test("Parser: comment in type annotation", &test_parser_comment_in_type);
}
