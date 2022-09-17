#ifndef KDL_COMMON_H_
#define KDL_COMMON_H_

#include <stddef.h>
#include "common-macros.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Parameter for kdl_escape - which characters should be escaped, and which should not?
enum kdl_escape_mode {
    KDL_ESCAPE_MINIMAL = 0,       // Only escape what *must* be escaped
    KDL_ESCAPE_CONTROL = 0x10,    // Escape ASCII control characters
    KDL_ESCAPE_NEWLINE = 0x20,    // Escape newline characters
    KDL_ESCAPE_TAB = 0x40,        // Escape tabs
    KDL_ESCAPE_ASCII_MODE =0x170, // Escape all non-ASCII charscters
    // "Sensible" default: escape tabs, newlines, and other control characters, but leave
    // unicode intact
    KDL_ESCAPE_DEFAULT = KDL_ESCAPE_CONTROL | KDL_ESCAPE_NEWLINE | KDL_ESCAPE_TAB
};

// Function pointers used to interface with external IO
typedef size_t (*kdl_read_func)(void *user_data, char *buf, size_t bufsize);
typedef size_t (*kdl_write_func)(void *user_data, char const *data, size_t nbytes);

typedef struct kdl_str kdl_str;
typedef struct kdl_owned_string kdl_owned_string;
typedef enum kdl_escape_mode kdl_escape_mode;

// A reference to a string, like Rust's str or C++'s std::u8string_view
// Need not be nul-terminated!
struct kdl_str {
    char const *data; // Data pointer - NULL means no string
    size_t len;       // Length of the string - 0 means empty string
};

// An owned string. Should be destroyed using kdl_free_string
// Owned strings are nul-terminated.
struct kdl_owned_string {
    char *data;
    size_t len;
};

// Get a reference to an owned string
KDL_EXPORT_INLINE kdl_str kdl_borrow_str(kdl_owned_string const *str)
{
    kdl_str result = { str->data, str->len };
    return result;
}

// Create a kdl_str from a nul-terminated C string
KDL_EXPORT kdl_str kdl_str_from_cstr(char const *s);
// Create an owned string with the same content as another string
KDL_NODISCARD KDL_EXPORT kdl_owned_string kdl_clone_str(kdl_str const *s);
// Free the memory associated with an owned string, and set the pointer to NULL
KDL_EXPORT void kdl_free_string(kdl_owned_string *s);

// Escape special characters in a string according to KDL string rules
KDL_NODISCARD KDL_EXPORT kdl_owned_string kdl_escape(kdl_str const *s, kdl_escape_mode mode);
// Resolve backslash escape sequences
KDL_NODISCARD KDL_EXPORT kdl_owned_string kdl_unescape(kdl_str const *s);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_COMMON_H_
