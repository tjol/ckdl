#ifndef KDL_VALUE_H_
#define KDL_VALUE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum kdl_type {
    KDL_TYPE_NULL,
    KDL_TYPE_STRING,
    KDL_TYPE_NUMBER,
    KDL_TYPE_BOOLEAN
};

enum kdl_number_type {
    KDL_NUMBER_TYPE_INTEGER,
    KDL_NUMBER_TYPE_FLOATING_POINT,
    KDL_NUMBER_TYPE_STRING_ENCODED
};

typedef enum kdl_type kdl_type;
typedef enum kdl_number_type kdl_number_type;
typedef struct kdl_number kdl_number;
typedef struct kdl_value kdl_value;

struct kdl_number {
    kdl_number_type type;
    union {
        long long integer;
        double floating_point;
        kdl_str string;
    } value;
};

struct kdl_value {
    kdl_type type;
    union {
        kdl_str string;
        kdl_number number;
        bool boolean;
    } value;
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_VALUE_H_
