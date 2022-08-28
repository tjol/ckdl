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

    kdl_parser *parser = kdl_create_parser_for_stream(&read_func, (void*)in, 0);

    bool have_error = false;
    bool eof = false;
    while (1) {
        kdl_event_data *event = kdl_parser_next_event(parser);
        char const *event_name = NULL;

        switch (event->event) {
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

        printf("%s value=", event_name);
        switch (event->value.type) {
        case KDL_TYPE_NUMBER:
            switch (event->value.value.number.type) {
            case KDL_NUMBER_TYPE_INTEGER:
                printf("%lld", event->value.value.number.value.integer);
                break;
            case KDL_NUMBER_TYPE_FLOATING_POINT:
                printf("%g", event->value.value.number.value.floating_point);
                break;
            case KDL_NUMBER_TYPE_STRING_ENCODED:
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
