#ifndef KDL_COMMON_H_
#define KDL_COMMON_H_

#include <stddef.h>

// define attributes for functions
#if defined(__cplusplus)
#define KDL_NODISCARD [[nodiscard]]
#elif defined(__GNUC__)
#define KDL_NODISCARD __attribute__((warn_unused_result))
#else
#define KDL_NODISCARD
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum kdl_escape_mode {
    KDL_ESCAPE_MINIMAL = 0,
    KDL_ESCAPE_CONTROL = 0x10,
    KDL_ESCAPE_NEWLINE = 0x20,
    KDL_ESCAPE_TAB = 0x40,
    KDL_ESCAPE_ASCII_MODE =0x170
};

typedef size_t (*kdl_read_func)(void *user_data, char *buf, size_t bufsize);
typedef size_t (*kdl_write_func)(void *user_data, char const *data, size_t nbytes);

typedef struct kdl_str kdl_str;
typedef struct kdl_owned_string kdl_owned_string;
typedef enum kdl_escape_mode kdl_escape_mode;

struct kdl_str {
    char const *data;
    size_t len;
};

struct kdl_owned_string {
    char *data;
    size_t len;
};

inline kdl_str kdl_borrow_str(kdl_owned_string const *str)
{
    return (kdl_str){ str->data, str->len };
}

KDL_NODISCARD kdl_owned_string kdl_clone_str(kdl_str const *s);
void kdl_free_string(kdl_owned_string *s);

KDL_NODISCARD kdl_owned_string kdl_escape(kdl_str const *s, kdl_escape_mode mode);
KDL_NODISCARD kdl_owned_string kdl_unescape(kdl_str const *s);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_COMMON_H_