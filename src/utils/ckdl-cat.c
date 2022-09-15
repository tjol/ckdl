#include <kdl/kdl.h>
#include "ckdl-cat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static size_t read_func(void *user_data, char *buf, size_t bufsize);
static size_t write_func(void *user_data, char const *data, size_t nbytes);

struct proplist {
    struct prop {
        kdl_owned_string name;
        kdl_value value;
        kdl_owned_string val_str;
        kdl_owned_string type_str;
    } *props;
    size_t count;
    size_t size;
};

static void proplist_clear(struct proplist *pl);
static void proplist_append(struct proplist *pl, kdl_str name, kdl_value const* value);
static void proplist_emit(kdl_emitter *emitter, struct proplist *pl);

static bool kdl_cat_impl(kdl_parser *parser, kdl_emitter *emitter);

bool kdl_cat_file_to_file(FILE *in, FILE *out)
{
    return kdl_cat_file_to_file_opt(in, out, &KDL_DEFAULT_EMITTER_OPTIONS);
}

bool kdl_cat_file_to_file_opt(FILE *in, FILE *out, kdl_emitter_options const *opt)
{
    bool ok = true;

    kdl_parser *parser = kdl_create_stream_parser(&read_func, (void*)in, KDL_DEFAULTS);
    kdl_emitter *emitter = kdl_create_stream_emitter(&write_func, (void*)out, opt);

    if (parser == NULL || emitter == NULL) {
        ok = false;
    } else {
        ok = kdl_cat_impl(parser, emitter);
    }

    kdl_destroy_emitter(emitter);
    kdl_destroy_parser(parser);
    return ok;
}

kdl_owned_string kdl_cat_file_to_string(FILE *in)
{
    return kdl_cat_file_to_string_opt(in, &KDL_DEFAULT_EMITTER_OPTIONS);
}

kdl_owned_string kdl_cat_file_to_string_opt(FILE *in, kdl_emitter_options const *opt)
{
    kdl_owned_string result = { NULL, 0 };

    kdl_parser *parser = kdl_create_stream_parser(&read_func, (void*)in, KDL_DEFAULTS);
    kdl_emitter *emitter = kdl_create_buffering_emitter(opt);

    bool ok = true;
    if (parser == NULL || emitter == NULL) {
        ok = false;
    } else {
        ok = kdl_cat_impl(parser, emitter);
    }

    if (ok) {
        kdl_str emitter_buffer = kdl_get_emitter_buffer(emitter);
        result = kdl_clone_str(&emitter_buffer);
    }

    kdl_destroy_emitter(emitter);
    kdl_destroy_parser(parser);
    return result;
}

static bool kdl_cat_impl(kdl_parser *parser, kdl_emitter *emitter)
{
    // state
    bool in_node_list = true;
    struct proplist props = { NULL, 0, 0 };

    while (true) {
        kdl_event_data *ev = kdl_parser_next_event(parser);
        switch (ev->event) {
        case KDL_EVENT_EOF:
            if (!kdl_emit_end(emitter)) return false;
            return true;
        case KDL_EVENT_PARSE_ERROR:
            return false;
        case KDL_EVENT_START_NODE:
            if (!in_node_list) {
                proplist_emit(emitter, &props);
                proplist_clear(&props);
                if (!kdl_start_emitting_children(emitter)) return false;
            }
            if (ev->value.type_annotation.data == NULL) {
                if (!kdl_emit_node(emitter, ev->name)) return false;
            } else {
                if (!kdl_emit_node_with_type(emitter, ev->value.type_annotation, ev->name)) return false;
            }
            in_node_list = false;
            break;
        case KDL_EVENT_END_NODE:
            if (in_node_list) {
                // just ended a node, end the parent now
                if (!kdl_finish_emitting_children(emitter)) return false;
            } else {
                // regular end to childless node
                proplist_emit(emitter, &props);
                proplist_clear(&props);
            }
            in_node_list = true;
            break;
        case KDL_EVENT_ARGUMENT:
            kdl_emit_arg(emitter, &ev->value);
            break;
        case KDL_EVENT_PROPERTY:
            proplist_append(&props, ev->name, &ev->value);
            break;
        default:
            return false;
        }
    }
}

static size_t read_func(void *user_data, char *buf, size_t bufsize)
{
    FILE *fp = (FILE *)user_data;
    return fread(buf, 1, bufsize, fp);
}

static size_t write_func(void *user_data, char const *data, size_t nbytes)
{
    FILE *fp = (FILE *)user_data;
    return fwrite(data, 1, nbytes, fp);
}

static void free_prop(struct prop *p)
{
    kdl_free_string(&p->name);
    kdl_free_string(&p->type_str);
    kdl_free_string(&p->val_str);
}

static void proplist_clear(struct proplist *pl)
{
    if (pl->props != NULL) {
        for (size_t i = 0; i < pl->count; ++i) {
            struct prop *p = &pl->props[i];
            free_prop(p);
        }
        free(pl->props);
    }
    pl->props = NULL;
    pl->count = 0;
    pl->size = 0;
}

static void proplist_append(struct proplist *pl, kdl_str name, kdl_value const* value)
{
    if (pl->size <= pl->count) {
        pl->size = pl->count + 10;
        struct prop *props = realloc(pl->props, pl->size * sizeof(struct prop));
        if (props != NULL) {
            pl->props = props;
        } else {
            return;
        }
    }
    struct prop *p = &pl->props[pl->count++];
    p->name = kdl_clone_str(&name);
    p->value = *value;

    // clone type annotation (if needed)
    if (p->value.type_annotation.data != NULL) {
        p->type_str = kdl_clone_str(&p->value.type_annotation);
        p->value.type_annotation = kdl_borrow_str(&p->type_str);
    } else {
        p->type_str = (kdl_owned_string){ NULL, 0 };
    }

    // clone string value (if needed)
    if (p->value.type == KDL_TYPE_STRING) {
        p->val_str = kdl_clone_str(&p->value.string);
        p->value.string = kdl_borrow_str(&p->val_str);
    } else if (p->value.type == KDL_TYPE_NUMBER
        && p->value.number.type == KDL_NUMBER_TYPE_STRING_ENCODED) {
        p->val_str = kdl_clone_str(&p->value.number.string);
        p->value.number.string = kdl_borrow_str(&p->val_str);
    } else {
        p->val_str = (kdl_owned_string){ NULL, 0 };
    }
}

static struct prop *overwriting_merge(struct prop *out,
    struct prop *left, struct prop *right, struct prop *end);

static void proplist_emit(kdl_emitter *emitter, struct proplist *pl)
{
    // We need to emit the properties without duplicates, in lexical order
    // do a merge sort which overwrites duplicates
    struct prop *props = pl->props;
    struct prop *dest = malloc(sizeof(struct prop) * pl->count);
    if (dest == NULL) {
        return;
    }

    size_t count = pl->count;
    for (size_t wnd = 1; wnd < count; wnd *= 2) {
        struct prop *out_end = dest;
        for (size_t i = 0; i < count; i += 2 * wnd) {
            size_t r = i + wnd;
            size_t e = r + wnd;
            if (r > count) r = count;
            if (e > count) e = count;
            out_end = overwriting_merge(out_end, &props[i], &props[r], &props[e]);
            while (out_end - dest < (ptrdiff_t)e) {
                // mark as invalid
                out_end->name.data = NULL;
                // make sure it isn't freed later
                --pl->count;
                ++out_end;
            }
        }
        struct prop *tmp = dest;
        dest = props;
        props = tmp;
    }
    pl->size = count;
    pl->props = props; // might have swapped buffers

    free(dest);

    for (size_t i = 0; i < count; ++i) {
        struct prop *p = &props[i];
        if (p->name.data != NULL)
            kdl_emit_property(emitter, kdl_borrow_str(&p->name), &p->value);
    }
}

static int kdl_str_cmp(kdl_owned_string const *s1, kdl_owned_string const *s2)
{
    // sort NULL at the end
    if (s1->data == NULL) return 1;
    else if (s2->data == NULL) return -1;

    size_t min_len = s1->len > s2->len ? s2->len : s1->len;
    int cmp = memcmp(s1->data, s2->data, min_len);
    // deal with different length names
    if (cmp == 0) cmp = (int)(s1->len - s2->len);
    return cmp;
}

static struct prop *overwriting_merge(struct prop *out,
    struct prop *left, struct prop *right, struct prop *end)
{
    struct prop *boundary = right;
    struct prop *prev = NULL;
    while (left < boundary || right < end) {
        struct prop *next_item;
        if (left >= boundary) {
            next_item = right++;
        } else if (right >= end) {
            next_item = left++;
        } else {
            int cmp = kdl_str_cmp(&left->name, &right->name);
            if (cmp < 0) {
                // left is less
                next_item = left++;
            } else if (cmp > 0) {
                // right is less
                next_item = right++;
            } else {
                // they're the same
                // ignore left, use right
                free_prop(left++);
                next_item = right++;
            }
        }
        // insert the item into the destination array
        if (prev != NULL) {
            int cmp = kdl_str_cmp(&prev->name, &next_item->name);
            if (cmp == 0) {
                // same property name
                // overwrite
                out = prev;
                free_prop(out);
            }
        }
        prev = out;
        *(out++) = *next_item;
    }
    return out;
}
