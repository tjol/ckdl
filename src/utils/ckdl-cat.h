#ifndef CKDL_CAT_H_
#define CKDL_CAT_H_

#include <kdl/common.h>
#include <kdl/emitter.h>
#include <kdl/parser.h>

#include <stdbool.h>
#include <stdio.h>

bool kdl_cat_file_to_file(FILE* in, FILE* out);
bool kdl_cat_file_to_file_opt(FILE* in, FILE* out, kdl_emitter_options const* opt);
bool kdl_cat_file_to_file_ex(FILE* in, FILE* out, kdl_parse_option parse_opt, kdl_emitter_options const* emit_opt);
kdl_owned_string kdl_cat_file_to_string(FILE* in);
kdl_owned_string kdl_cat_file_to_string_opt(FILE* in, kdl_emitter_options const* opt);
kdl_owned_string kdl_cat_file_to_string_ex(FILE* in, kdl_parse_option parse_opt, kdl_emitter_options const* emit_opt);

#endif // CKDL_CAT_H_
