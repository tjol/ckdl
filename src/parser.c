#include "kdl/parser.h"
#include "kdl/common.h"
#include "kdl/tokenizer.h"

#include "compat.h"
#include "bigint.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

enum _kdl_parser_state {
    // Basic states
    PARSER_OUTSIDE_NODE,
    PARSER_IN_NODE,
    // Flags modifying the state
    PARSER_FLAG_LINE_CONT = 0x100,
    PARSER_FLAG_TYPE_ANNOTATION_START = 0x200,
    PARSER_FLAG_TYPE_ANNOTATION_END = 0x400,
    PARSER_FLAG_TYPE_ANNOTATION_ENDED = 0x800,
    PARSER_FLAG_IN_PROPERTY = 0x1000,
    // Bitmask for testing the state
    PARSER_MASK_WHITESPACE_BANNED = PARSER_FLAG_TYPE_ANNOTATION_START
                                  | PARSER_FLAG_TYPE_ANNOTATION_END
                                  | PARSER_FLAG_TYPE_ANNOTATION_ENDED
                                  | PARSER_FLAG_IN_PROPERTY
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
    if (self != NULL) {
        _init_kdl_parser(self);
        self->tokenizer = kdl_create_string_tokenizer(doc);
        self->opt = opt;
    }
    return self;
}


kdl_parser *kdl_create_stream_parser(kdl_read_func read_func, void *user_data, kdl_parse_option opt)
{
    kdl_parser *self = malloc(sizeof(kdl_parser));
    if (self != NULL) {
        _init_kdl_parser(self);
        self->tokenizer = kdl_create_stream_tokenizer(read_func, user_data);
        self->opt = opt;
    }
    return self;
}


void kdl_destroy_parser(kdl_parser *self)
{
    kdl_destroy_tokenizer(self->tokenizer);
    kdl_free_string(&self->tmp_string_type);
    kdl_free_string(&self->tmp_string_key);
    kdl_free_string(&self->tmp_string_value);
    free(self);
}

static void _reset_event(kdl_parser *self)
{
    self->event.name = (kdl_str){ NULL, 0 };
    self->event.value.type = KDL_TYPE_NULL;
    self->event.value.type_annotation = (kdl_str){ NULL, 0 };
}

static void _set_parse_error(kdl_parser *self, char const *message)
{
    self->event.event = KDL_EVENT_PARSE_ERROR;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.string = (kdl_str){ message, strlen(message) };
}

static void _set_comment_event(kdl_parser *self, kdl_token const *token)
{
    self->event.event = KDL_EVENT_COMMENT;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.string = token->value;
}

static kdl_event_data *_next_node(kdl_parser *self, kdl_token *token);
static kdl_event_data *_next_event_in_node(kdl_parser *self, kdl_token *token);
static kdl_event_data *_apply_slashdash(kdl_parser *self);
static bool _parse_value(kdl_token const *token, kdl_value *val, kdl_owned_string *s);
static bool _parse_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_decimal_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_decimal_integer(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_decimal_float(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_hex_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_octal_number(kdl_str number, kdl_value *val, kdl_owned_string *s);
static bool _parse_binary_number(kdl_str number, kdl_value *val, kdl_owned_string *s);

kdl_event_data *kdl_parser_next_event(kdl_parser *self)
{
    kdl_token token;
    kdl_event_data *ev;

    _reset_event(self);

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
                    _reset_event(self);
                    self->event.event = KDL_EVENT_END_NODE;
                    ev = _apply_slashdash(self);
                    if (ev) return ev;
                    else continue;
                } else if (self->depth > 0) {
                    _set_parse_error(self, "Unexpected end of data (unclosed lists of children)");
                    return &self->event;
                } else if (self->slashdash_depth > 0) {
                    _set_parse_error(self, "Dangling slashdash (/-)");
                    return &self->event;
                } else if (self->state != PARSER_OUTSIDE_NODE) {
                    // Some flags are active, this is not allowed
                    _set_parse_error(self, "Unexpected end of data");
                    return &self->event;
                } else {
                    // state == PARSER_OUTYSIDE_NODE, at top level => EOF ok
                    _reset_event(self);
                    self->event.event = KDL_EVENT_EOF;
                    return &self->event;
                }
            case KDL_TOKENIZER_OK:
                break;
            default:
            case KDL_TOKENIZER_ERROR:
                _set_parse_error(self, "Parse error");
                return &self->event;
            }
        }

        switch (token.type) {
        case KDL_TOKEN_WHITESPACE:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED) {
                _set_parse_error(self, "Whitespace not allowed here");
                return &self->event;
            } else {
                break; // ignore whitespace
            }
        case KDL_TOKEN_MULTI_LINE_COMMENT:
        case KDL_TOKEN_SINGLE_LINE_COMMENT:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED) {
                _set_parse_error(self, "Comment not allowed here");
                return &self->event;
            }
            // Comments may or may not be emitted
            if (self->opt & KDL_EMIT_COMMENTS) {
                _set_comment_event(self, &token);
                return &self->event;
            }
            break;
        case KDL_TOKEN_SLASHDASH:
            // slashdash comments out the next node or argument or property
            if (self->slashdash_depth < 0) {
                self->slashdash_depth = self->depth + 1;
            }
            break;
        default:
            switch (self->state & 0xff) {
            case PARSER_OUTSIDE_NODE:
                ev = _next_node(self, &token);
                if (ev) return ev;
                else continue;
            case PARSER_IN_NODE:
                ev = _next_event_in_node(self, &token);
                if (ev) return ev;
                else continue;
            default:
                _set_parse_error(self, "Inconsistent state");
                return &self->event;
            }
        }
    }
}

static kdl_event_data *_apply_slashdash(kdl_parser *self)
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
            _reset_event(self);
            return NULL;
        }
    } else {
        return &self->event;
    }
}


static kdl_event_data *_next_node(kdl_parser *self, kdl_token *token)
{
    kdl_value tmp_val;
    kdl_event_data *ev;

    if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_parse_value(token, &tmp_val, &self->tmp_string_type)) {
                _set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START)
                    | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->event.value.type_annotation = tmp_val.string;
                return NULL;
            } else {
                _set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        default:
            _set_parse_error(self, "Unexpected token, expected type");
            return &self->event;
        }
    } else if (self->state & PARSER_FLAG_TYPE_ANNOTATION_END) {
        switch (token->type) {
        case KDL_TOKEN_END_TYPE:
            self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_END) | PARSER_FLAG_TYPE_ANNOTATION_ENDED;
            return NULL;
        default:
            _set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        switch (token->type) {
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED) {
                _set_parse_error(self, "Unexpected end of node (incomplete node name?)");
                return &self->event;
            } else {
                // When there is no open type annotation, additional newlines are allowed
                return NULL;
            }
        case KDL_TOKEN_START_TYPE:
            // only one type allowed!
            if (self->event.value.type_annotation.data != NULL) {
                _set_parse_error(self, "Unexpected second type annotation");
                return &self->event;
            } else {
                self->state |= PARSER_FLAG_TYPE_ANNOTATION_START;
                return NULL;
            }
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_parse_value(token, &tmp_val, &self->tmp_string_key)) {
                _set_parse_error(self, "Error parsing node name");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = PARSER_IN_NODE;
                self->event.event = KDL_EVENT_START_NODE;
                self->event.name = tmp_val.string;
                ++self->depth;
                ev = _apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            } else {
                _set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        case KDL_TOKEN_END_CHILDREN:
            // end the parent node
            if (self->depth == 0) {
                _set_parse_error(self, "Unexpected '}'");
                return &self->event;
            } else {
                --self->depth;
                _reset_event(self);
                self->event.event = KDL_EVENT_END_NODE;
                ev = _apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            }
        default:
            _set_parse_error(self, "Unexpected token, expected node");
            return &self->event;
        }
    }
}


static kdl_event_data *_next_event_in_node(kdl_parser *self, kdl_token *token)
{
    kdl_event_data *ev;
    bool is_property = false;

    if (self->state & PARSER_FLAG_LINE_CONT) {
        switch (token->type) {
        case KDL_TOKEN_SINGLE_LINE_COMMENT:
            break; // fine
        case KDL_TOKEN_NEWLINE:
            self->state &= ~PARSER_FLAG_LINE_CONT;
            return NULL; // Get next token - \ and newline cancel out
        default:
            _set_parse_error(self, "Illegal token after line continuation");
            return &self->event;
        }
    }

    if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        kdl_value tmp_val;
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
            if (!_parse_value(token, &tmp_val, &self->tmp_string_type)) {
                _set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START)
                    | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->event.value.type_annotation = tmp_val.string;
                return NULL;
            } else {
                _set_parse_error(self, "Expected identifier or string");
                return &self->event;
            }
        default:
            _set_parse_error(self, "Unexpected token, expected type");
            return &self->event;
        }
    } else if (self->state & PARSER_FLAG_TYPE_ANNOTATION_END) {
        switch (token->type) {
        case KDL_TOKEN_END_TYPE:
            self->state = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_END) | PARSER_FLAG_TYPE_ANNOTATION_ENDED;
            return NULL;
        default:
            _set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        switch (token->type) {
        case KDL_TOKEN_LINE_CONTINUATION:
            self->state |= PARSER_FLAG_LINE_CONT;
            return NULL;
        case KDL_TOKEN_END_CHILDREN:
            // end this node, and process the token again
            self->next_token = *token;
            self->have_next_token = true;
            _fallthrough_;
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED) {
                _set_parse_error(self, "Unexpected end of node (incomplete argument or property?)");
                return &self->event;
            } else {
                // end the node
                self->state = PARSER_OUTSIDE_NODE;
                --self->depth;
                _reset_event(self);
                self->event.event = KDL_EVENT_END_NODE;
                ev = _apply_slashdash(self);
                if (ev) return ev;
                else return NULL;
            }
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_RAW_STRING:
        {
            kdl_owned_string tmp_str = { NULL, 0 };
            // either a property key, or a property value, or an argument
            if (self->event.name.data == NULL && self->event.value.type_annotation.data == NULL) {
                // the call to kdl_pop_token will invalidate the old token's value - clone it
                tmp_str = kdl_clone_str(&token->value);
                token->value = kdl_borrow_str(&tmp_str);
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
                    _set_parse_error(self, "Parse error");
                    kdl_free_string(&tmp_str);
                    return &self->event;
                }
            }

            if (is_property) {
                // Parse the property name
                self->state |= PARSER_FLAG_IN_PROPERTY;
                kdl_value tmp_val;
                bool parse_ok = _parse_value(token, &tmp_val, &self->tmp_string_key);
                kdl_free_string(&tmp_str);
                if (!parse_ok) {
                    _set_parse_error(self, "Error parsing property key");
                    return &self->event;
                }
                if (tmp_val.type == KDL_TYPE_STRING) {
                    // all good
                    self->event.name = tmp_val.string;
                    return NULL;
                } else {
                    _set_parse_error(self, "Property keys must be strings or identifiers");
                    return &self->event;
                }
            } else {
                // Parse the argument
                bool parse_ok = _parse_value(token, &self->event.value, &self->tmp_string_value);
                kdl_free_string(&tmp_str);
                if (!parse_ok) {
                    _set_parse_error(self, "Error parsing argument");
                    return &self->event;
                }
                // check that it's not a bare identifier
                if (token->type == KDL_TOKEN_WORD && self->event.value.type == KDL_TYPE_STRING) {
                    _set_parse_error(self, "Bare identifier not allowed here");
                    return &self->event;
                }
                // return it
                if (self->event.name.data != NULL) {
                    self->event.event = KDL_EVENT_PROPERTY;
                } else {
                    self->event.event = KDL_EVENT_ARGUMENT;
                }
                ev = _apply_slashdash(self);
                // be sure to reset the parser state
                self->state = PARSER_IN_NODE;
                if (ev) return ev;
                else return NULL;
            }
        }
        case KDL_TOKEN_START_TYPE:
            // only one type allowed!
            if (self->event.value.type_annotation.data != NULL) {
                _set_parse_error(self, "Unexpected second type annotation");
                return &self->event;
            } else {
                self->state |= PARSER_FLAG_TYPE_ANNOTATION_START;
                return NULL;
            }
        case KDL_TOKEN_START_CHILDREN:
            // enter the node / allow new children
            self->state = PARSER_OUTSIDE_NODE;
            // if we just opened a slashdash, reduced its depth so that it ends
            // at the end of the parent node (we're commenting out the children block)
            if (self->slashdash_depth == self->depth + 1) {
                --self->slashdash_depth; // as if the /- had appeared before the node
                // the node has already been forwarded, so that's fine
            }
            _reset_event(self);
            return NULL;
        default:
            _set_parse_error(self, "Unexpected token");
            return &self->event;
        }
    }
}

static bool _parse_value(kdl_token const *token, kdl_value *val, kdl_owned_string *s)
{
    kdl_free_string(s);

    switch (token->type) {
    case KDL_TOKEN_RAW_STRING:
        // no parsing necessary
        *s = kdl_clone_str(&token->value);
        val->type = KDL_TYPE_STRING;
        val->string = kdl_borrow_str(s);
        return true;
    case KDL_TOKEN_STRING:
        // parse escapes
        *s = kdl_unescape(&token->value);
        if (s->data == NULL) {
            return false;
        } else {
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        }
    case KDL_TOKEN_WORD:
        if (token->value.len == 4) {
            if (memcmp("null", token->value.data, 4) == 0) {
                val->type = KDL_TYPE_NULL;
                return true;
            } else if (memcmp("true", token->value.data, 4) == 0) {
                val->type = KDL_TYPE_BOOLEAN;
                val->boolean = true;
                return true;
            }
        } else if (token->value.len == 5 && memcmp("false", token->value.data, 4) == 0) {
            val->type = KDL_TYPE_BOOLEAN;
            val->boolean = false;
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
                return _parse_number(token->value, val, s);
            }
        }
        // this is a regular identifier
        *s = kdl_clone_str(&token->value);
        val->type = KDL_TYPE_STRING;
        val->string = kdl_borrow_str(s);
        return true;
    default:
        return false;
    }
}

static bool _parse_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
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
            return _parse_hex_number(orig_number, val, s);
        case 'o':
            return _parse_octal_number(orig_number, val, s);
        case 'b':
            return _parse_binary_number(orig_number, val, s);
        default:
            break;
        }
    }
    return _parse_decimal_number(orig_number, val, s);
}

static bool _parse_decimal_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    // Check this is an integer or a decimal
    for (size_t i = 0; i < number.len; ++i) {
        switch (number.data[i]) {
        case '.':
        case 'e':
        case 'E':
            return _parse_decimal_float(number, val, s);
        }
    }
    return _parse_decimal_integer(number, val, s);
}

static bool _parse_decimal_integer(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    _kdl_ubigint *n = _kdl_ubigint_new(0);
    if (n == NULL) return false;

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

    if (number.len - i > 0 && number.data[i] == '_') goto error;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];
        if (c == '_') continue;
        else if (c < '0' || c > '9') goto error;

        int digit = c - '0';
        n = _kdl_ubigint_multiply_inplace(n, 10);
        n = _kdl_ubigint_add_inplace(n, digit);
    }

    // try to reduce down to long long
    long long n_ll;
    if (_kdl_ubigint_as_long_long(n, &n_ll)) {
        // number is representable as long long
        if (negative) n_ll = -n_ll;
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_INTEGER;
        val->number.integer = n_ll;
    } else {
        // represent number as string
        *s = _kdl_ubigint_as_string_sgn(negative ? -1 : +1, n);
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->number.string = kdl_borrow_str(s);
    }
    _kdl_ubigint_free(n);
    return true;
error:
    _kdl_ubigint_free(n);
    return false;
}

static bool _parse_decimal_float(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    int digits_before_decimal = 0;
    int digits_after_decimal = 0;
    unsigned long long decimal_mantissa = 0;
    int explicit_exponent = 0;
    bool exponent_negative = false;
    enum {
        before_decimal,
        after_decimal_nodigit,
        after_decimal,
        exponent_nodigit,
        exponent
    } state = before_decimal;

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
            state = after_decimal_nodigit;
            if (number.len - i <= 1 || number.data[i+1] == '_') return false;
        } else if ((c == 'e' || c == 'E') && state != exponent && state != after_decimal_nodigit) {
            state = exponent_nodigit;
            if (i + 1 < number.len) {
                // handle exponent sign
                switch (number.data[i+1]) {
                case '-':
                    exponent_negative = true;
                    _fallthrough_;
                case '+':
                    ++i;
                }
                if (number.len - i <= 1 || number.data[i+1] == '_') return false;
            }
        } else if (c >= '0' && c <= '9') {
            // digit!
            int digit = c - '0';
            if (state == exponent || state == exponent_nodigit) {
                state = exponent;
                explicit_exponent = explicit_exponent * 10 + digit;
            } else {
                decimal_mantissa = decimal_mantissa * 10 + digit;
                if (state == before_decimal) {
                    ++digits_before_decimal;
                } else {
                    state = after_decimal;
                    ++digits_after_decimal;
                }
            }
        } else if (c == '_') {
            // underscores are allowed
        } else {
            // invalid
            return false;
        }
    }

    if (state == after_decimal_nodigit || state == exponent_nodigit) {
        // invalid
        return false;
    }

    // rough heuristic for numbers that fit into a double exactly
    if (digits_before_decimal + digits_after_decimal <= 15 && explicit_exponent < 285 && explicit_exponent > -285) {
        // "strip zeros"
        while (decimal_mantissa % 10 == 0 && digits_after_decimal > 0) {
            decimal_mantissa /= 10;
            --digits_after_decimal;
        }
        double n = (double)decimal_mantissa;
        if (negative) n = -n;
        if (exponent_negative) explicit_exponent = -explicit_exponent;
        int net_exponent = explicit_exponent - digits_after_decimal;
        if (net_exponent < 0) {
            n /= pow(10.0, -net_exponent);
        }
        if (net_exponent > 0) {
            n *= pow(10.0, net_exponent);
        }

        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_FLOATING_POINT;
        val->number.floating_point = n;
        return true;
    } else {
        // Remove all underscores and the initial plus
        *s = kdl_clone_str(&number);
        char const *p1 = number.data;
        char const *end = number.data + number.len;
        char *p2 = s->data;
        s->len = 0;
        if (p1 != end && *p1 == '+') ++p1;
        while (p1 != end) {
            int c = *(p1++);
            if (c != '_') {
                // copy character
                *(p2++) = (char)c;
                ++s->len;
            }
        }
        *p2 = 0;

        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->number.string = kdl_borrow_str(s);
        return true;
    }
}


static bool _parse_hex_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    _kdl_ubigint *n = _kdl_ubigint_new(0);
    if (n == NULL) return false;

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
        goto error;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];
        int digit;

        if (c == '_') continue;
        else if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 0xa;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 0xa;
        else goto error;

        n = _kdl_ubigint_multiply_inplace(n, 16);
        n = _kdl_ubigint_add_inplace(n, digit);
    }

    // try to reduce down to long long
    long long n_ll;
    if (_kdl_ubigint_as_long_long(n, &n_ll)) {
        // number is representable as long long
        if (negative) n_ll = -n_ll;
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_INTEGER;
        val->number.integer = n_ll;
    } else {
        // represent number as string
        *s = _kdl_ubigint_as_string_sgn(negative ? -1 : +1, n);
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->number.string = kdl_borrow_str(s);
    }
    _kdl_ubigint_free(n);
    return true;
error:
    _kdl_ubigint_free(n);
    return false;
}


static bool _parse_octal_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    _kdl_ubigint *n = _kdl_ubigint_new(0);
    if (n == NULL) return false;

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
        goto error;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];

        if (c == '_') continue;
        else if (c < '0' || c > '7') goto error;

        int digit = c - '0';
        n = _kdl_ubigint_multiply_inplace(n, 8);
        n = _kdl_ubigint_add_inplace(n, digit);
    }

    // try to reduce down to long long
    long long n_ll;
    if (_kdl_ubigint_as_long_long(n, &n_ll)) {
        // number is representable as long long
        if (negative) n_ll = -n_ll;
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_INTEGER;
        val->number.integer = n_ll;
    } else {
        // represent number as string
        *s = _kdl_ubigint_as_string_sgn(negative ? -1 : +1, n);
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->number.string = kdl_borrow_str(s);
    }
    _kdl_ubigint_free(n);
    return true;
error:
    _kdl_ubigint_free(n);
    return false;
}


static bool _parse_binary_number(kdl_str number, kdl_value *val, kdl_owned_string *s)
{
    bool negative = false;
    _kdl_ubigint *n = _kdl_ubigint_new(0);
    if (n == NULL) return false;

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
        goto error;
    i += 2;

    // scan through entire number
    for (; i < number.len; ++i) {
        char c = number.data[i];

        if (c == '_') continue;
        else if (c < '0' || c > '1') goto error;

        int digit = c - '0';
        n = _kdl_ubigint_multiply_inplace(n, 2);
        n = _kdl_ubigint_add_inplace(n, digit);
    }

    // try to reduce down to long long
    long long n_ll;
    if (_kdl_ubigint_as_long_long(n, &n_ll)) {
        // number is representable as long long
        if (negative) n_ll = -n_ll;
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_INTEGER;
        val->number.integer = n_ll;
    } else {
        // represent number as string
        *s = _kdl_ubigint_as_string_sgn(negative ? -1 : +1, n);
        val->type = KDL_TYPE_NUMBER;
        val->number.type = KDL_NUMBER_TYPE_STRING_ENCODED;
        val->number.string = kdl_borrow_str(s);
    }
    _kdl_ubigint_free(n);
    return true;
error:
    _kdl_ubigint_free(n);
    return false;
}


