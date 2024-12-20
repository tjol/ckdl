#include "kdl/tokenizer.h"
#include "compat.h"
#include "grammar.h"
#include "utf8.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// No buffering in Debug mode to find more bugs
#ifdef KDL_DEBUG
#    define MIN_BUFFER_SIZE 1
#    define BUFFER_SIZE_INCREMENT 1
#else
#    define MIN_BUFFER_SIZE 1024
#    define BUFFER_SIZE_INCREMENT 4096
#endif

struct _kdl_tokenizer {
    kdl_str document;
    kdl_character_set charset;
    kdl_read_func read_func;
    void* read_user_data;
    char* buffer;
    size_t buffer_size;
};

static inline void _remove_initial_bom(kdl_tokenizer* self);
static inline void _update_doc_ptr(kdl_tokenizer* self, char const* new_ptr);
static inline kdl_utf8_status _tok_get_char(
    kdl_tokenizer* self, char const** cur, char const** next, uint32_t* codepoint);

kdl_tokenizer* kdl_create_string_tokenizer(kdl_str doc)
{
    kdl_tokenizer* self = malloc(sizeof(kdl_tokenizer));
    if (self != NULL) {
        self->document = doc;
        self->charset = KDL_CHARACTER_SET_DEFAULT;
        self->read_func = NULL;
        self->read_user_data = NULL;
        self->buffer = NULL;
        self->buffer_size = 0;
    }
    _remove_initial_bom(self);
    return self;
}

kdl_tokenizer* kdl_create_stream_tokenizer(kdl_read_func read_func, void* user_data)
{
    kdl_tokenizer* self = malloc(sizeof(kdl_tokenizer));
    if (self != NULL) {
        self->document = (kdl_str){.data = NULL, .len = 0};
        self->charset = KDL_CHARACTER_SET_DEFAULT;
        self->read_func = read_func;
        self->read_user_data = user_data;
        self->buffer = NULL;
        self->buffer_size = 0;
    }
    _remove_initial_bom(self);
    return self;
}

void kdl_destroy_tokenizer(kdl_tokenizer* tokenizer)
{
    if (tokenizer->buffer != NULL) {
        free(tokenizer->buffer);
    }
    free(tokenizer);
}

static inline void _remove_initial_bom(kdl_tokenizer* self)
{
    uint32_t c = 0;
    char const* cur = self->document.data;
    char const* next = NULL;

    if (_tok_get_char(self, &cur, &next, &c) == KDL_UTF8_OK && c == 0xFEFF) {
        // skip initial BOM
        _update_doc_ptr(self, next);
    }
}

void kdl_tokenizer_set_character_set(kdl_tokenizer* self, kdl_character_set cs) { self->charset = cs; }

static size_t _refill_tokenizer(kdl_tokenizer* self)
{
    if (self->read_func == NULL) return 0;

    if (self->buffer == NULL) {
        self->buffer = malloc(BUFFER_SIZE_INCREMENT);
        if (self->buffer == NULL) {
            // memory allocation failed
            return 0;
        }
        self->buffer_size = BUFFER_SIZE_INCREMENT;
        self->document.len = 0;
    }
    // Move whatever data is left unparsed to the top of the buffer
    memmove(self->buffer, self->document.data, self->document.len);
    self->document.data = self->buffer;
    size_t len_available = self->buffer_size - self->document.len;
    if (len_available < MIN_BUFFER_SIZE) {
        // Need more room
        size_t new_buf_size = self->buffer_size + BUFFER_SIZE_INCREMENT;
        char* new_buffer = realloc(self->buffer, new_buf_size);
        if (new_buffer == NULL) {
            return 0;
        } else {
            self->buffer = new_buffer;
            self->buffer_size = new_buf_size;
            self->document.data = new_buffer;
            len_available = self->buffer_size - self->document.len;
        }
    }
    char* end = self->buffer + self->document.len;

    size_t read_count = self->read_func(self->read_user_data, end, len_available);
    self->document.len += read_count;
    return read_count;
}

bool _kdl_is_whitespace(kdl_character_set charset, uint32_t c)
{
    return c == 0x0009 || // Character Tabulation
        c == 0x0020 ||    // Space
        c == 0x00A0 ||    // No-Break Space
        c == 0x1680 ||    // Ogham Space Mark
        c == 0x2000 ||    // En Quad
        c == 0x2001 ||    // Em Quad
        c == 0x2002 ||    // En Space
        c == 0x2003 ||    // Em Space
        c == 0x2004 ||    // Three-Per-Em Space
        c == 0x2005 ||    // Four-Per-Em Space
        c == 0x2006 ||    // Six-Per-Em Space
        c == 0x2007 ||    // Figure Space
        c == 0x2008 ||    // Punctuation Space
        c == 0x2009 ||    // Thin Space
        c == 0x200A ||    // Hair Space
        c == 0x202F ||    // Narrow No-Break Space
        c == 0x205F ||    // Medium Mathematical Space
        c == 0x3000 ||    // Ideographic Space

        (charset == KDL_CHARACTER_SET_V1 && c == 0xFEFF) || // Byte-order mark
        (charset == KDL_CHARACTER_SET_V2 && c == 0x000B)    // Vertical Tab
        ;
}

bool _kdl_is_newline(uint32_t c)
{
    return c == 0x000D || // CR  Carriage Return
        c == 0x000A ||    // LF  Line Feed
        c == 0x0085 ||    // NEL Next Line
        c == 0x000C ||    // FF  Form Feed
        c == 0x2028 ||    // LS  Line Separator
        c == 0x2029;      // PS  Paragraph Separator
}

bool _kdl_is_id(kdl_character_set charset, uint32_t c)
{
    return _kdl_is_word_char(charset, c) && !(charset == KDL_CHARACTER_SET_V2 && c == '#');
}

bool _kdl_is_id_start(kdl_character_set charset, uint32_t c)
{
    return _kdl_is_word_start(charset, c) && !(charset == KDL_CHARACTER_SET_V2 && c == '#');
}

bool _kdl_is_word_char(kdl_character_set charset, uint32_t c)
{
    return c > 0x20 && c <= 0x10FFFF && c != '\\' && c != '/' && c != '(' && c != ')' && c != '{' && c != '}'
        && c != ';' && c != '[' && c != ']' && c != '"' && c != '='
        && !(charset == KDL_CHARACTER_SET_V1 && (c == '<' || c == '>' || c == ','))
        && !_kdl_is_whitespace(charset, c) && !_kdl_is_newline(c) && !_kdl_is_illegal_char(charset, c);
}

bool _kdl_is_word_start(kdl_character_set charset, uint32_t c)
{
    return _kdl_is_id(charset, c) && (c < '0' || c > '9');
}

bool _kdl_is_end_of_word(kdl_character_set charset, uint32_t c)
{
    // is this character something that could terminate an identifier (or number) in some situation?
    return _kdl_is_whitespace(charset, c) || _kdl_is_newline(c) //
        || c == ';' || c == ')' || c == '}' || c == '/' || c == '\\' || c == '=';
}

bool _kdl_is_illegal_char(kdl_character_set charset, uint32_t c)
{
    return charset == KDL_CHARACTER_SET_V2
        && (c <= 0x0008 ||                  // control characters
            (0x000E <= c && c <= 0x001F) || //    ''
            c == 0x007F ||                  // delete
            (0xD800 <= c && c <= 0xDFFF) || // UTF-16 surrogates
            c == 0x200E || c == 0x200F ||   // directional control characters
            (0x202A <= c && c <= 0x202E) || //    ''
            (0x2066 <= c && c <= 0x2069) || //    ''
            c == 0xFEFF ||                  // ZWNBSP = BOM
            c > 0x10FFFF                    // not a codepoint
        );
}

static inline kdl_utf8_status _tok_get_char(
    kdl_tokenizer* self, char const** cur, char const** next, uint32_t* codepoint)
{
    while (true) {
        ptrdiff_t offset;
        kdl_str s = {*cur, self->document.len - (*cur - self->document.data)};
        kdl_utf8_status status = _kdl_pop_codepoint(&s, codepoint);
        switch (status) {
        case KDL_UTF8_OK:
            *next = s.data;
            return KDL_UTF8_OK;
        case KDL_UTF8_EOF:
        case KDL_UTF8_INCOMPLETE:
            offset = *cur - self->document.data;
            size_t bytes_read = _refill_tokenizer(self);
            // make sure *cur is still valid despite possible realloc call
            *cur = self->document.data + offset;
            if (bytes_read == 0) {
                // actual EOF
                return status;
            } else {
                // more data, try again
                break; // out of switch
            }
        default: // error
            return status;
        }
    }
}

static inline void _update_doc_ptr(kdl_tokenizer* self, char const* new_ptr)
{
    self->document.len -= (new_ptr - self->document.data);
    self->document.data = new_ptr;
}

static kdl_tokenizer_status _pop_word(kdl_tokenizer* self, kdl_token* dest);
static kdl_tokenizer_status _pop_comment(kdl_tokenizer* self, kdl_token* dest);
static kdl_tokenizer_status _pop_string(kdl_tokenizer* self, kdl_token* dest);

kdl_tokenizer_status kdl_pop_token(kdl_tokenizer* self, kdl_token* dest)
{
    uint32_t c = 0;
    char const* cur = self->document.data;
    char const* next = NULL;

    while (true) {
        switch (_tok_get_char(self, &cur, &next, &c)) {
        case KDL_UTF8_OK:
            break;
        case KDL_UTF8_EOF:
            return KDL_TOKENIZER_EOF;
        default: // error
            return KDL_TOKENIZER_ERROR;
        }

        // Could be the start of a new token
        if (_kdl_is_illegal_char(self->charset, c)) {
            return KDL_TOKENIZER_ERROR;
        } else if (_kdl_is_whitespace(self->charset, c)) {
            // find whitespace run
            size_t ws_start_offset = cur - self->document.data;
            cur = next;
            while (
                _tok_get_char(self, &cur, &next, &c) == KDL_UTF8_OK && _kdl_is_whitespace(self->charset, c)) {
                // accept whitespace character
                cur = next;
            }
            // cur is now the first non-whitespace character.
            dest->type = KDL_TOKEN_WHITESPACE;
            dest->value.data = self->document.data + ws_start_offset;
            dest->value.len = cur - dest->value.data;
            _update_doc_ptr(self, cur);
            return KDL_TOKENIZER_OK;
        } else if (_kdl_is_newline(c)) {
            // end of line
            // special treatment for CRLF
            if (c == '\r') {
                char const* nnext;
                uint32_t c2;
                bool is_crlf = _tok_get_char(self, &next, &nnext, &c2) == KDL_UTF8_OK && c2 == '\n';
                // cur has been invalidated by _tok_get_char
                // but we know '\r' takes 1 byte
                cur = next - 1;
                if (is_crlf) next = nnext;
            }
            dest->type = KDL_TOKEN_NEWLINE;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == ';') {
            // semicolon (end of node)
            dest->type = KDL_TOKEN_SEMICOLON;
            dest->value.data = self->document.data;
            dest->value.len = (size_t)(cur - self->document.data);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '\\') {
            // line continuation
            dest->type = KDL_TOKEN_LINE_CONTINUATION;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '(') {
            // start of type annotation
            dest->type = KDL_TOKEN_START_TYPE;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == ')') {
            // end of type annotation
            dest->type = KDL_TOKEN_END_TYPE;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '{') {
            // start of list of children
            dest->type = KDL_TOKEN_START_CHILDREN;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '}') {
            // end of list of children
            dest->type = KDL_TOKEN_END_CHILDREN;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '=') {
            // attribute assignment
            dest->type = KDL_TOKEN_EQUALS;
            dest->value.data = cur;
            dest->value.len = (size_t)(next - cur);
            _update_doc_ptr(self, next);
            return KDL_TOKENIZER_OK;
        } else if (c == '/') {
            // could be a comment
            return _pop_comment(self, dest);
        } else if (c == '"') {
            // string
            return _pop_string(self, dest);
        } else if (_kdl_is_word_char(self->charset, c)) {
            if (c == 'r' || c == '#') {
                // this *could* be a raw string
                kdl_tokenizer_status rstring_status = _pop_string(self, dest);
                if (rstring_status == KDL_TOKENIZER_OK) return KDL_TOKENIZER_OK;
                // else: parse this as an identifier instead (which may also fail)
            }
            // start of an identfier, number, bool, or null
            return _pop_word(self, dest);
        } else {
            return KDL_TOKENIZER_ERROR;
        }
    }
}

static kdl_tokenizer_status _pop_word(kdl_tokenizer* self, kdl_token* dest)
{
    uint32_t c = 0;
    char const* cur = self->document.data;
    char const* next = NULL;

    while (true) {
        switch (_tok_get_char(self, &cur, &next, &c)) {
        case KDL_UTF8_OK:
            break;
        case KDL_UTF8_EOF:
            goto end_of_word;
        default: // error
            return KDL_TOKENIZER_ERROR;
        }

        if (_kdl_is_end_of_word(self->charset, c)) {
            // end the word
            goto end_of_word;
        } else if (!_kdl_is_word_char(self->charset, c)) {
            // invalid character
            return KDL_TOKENIZER_ERROR;
        }
        // Accept this character
        cur = next;
    }
end_of_word:
    dest->value = self->document;
    dest->value.len = (size_t)(cur - dest->value.data);
    _update_doc_ptr(self, cur);
    dest->type = KDL_TOKEN_WORD;
    return KDL_TOKENIZER_OK;
}

static kdl_tokenizer_status _pop_comment(kdl_tokenizer* self, kdl_token* dest)
{
    uint32_t c1 = 0;
    uint32_t c2 = 0;
    char const* cur = self->document.data;
    char const* next = NULL;
    if (_tok_get_char(self, &cur, &next, &c1) != KDL_UTF8_OK) return KDL_TOKENIZER_ERROR;
    cur = next;
    if (_tok_get_char(self, &cur, &next, &c2) != KDL_UTF8_OK) return KDL_TOKENIZER_ERROR;
    if (c1 != '/') return KDL_TOKENIZER_ERROR;

    if (c2 == '-') {
        // slashdash /-
        dest->value.data = self->document.data;
        dest->value.len = (size_t)2;
        dest->type = KDL_TOKEN_SLASHDASH;
        _update_doc_ptr(self, next);
        return KDL_TOKENIZER_OK;
    } else if (c2 == '/') {
        // C++ style comment
        // scan until a newline or EOF
        uint32_t c = 0;
        cur = next;
        while (true) {
            switch (_tok_get_char(self, &cur, &next, &c)) {
            case KDL_UTF8_OK:
                break;
            case KDL_UTF8_EOF:
                goto end_of_line;
            default: // error
                return KDL_TOKENIZER_ERROR;
            }
            if (_kdl_is_illegal_char(self->charset, c)) {
                return KDL_TOKENIZER_ERROR;
            } else if (_kdl_is_newline(c)) {
                goto end_of_line;
            }
            // Accept this character
            cur = next;
        }
end_of_line:
        dest->value = self->document;
        dest->value.len = (size_t)(cur - dest->value.data);
        _update_doc_ptr(self, cur);
        dest->type = KDL_TOKEN_SINGLE_LINE_COMMENT;
        return KDL_TOKENIZER_OK;
    } else if (c2 == '*') {
        // C style multi line comment (with nesting)
        int depth = 1;
        uint32_t c = 0;
        uint32_t prev_char = 0;
        cur = next;
        while (depth > 0) {
            switch (_tok_get_char(self, &cur, &next, &c)) {
            case KDL_UTF8_OK:
                break;
            case KDL_UTF8_EOF: // EOF in a comment is an error
            default:           // error
                return KDL_TOKENIZER_ERROR;
            }

            if (_kdl_is_illegal_char(self->charset, c)) {
                return KDL_TOKENIZER_ERROR;
            } else if (c == '*' && prev_char == '/') {
                // another level of nesting
                ++depth;
                c = 0; // "/*/" doesn't count as self-closing
            } else if (c == '/' && prev_char == '*') {
                --depth;
                c = 0; // "*/*" doesn't count as reopening
            }

            // Accept this character
            cur = next;
            prev_char = c;
        }
        // end of comment reached
        dest->value = self->document;
        dest->value.len = (size_t)(cur - dest->value.data);
        _update_doc_ptr(self, cur);
        dest->type = KDL_TOKEN_MULTI_LINE_COMMENT;
        return KDL_TOKENIZER_OK;
    } else {
        return KDL_TOKENIZER_ERROR;
    }
}

static kdl_tokenizer_status _pop_string(kdl_tokenizer* self, kdl_token* dest)
{
    uint32_t c = 0;
    char const* cur = self->document.data;
    char const* next = NULL;
    bool is_raw = false;
    bool is_v1 = false;

    if (_tok_get_char(self, &cur, &next, &c) != KDL_UTF8_OK) return KDL_TOKENIZER_ERROR;
    switch (c) {
    case 'r':
        is_raw = true;
        is_v1 = true;
        cur = next;
        break;
    case '#':
        is_raw = true;
        break;
    case '"':
        break;
    default:
        return KDL_TOKENIZER_ERROR;
    }

    // Count the hashes
    int hashes = 0;
    while (is_raw) {
        switch (_tok_get_char(self, &cur, &next, &c)) {
        case KDL_UTF8_OK:
            break;
        case KDL_UTF8_EOF: // eof in a string is an error
        default:           // error
            return KDL_TOKENIZER_ERROR;
        }

        if (c == '#') {
            ++hashes;
            cur = next;
        } else if (c == '"') {
            break;
        } else {
            return KDL_TOKENIZER_ERROR;
        }
    }

    // Count the quotes
    int initial_quote_count = 0;
    int max_quote_count = is_v1 ? 1 : 3;
    while (initial_quote_count < max_quote_count) {
        switch (_tok_get_char(self, &cur, &next, &c)) {
        case KDL_UTF8_OK:
            break;
        case KDL_UTF8_EOF:
            // is this an empty string?
            if (!is_raw && initial_quote_count == 2) {
                dest->value.data = self->document.data;
                dest->value.len = 0;
                _update_doc_ptr(self, cur);
                dest->type = KDL_TOKEN_STRING;
                return KDL_TOKENIZER_OK;
            }
            _fallthrough_;
        default: // error
            return KDL_TOKENIZER_ERROR;
        }

        if (c == '"') {
            ++initial_quote_count;
            cur = next;
        } else {
            break;
        }
    }

    if (initial_quote_count == 2) {
        // this is an empty string and we overshot - backtrack by one char
        --cur;
        --initial_quote_count;
    }

    // cur is now the first character after the quote(s)
    size_t string_start_offset = cur - self->document.data;

    // Scan the string itself
    int hashes_found = 0;
    int quotes_found = 0;
    size_t end_quote_offset = 0;
    uint32_t prev_char = 0;

    while (true) {
        switch (_tok_get_char(self, &cur, &next, &c)) {
        case KDL_UTF8_OK:
            break;
        case KDL_UTF8_EOF: // eof in a string is an error
        default:           // error
            return KDL_TOKENIZER_ERROR;
        }

        if (_kdl_is_illegal_char(self->charset, c)) {
            return KDL_TOKENIZER_ERROR;
        } else if (!is_raw && c == '\\' && prev_char == '\\') {
            c = 0; // double backslash is no backslash
            quotes_found = hashes_found = 0;
            end_quote_offset = 0;
        } else if (c == '"' && (is_raw || prev_char != '\\')) {
            if (quotes_found == 0) {
                end_quote_offset = cur - self->document.data;
            }
            if (quotes_found < initial_quote_count) {
                ++quotes_found;
            } else {
                // possibly extra quotes at the end of a raw string.
                ++end_quote_offset;
            }
            hashes_found = 0;
        } else if (c == '#' && (hashes_found != 0 || quotes_found == initial_quote_count)) {
            ++hashes_found;
            quotes_found = 0;
        } else {
            // this is neither a quote nor a hash
            quotes_found = hashes_found = 0;
            end_quote_offset = 0;
        }

        if (end_quote_offset != 0
            && ((hashes == 0 && quotes_found == initial_quote_count)
                || (hashes != 0 && hashes_found == hashes))) {
            // end of string!
            break;
        } else {
            // Accept character
            prev_char = c;
            cur = next;
        }
    }

    // end_quote_ptr = the (first) end quote, next = the char after the quote and hashes
    dest->value.data = self->document.data + string_start_offset;
    dest->value.len = end_quote_offset - string_start_offset;
    _update_doc_ptr(self, next);
    if (initial_quote_count == 3) {
        dest->type = is_raw ? KDL_TOKEN_RAW_MULTILINE_STRING : KDL_TOKEN_MULTILINE_STRING;
    } else if (is_raw) {
        dest->type = is_v1 ? KDL_TOKEN_RAW_STRING_V1 : KDL_TOKEN_RAW_STRING_V2;
    } else {
        dest->type = KDL_TOKEN_STRING;
    }
    return KDL_TOKENIZER_OK;
}
