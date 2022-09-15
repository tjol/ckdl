#include <kdl/kdl.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

static size_t read_func(void *user_data, char *buf, size_t bufsize)
{
    FILE *fp = (FILE*) user_data;
    return fread(buf, 1, bufsize, fp);
}

static size_t write_func(void *user_data, char const *data, size_t nbytes)
{
    (void)user_data;
    return fwrite(data, 1, nbytes, stdout);
}

int main(int argc, char **argv)
{
    FILE *in = NULL;
    if (argc == 1) {
        in = stdin;
    } else if (argc == 2) {
        char const *fn = argv[1];
        if (strcmp(fn, "-") == 0) {
            in = stdin;
        } else {
            in = fopen(fn, "r");
            if (in == NULL) {
                fprintf(stderr, "Error opening file \"%s\": %s\n",
                    fn, strerror(errno));
                return 1;
            }
        }
    } else {
        fprintf(stderr, "Error: Too many arguments\n");
        return 2;
    }

    kdl_tokenizer *tokenizer = kdl_create_stream_tokenizer(&read_func, (void*)in);
    kdl_emitter *emitter = kdl_create_stream_emitter(&write_func, NULL, &KDL_DEFAULT_EMITTER_OPTIONS);

    if (tokenizer == NULL || emitter == NULL) {
        fprintf(stderr, "Initialization error\n");
        return -1;
    }

    bool have_error = false;
    while (1) {
        kdl_token token;
        kdl_tokenizer_status status = kdl_pop_token(tokenizer, &token);
        if (status == KDL_TOKENIZER_ERROR) {
            kdl_emit_end(emitter);
            fprintf(stderr, "Tokenization error\n");
            have_error = true;
            break;
        } else if (status == KDL_TOKENIZER_EOF) {
            break;
        }

        char const *token_type_name = NULL;
        switch (token.type) {
        case KDL_TOKEN_START_TYPE:
            token_type_name = "KDL_TOKEN_START_TYPE";
            break;
        case KDL_TOKEN_END_TYPE:
            token_type_name = "KDL_TOKEN_END_TYPE";
            break;
        case KDL_TOKEN_WORD:
            token_type_name = "KDL_TOKEN_WORD";
            break;
        case KDL_TOKEN_STRING:
            token_type_name = "KDL_TOKEN_STRING";
            break;
        case KDL_TOKEN_RAW_STRING:
            token_type_name = "KDL_TOKEN_RAW_STRING";
            break;
        case KDL_TOKEN_SINGLE_LINE_COMMENT:
            token_type_name = "KDL_TOKEN_SINGLE_LINE_COMMENT";
            break;
        case KDL_TOKEN_SLASHDASH:
            token_type_name = "KDL_TOKEN_SLASHDASH";
            break;
        case KDL_TOKEN_MULTI_LINE_COMMENT:
            token_type_name = "KDL_TOKEN_MULTI_LINE_COMMENT";
            break;
        case KDL_TOKEN_EQUALS:
            token_type_name = "KDL_TOKEN_EQUALS";
            break;
        case KDL_TOKEN_START_CHILDREN:
            token_type_name = "KDL_TOKEN_START_CHILDREN";
            break;
        case KDL_TOKEN_END_CHILDREN:
            token_type_name = "KDL_TOKEN_END_CHILDREN";
            break;
        case KDL_TOKEN_NEWLINE:
            token_type_name = "KDL_TOKEN_NEWLINE";
            break;
        case KDL_TOKEN_SEMICOLON:
            token_type_name = "KDL_TOKEN_SEMICOLON";
            break;
        case KDL_TOKEN_LINE_CONTINUATION:
            token_type_name = "KDL_TOKEN_LINE_CONTINUATION";
            break;
        case KDL_TOKEN_WHITESPACE:
            token_type_name = "KDL_TOKEN_WHITESPACE";
            break;
        default:
            break;
        }

        if (token_type_name == NULL) {
            fprintf(stderr, "Unknown token type %d\n", (int)token.type);
            have_error = true;
            break;
        }

        kdl_emit_node(emitter, kdl_str_from_cstr(token_type_name));
        kdl_value val = (kdl_value){
            .type = KDL_TYPE_STRING,
            .string = token.value
        };
        kdl_emit_arg(emitter, &val);
    }

    kdl_destroy_emitter(emitter);
    kdl_destroy_tokenizer(tokenizer);
    if (in != stdin) {
        fclose(in);
    }
    return have_error ? 1 : 0;
}
