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

void print_usage(char const *argv0, FILE *fp)
{
    fprintf(fp, "Usage: %s [-h] [-c]\n\n", argv0);
    fprintf(fp, "    -h    Print usage information\n");
    fprintf(fp, "    -c    Emit comments\n");
}


int main(int argc, char **argv)
{
    FILE *in = stdin;
    char const* argv0 = argv[0];
    kdl_parse_option parse_opts = KDL_DEFAULTS;
    bool opts_ended = false;

    while (--argc) {
        ++argv;
        if (!opts_ended && **argv == '-') {
            // options
            for (char const *p = *argv+1; *p; ++p) {
                if (*p == 'h') {
                    print_usage(argv0, stdout);
                    return 0;
                } else if (*p == 'c') {
                    parse_opts |= KDL_EMIT_COMMENTS;
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

    kdl_parser *parser = kdl_create_stream_parser(&read_func, (void*)in, parse_opts);
    kdl_emitter *emitter = kdl_create_stream_emitter(&write_func, NULL, &KDL_DEFAULT_EMITTER_OPTIONS);

    if (parser == NULL || emitter == NULL) {
        fprintf(stderr, "Initialization error\n");
        return -1;
    }

    // constants
    kdl_str const slashdash_type_str = (kdl_str){ "commented-out", 13 };
    kdl_str const value_str = (kdl_str){ "value", 5 };
    kdl_str const name_str = (kdl_str){ "name", 4 };

    bool have_error = false;
    bool eof = false;
    bool slashdash = false;
    while (1) {
        kdl_event_data *event = kdl_parser_next_event(parser);
        char const *event_name = NULL;

        if (event->event == KDL_EVENT_COMMENT) {
            // regular comment
            event_name = "KDL_EVENT_COMMENT";
            slashdash = false;
        } else {
            // event could be a slashdash variant
            kdl_event bare_event = event->event & 0xffff;
            slashdash = (event->event & KDL_EVENT_COMMENT) != 0;
            switch (bare_event) {
            case KDL_EVENT_EOF:
                event_name = "KDL_EVENT_EOF";
                eof = true;
                break;
            case KDL_EVENT_START_NODE:
                event_name = "KDL_EVENT_START_NODE";
                break;
            case KDL_EVENT_END_NODE:
                event_name = "KDL_EVENT_END_NODE";
                break;
            case KDL_EVENT_ARGUMENT:
                event_name = "KDL_EVENT_ARGUMENT";
                break;
            case KDL_EVENT_PROPERTY:
                event_name = "KDL_EVENT_PROPERTY";
                break;
            case KDL_EVENT_PARSE_ERROR:
                event_name = "KDL_EVENT_PARSE_ERROR";
                have_error = true;
                break;
            default:
                event_name = "unknown-event";
                have_error = true;
                break;
            }
        }

        kdl_str event_name_str = kdl_str_from_cstr(event_name);

        if (slashdash) {
            kdl_emit_node_with_type(emitter, slashdash_type_str, event_name_str);
        } else {
            kdl_emit_node(emitter, event_name_str);
        }
        if (event->name.data != NULL) {
            kdl_value name_val = (kdl_value) {
                .type = KDL_TYPE_STRING,
                .type_annotation = { NULL, 0 },
                .string = event->name
            };
            kdl_emit_property(emitter, name_str, &name_val);
        }

        kdl_emit_property(emitter, value_str, &event->value);


        if (have_error || eof) break;
    }

    kdl_destroy_emitter(emitter);
    kdl_destroy_parser(parser);

    if (in != stdin) {
        fclose(in);
    }
    return have_error ? 1 : 0;
}
