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

void TEST_MAIN(void)
{
    run_test("Tokenizer: KDLv2 strings", &test_tokenizer_strings);
}
