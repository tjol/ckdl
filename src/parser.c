#include "kdl/parser.h"
#include "kdl/common.h"
#include "kdl/tokenizer.h"

#include "bigint.h"
#include "compat.h"
#include "grammar.h"
#include "str.h"
#include "utf8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _str_equals_literal(k, l)                                                                            \
    ((k).len == (sizeof(l "") - 1) && 0 == memcmp(("" l), (k).data, (sizeof(l) - 1)))

#define KDL_DETECT_VERSION_BIT (kdl_parse_option)0x10000
#define KDL_PARSE_OPT_VERSION_BITS KDL_DETECT_VERSION

#define _v1_only(self) ((self->opt & KDL_PARSE_OPT_VERSION_BITS) == KDL_READ_VERSION_1)
#define _v1_allowed(self) ((self->opt & KDL_READ_VERSION_1) == KDL_READ_VERSION_1)
#define _v2_only(self) ((self->opt & KDL_PARSE_OPT_VERSION_BITS) == KDL_READ_VERSION_2)
#define _v2_allowed(self) ((self->opt & KDL_READ_VERSION_2) == KDL_READ_VERSION_2)

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
    PARSER_FLAG_MAYBE_IN_PROPERTY = 0x2000,
    PARSER_FLAG_BARE_PROPERTY_NAME = 0x4000,
    PARSER_FLAG_NEWLINES_ARE_WHITESPACE = 0x8000,
    PARSER_FLAG_END_OF_NODE = 0x10000,
    PARSER_FLAG_END_OF_NODE_OR_CHILD_BLOCK = 0x20000,
    PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE = 0x100000,
    // Bitmask for testing the state
    PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED = //
        PARSER_FLAG_MAYBE_IN_PROPERTY,
    PARSER_MASK_WHITESPACE_BANNED_V1 =                //
        PARSER_FLAG_TYPE_ANNOTATION_START             //
        | PARSER_FLAG_TYPE_ANNOTATION_END             //
        | PARSER_FLAG_TYPE_ANNOTATION_ENDED           //
        | PARSER_FLAG_IN_PROPERTY,                    //
    PARSER_MASK_NODE_CANNOT_END_HERE =                //
        PARSER_MASK_WHITESPACE_BANNED_V1              //
        | PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED, //
};

struct _kdl_parser {
    kdl_tokenizer* tokenizer;
    kdl_parse_option opt;
    int depth;
    int slashdash_depth;
    enum _kdl_parser_state state;
    kdl_event_data event;
    kdl_owned_string tmp_string_type;
    kdl_owned_string tmp_string_key;
    kdl_owned_string tmp_string_value;
    kdl_str waiting_type_annotation;
    kdl_owned_string waiting_prop_name;
    kdl_token next_token;
    bool have_next_token;
};

static void _init_kdl_parser(kdl_parser* self, kdl_parse_option opt)
{
    self->depth = 0;
    self->slashdash_depth = -1;
    self->state = PARSER_OUTSIDE_NODE;
    self->tmp_string_type = (kdl_owned_string){NULL, 0};
    self->tmp_string_key = (kdl_owned_string){NULL, 0};
    self->tmp_string_value = (kdl_owned_string){NULL, 0};
    self->waiting_type_annotation = (kdl_str){NULL, 0};
    self->waiting_prop_name = (kdl_owned_string){NULL, 0};
    self->have_next_token = false;

    // Fallback: use KDLv1 only
    if ((opt & KDL_PARSE_OPT_VERSION_BITS) == 0) {
        opt |= KDL_READ_VERSION_1;
    }

    self->opt = opt;
}

inline static kdl_character_set _default_character_set(kdl_parse_option opt)
{
    if (opt & KDL_READ_VERSION_2) {
        return KDL_CHARACTER_SET_V2;
    } else {
        return KDL_CHARACTER_SET_V1;
    }
}

kdl_parser* kdl_create_string_parser(kdl_str doc, kdl_parse_option opt)
{
    kdl_parser* self = malloc(sizeof(kdl_parser));
    if (self != NULL) {
        _init_kdl_parser(self, opt);
        self->tokenizer = kdl_create_string_tokenizer(doc);
        kdl_tokenizer_set_character_set(self->tokenizer, _default_character_set(self->opt));
    }
    return self;
}

kdl_parser* kdl_create_stream_parser(kdl_read_func read_func, void* user_data, kdl_parse_option opt)
{
    kdl_parser* self = malloc(sizeof(kdl_parser));
    if (self != NULL) {
        _init_kdl_parser(self, opt);
        self->tokenizer = kdl_create_stream_tokenizer(read_func, user_data);
        kdl_tokenizer_set_character_set(self->tokenizer, _default_character_set(self->opt));
    }
    return self;
}

void kdl_destroy_parser(kdl_parser* self)
{
    kdl_destroy_tokenizer(self->tokenizer);
    kdl_free_string(&self->tmp_string_type);
    kdl_free_string(&self->tmp_string_key);
    kdl_free_string(&self->tmp_string_value);
    kdl_free_string(&self->waiting_prop_name);
    free(self);
}

static void _set_version(kdl_parser* self, kdl_version version)
{
    kdl_parse_option version_flag = version == KDL_VERSION_1 ? KDL_READ_VERSION_1 : KDL_READ_VERSION_2;

    self->opt = (self->opt & ~KDL_PARSE_OPT_VERSION_BITS) | version_flag;
    kdl_tokenizer_set_character_set(self->tokenizer, _default_character_set(self->opt));
}

static void _reset_event(kdl_parser* self)
{
    self->event.name = (kdl_str){NULL, 0};
    self->event.value.type = KDL_TYPE_NULL;
    self->event.value.type_annotation = (kdl_str){NULL, 0};
}

static void _set_parse_error(kdl_parser* self, char const* message)
{
    self->event.event = KDL_EVENT_PARSE_ERROR;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.string = (kdl_str){message, strlen(message)};
}

static void _set_comment_event(kdl_parser* self, kdl_token const* token)
{
    self->event.event = KDL_EVENT_COMMENT;
    self->event.value.type = KDL_TYPE_STRING;
    self->event.value.string = token->value;
}

static kdl_event_data* _next_node(kdl_parser* self, kdl_token* token);
static kdl_event_data* _next_event_in_node(kdl_parser* self, kdl_token* token);
static kdl_event_data* _apply_slashdash(kdl_parser* self);
static bool _parse_value(kdl_parser* self, kdl_token const* token, kdl_value* val, kdl_owned_string* s);
static bool _parse_number(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_decimal_number(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_decimal_integer(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_decimal_float(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_hex_number(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_octal_number(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _parse_binary_number(kdl_str number, kdl_value* val, kdl_owned_string* s);
static bool _identifier_is_valid_v1(kdl_str value);
static bool _identifier_is_valid_v2(kdl_str value);

kdl_event_data* kdl_parser_next_event(kdl_parser* self)
{
    kdl_token token;
    kdl_event_data* ev;

    _reset_event(self);

    while (true) {

        // get the next token (if available)
        if (self->have_next_token) {
            token = self->next_token;
            self->have_next_token = false;
        } else {
            switch (kdl_pop_token(self->tokenizer, &token)) {
            case KDL_TOKENIZER_EOF:
                if ((self->state & 0xff) == PARSER_IN_NODE) {
                    // EOF may be ok, but we have to close the node first
                    token.type = KDL_TOKEN_NEWLINE;
                    token.value = (kdl_str){NULL, 0};
                    break;
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

        if (token.type == KDL_TOKEN_NEWLINE && self->state & PARSER_FLAG_NEWLINES_ARE_WHITESPACE) {
            token.type = KDL_TOKEN_WHITESPACE;
        }

        switch (token.type) {
        case KDL_TOKEN_WHITESPACE:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED_V1) {
                if (_v1_only(self)) {
                    _set_parse_error(self, "Whitespace not allowed here");
                    return &self->event;
                } else {
                    _set_version(self, KDL_VERSION_2);
                }
            } else if (self->state & PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED) {
                self->state |= PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE;
            }
            break; // ignore whitespace

        case KDL_TOKEN_MULTI_LINE_COMMENT:
        case KDL_TOKEN_SINGLE_LINE_COMMENT:
            if (self->state & PARSER_MASK_WHITESPACE_BANNED_V1) {
                if (_v1_only(self)) {
                    _set_parse_error(self, "Comment not allowed here");
                    return &self->event;
                } else {
                    _set_version(self, KDL_VERSION_2);
                }
            } else if (self->state & PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED) {
                self->state |= PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE;
            }
            // Comments may or may not be emitted
            if (self->opt & KDL_EMIT_COMMENTS) {
                _set_comment_event(self, &token);
                return &self->event;
            }
            break;
        case KDL_TOKEN_SLASHDASH:
            // slashdash comments out the next node or argument or property
            if (self->state & PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED) {
                // fall through to _next_event_in_node() - this event should be
                // bubbled back up
            } else if (self->state & PARSER_MASK_NODE_CANNOT_END_HERE) {
                _set_parse_error(self, "/- not allowed here");
                return &self->event;
            } else {
                if (self->slashdash_depth < 0) {
                    self->slashdash_depth = self->depth + 1;
                }
                self->state |= PARSER_FLAG_NEWLINES_ARE_WHITESPACE;
                break;
            }
            _fallthrough_;
        default:
            self->state &= ~PARSER_FLAG_NEWLINES_ARE_WHITESPACE;

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

static kdl_event_data* _apply_slashdash(kdl_parser* self)
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

static kdl_event_data* _next_node(kdl_parser* self, kdl_token* token)
{
    kdl_value tmp_val;
    kdl_event_data* ev;

    if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_MULTILINE_STRING:
        case KDL_TOKEN_RAW_STRING_V1:
        case KDL_TOKEN_RAW_STRING_V2:
        case KDL_TOKEN_RAW_MULTILINE_STRING:
            if (!_parse_value(self, token, &tmp_val, &self->tmp_string_type)) {
                _set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state
                    = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START) | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->waiting_type_annotation = tmp_val.string;
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
            self->state
                = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_END) | PARSER_FLAG_TYPE_ANNOTATION_ENDED;
            return NULL;
        default:
            _set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        switch (token->type) {
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            if (self->state & PARSER_MASK_NODE_CANNOT_END_HERE) {
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
        case KDL_TOKEN_MULTILINE_STRING:
        case KDL_TOKEN_RAW_STRING_V1:
        case KDL_TOKEN_RAW_STRING_V2:
        case KDL_TOKEN_RAW_MULTILINE_STRING:
            if (!_parse_value(self, token, &tmp_val, &self->tmp_string_key)) {
                _set_parse_error(self, "Error parsing node name");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state = PARSER_IN_NODE;
                self->event.event = KDL_EVENT_START_NODE;
                self->event.name = tmp_val.string;
                if (self->waiting_type_annotation.data != NULL) {
                    self->event.value.type_annotation = self->waiting_type_annotation;
                    self->waiting_type_annotation = (kdl_str){NULL, 0};
                }
                ++self->depth;
                ev = _apply_slashdash(self);
                return ev;
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
                self->state = PARSER_IN_NODE;
                --self->depth;
                _reset_event(self);
                if (self->slashdash_depth == self->depth + 1) {
                    // slashdash is ending here
                    self->slashdash_depth = -1;
                    self->state |= PARSER_FLAG_END_OF_NODE_OR_CHILD_BLOCK;
                } else {
                    self->state |= PARSER_FLAG_END_OF_NODE;
                }
                return NULL;
            }
        default:
            _set_parse_error(self, "Unexpected token, expected node");
            return &self->event;
        }
    }
}

static kdl_event_data* _next_event_in_node(kdl_parser* self, kdl_token* token)
{
    kdl_event_data* ev;

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

    if (token->type == KDL_TOKEN_LINE_CONTINUATION) {
        if (self->state & PARSER_MASK_WHITESPACE_BANNED_V1) {
            if (_v1_only(self)) {
                _set_parse_error(self, "Line continuation not allowed here");
                return &self->event;
            } else {
                _set_version(self, KDL_VERSION_2);
            }
        } else if (self->state & PARSER_MASK_WHITESPACE_CONTEXTUALLY_BANNED) {
            self->state |= PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE;
        }
        self->state |= PARSER_FLAG_LINE_CONT;
        return NULL;
    } else if (self->state & PARSER_FLAG_MAYBE_IN_PROPERTY) {
        if (token->type == KDL_TOKEN_EQUALS) {
            // Property value comes next
            self->state = (self->state & ~PARSER_FLAG_MAYBE_IN_PROPERTY) | PARSER_FLAG_IN_PROPERTY;

            // Whitespace before the equals sign?
            if (self->state & PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE) {
                self->state &= ~PARSER_FLAG_CONTEXTUALLY_ILLEGAL_WHITESPACE;
                if (_v1_only(self)) {
                    _set_parse_error(self, "Whitespace not allowed before = sign");
                    return &self->event;
                } else {
                    _set_version(self, KDL_VERSION_2);
                }
            }

            return NULL;
        } else {
            // We'll need that token again
            self->next_token = *token;
            self->have_next_token = true;

            // Not in property; emit as argument (if legal)
            if (self->state & PARSER_FLAG_BARE_PROPERTY_NAME) {
                if (_v1_only(self)) {
                    _set_parse_error(self, "Bare identifier not allowed here");
                    return &self->event;
                } else {
                    _set_version(self, KDL_VERSION_2);
                }
            }

            self->event.event = KDL_EVENT_ARGUMENT;
            self->event.value.type = KDL_TYPE_STRING;
            self->tmp_string_value = self->waiting_prop_name;
            self->waiting_prop_name = (kdl_owned_string){NULL, 0};
            self->event.value.string = kdl_borrow_str(&self->tmp_string_value);
            ev = _apply_slashdash(self);
            self->state = PARSER_IN_NODE;
            return ev;
        }
    } else if (self->state & PARSER_FLAG_TYPE_ANNOTATION_START) {
        kdl_value tmp_val;
        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_MULTILINE_STRING:
        case KDL_TOKEN_RAW_STRING_V1:
        case KDL_TOKEN_RAW_STRING_V2:
        case KDL_TOKEN_RAW_MULTILINE_STRING:
            if (!_parse_value(self, token, &tmp_val, &self->tmp_string_type)) {
                _set_parse_error(self, "Error parsing type annotation");
                return &self->event;
            }
            if (tmp_val.type == KDL_TYPE_STRING) {
                // We're good, this is an identifier
                self->state
                    = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_START) | PARSER_FLAG_TYPE_ANNOTATION_END;
                self->waiting_type_annotation = tmp_val.string;
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
            self->state
                = (self->state & ~PARSER_FLAG_TYPE_ANNOTATION_END) | PARSER_FLAG_TYPE_ANNOTATION_ENDED;
            return NULL;
        default:
            _set_parse_error(self, "Unexpected token, expected ')'");
            return &self->event;
        }
    } else {
        // End of node cases: always valid
        switch (token->type) {
        case KDL_TOKEN_END_CHILDREN:
            // end this node, and process the token again
            self->next_token = *token;
            self->have_next_token = true;
            _fallthrough_;
        case KDL_TOKEN_NEWLINE:
        case KDL_TOKEN_SEMICOLON:
            if (self->state & PARSER_MASK_NODE_CANNOT_END_HERE) {
                _set_parse_error(self, "Unexpected end of node (incomplete argument or property?)");
                return &self->event;
            } else {
                // end the node
                self->state = PARSER_OUTSIDE_NODE;
                --self->depth;
                _reset_event(self);
                self->event.event = KDL_EVENT_END_NODE;
                ev = _apply_slashdash(self);
                return ev;
            }
        default:
            break;
        }

        // Child block: valid unless we've had a non-slashdashed child block
        // already or slashdash is active now
        if (self->state & PARSER_FLAG_END_OF_NODE && self->slashdash_depth < 0) {
            _set_parse_error(self, "Expected end of node");
            return &self->event;
        }
        if (token->type == KDL_TOKEN_START_CHILDREN) {
            // Next event will probably be a new node
            self->state = PARSER_OUTSIDE_NODE;
            ++self->depth;
            _reset_event(self);
            return NULL;
        }

        // Args, props: only valid at the start
        if (self->state & (PARSER_FLAG_END_OF_NODE | PARSER_FLAG_END_OF_NODE_OR_CHILD_BLOCK)) {
            _set_parse_error(self, "Expected end of node or child block");
            return &self->event;
        }

        switch (token->type) {
        case KDL_TOKEN_WORD:
        case KDL_TOKEN_STRING:
        case KDL_TOKEN_MULTILINE_STRING:
        case KDL_TOKEN_RAW_STRING_V1:
        case KDL_TOKEN_RAW_STRING_V2:
        case KDL_TOKEN_RAW_MULTILINE_STRING: {
            // Either a property key, or a property value, or an argument.
            if (!_parse_value(self, token, &self->event.value, &self->tmp_string_value)) {
                _set_parse_error(self, "Error parsing property or argument");
                return &self->event;
            }

            // Can this be a property key?
            if (self->waiting_type_annotation.data == NULL && (self->state & PARSER_FLAG_IN_PROPERTY) == 0
                && self->event.value.type == KDL_TYPE_STRING) {
                // carry over the potential name in waiting_prop_name
                self->waiting_prop_name = self->tmp_string_value;
                self->tmp_string_value = (kdl_owned_string){NULL, 0};
                self->event.value.type = KDL_TYPE_NULL;
                self->state |= PARSER_FLAG_MAYBE_IN_PROPERTY;

                if (token->type == KDL_TOKEN_WORD) {
                    // bare identifiers can't be arguments in KDLv1
                    self->state |= PARSER_FLAG_BARE_PROPERTY_NAME;
                }

                return NULL;
            }

            // This is an argument or a property value.
            // KDLv1 bans bare identifiers in this position
            if (token->type == KDL_TOKEN_WORD && self->event.value.type == KDL_TYPE_STRING) {
                if (_v1_only(self)) {
                    _set_parse_error(self, "Bare identifier not allowed here");
                    return &self->event;
                } else {
                    _set_version(self, KDL_VERSION_2);
                }
            }

            // We can return the event
            if (self->waiting_type_annotation.data != NULL) {
                self->event.value.type_annotation = self->waiting_type_annotation;
                self->waiting_type_annotation = (kdl_str){NULL, 0};
            }
            if (self->state & PARSER_FLAG_IN_PROPERTY) {
                self->tmp_string_key = self->waiting_prop_name;
                self->waiting_prop_name = (kdl_owned_string){NULL, 0};
                self->event.name = kdl_borrow_str(&self->tmp_string_key);
                self->event.event = KDL_EVENT_PROPERTY;
            } else {
                self->event.event = KDL_EVENT_ARGUMENT;
            }
            ev = _apply_slashdash(self);
            // be sure to reset the parser state
            self->state = PARSER_IN_NODE;
            return ev;
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
        default:
            _set_parse_error(self, "Unexpected token");
            return &self->event;
        }
    }
}

static bool _parse_value(kdl_parser* self, kdl_token const* token, kdl_value* val, kdl_owned_string* s)
{
    kdl_free_string(s);

    switch (token->type) {
    case KDL_TOKEN_RAW_STRING_V1:
        if (_v1_allowed(self)) {
            _set_version(self, KDL_VERSION_1);
            // no parsing necessary
            *s = kdl_clone_str(&token->value);
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        } else {
            return false;
        }
    case KDL_TOKEN_RAW_STRING_V2:
        if (_v2_allowed(self)) {
            _set_version(self, KDL_VERSION_2);
            // newlines are not allowed in plain raw strings in v2
            if (_kdl_str_contains_newline(&token->value)) {
                return false;
            }
            // no parsing necessary
            *s = kdl_clone_str(&token->value);
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        } else {
            return false;
        }
    case KDL_TOKEN_RAW_MULTILINE_STRING:
        if (_v2_allowed(self)) {
            _set_version(self, KDL_VERSION_2);
            // dedent multi-line string
            *s = _kdl_dedent_multiline_string(&token->value);
            if (s->data == NULL) {
                return false;
            }
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        } else {
            return false;
        }
    case KDL_TOKEN_STRING: {
        // parse escapes
        kdl_owned_string v1_str = (kdl_owned_string){NULL, 0};
        kdl_owned_string v2_str = (kdl_owned_string){NULL, 0};

        if (_v1_allowed(self)) v1_str = kdl_unescape_v1(&token->value);
        if (_v2_allowed(self)) v2_str = kdl_unescape_v2_single_line(&token->value);

        if (v1_str.data == NULL && v2_str.data != NULL) {
            _set_version(self, KDL_VERSION_2);
            *s = v2_str;
        } else if (v1_str.data != NULL && v2_str.data == NULL) {
            _set_version(self, KDL_VERSION_1);
            *s = v1_str;
        } else if (v1_str.data != NULL && v2_str.data != NULL) {
            // Could be either version, used KDLv2 value
            *s = v2_str;
            kdl_free_string(&v1_str);
        }

        if (s->data == NULL) {
            return false;
        } else {
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        }
    }
    case KDL_TOKEN_MULTILINE_STRING: {
        if (_v2_allowed(self)) {
            *s = kdl_unescape_v2_multi_line(&token->value);
            if (s->data == NULL) {
                return false;
            } else {
                val->type = KDL_TYPE_STRING;
                val->string = kdl_borrow_str(s);
                return true;
            }
        } else {
            return false;
        }
    }
    case KDL_TOKEN_WORD: {
        if (_str_equals_literal(token->value, "null")) {
            if (_v1_allowed(self)) {
                _set_version(self, KDL_VERSION_1);
                val->type = KDL_TYPE_NULL;
                return true;
            } else {
                return false;
            }
        } else if (_str_equals_literal(token->value, "true")) {
            if (_v1_allowed(self)) {
                _set_version(self, KDL_VERSION_1);
                val->type = KDL_TYPE_BOOLEAN;
                val->boolean = true;
                return true;
            } else {
                return false;
            }
        } else if (_str_equals_literal(token->value, "false")) {
            if (_v1_allowed(self)) {
                _set_version(self, KDL_VERSION_1);
                val->type = KDL_TYPE_BOOLEAN;
                val->boolean = false;
                return true;
            } else {
                return false;
            }
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#null")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_NULL;
            return true;
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#true")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_BOOLEAN;
            val->boolean = true;
            return true;
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#false")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_BOOLEAN;
            val->boolean = false;
            return true;
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#inf")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_NUMBER;
            val->number.type = KDL_NUMBER_TYPE_FLOATING_POINT;
            val->number.floating_point = INFINITY;
            return true;
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#-inf")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_NUMBER;
            val->number.type = KDL_NUMBER_TYPE_FLOATING_POINT;
            val->number.floating_point = -INFINITY;
            return true;
        } else if (_v2_allowed(self) && _str_equals_literal(token->value, "#nan")) {
            _set_version(self, KDL_VERSION_2);
            val->type = KDL_TYPE_NUMBER;
            val->number.type = KDL_NUMBER_TYPE_FLOATING_POINT;
            val->number.floating_point = NAN;
            return true;
        }
        // either a number or an identifier
        if (token->value.len >= 1) {
            char first_char = token->value.data[0];
            int offset = 0;
            // skip sign if present
            if ((first_char == '+' || first_char == '-') && token->value.len >= 2) {
                first_char = token->value.data[1];
                offset = 1;
            }
            if (first_char >= '0' && first_char <= '9') {
                // first character after sign is a digit, this value should be interpreted as a number
                return _parse_number(token->value, val, s);
            } else if (_v2_only(self) && first_char == '.' && token->value.len - offset >= 2) {
                // check for v2 rule of banned "almost numbers"
                char second_char = token->value.data[offset + 1];
                if (second_char >= '0' && second_char <= '9') {
                    return false;
                }
            }
        }
        // this is a regular identifier (or a syntax error)
        bool is_v1_identifier = _v1_allowed(self) && _identifier_is_valid_v1(token->value);
        bool is_v2_identifier = _v2_allowed(self) && _identifier_is_valid_v2(token->value);
        bool is_identifier = is_v1_identifier || is_v2_identifier;
        if (is_v1_identifier && !is_v2_identifier) {
            _set_version(self, KDL_VERSION_1);
        } else if (is_v2_identifier && !is_v1_identifier) {
            _set_version(self, KDL_VERSION_2);
        }
        if (is_identifier) {
            *s = kdl_clone_str(&token->value);
            val->type = KDL_TYPE_STRING;
            val->string = kdl_borrow_str(s);
            return true;
        }
        _fallthrough_;
    }
    default:
        return false;
    }
}

static bool _parse_number(kdl_str number, kdl_value* val, kdl_owned_string* s)
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

static bool _parse_decimal_number(kdl_str number, kdl_value* val, kdl_owned_string* s)
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

static bool _parse_decimal_integer(kdl_str number, kdl_value* val, kdl_owned_string* s)
{
    bool negative = false;
    _kdl_ubigint* n = _kdl_ubigint_new(0);
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

static bool _parse_decimal_float(kdl_str number, kdl_value* val, kdl_owned_string* s)
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
            if (number.len - i <= 1 || number.data[i + 1] == '_') return false;
        } else if ((c == 'e' || c == 'E') && state != exponent && state != after_decimal_nodigit) {
            state = exponent_nodigit;
            if (i + 1 < number.len) {
                // handle exponent sign
                switch (number.data[i + 1]) {
                case '-':
                    exponent_negative = true;
                    _fallthrough_;
                case '+':
                    ++i;
                }
                if (number.len - i <= 1 || number.data[i + 1] == '_') return false;
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
    if (digits_before_decimal + digits_after_decimal <= 15 && explicit_exponent < 285
        && explicit_exponent > -285) {
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
        char const* p1 = number.data;
        char const* end = number.data + number.len;
        char* p2 = s->data;
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

static bool _parse_hex_number(kdl_str number, kdl_value* val, kdl_owned_string* s)
{
    bool negative = false;
    _kdl_ubigint* n = _kdl_ubigint_new(0);
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

    if (number.len - i < 3 || number.data[i] != '0' || number.data[i + 1] != 'x' || number.data[i + 2] == '_')
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

static bool _parse_octal_number(kdl_str number, kdl_value* val, kdl_owned_string* s)
{
    bool negative = false;
    _kdl_ubigint* n = _kdl_ubigint_new(0);
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

    if (number.len - i < 3 || number.data[i] != '0' || number.data[i + 1] != 'o' || number.data[i + 2] == '_')
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

static bool _parse_binary_number(kdl_str number, kdl_value* val, kdl_owned_string* s)
{
    bool negative = false;
    _kdl_ubigint* n = _kdl_ubigint_new(0);
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

    if (number.len - i < 3 || number.data[i] != '0' || number.data[i + 1] != 'b' || number.data[i + 2] == '_')
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

static bool _identifier_is_valid_v1(kdl_str value)
{
    uint32_t c = 0;
    if (_kdl_pop_codepoint(&value, &c) != KDL_UTF8_OK || !_kdl_is_id_start(KDL_CHARACTER_SET_V1, c)) {
        return false;
    }

    while (true) {
        switch (_kdl_pop_codepoint(&value, &c)) {
        case KDL_UTF8_OK:
            if (!_kdl_is_id(KDL_CHARACTER_SET_V1, c)) return false;
            break;
        case KDL_UTF8_EOF:
            return true;
        default:
            return false;
        }
    }
}

static bool _identifier_is_valid_v2(kdl_str value)
{
    if (_str_equals_literal(value, "inf") || _str_equals_literal(value, "-inf")
        || _str_equals_literal(value, "nan") || _str_equals_literal(value, "null")
        || _str_equals_literal(value, "true") || _str_equals_literal(value, "false")) {
        return false;
    }

    uint32_t c = 0;
    if (_kdl_pop_codepoint(&value, &c) != KDL_UTF8_OK || !_kdl_is_id_start(KDL_CHARACTER_SET_V2, c)) {
        return false;
    }

    while (true) {
        switch (_kdl_pop_codepoint(&value, &c)) {
        case KDL_UTF8_OK:
            if (!_kdl_is_id(KDL_CHARACTER_SET_V2, c)) return false;
            break;
        case KDL_UTF8_EOF:
            return true;
        default:
            return false;
        }
    }
}
