#ifndef KDL_TOKENIZER_H_
#define KDL_TOKENIZER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum kdl_tokenizer_status {
    KDL_TOKENIZER_OK,
    KDL_TOKENIZER_EOF,
    KDL_TOKENIZER_ERROR
};

enum kdl_token_type {
    KDL_TOKEN_START_TYPE,
    KDL_TOKEN_END_TYPE,
    KDL_TOKEN_WORD,
    KDL_TOKEN_STRING,
    KDL_TOKEN_RAW_STRING,
    KDL_TOKEN_SINGLE_LINE_COMMENT,
    KDL_TOKEN_SLASHDASH,
    KDL_TOKEN_MULTI_LINE_COMMENT,
    KDL_TOKEN_EQUALS,
    KDL_TOKEN_START_CHILDREN,
    KDL_TOKEN_END_CHILDREN,
    KDL_TOKEN_NEWLINE,
    KDL_TOKEN_SEMICOLON,
    KDL_TOKEN_LINE_CONTINUATION,
    KDL_TOKEN_WHITESPACE
};

typedef enum kdl_tokenizer_status kdl_tokenizer_status;
typedef enum kdl_token_type kdl_token_type;
typedef struct kdl_token kdl_token;
typedef struct _kdl_tokenizer kdl_tokenizer;

struct kdl_token {
    enum kdl_token_type type;
    kdl_str value;
};

KDL_NODISCARD KDL_EXPORT kdl_tokenizer *kdl_create_string_tokenizer(kdl_str doc);
KDL_NODISCARD KDL_EXPORT kdl_tokenizer *kdl_create_stream_tokenizer(kdl_read_func read_func, void *user_data);
KDL_EXPORT void kdl_destroy_tokenizer(kdl_tokenizer *tokenizer);

KDL_EXPORT kdl_tokenizer_status kdl_pop_token(kdl_tokenizer *tokenizer, kdl_token *dest);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // KDL_TOKENIZER_H_
