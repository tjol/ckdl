#include "kdl/parser.h"
#include "kdl/common.h"
#include "kdl/tokenizer.h"

#include "compiler_compat.h"

#include <stdlib.h>
#include <string.h>

enum _kdl_parser_state {
    PARSER_OUTSIDE_NODE,
    PARSER_IN_NODE,
    PARSER_FLAG_LINE_CONT = 0x100,
    PARSER_FLAG_TYPE_ANNOTATION_START = 0x200,
    PARSER_FLAG_TYPE_ANNOTATION_END = 0x400
};

struct _kdl_parser {
    kdl_tokenizer *tokenizer;
    kdl_parse_option opt;
    int depth;
    int slashdash_depth;
    enum _kdl_parser_state state;
    kdl_event_data event;
    kdl_owned_string tmp_string_type;
    kdl_owned_string tmp_string_key;
    kdl_owned_string tmp_string_value;
    kdl_token next_token;
    bool have_next_token;
};


static void _init_kdl_parser(kdl_parser *self)
{
    self->depth = 0;
    self->slashdash_depth = -1;
    self->state = PARSER_OUTSIDE_NODE;
    self->tmp_string_type = (kdl_owned_string){ NULL, 0 };
    self->tmp_string_key = (kdl_owned_string){ NULL, 0 };
    self->tmp_string_value = (kdl_owned_string){ NULL, 0 };
    self->have_next_token = false;
}


kdl_parser *kdl_create_string_parser(kdl_str doc, kdl_parse_option opt)
{
    kdl_parser *self = malloc(sizeof(kdl_parser));
    _init_kdl_parser(self);
    self->tokenizer = kdl_create_string_tokenizer(doc);
    self->opt = opt;
    return self;
}


kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt)
{
    kdl_parser *self = malloc(sizeof(kdl_parser));
    _init_kdl_parser(self);
    self->tokenizer = kdl_create_stream_tokenizer(read_func, user_data);
    self->opt = opt;
    return self;
}


void kdl_destroy_parser(kdl_parser *self)
{
    kdl_destroy_tokenizer(self->tokenizer);
    kdl_free_string(&self->tmp_string_type);
    kdl_free_string(&self->tmp_string_key);
    kdl_free_string(&self->tmp_string_value);
}

static void reset_event(kdl_parser *self)
{
    self->event.value.type = KDL_TYPE_NULL;
    self->event.type_annotation = (kdl_str){ NULL, 0 };
    self->event.property_key = (kdl_str){ NULL, 0 };
}

static void set_parse_error(kdl_parser *self, char const *message)
{
    self->event.event = KDL_EVENT_PARSE_ERROR;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.value.string = (kdl_str){ message, strlen(message) };
}

static void set_comment_event(kdl_parser *self, kdl_token const *token)
{
    self->event.event = KDL_EVENT_COMMENT;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.value.string = token->value;
}

static kdl_event_data *_kdl_parser_next_node(kdl_parser *self, kdl_token *token);
static kdl_event_data *_kdl_parser_next_event_in_node(kdl_parser *self, kdl_token *token);
static kdl_event_data *_kdl_parser_apply_slashdash(kdl_parser *self);
static bool _kdl_parse_value(kdl_token const *token, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_decimal_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_decimal_integer(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_decimal_float(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_hex_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_octal_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _kdl_parse_binary_number(kdl_str number, kdl_value *val, kdl_owned_string *s);

kdl_event_data *kdl_parser_next_event(kdl_parser *self)
{
    kdl_token token;
    kdl_event_data *ev;

    reset_event(self);

    while (true) {

        // get the next token (if available)
        if (self->have_next_token) {
            token = self->next_token;
            self->have_next_token = false;
        } else {
            switch (kdl_pop_token(self->tokenizer, &token)) {
            case KDL_TOKENIZER_EOF:
                if (self->state == PARSER_IN_NODE) {
                    // EOF may be ok, but we have to close the node first
                    --self->depth;
                    self->state = PARSER_OUTSIDE_NODE;
                    reset_event(self);
                    self->event.event = KDL_EVENT_END_NODE;
                    return &self->event;
                } else if (self->depth > 0) {
                    set_parse_error(self, "Unexpected end of data");
                    return &self->event;
                } else if (self->slashdash_depth > 0) {
                    set_parse_error(self, "Dangling slashdash (/-)");
                    return &self->event;
                } else {
                    // EOF ok
                    reset_event(self);
                    self->event.event = KDL_EVENT_EOF;
                    return &self->event;
                }
            case KDL_TOKENIZER_OK:
                break;
            default:
            case KDL_TOKENIZER_ERROR:
                set_parse_error(self, "Parse error");
                return &self->event;
            }
        }

        if (self->state & PARSER_FLAG_LINE_CONT) {
            switch (token.type) {
            case KDL_TOKEN_SINGLE_LINE_COMMENT:
                break; // fine
            case KDL_TOKEN_NEWLINE:
                self->state &= ~PARSER_FLAG_LINE_CONT;
                continue; // Get next token - \ and newline cancel out
            default:
                set_parse_error(self, "Illegal token after line continuation");
                return &self->event;
            }
        }

        switch (token.type) {
        case KDL_TOKEN_MULTI_LINE_COMMENT:
        case KDL_TOKEN_SINGLE_LINE_COMMENT:
            // Comments may or may not be emitted
            if (self->opt & KDL_EMIT_COMMENTS) {
                set_comment_event(self, &token);
                return &self->event;
            }
            break;
        case KDL_TOKEN_SLASHDASH:
            // slashdash comments out the next node or argument or property
            if (self->slashdash_depth < 0) {
                self->slashdash_depth = self->depth + 1;
            }
            break;
        case KDL_TOKEN_LINE_CONTINUATION:
            self->state |= PARSER_FLAG_LINE_CONT;
            break;
        default:
            switch (self->state & 0xff) {
            case PARSER_OUTSIDE_NODE:
                ev = _kdl_parser_next_node(self, &token);
                if (ev) return ev;
                else break;
            case PARSER_IN_NODE:
                ev = _kdl_parser_next_event_in_node(self, &token);
                if (ev) return ev;
                else break;
            default:
                set_parse_error(self, "Inconsistent state");
                return &self->event;
            }
        }

    }
}

static kdl_event_data *_kdl_parser_apply_slashdash(kdl_parser *self)
{
    if (self->slashdash_depth >= 0) {
        // slashdash is active
        if (self->slashdash_depth == self->depth + 1) {
            // slashdash is ending here
            self->slashdash_depth = -1;
        }
        if (self->opt & KDL_EMIT_COMMENTS) {
            self->event.event |= KDL_EVENT_COMMENT;
            return &self->event;
        } else {
            // eat this event and all its attributes
            reset_event(self);
            return NULL;
        }
    } else {
        return &self->event;
    }
}


static kdl_event_data *_kdl_parser_next_node(kdl_parser *self, kdl_token *token)
{
    kdl_value tmp_val;
    kdl_event_data *ev;

    if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_kdl_parse_value(token, &tmp_val, &self->tmp_string_type)) {
                set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START)
                    | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->event.type_annotation = tmp_val.value.string;
                return NULL;
            } else {
                set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        default:
            set_parse_error(self, "Unexpected token, expected type");
            return &self->event;
        }
    } else if (self->state & PARSER_FLAG_TYPE_ANNOTATION_END) {
        switch (token->type) {
        case KDL_TOKEN_END_TYPE:
            self->state &= ~PARSER_FLAG_TYPE_ANNOTATION_END;
            return NULL;
        default:
            set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        switch (token->type) {
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            return NULL;
        case KDL_TOKEN_START_TYPE:
            // only one type allowed!
            if (self->event.type_annotation.data != NULL) {
                set_parse_error(self, "Unexpected second type annotation");
                return &self->event;
            } else {
                self->state |= PARSER_FLAG_TYPE_ANNOTATION_START;
                return NULL;
            }
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_kdl_parse_value(token, &self->event.value, &self->tmp_string_value)) {
                set_parse_error(self, "Error parsing node name");
                return &self->event;
            }
            if (self->event.value.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = PARSER_IN_NODE;
                self->event.event = KDL_EVENT_START_NODE;
                ++self->depth;
                ev = _kdl_parser_apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            } else {
                set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        case KDL_TOKEN_END_CHILDREN:
            // end the parent node
            if (self->depth == 0) {
                set_parse_error(self, "Unexpected '}'");
                return &self->event;
            } else {
                --self->depth;
                reset_event(self);
                self->event.event = KDL_EVENT_END_NODE;
                ev = _kdl_parser_apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            }
        default:
            set_parse_error(self, "Unexpected token, expected node");
            return &self->event;
        }
    }
}


static kdl_event_data *_kdl_parser_next_event_in_node(kdl_parser *self, kdl_token *token)
{
    kdl_event_data *ev;
    bool is_property = false;

    if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        kdl_value tmp_val;
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_kdl_parse_value(token, &tmp_val, &self->tmp_string_type)) {
                set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START)
                    | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->event.type_annotation = tmp_val.value.string;
                return NULL;
            } else {
                set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        default:
            set_parse_error(self, "Unexpected token, expected type");
            return &self->event;
        }
    } else if (self->state & PARSER_FLAG_TYPE_ANNOTATION_END) {
        switch (token->type) {
        case KDL_TOKEN_END_TYPE:
            self->state &= ~PARSER_FLAG_TYPE_ANNOTATION_END;
            return NULL;
        default:
            set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        switch (token->type) {
        case KDL_TOKEN_END_CHILDREN:
            // end this node, and process the token again
            self->next_token = *token;
            self->have_next_token = true;
            _fallthrough_;
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            // end the node
            self->state = PARSER_OUTSIDE_NODE;
            --self->depth;
            reset_event(self);
            self->event.event = KDL_EVENT_END_NODE;
            ev = _kdl_parser_apply_slashdash(self);
            if (ev) return ev;
            else return NULL;
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            // either a property key, or a property value, or an argument
            if (self->event.property_key.data == NULL && self->event.type_annotation.data == NULL) {
                // property key only possible if we don't already have one, and if we don't
                // yet have a type annotation (values are annotated, keys are not)
                // -> Check the next token
                switch (kdl_pop_token(self->tokenizer, &self->next_token)) {
                case KDL_TOKENIZER_EOF:
                    break; // all good
                case KDL_TOKENIZER_OK:
                    if (self->next_token.type == KDL_TOKEN_EQUALS)
                        is_property = true;
                    else
                        // process this token next time
                        self->have_next_token = true;
                    break;
                default:
                case KDL_TOKENIZER_ERROR:
                    // parse error
                    set_parse_error(self, "Parse error");
                    return &self->event;
                }
            }

            if (is_property) {
                // Parse the property name
                kdl_value tmp_val;
                if (!_kdl_parse_value(token, &tmp_val, &self->tmp_string_key)) {
                    set_parse_error(self, "Error parsing property key");
                    return &self->event;
                }
                if (tmp_val.type == KDL_TYPE_STRING) {
                    // all good
                    self->event.property_key = tmp_val.value.string;
                    return NULL;
                } else {
                    set_parse_error(self, "Property keys must be strings or identifiers");
                    return &self->event;
                }
            } else {
                // Parse the argument
                if (!_kdl_parse_value(token, &self->event.value, &self->tmp_string_value)) {
                    set_parse_error(self, "Error parsing argument");
                    return &self->event;
                }
                // check that it's not a bare identifier
                if (token->type == KDL_TOKEN_WORD && self->event.value.type == KDL_TYPE_STRING) {
                    set_parse_error(self, "Bare identifier not allowed here");
                    return &self->event;
                }
                // return it
                if (self->event.property_key.data != NULL) {
                    self->event.event = KDL_EVENT_PROPERTY;
                } else {
                    self->event.event = KDL_EVENT_ARGUMENT;
                }
                ev = _kdl_parser_apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            }
        case KDL_TOKEN_START_TYPE:
            // only one type allowed!
            if (self->event.type_annotation.data != NULL) {
                set_parse_error(self, "Unexpected second type annotation");
                return &self->event;
            } else {
                self->state |= PARSER_FLAG_TYPE_ANNOTATION_START;
                return NULL;
            }
        case KDL_TOKEN_START_CHILDREN:
            // enter the node / allow new children
            self->state = PARSER_OUTSIDE_NODE;
            reset_event(self);
            return NULL;
        default:
            set_parse_error(self, "Unexpected token");
            return &self->event;
        }
    }
}

static bool _kdl_parse_value(kdl_token const *token, kdl_value *val, kdl_owned_string *s)
{
    kdl_free_string(s);

    switch (token->type) {
    case KDL_TOKEN_RAW_STRING:
        // no parsing necessary
        *s = kdl_clone_str(&token->value);
        val->type = KDL_TYPE_STRING;
        val->value.string = kdl_borrow_str(s);
        return true;
    case KDL_TOKEN_STRING:
        // parse escapes
        *s = kdl_unescape(&token->value);
        if (s->data == NULL) {
            return false;
        } else {
            val->type = KDL_TYPE_STRING;
            val->value.string = kdl_borrow_str(s);
            return true;
        }
    case KDL_TOKEN_WORD:
        if (token->value.len == 4) {
            if (memcmp("null", token->value.data, 4) == 0) {
                val->type = KDL_TYPE_NULL;
                return true;
            } else if (memcmp("true", token->value.data, 4) == 0) {
                val->type = KDL_TYPE_BOOLEAN;
                val->value.boolean = true;
                return true;
            }
        } else if (token->value.len == 5 && memcmp("false", token->value.data, 4) == 0) {
            val->type = KDL_TYPE_BOOLEAN;
            val->value.boolean = false;
            return true;
        }
        // either a number or an identifier
        if (token->value.len >= 1) {
            char first_char = token->value.data[0];
            // skip sign if present
            if ((first_char == '+' || first_char == '-') && token->value.len >= 2)
                first_char = token->value.data[1];
            if (first_char >= '0' && first_char <= '9') {
                // first character after sign is a digit, this value should be interpreted
                // as a number
                return _kdl_parse_number(token->value, val, s);
            }
        }
        // this is a regular identifier
        *s = kdl_clone_str(&token->value);
        val->type = KDL_TYPE_STRING;
        val->value.string = kdl_borrow_str(s);
        return true;
    default:
        return false;
    }
}

static bool _kdl_parse_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    kdl_str orig_number = number;
    if (number.len >= 1) {
        switch (number.data[0]) {
        case '-':
        case '+':
            ++number.data;
            --number.len;
        default:
            break;
        }
    }

    if (number.len > 2 && number.data[0] == '0') {
        switch (number.data[1]) {
        case 'x':
            return _kdl_parse_hex_number(orig_number, val, s);
        case 'o':
            return _kdl_parse_octal_number(orig_number, val, s);
        case 'b':
            return _kdl_parse_binary_number(orig_number, val, s);
        default:
            break;
        }
    }
    return _kdl_parse_decimal_number(orig_number, val, s);
}

static bool _kdl_parse_decimal_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    // Check this is an integer or a decimal
    for (size_t i = 0; i < number.len; ++i) {
        switch (number.data[i]) {
        case '.':
        case 'e':
        case 'E':
            return _kdl_parse_decimal_float(number, val, s);
        }
    }
    return _kdl_parse_decimal_integer(number, val, s);
}

static bool _kdl_parse_decimal_integer(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    bool overflow = false;
    long long n = 0;

    size_t i = 0; // index into number-string

    // handle sign
    if (number.len > 1) {
        switch (number.data[0]) {
        case '-':
            negative = true;
            _fallthrough_;
        case '+':
            i = 1;
        }
    }

    if (number.len - i > 0 && number.data[i] == '_') return false;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];
        if (c == '_') continue;
        else if (c < '0' || c > '9') return false;

        int digit = c - '0';
        n = n * 10 + digit;
        if (n < 0) overflow = true;
    }

    if (overflow) {
        // represent number as string
        *s = kdl_clone_str(&number);
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->value.number.value.string = kdl_borrow_str(s);
        return true;
    } else {
        // number is representable as long long
        if (negative) n = -n;
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_INTEGER;
        val->value.number.value.integer = n;
        return true;
    }
}

static bool _kdl_parse_decimal_float(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    int digits_before_decimal = 0;
    int digits_after_decimal = 0;
    unsigned long long decimal_mantissa = 0;
    int explicit_exponent = 0;
    bool exponent_negative = false;
    enum { before_decimal, after_decimal, exponent } state = before_decimal;

    size_t i = 0; // index into number-string

    // handle sign
    if (number.len > 1) {
        switch (number.data[0]) {
        case '-':
            negative = true;
            _fallthrough_;
        case '+':
            i = 1;
        }
    }

    if (number.len - i > 0 && number.data[i] == '_') return false;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];
        if (c == '.' && state == before_decimal) {
            state = after_decimal;
            if (number.len - i > 1 && number.data[i+1] == '_') return false;
        } else if ((c == 'e' || c == 'E') && state != exponent) {
            state = exponent;
            if (i + 1 < number.len) {
                // handle exponent sign
                switch (number.data[i+1]) {
                case '-':
                    exponent_negative = true;
                    _fallthrough_;
                case '+':
                    ++i;
                }
                if (number.len - i > 1 && number.data[i+1] == '_') return false;
            }
        } else if (c >= '0' && c <= '9') {
            // digit!
            int digit = c - '0';
            if (state == exponent) {
                explicit_exponent = explicit_exponent * 10 + digit;
            } else {
                decimal_mantissa = decimal_mantissa * 10 + digit;
                if (state == before_decimal)
                    ++digits_before_decimal;
                else
                    ++digits_after_decimal;
            }
        } else if (c == '_') {
            // underscores are allowed
        } else {
            // invalid
            return false;
        }
    }

    // rough heuristic for numbers that fit into a double exactly
    if (digits_before_decimal + digits_after_decimal <= 15 && exponent < 285 && exponent > -285) {
        double n = (double)decimal_mantissa;
        if (negative) n = -n;
        if (exponent_negative) explicit_exponent = -explicit_exponent;
        int exponent = explicit_exponent - digits_after_decimal;
        while (exponent < 0) {
            ++exponent;
            n *= 0.1;
        }
        while (exponent > 0) {
            --exponent;
            n *= 10.0;
        }

        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_FLOATING_POINT;
        val->value.number.value.floating_point = n;
        return true;
    } else {
        *s = kdl_clone_str(&number);
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->value.number.value.string = kdl_borrow_str(s);
        return true;
    }
}


static bool _kdl_parse_hex_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    bool overflow = false;
    long long n = 0;

    size_t i = 0; // index into number-string

    // handle sign
    if (number.len > 1) {
        switch (number.data[0]) {
        case '-':
            negative = true;
            _fallthrough_;
        case '+':
            i = 1;
        }
    }

    if (number.len - i < 3 || number.data[i] != '0'
                           || number.data[i+1] != 'x'
                           || number.data[i+2] == '_')
        return false;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];
        int digit;

        if (c == '_') continue;
        else if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 0xa;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 0xa;
        else return false;

        n = (n << 4) + digit;
        if (n < 0) overflow = true;
    }

    if (overflow) {
        // represent number as string
        *s = kdl_clone_str(&number);
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->value.number.value.string = kdl_borrow_str(s);
        return true;
    } else {
        // number is representable as long long
        if (negative) n = -n;
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_INTEGER;
        val->value.number.value.integer = n;
        return true;
    }
}


static bool _kdl_parse_octal_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    bool overflow = false;
    long long n = 0;

    size_t i = 0; // index into number-string

    // handle sign
    if (number.len > 1) {
        switch (number.data[0]) {
        case '-':
            negative = true;
            _fallthrough_;
        case '+':
            i = 1;
        }
    }

    if (number.len - i < 3 || number.data[i] != '0'
                           || number.data[i+1] != 'o'
                           || number.data[i+2] == '_')
        return false;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];

        if (c == '_') continue;
        else if (c < '0' || c > '7') return false;

        int digit = c - '0';
        n = (n << 3) + digit;
        if (n < 0) overflow = true;
    }

    if (overflow) {
        // represent number as string
        *s = kdl_clone_str(&number);
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->value.number.value.string = kdl_borrow_str(s);
        return true;
    } else {
        // number is representable as long long
        if (negative) n = -n;
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_INTEGER;
        val->value.number.value.integer = n;
        return true;
    }
}


static bool _kdl_parse_binary_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    bool overflow = false;
    long long n = 0;

    size_t i = 0; // index into number-string

    // handle sign
    if (number.len > 1) {
        switch (number.data[0]) {
        case '-':
            negative = true;
            _fallthrough_;
        case '+':
            i = 1;
        }
    }

    if (number.len - i < 3 || number.data[i] != '0'
                           || number.data[i+1] != 'b'
                           || number.data[i+2] == '_')
        return false;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];

        if (c == '_') continue;
        else if (c < '0' || c > '1') return false;

        int digit = c - '0';
        n = (n << 1) + digit;
        if (n < 0) overflow = true;
    }

    if (overflow) {
        // represent number as string
        *s = kdl_clone_str(&number);
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->value.number.value.string = kdl_borrow_str(s);
        return true;
    } else {
        // number is representable as long long
        if (negative) n = -n;
        val->type = KDL_TYPE_NUMBER;
        val->value.number.type = KDL_NUMBER_TYPE_INTEGER;
        val->value.number.value.integer = n;
        return true;
    }
}


