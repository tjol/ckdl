#include <kdl/parser.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

static size_t read_func(void *user_data, char *buf, size_t bufsize)
{
    FILE *fp = (FILE*) user_data;
    return fread(buf, 1, bufsize, fp);
}

static inline void print_kdl_str(kdl_str const *s)
{
    fwrite(s->data, 1, s->len, stdout);
}

static inline void print_kdl_str_repr(kdl_str const *s)
{
    kdl_owned_string tmp_str = kdl_escape(s);
    printf("\"%s\"", tmp_str.data);
    kdl_free_string(&tmp_str);
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

    kdl_parser *parser = kdl_create_parser_for_stream(&read_func, (void*)in, parse_opts);

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

        if (slashdash) {
            printf("(commented-out)");
        }
        printf("%s value=", event_name);
        switch (event->value.type) {
        case KDL_TYPE_NUMBER:
            switch (event->value.value.number.type) {
            case KDL_NUMBER_TYPE_INTEGER:
                printf("(i64)%lld", event->value.value.number.value.integer);
                break;
            case KDL_NUMBER_TYPE_FLOATING_POINT:
                printf("(f64)%g", event->value.value.number.value.floating_point);
                break;
            case KDL_NUMBER_TYPE_STRING_ENCODED:
                printf("(str)");
                print_kdl_str(&event->value.value.number.value.string);
                break;
            default:
                printf("(error)null");
                break;
            }
            break;
        case KDL_TYPE_BOOLEAN:
            if (event->value.value.boolean) {
                printf("true");
            } else {
                printf("false");
            }
            break;
        case KDL_TYPE_NULL:
            printf("null");
            break;
        case KDL_TYPE_STRING:
            print_kdl_str_repr(&event->value.value.string);
            break;
        default:
            printf("(error)null");
            break;
        }

        if (event->type_annotation.data != NULL) {
            printf(" type=");
            print_kdl_str_repr(&event->type_annotation);
        }

        if (event->property_key.data != NULL) {
            printf(" key=");
            print_kdl_str_repr(&event->property_key);
        }

        puts("");

        if (have_error || eof) break;
    }
    
    if (in != stdin) {
        fclose(in);
    }
    return have_error ? 1 : 0;
}
