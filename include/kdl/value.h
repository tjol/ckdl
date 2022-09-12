#ifndef KDL_VALUE_H_
#define KDL_VALUE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Data type
enum kdl_type {
    KDL_TYPE_NULL,
    KDL_TYPE_BOOLEAN,
    KDL_TYPE_NUMBER,
    KDL_TYPE_STRING
};

// C representation of a KDL "number"
enum kdl_number_type {
    KDL_NUMBER_TYPE_INTEGER,        // numbers that fit in a long long
    KDL_NUMBER_TYPE_FLOATING_POINT, // numbers exactly representable in a double
    KDL_NUMBER_TYPE_STRING_ENCODED  // other numbers are stored as strings
};

typedef enum kdl_type kdl_type;
typedef enum kdl_number_type kdl_number_type;
typedef struct kdl_number kdl_number;
typedef struct kdl_value kdl_value;

// A KDL number
struct kdl_number {
    kdl_number_type type;
    union {
        long long integer;
        double floating_point;
        kdl_str string;
    };
};

// A KDL value, including its type annotation (if it has one)
struct kdl_value {
    kdl_type type;
    kdl_str type_annotation;
    union {
        bool boolean;
        kdl_number number;
        kdl_str string;
    };
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_VALUE_H_
