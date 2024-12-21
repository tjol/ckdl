#include "ckdl-cat.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

static void print_usage(char const* argv0, FILE* fp)
{
    fprintf(fp, "Usage: %s [-h]\n\n", argv0);
    fprintf(fp, "    -h    Print usage information\n");
    fprintf(fp, "    -1    Output KDLv1 (default)\n");
    fprintf(fp, "    -2    Output KDLv2\n");
    fprintf(fp, "    -m    Monoglot mode: accept only one KDL version as input\n");
}

int main(int argc, char** argv)
{
    FILE* in = stdin;
    char const* argv0 = argv[0];
    bool opts_ended = false;

    bool monoglot = false;
    kdl_version version = KDL_VERSION_2;

    while (--argc) {
        ++argv;
        if (!opts_ended && **argv == '-') {
            // options
            for (char const* p = *argv + 1; *p; ++p) {
                if (*p == 'h') {
                    print_usage(argv0, stdout);
                    return 0;
                } else if (*p == '1') {
                    version = KDL_VERSION_1;
                } else if (*p == '2') {
                    version = KDL_VERSION_2;
                } else if (*p == 'm') {
                    monoglot = true;
                } else if (*p == '-') {
                    opts_ended = true;
                } else {
                    print_usage(argv0, stderr);
                    return 2;
                }
            }
        } else {
            char const* fn = *argv;
            in = fopen(fn, "r");
            if (in == NULL) {
                fprintf(stderr, "Error opening file \"%s\": %s\n", fn, strerror(errno));
                return 1;
            }
        }
    }

    kdl_parse_option parse_opt = monoglot
        ? (version == KDL_VERSION_1 ? KDL_READ_VERSION_1 : KDL_READ_VERSION_2)
        : KDL_DETECT_VERSION;
    kdl_emitter_options emit_opt = KDL_DEFAULT_EMITTER_OPTIONS;
    emit_opt.version = version;

    bool ok = kdl_cat_file_to_file_ex(in, stdout, parse_opt, &emit_opt);

    if (in != stdin) {
        fclose(in);
    }
    return ok ? 0 : 1;
}
