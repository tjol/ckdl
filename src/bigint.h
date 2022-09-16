#ifndef KDL_INTERNAL_BIGINT_H_
#define KDL_INTERNAL_BIGINT_H_

#include <kdl/common.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

struct _kdl_ubigint {
    size_t n_digits;
    uint32_t num[];
};

typedef struct _kdl_ubigint _kdl_ubigint;

_kdl_ubigint *_kdl_ubigint_new(uint32_t initial_value);
_kdl_ubigint *_kdl_ubigint_dup(_kdl_ubigint const *value);
void _kdl_ubigint_free(_kdl_ubigint *i);

// a += b
_kdl_ubigint *_kdl_ubigint_add_inplace(_kdl_ubigint *a, uint32_t b);
// a *= b
_kdl_ubigint *_kdl_ubigint_multiply_inplace(_kdl_ubigint *a, uint32_t b);
// a /= b (returns: remainder)
uint32_t _kdl_ubigint_divide_inplace(_kdl_ubigint *a, uint32_t b);

bool _kdl_ubigint_as_long_long(_kdl_ubigint *i, long long *dest);
kdl_owned_string _kdl_ubigint_as_string(_kdl_ubigint *i);
kdl_owned_string _kdl_ubigint_as_string_sgn(int sign, _kdl_ubigint *i);

#endif // KDL_INTERNAL_BIGINT_H_
