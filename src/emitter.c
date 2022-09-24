#include "kdl/emitter.h"

#include "str.h"
#include "grammar.h"
#include "utf8.h"

#include <math.h>
#include <stdio.h>

#define INITIAL_BUFFER_SIZE 4096

const kdl_emitter_options KDL_DEFAULT_EMITTER_OPTIONS = {
    .indent = 4,
    .escape_mode = KDL_ESCAPE_DEFAULT,
    .identifier_mode = KDL_PREFER_BARE_IDENTIFIERS,
    .float_mode = {
        .always_write_decimal_point = false,
        .always_write_decimal_point_or_exponent = true,
        .capital_e = false,
        .exponent_plus = false,
        .plus = false,
        .min_exponent = 4
    }
};

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

kdl_emitter *kdl_create_buffering_emitter(kdl_emitter_options const *opt)
{
    kdl_emitter *self = malloc(sizeof(kdl_emitter));
    if (self == NULL) return NULL;
    self->opt = *opt;
    self->write_func = &_buffer_write_func;
    self->write_user_data = &self->buf;
    self->depth = 0;
    self->start_of_line = true;
    self->buf = _kdl_new_write_buffer(INITIAL_BUFFER_SIZE);
    if (self->buf.buf == NULL) {
        free(self);
        return NULL;
    }
    return self;
}

kdl_emitter *kdl_create_stream_emitter(kdl_write_func write_func, void *user_data, kdl_emitter_options const *opt)
{
    kdl_emitter *self = malloc(sizeof(kdl_emitter));
    if (self == NULL) return NULL;
    self->opt = *opt;
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
    free(self);
}

static bool _emit_str(kdl_emitter *self, kdl_str s)
{
    kdl_owned_string escaped = kdl_escape(&s, self->opt.escape_mode);
    bool ok = self->write_func(self->write_user_data, "\"", 1) == 1
        && self->write_func(self->write_user_data, escaped.data, escaped.len) == escaped.len
        && self->write_func(self->write_user_data, "\"", 1) == 1;
    kdl_free_string(&escaped);
    return ok;
}

static kdl_owned_string _float_to_string(double f, kdl_float_printing_options const *opts)
{
    bool negative = f < 0.0;
    f = fabs(f);
    int exponent = (int)floor(log10(f));
    double exp_factor = 1.0;
    if (abs(exponent) < opts->min_exponent) {
        // don't use scientific notation
        exponent = 0;
    } else {
        // do use scientific notation
        exp_factor = pow(10.0, exponent);
    }

    int integer_part = (int)floor(f / exp_factor);

    // f is now the positive fractional part as displayed
    _kdl_write_buffer buf = _kdl_new_write_buffer(32);
    if (negative) _kdl_buf_push_char(&buf, '-');
    else if (opts->plus) _kdl_buf_push_char(&buf, '+');

    // Write the integer part
    buf.str_len += snprintf(buf.buf + buf.str_len, buf.buf_len - buf.str_len, "%d", integer_part);
    // Write the decimal part if required
    double f_intpart = integer_part * exp_factor;
    bool written_point = false;
    int zeros = 0;
    int nines = 0;
    int queued_digit = -1;
    unsigned long long fractional_part_so_far = 0;
    double pos = 0.1 * exp_factor;

    double f_so_far = f_intpart;

    while (f + pos != f && f_so_far < f) { // while this digit makes a difference
        double remainder = f - f_so_far;

        int next_digit = (int)floor(remainder / pos);
        fractional_part_so_far = 10 * fractional_part_so_far + next_digit;

        while (f_intpart + (fractional_part_so_far + 1) * pos <= f) {
            ++next_digit;
            ++fractional_part_so_far;
        }

        f_so_far = f_intpart + fractional_part_so_far * pos;

        if (next_digit == 0) {
            ++zeros;
        } else if (next_digit == 9) {
            ++nines;
        } else if (next_digit >= 10) {
            fprintf(stderr, "- ckdl WARNING - _float_to_string calculated digit > 9\n");
            int overflow = next_digit - 9;
            next_digit -= overflow;
            fractional_part_so_far -= overflow;
        } else {
            // write the queued digit
            if (queued_digit >= 0 || zeros != 0 || nines != 0) {
                if (!written_point) {
                    _kdl_buf_push_char(&buf, '.');
                    written_point = true;
                }
                if (queued_digit >= 0) _kdl_buf_push_char(&buf, (char)('0' + queued_digit));
                while (zeros != 0) {
                    _kdl_buf_push_char(&buf, '0');
                    --zeros;
                }
                while (nines != 0) {
                    _kdl_buf_push_char(&buf, '9');
                    --nines;
                }
            }
            // queue this one
            queued_digit = next_digit;
        }

        pos /= 10.0;
    }
    // Write the queued digit (if any)
    if (queued_digit != -1) {
        if (!written_point) {
            _kdl_buf_push_char(&buf, '.');
            written_point = true;
        }
        if (nines != 0) {
            ++queued_digit;
        }
        _kdl_buf_push_char(&buf, (char)('0' + queued_digit));
    }
    // Adding more decimal digits now makes no difference to the number.

    // Add ".0" IFF requested
    if (!written_point && opts->always_write_decimal_point) {
        _kdl_buf_push_chars(&buf, ".0", 2);
        written_point = true;
    }

    // Add exponent (if any)
    if (exponent != 0) {
        char exp_buf[32];
        int exp_len = snprintf(exp_buf, 32, "%d", exponent);
        _kdl_buf_push_char(&buf, opts->capital_e ? 'E' : 'e');
        if (exponent >= 0 && opts->exponent_plus) {
            _kdl_buf_push_char(&buf, '+');
        }
        _kdl_buf_push_chars(&buf, exp_buf, exp_len);
    } else if (!written_point && opts->always_write_decimal_point_or_exponent) {
        // Always write either an exponent or a decimal point, to mark this
        // number as float
        _kdl_buf_push_chars(&buf, ".0", 2);
    }
    return _kdl_buf_to_string(&buf);
}

static bool _emit_number(kdl_emitter *self, kdl_number const *n)
{
    char int_buf[32];
    int int_len = 0;
    kdl_owned_string float_str;
    bool ok;

    switch (n->type) {
    case KDL_NUMBER_TYPE_INTEGER:
        int_len = snprintf(int_buf, 32, "%lld", n->integer);
        return (int)self->write_func(self->write_user_data, int_buf, int_len) == int_len;
    case KDL_NUMBER_TYPE_FLOATING_POINT:
        float_str = _float_to_string(n->floating_point, &self->opt.float_mode);
        ok = self->write_func(self->write_user_data, float_str.data, float_str.len) == float_str.len;
        kdl_free_string(&float_str);
        return ok;
    case KDL_NUMBER_TYPE_STRING_ENCODED:
        return self->write_func(self->write_user_data, n->string.data, n->string.len)
            == n->string.len;
    }
    return false;
}

static bool _emit_identifier(kdl_emitter *self, kdl_str name)
{
    bool bare = true;
    if (self->opt.identifier_mode == KDL_QUOTE_ALL_IDENTIFIERS) {
        bare = false;
    } else if (name.len == 0) {
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
        return _emit_str(self, name);
    }
}

static bool _emit_value(kdl_emitter *self, kdl_value const* v)
{
    if (v->type_annotation.data != NULL)
    {
        if ((self->write_func(self->write_user_data, "(", 1) != 1)
            || !_emit_identifier(self, v->type_annotation)
            || (self->write_func(self->write_user_data, ")", 1) != 1))
            return false;
    }
    switch (v->type) {
    case KDL_TYPE_NULL:
        return self->write_func(self->write_user_data, "null", 4) == 4;
    case KDL_TYPE_STRING:
        return _emit_str(self, v->string);
    case KDL_TYPE_NUMBER:
        return _emit_number(self, &v->number);
    case KDL_TYPE_BOOLEAN:
        if (v->boolean) {
            return self->write_func(self->write_user_data, "true", 4) == 4;
        } else {
            return self->write_func(self->write_user_data, "false", 5) == 4;
        }
    }
    return false;
}

static bool _emit_node_preamble(kdl_emitter *self)
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
    return _emit_node_preamble(self) && _emit_identifier(self, name);
}

bool kdl_emit_node_with_type(kdl_emitter *self, kdl_str type, kdl_str name)
{
    return _emit_node_preamble(self)
        && (self->write_func(self->write_user_data, "(", 1) == 1)
        && _emit_identifier(self, type)
        && (self->write_func(self->write_user_data, ")", 1) == 1)
        && _emit_identifier(self, name);
}

bool kdl_emit_arg(kdl_emitter *self, kdl_value const *value)
{
    return (self->write_func(self->write_user_data, " ", 1) == 1)
        && _emit_value(self, value);
}

bool kdl_emit_property(kdl_emitter *self, kdl_str name, kdl_value const *value)
{
    return (self->write_func(self->write_user_data, " ", 1) == 1)
        && _emit_identifier(self, name)
        && (self->write_func(self->write_user_data, "=", 1) == 1)
        && _emit_value(self, value);
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
    if (!_emit_node_preamble(self)) return false;
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
        self->start_of_line = true;
    }
    return true;
}

kdl_str kdl_get_emitter_buffer(kdl_emitter *self)
{
    return (kdl_str){ self->buf.buf, self->buf.str_len };
}
