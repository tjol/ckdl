#include "kdl/emitter.h"

#include "str.h"
#include "grammar.h"
#include "utf8.h"

#include <float.h>
#include <stdio.h>

#define INITIAL_BUFFER_SIZE 4096

struct _kdl_emitter
{
    kdl_emitter_options opt;
    kdl_write_func write_func;
    void *write_user_data;
    int depth;
    bool start_of_line;
    _kdl_write_buffer buf;
};

static size_t _buffer_write_func(void *user_data, char const *data, size_t nbytes)
{
    _kdl_write_buffer *buf = (_kdl_write_buffer*)user_data;
    if (_kdl_buf_push_chars(buf, data, nbytes))
        return nbytes;
    else
        return 0;
}

kdl_emitter *kdl_create_buffering_emitter(kdl_emitter_options opt)
{
    kdl_emitter *self = malloc(sizeof(kdl_emitter));
    self->opt = opt;
    self->write_func = &_buffer_write_func;
    self->write_user_data = &self->buf;
    self->depth = 0;
    self->start_of_line = true;
    self->buf = _kdl_new_write_buffer(INITIAL_BUFFER_SIZE);
    return self;
}

kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, kdl_emitter_options opt)
{
    kdl_emitter *self = malloc(sizeof(kdl_emitter));
    self->opt = opt;
    self->write_func = write_func;
    self->write_user_data = user_data;
    self->depth = 0;
    self->start_of_line = true;
    self->buf = (_kdl_write_buffer){ NULL, 0, 0 };
    return self;
}

void kdl_destroy_emitter(kdl_emitter *self)
{
    kdl_emit_end(self);
    if (self->buf.buf != NULL) {
        _kdl_free_write_buffer(&self->buf);
    }
}

static bool _kdl_emit_str(kdl_emitter *self, kdl_str s)
{
    kdl_owned_string escaped = kdl_escape(&s, self->opt.escape_mode);
    bool ok = self->write_func(self->write_user_data, "\"", 1) == 1
        && self->write_func(self->write_user_data, escaped.data, escaped.len) == escaped.len
        && self->write_func(self->write_user_data, "\"", 1) == 1;
    kdl_free_string(&escaped);
    return ok;
}

static bool _kdl_emit_number(kdl_emitter *self, kdl_number const *n)
{
    char buf[32];
    int len = 0;
    switch (n->type) {
    case KDL_NUMBER_TYPE_INTEGER:
        len = snprintf(buf, 32, "%lld", n->value.integer);
        return (int)self->write_func(self->write_user_data, buf, len) == len;
    case KDL_NUMBER_TYPE_FLOATING_POINT:
        len = snprintf(buf, 32, "%.*g", DBL_DECIMAL_DIG, n->value.floating_point);
        return (int)self->write_func(self->write_user_data, buf, len) == len;
    case KDL_NUMBER_TYPE_STRING_ENCODED:
        return self->write_func(self->write_user_data, n->value.string.data, n->value.string.len)
            == n->value.string.len;
    }
    return false;
}

static bool _kdl_emit_identifier(kdl_emitter *self, kdl_str name)
{
    bool bare = true;
    if (self->opt.identifier_mode == KDL_QUOTE_ALL_IDENTIFIERS) {
        bare = false;
    } else {
        uint32_t c;
        kdl_str tail = name;
        bool first = true;
        while (KDL_UTF8_OK == _kdl_pop_codepoint(&tail, &c)) {
            if ((first && !_kdl_is_id_start(c))
                || !_kdl_is_id(c)
                || (self->opt.identifier_mode == KDL_ASCII_IDENTIFIERS && c >= 0x7f)) {
                bare = false;
                break;
            }
            first = false;
        }
    }

    if (bare) {
        return self->write_func(self->write_user_data, name.data, name.len) == name.len;
    } else {
        return _kdl_emit_str(self, name);
    }
}

static bool _kdl_emit_value(kdl_emitter *self, kdl_value const* v)
{
    if (v->type_annotation.data != NULL)
    {
        if ((self->write_func(self->write_user_data, "(", 1) != 1)
            || !_kdl_emit_identifier(self, v->type_annotation)
            || (self->write_func(self->write_user_data, ")", 1) != 1))
            return false;
    }
    switch (v->type) {
    case KDL_TYPE_NULL:
        return self->write_func(self->write_user_data, "null", 4) == 4;
    case KDL_TYPE_STRING:
        return _kdl_emit_str(self, v->value.string);
    case KDL_TYPE_NUMBER:
        return _kdl_emit_number(self, &v->value.number);
    case KDL_TYPE_BOOLEAN:
        if (v->value.boolean) {
            return self->write_func(self->write_user_data, "true", 4) == 4;
        } else {
            return self->write_func(self->write_user_data, "false", 5) == 4;
        }
    }
    return false;
}

static bool _kdl_emit_node_preamble(kdl_emitter *self)
{
    if (!self->start_of_line) {
        if (self->write_func(self->write_user_data, "\n", 1) != 1) return false;
    }
    int indent = 0;
    for (int d = 0; d < self->depth; ++d) {
        indent += self->opt.indent;
    }
    for (int i = 0; i < indent; ++i) {
        if (self->write_func(self->write_user_data, " ", 1) != 1) return false;
    }
    self->start_of_line = false;
    return true;
}

bool kdl_emit_node(kdl_emitter *self, kdl_str name)
{
    return _kdl_emit_node_preamble(self) && _kdl_emit_identifier(self, name);
}

bool kdl_emit_node_with_type(kdl_emitter *self, kdl_str type, kdl_str name)
{
    return _kdl_emit_node_preamble(self)
        && (self->write_func(self->write_user_data, "(", 1) == 1)
        && _kdl_emit_identifier(self, type)
        && (self->write_func(self->write_user_data, ")", 1) == 1)
        && _kdl_emit_identifier(self, name);
}

bool kdl_emit_arg(kdl_emitter *self, kdl_value const *value)
{
    return (self->write_func(self->write_user_data, " ", 1) == 1)
        && _kdl_emit_value(self, value);
}

bool kdl_emit_property(kdl_emitter *self, kdl_str name, kdl_value const *value)
{
    return (self->write_func(self->write_user_data, " ", 1) == 1)
        && _kdl_emit_identifier(self, name)
        && (self->write_func(self->write_user_data, "=", 1) == 1)
        && _kdl_emit_value(self, value);
}

bool kdl_start_emitting_children(kdl_emitter *self)
{
    self->start_of_line = true;
    ++self->depth;
    return (self->write_func(self->write_user_data, " {\n", 3) == 3);
}

bool kdl_finish_emitting_children(kdl_emitter *self)
{
    if (self->depth == 0) return false;
    --self->depth;
    if (!_kdl_emit_node_preamble(self)) return false;
    self->start_of_line = true;
    return (self->write_func(self->write_user_data, "}\n", 2) == 2);
}

bool kdl_emit_end(kdl_emitter *self)
{
    while (self->depth != 0) {
        if (!kdl_finish_emitting_children(self)) return false;
    }
    if (!self->start_of_line) {
        if (self->write_func(self->write_user_data, "\n", 1) != 1) return false;
    }
    return true;
}

kdl_str kdl_get_emitter_buffer(kdl_emitter *self)
{
    return (kdl_str){ self->buf.buf, self->buf.str_len };
}
