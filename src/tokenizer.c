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
typedef enum {
  TOKENIZER_STATE_TOKEN_START       = 0,
  TOKENIZER_STATE_TOKEN_WHITESPACE  = 1,
  TOKENIZER_STATE_TOKEN_CRLF        = 2,
  TOKENIZER_STATE_TOKEN_WORD        = 3,
  TOKENIZER_STATE_TOKEN_READY       = 4,
  TOKENIZER_STATE_COMMENT_SECOND    = 5,
  TOKENIZER_STATE_COMMENT_CPP       = 6,
  TOKENIZER_STATE_COMMENT_MULTILINE = 7,
  TOKENIZER_STATE_WORD_RUN          = 8,
  TOKENIZER_STATE_STRING_HASHES     = 9,
  TOKENIZER_STATE_STRING_QUOTES     = 10,
  TOKENIZER_STATE_STRING_EMPTY      = 11,
  TOKENIZER_STATE_STRING_ITSELF     = 12
} _kdl_tokenizer_state;

struct _kdl_tokenizer {
    kdl_str document;
    kdl_character_set charset;
    kdl_read_func read_func;
    void* read_user_data;
    char* buffer;
    size_t buffer_size;

    _kdl_tokenizer_state state;

    // common state
    uint32_t c;
    uint32_t prev_char;
    char const* cur;
    char const* next;
    kdl_utf8_status utf8_status;
    kdl_tokenizer_status status;
    size_t start_offset;
    int counter;

    // _pop_string state
    bool is_raw;
    int hashes;
    bool is_v1;
    int initial_quote_count;
    int max_quote_count;
    int hashes_found;
    size_t end_quote_offset;
    _kdl_tokenizer_state next_state;
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
        self->state = TOKENIZER_STATE_TOKEN_START;
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
        self->state = TOKENIZER_STATE_TOKEN_START;
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

    kdl_utf8_status utf8_status;

    do {
      utf8_status = _tok_get_char(self, &cur, &next, &c);
    } while (utf8_status == KDL_UTF8_WOULDBLOCK);

    if (utf8_status == KDL_UTF8_OK && c == 0xFEFF) {
        // skip initial BOM
        _update_doc_ptr(self, next);
    }
}

void kdl_tokenizer_set_character_set(kdl_tokenizer* self, kdl_character_set cs) { self->charset = cs; }

static ptrdiff_t _refill_tokenizer(kdl_tokenizer* self)
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
    if (self->document.len > 0) {
        memmove(self->buffer, self->document.data, self->document.len);
    }
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
    ptrdiff_t read_count;

    // Fuzz the reads in debug mode to force the tokenizer into asynchronous
    // mode.
#ifdef KDL_DEBUG
    if (rand() % 2 < 1) read_count = -1; else
#endif //KDL_DEBUG
    read_count = self->read_func(self->read_user_data, end, len_available);

    if (read_count > 0)
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
            ptrdiff_t bytes_read = _refill_tokenizer(self);
            // make sure *cur is still valid despite possible realloc call
            *cur = self->document.data + offset;

            if (bytes_read < 0) {
                return KDL_UTF8_WOULDBLOCK;
            } else if (bytes_read == 0) {
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

static void _pop_word(kdl_tokenizer* self, kdl_token* dest);
static void _pop_comment(kdl_tokenizer* self, kdl_token* dest);
static void _pop_string(kdl_tokenizer* self, kdl_token* dest);

kdl_tokenizer_status kdl_pop_token(kdl_tokenizer* self, kdl_token* dest)
{
    self->cur = self->document.data;
    self->next = NULL;
    self->state = TOKENIZER_STATE_TOKEN_START;

    while (self->state != TOKENIZER_STATE_TOKEN_READY) {
        self->utf8_status = _tok_get_char(self, &self->cur, &self->next, &self->c);
        if (self->utf8_status == KDL_UTF8_WOULDBLOCK)
        {
          self->status = KDL_TOKENIZER_WOULDBLOCK;
          break;
        }

        switch (self->state) {
        case TOKENIZER_STATE_TOKEN_START:
            if (self->utf8_status == KDL_UTF8_EOF)
            {
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_EOF;
                break;
            }

            if (self->utf8_status != KDL_UTF8_OK || _kdl_is_illegal_char(self->charset, self->c))
            {
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_ERROR;
                break;
            }

            if (_kdl_is_whitespace(self->charset, self->c)) {
                // find whitespace run
                self->start_offset = self->cur - self->document.data;
                self->cur = self->next;
                self->state = TOKENIZER_STATE_TOKEN_WHITESPACE;
                break;
            }

            if (_kdl_is_newline(self->c)) {
                // end of line
                // special treatment for CRLF
                if (self->c == '\r') {
                    self->cur = self->next;
                    self->state = TOKENIZER_STATE_TOKEN_CRLF;
                    break;
                }
                dest->type = KDL_TOKEN_NEWLINE;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == ';') {
                // semicolon (end of node)
                dest->type = KDL_TOKEN_SEMICOLON;
                dest->value.data = self->document.data;
                dest->value.len = (size_t)(self->cur - self->document.data);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '\\') {
                // line continuation
                dest->type = KDL_TOKEN_LINE_CONTINUATION;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '(') {
                // start of type annotation
                dest->type = KDL_TOKEN_START_TYPE;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == ')') {
                // end of type annotation
                dest->type = KDL_TOKEN_END_TYPE;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '{') {
                // start of list of children
                dest->type = KDL_TOKEN_START_CHILDREN;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '}') {
                // end of list of children
                dest->type = KDL_TOKEN_END_CHILDREN;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '=') {
                // attribute assignment
                dest->type = KDL_TOKEN_EQUALS;
                dest->value.data = self->cur;
                dest->value.len = (size_t)(self->next - self->cur);
                _update_doc_ptr(self, self->next);
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_OK;
                break;
            } else if (self->c == '/') {
                // could be a comment
                _pop_comment(self, dest);
                break;
            } else if (self->c == '"') {
                // string
                self->state = TOKENIZER_STATE_TOKEN_READY;
                _pop_string(self, dest);
                break;
            } else if (_kdl_is_word_char(self->charset, self->c)) {
                if (self->c == 'r' || self->c == '#') {
                    // this *could* be a raw string
                    self->state = TOKENIZER_STATE_TOKEN_WORD;
                    _pop_string(self, dest);
                    break;
                    // else: parse this as an identifier instead (which may also fail)
                }
                // start of an identfier, number, bool, or null
                _pop_word(self, dest);
                break;
            }
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        case TOKENIZER_STATE_TOKEN_WORD:
            if (self->status == KDL_TOKENIZER_OK)
            {
              self->state = TOKENIZER_STATE_TOKEN_READY;
              break;
            }
            _pop_word(self, dest);
            break;

        case TOKENIZER_STATE_TOKEN_WHITESPACE:
            if (self->utf8_status == KDL_UTF8_OK && _kdl_is_whitespace(self->charset, self->c)) {
                // accept whitespace character
                self->cur = self->next;
                break;
            }
            // cur is now the first non-whitespace character.
            dest->type = KDL_TOKEN_WHITESPACE;
            dest->value.data = self->document.data + self->start_offset;
            dest->value.len = self->cur - dest->value.data;
            _update_doc_ptr(self, self->cur);
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_OK;
            break;
        case TOKENIZER_STATE_TOKEN_CRLF:
            // We know '\r' takes 1 byte
            if (self->utf8_status != KDL_UTF8_OK || self->c != '\n')
            {
              self->next = self->cur;
            }
            // We know '\r' takes 1 byte
            self->cur = self->cur - 1;
            dest->type = KDL_TOKEN_NEWLINE;
            dest->value.data = self->cur;
            dest->value.len = (size_t)(self->next - self->cur);
            _update_doc_ptr(self, self->next);
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_OK;
            break;
        case TOKENIZER_STATE_COMMENT_SECOND:
        case TOKENIZER_STATE_COMMENT_CPP:
        case TOKENIZER_STATE_COMMENT_MULTILINE:
            _pop_comment(self, dest);
            break;
        case TOKENIZER_STATE_WORD_RUN:
            _pop_word(self, dest);
            break;
        case TOKENIZER_STATE_STRING_HASHES:
        case TOKENIZER_STATE_STRING_QUOTES:
        case TOKENIZER_STATE_STRING_EMPTY:
        case TOKENIZER_STATE_STRING_ITSELF:
            _pop_string(self, dest);
            break;
        default:
            // We shouldn't be outside scanning states.
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
        }
    }

    if (self->status != KDL_TOKENIZER_WOULDBLOCK)
    {
      // Back to initial state
      self->state = TOKENIZER_STATE_TOKEN_START;
    }

    return self->status;
}

static void _pop_word(kdl_tokenizer* self, kdl_token* dest)
{
    if (self->utf8_status == KDL_UTF8_EOF)
        goto end_of_word;
    if (self->utf8_status != KDL_UTF8_OK)
    {
        self->state = TOKENIZER_STATE_TOKEN_READY;
        self->status = KDL_TOKENIZER_ERROR;
        return;
    }
    if (_kdl_is_end_of_word(self->charset, self->c)) {
        // end the word
        goto end_of_word;
    } else if (!_kdl_is_word_char(self->charset, self->c)) {
        // invalid character
        self->state = TOKENIZER_STATE_TOKEN_READY;
        self->status = KDL_TOKENIZER_ERROR;
        return;
    }
    // Accept this character
    self->cur = self->next;
    self->state = TOKENIZER_STATE_WORD_RUN;
    return;
end_of_word:
    dest->value = self->document;
    dest->value.len = (size_t)(self->cur - dest->value.data);
    _update_doc_ptr(self, self->cur);
    dest->type = KDL_TOKEN_WORD;
    self->state = TOKENIZER_STATE_TOKEN_READY;
    self->status = KDL_TOKENIZER_OK;
}

static void _pop_comment(kdl_tokenizer* self, kdl_token* dest)
{
    switch (self->state) {
    default:
        if (self->utf8_status != KDL_UTF8_OK)
        {
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }
        self->prev_char    = self->c;
        self->cur          = self->next;
        self->state        = TOKENIZER_STATE_COMMENT_SECOND;
        break;

    case TOKENIZER_STATE_COMMENT_SECOND:
        if (self->utf8_status != KDL_UTF8_OK || self->prev_char != '/')
        {
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }

        if (self->c == '-') {
            // slashdash /-
            dest->value.data = self->document.data;
            dest->value.len = (size_t)2;
            dest->type = KDL_TOKEN_SLASHDASH;
            _update_doc_ptr(self, self->next);
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_OK;
            break;
        } else if (self->c == '/') {
            // C++ style comment
            // scan until a newline or EOF
            self->cur = self->next;
            self->state = TOKENIZER_STATE_COMMENT_CPP;
            break;
        } else if (self->c == '*') {
            // C style multi line comment (with nesting)
            self->counter = 1;
            self->prev_char = 0;
            self->cur = self->next;
            self->state = TOKENIZER_STATE_COMMENT_MULTILINE;
            break;
        }
        self->state = TOKENIZER_STATE_TOKEN_READY;
        self->status = KDL_TOKENIZER_ERROR;
        break;

    case TOKENIZER_STATE_COMMENT_CPP:
        if (self->utf8_status == KDL_UTF8_EOF)
            goto end_of_line;
        if (self->utf8_status != KDL_UTF8_OK)
        {
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }
        if (_kdl_is_illegal_char(self->charset, self->c)) {
            self->state = TOKENIZER_STATE_TOKEN_READY;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        } else if (_kdl_is_newline(self->c)) {
            goto end_of_line;
        }
        self->cur = self->next;
        break;
end_of_line:
        dest->value = self->document;
        dest->value.len = (size_t)(self->cur - dest->value.data);
        _update_doc_ptr(self, self->cur);
        dest->type = KDL_TOKEN_SINGLE_LINE_COMMENT;
        self->state = TOKENIZER_STATE_TOKEN_READY;
        self->status = KDL_TOKENIZER_OK;
        break;
    case TOKENIZER_STATE_COMMENT_MULTILINE:
        if (self->counter > 0) {
            if (self->utf8_status != KDL_UTF8_OK || _kdl_is_illegal_char(self->charset, self->c))
            {
                self->state = TOKENIZER_STATE_TOKEN_READY;
                self->status = KDL_TOKENIZER_ERROR;
                break;
            }

            if (self->c == '*' && self->prev_char == '/') {
                // another level of nesting
                ++self->counter;
                self->c = 0; // "/*/" doesn't count as self-closing
            } else if (self->c == '/' && self->prev_char == '*') {
                --self->counter;
                self->c = 0; // "*/*" doesn't count as reopening
            }

            // Accept this character
            self->cur = self->next;
            self->prev_char = self->c;
            break;
        }
        // end of comment reached
        dest->value = self->document;
        dest->value.len = (size_t)(self->cur - dest->value.data);
        _update_doc_ptr(self, self->cur);
        dest->type = KDL_TOKEN_MULTI_LINE_COMMENT;
        self->state = TOKENIZER_STATE_TOKEN_READY;
        self->status = KDL_TOKENIZER_OK;
        break;
    }
}

static void _pop_string(kdl_tokenizer* self, kdl_token* dest)
{
    switch (self->state) {
    default:
        self->is_raw = false;
        self->hashes = 0;
        self->is_v1 = false;
        self->initial_quote_count = 0;
        self->max_quote_count = 3;

        self->hashes_found = 0;
        self->counter = 0;
        self->end_quote_offset = 0;
        self->prev_char = 0;

        if (self->utf8_status != KDL_UTF8_OK) {
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }

        self->next_state = self->state;

        switch (self->c) {
        case 'r':
            self->is_raw = true;
            self->state = TOKENIZER_STATE_STRING_HASHES;
            self->is_v1 = true;
            self->max_quote_count = 1;
            self->cur = self->next;
            break;
        case '#':
            self->is_raw = true;
            self->state = TOKENIZER_STATE_STRING_HASHES;
            break;
        case '"':
            self->state = TOKENIZER_STATE_STRING_QUOTES;
            break;
        default:
            self->state = self->next_state;
            self->status = KDL_TOKENIZER_ERROR;
        }
        break;

    case TOKENIZER_STATE_STRING_HASHES:
        // Count the hashes
        if (self->utf8_status != KDL_UTF8_OK) {
            self->state = self->next_state;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }

        if (self->c == '#') {
            ++self->hashes;
            self->cur = self->next;
        } else if (self->c == '"') {
            self->state = TOKENIZER_STATE_STRING_QUOTES;
        } else {
            self->state = self->next_state;
            self->status = KDL_TOKENIZER_ERROR;
        }
        break;

    case TOKENIZER_STATE_STRING_QUOTES:
        // Count the quotes
        if (self->initial_quote_count < self->max_quote_count) {
            if (self->utf8_status != KDL_UTF8_OK) {
                if (self->utf8_status == KDL_UTF8_EOF) {
                    // is this an empty string?
                    if (!self->is_raw && self->initial_quote_count == 2) {
                        dest->value.data = self->document.data;
                        dest->value.len = 0;
                        _update_doc_ptr(self, self->cur);
                        dest->type = KDL_TOKEN_STRING;
                        self->state = self->next_state;
                        self->status = KDL_TOKENIZER_OK;
                        break;
                    }
                }
                self->state = self->next_state;
                self->status = KDL_TOKENIZER_ERROR;
                break;
            }

            if (self->c == '"') {
                ++self->initial_quote_count;
                self->cur = self->next;
                break;
            }
        }

        if (self->initial_quote_count == 2) {
            // this is an empty string and we overshot - backtrack by one char
            --self->cur;
            --self->initial_quote_count;
        }
        // cur is now the first character after the quote(s)
        self->start_offset = self->cur - self->document.data;
        self->state = TOKENIZER_STATE_STRING_ITSELF;
        break;

    case TOKENIZER_STATE_STRING_ITSELF:
        // Scan the string itself
        if (self->utf8_status != KDL_UTF8_OK) {
            self->state = self->next_state;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        }

        if (_kdl_is_illegal_char(self->charset, self->c)) {
            self->state = self->next_state;
            self->status = KDL_TOKENIZER_ERROR;
            break;
        } else if (!self->is_raw && self->c == '\\' && self->prev_char == '\\') {
            self->c = 0; // double backslash is no backslash
            self->counter = self->hashes_found = 0;
            self->end_quote_offset = 0;
        } else if (self->c == '"' && (self->is_raw || self->prev_char != '\\')) {
            if (self->counter == 0) {
                self->end_quote_offset = self->cur - self->document.data;
            }
            if (self->counter < self->initial_quote_count) {
                ++self->counter;
            } else {
                // possibly extra quotes at the end of a raw string.
                ++self->end_quote_offset;
            }
            self->hashes_found = 0;
        } else if (self->c == '#' && (self->hashes_found != 0 || self->counter == self->initial_quote_count)) {
            ++self->hashes_found;
            self->counter = 0;
        } else {
            // this is neither a quote nor a hash
            self->counter = self->hashes_found = 0;
            self->end_quote_offset = 0;
        }

        if (self->end_quote_offset != 0
            && ((self->hashes == 0 && self->counter == self->initial_quote_count)
                || (self->hashes != 0 && self->hashes_found == self->hashes))) {
            // end of string!
        } else {
            // Accept character
            self->prev_char = self->c;
            self->cur = self->next;
            break;
        }
        // end_quote_ptr = the (first) end quote, next = the char after the quote and hashes
        dest->value.data = self->document.data + self->start_offset;
        dest->value.len = self->end_quote_offset - self->start_offset;
        _update_doc_ptr(self, self->next);
        if (self->initial_quote_count == 3) {
            dest->type = self->is_raw ? KDL_TOKEN_RAW_MULTILINE_STRING : KDL_TOKEN_MULTILINE_STRING;
        } else if (self->is_raw) {
            dest->type = self->is_v1 ? KDL_TOKEN_RAW_STRING_V1 : KDL_TOKEN_RAW_STRING_V2;
        } else {
            dest->type = KDL_TOKEN_STRING;
        }
        self->state = self->next_state;
        self->status = KDL_TOKENIZER_OK;
        break;
    }
}
