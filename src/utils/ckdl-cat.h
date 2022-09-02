#ifndef CKDL_CAT_H_
#define CKDL_CAT_H_

#include <kdl/common.h>

#include <stdio.h>
#include <stdbool.h>

bool kdl_cat_file_to_file(FILE *in, FILE *out);
kdl_owned_string kdl_cat_file_to_string(FILE *in);

#endif // CKDL_CAT_H_
