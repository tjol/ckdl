#ifndef CKDL_CAT_H_
#define CKDL_CAT_H_

#include <kdl/common.h>
#include <kdl/emitter.h>

#include <stdio.h>
#include <stdbool.h>

bool kdl_cat_file_to_file(FILE *in, FILE *out);
bool kdl_cat_file_to_file_opt(FILE *in, FILE *out, kdl_emitter_options const *opt);
kdl_owned_string kdl_cat_file_to_string(FILE *in);
kdl_owned_string kdl_cat_file_to_string_opt(FILE *in, kdl_emitter_options const *opt);

#endif // CKDL_CAT_H_
