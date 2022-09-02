#include "ckdl-cat.h"

#include <stdbool.h>
#include <string.h>
#include <errno.h>

static void print_usage(char const *argv0, FILE *fp)
{
    fprintf(fp, "Usage: %s [-h]\n\n", argv0);
    fprintf(fp, "    -h    Print usage information\n");
}

int main(int argc, char **argv)
{
    FILE *in = stdin;
    char const* argv0 = argv[0];
    bool opts_ended = false;

    while (--argc) {
        ++argv;
        if (!opts_ended && **argv == '-') {
            // options
            for (char const *p = *argv+1; *p; ++p) {
                if (*p == 'h') {
                    print_usage(argv0, stdout);
                    return 0;
                } else if (*p == '-') {
                    opts_ended = true;
                } else {
                    print_usage(argv0, stderr);
                    return 2;
                }
            }
        } else {
            char const *fn = *argv;
            in = fopen(fn, "r");
            if (in == NULL) {
                fprintf(stderr, "Error opening file \"%s\": %s\n",
                    fn, strerror(errno));
                return 1;
            }
        }
    }

    bool ok = kdl_cat_file_to_file(in, stdout);

    if (in != stdin) {
        fclose(in);
    }
    return ok ? 0 : 1;
}
