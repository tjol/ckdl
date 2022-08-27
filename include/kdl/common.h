#ifndef KDL_COMMON_H_
#define KDL_COMMON_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct _kdl_str {
    char const *data;
    size_t len;
};
typedef struct _kdl_str kdl_str;

struct _kdl_owned_string {
    char *data;
    size_t len;
};
typedef struct _kdl_owned_string kdl_owned_string;

inline kdl_str kdl_borrow_str(kdl_owned_string *str)
{
    return (kdl_str){ str->data, str->len };
}

kdl_owned_string kdl_clone_str(kdl_str const *s);
void kdl_free_string(kdl_owned_string *s);
kdl_owned_string kdl_escape(kdl_str const *s);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_COMMON_H_