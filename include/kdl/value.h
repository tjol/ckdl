#ifndef KDL_VALUE_H_
#define KDL_VALUE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum _kdl_type {
    KDL_TYPE_NULL,
    KDL_TYPE_STRING,
    KDL_TYPE_NUMBER,
    KDL_TYPE_BOOLEAN
};
typedef enum _kdl_type kdl_type;

enum _kdl_number_type {
    KDL_NUMBER_TYPE_INTEGER,
    KDL_NUMBER_TYPE_FLOATING_POINT,
    KDL_NUMBER_TYPE_STRING_ENCODED
};
typedef enum _kdl_number_type kdl_number_type;

struct _kdl_number {
    kdl_number_type type;
    union {
        long long integer;
        double floating_point;
        kdl_str string;
    } value;
};
typedef struct _kdl_number kdl_number;

struct _kdl_value {
    kdl_type type;
    union {
        kdl_str string;
        kdl_number number;
        bool boolean;
    } value;
};
typedef struct _kdl_value kdl_value;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_VALUE_H_
