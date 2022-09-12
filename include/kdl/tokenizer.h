#ifndef KDL_TOKENIZER_H_
#define KDL_TOKENIZER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Return code for the tokenizer
enum kdl_tokenizer_status {
    KDL_TOKENIZER_OK,    // ok: token returned
    KDL_TOKENIZER_EOF,   // regular end of file
    KDL_TOKENIZER_ERROR  // error
};

// Type of token
enum kdl_token_type {
    KDL_TOKEN_START_TYPE,          // '('
    KDL_TOKEN_END_TYPE,            // ')'
    KDL_TOKEN_WORD,                // identifier, number, boolean, or null
    KDL_TOKEN_STRING,              // regular string
    KDL_TOKEN_RAW_STRING,          // raw string
    KDL_TOKEN_SINGLE_LINE_COMMENT, // // ...
    KDL_TOKEN_SLASHDASH,           // /-
    KDL_TOKEN_MULTI_LINE_COMMENT,  // /* ... */
    KDL_TOKEN_EQUALS,              // '='
    KDL_TOKEN_START_CHILDREN,      // '{'
    KDL_TOKEN_END_CHILDREN,        // '}'
    KDL_TOKEN_NEWLINE,             // LF, CR, or CRLF
    KDL_TOKEN_SEMICOLON,           // ';'
    KDL_TOKEN_LINE_CONTINUATION,   // '\\'
    KDL_TOKEN_WHITESPACE           // any regular whitespace
};

typedef enum kdl_tokenizer_status kdl_tokenizer_status;
typedef enum kdl_token_type kdl_token_type;
typedef struct kdl_token kdl_token;
typedef struct _kdl_tokenizer kdl_tokenizer;

// A token, consisting of a token type and token text
struct kdl_token {
    enum kdl_token_type type;
    kdl_str value;
};

// Create a tokenizer that reads from a string
KDL_NODISCARD KDL_EXPORT kdl_tokenizer *kdl_create_string_tokenizer(kdl_str doc);
// Create a tokenizer that reads data by calling a user-supplied function
KDL_NODISCARD KDL_EXPORT kdl_tokenizer *kdl_create_stream_tokenizer(kdl_read_func read_func, void *user_data);
// Destroy a tokenizer
KDL_EXPORT void kdl_destroy_tokenizer(kdl_tokenizer *tokenizer);

// Get the next token and write it to a user-supplied structure (or return an error)
KDL_EXPORT kdl_tokenizer_status kdl_pop_token(kdl_tokenizer *tokenizer, kdl_token *dest);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // KDL_TOKENIZER_H_
