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
    ASSERT(token.type == KDL_TOKEN_RAW_STRING);
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

void TEST_MAIN(void)
{
    run_test("Tokenizer: KDLv2 strings", &test_tokenizer_strings);
    run_test("Tokenizer: KDLv2 identifiers", &test_tokenizer_identifiers);
    run_test("Tokenizer: KDLv2 identifiers", &test_tokenizer_whitespace);
}
