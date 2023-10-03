#ifndef KDL_INTERNAL_BIGINT_H_
#define KDL_INTERNAL_BIGINT_H_

#include <kdl/common.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// *Minimal* unsigned big integer type with only the features needed for integer
// parsing and formatting
struct _kdl_ubigint {
    size_t n_digits;
    uint32_t num[];
};

typedef struct _kdl_ubigint _kdl_ubigint;

// Create a new big int object
_kdl_ubigint* _kdl_ubigint_new(uint32_t initial_value);
// Copy a big int
_kdl_ubigint* _kdl_ubigint_dup(_kdl_ubigint const* value);
// Destroy a big int
void _kdl_ubigint_free(_kdl_ubigint* i);

// a += b
_kdl_ubigint* _kdl_ubigint_add_inplace(_kdl_ubigint* a, uint32_t b);
// a *= b
_kdl_ubigint* _kdl_ubigint_multiply_inplace(_kdl_ubigint* a, uint32_t b);
// a /= b (returns: remainder)
uint32_t _kdl_ubigint_divide_inplace(_kdl_ubigint* a, uint32_t b);

// Convert to long long, if possible (return true on success)
bool _kdl_ubigint_as_long_long(_kdl_ubigint* i, long long* dest);
// Format decimal representation as string
kdl_owned_string _kdl_ubigint_as_string(_kdl_ubigint* i);
// Format decimal representation, starting with a sign
// sign: -1 or +1
kdl_owned_string _kdl_ubigint_as_string_sgn(int sign, _kdl_ubigint* i);

#endif // KDL_INTERNAL_BIGINT_H_
