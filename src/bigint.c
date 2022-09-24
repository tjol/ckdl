#include "bigint.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

#define DIGIT_BITS ((uint64_t)32)
#define DIGIT_BITMASK ((uint64_t)0xFFFFFFFFUL)
#define HIGH_DIGIT_BITMASK (DIGIT_BITMASK << DIGIT_BITS)

_kdl_ubigint *_kdl_ubigint_new(uint32_t initial_value)
{
    _kdl_ubigint *i = malloc(sizeof(_kdl_ubigint) + sizeof(uint32_t));
    if (i == NULL) return NULL;
    i->n_digits = 1;
    i->num[0] = initial_value;
    return i;
}

_kdl_ubigint *_kdl_ubigint_dup(_kdl_ubigint const *value)
{
    size_t size = sizeof(_kdl_ubigint) + value->n_digits * sizeof(uint32_t);
    _kdl_ubigint *i = malloc(size);
    if (i == NULL) return NULL;
    memcpy(i, value, size);
    return i;
}

void _kdl_ubigint_free(_kdl_ubigint *i)
{
    free(i);
}

// a += b
_kdl_ubigint *_kdl_ubigint_add_inplace(_kdl_ubigint *a, unsigned int b)
{
    uint64_t carry = b;
    for (size_t i = 0; i < a->n_digits; ++i) {
        uint64_t tmp = a->num[i] + carry;
        carry = (tmp & HIGH_DIGIT_BITMASK) >> DIGIT_BITS;
        a->num[i] = tmp & DIGIT_BITMASK;
    }
    if (carry != 0) {
        // overflow
        a = reallocf(a,
            sizeof(_kdl_ubigint) + (++a->n_digits) * sizeof(uint32_t));
        if (a != NULL) {
            a->num[a->n_digits - 1] = (uint32_t)(carry & DIGIT_BITMASK);
        }
    }
	return a;
}

// a *= b
_kdl_ubigint *_kdl_ubigint_multiply_inplace(_kdl_ubigint *a, unsigned int b)
{
    uint32_t carry = 0;
    for (size_t i = 0; i < a->n_digits; ++i) {
        uint64_t tmp = a->num[i];
        tmp *= b;
        tmp += carry;
        carry = (tmp & HIGH_DIGIT_BITMASK) >> DIGIT_BITS;
        a->num[i] = tmp & DIGIT_BITMASK;
    }
    if (carry != 0) {
        // overflow
        a = reallocf(a,
            sizeof(_kdl_ubigint) + (++a->n_digits) * sizeof(uint32_t));
        if (a != NULL) {
            a->num[a->n_digits - 1] = carry;
        }
    }
    return a;
}

uint32_t _kdl_ubigint_divide_inplace(_kdl_ubigint *a, uint32_t b)
{
    uint64_t rem = 0;
    for (int i = (int)a->n_digits - 1; i >= 0; --i) {
        uint64_t tmp = a->num[i] + (rem << 32);
        rem = tmp % b;
        a->num[i] = (uint32_t)(tmp / b);
    }
    // shrink the number if possible
    while (a->n_digits > 1 && a->num[a->n_digits - 1] == 0) --a->n_digits;
    return (uint32_t)rem;
}

bool _kdl_ubigint_as_long_long(_kdl_ubigint *i, long long *dest)
{
    // does it fit?
    size_t digits_per_long_long = sizeof(long long) / sizeof(uint32_t);
    if (i->n_digits > digits_per_long_long) return false;
    // does it fit in a *signed* long long though?
    uint32_t top_bit = (uint32_t)1 << (uint32_t)(DIGIT_BITS - 1);
    if (i->num[i->n_digits - 1] & top_bit) return false;

    // ok, we're sure it fits.
    *dest = 0;
    for (int k = (int)i->n_digits - 1; k >= 0; --k) {
        *dest <<= DIGIT_BITS;
        *dest += i->num[k];
    }
    return true;
}

kdl_owned_string _kdl_ubigint_as_string(_kdl_ubigint *i)
{
    return _kdl_ubigint_as_string_sgn(+1, i);
}

kdl_owned_string _kdl_ubigint_as_string_sgn(int sign, _kdl_ubigint *i)
{
    i = _kdl_ubigint_dup(i);
    if (i == NULL) goto error;

    size_t max_digits = i->n_digits * 10; // max 10 decimal digits per 32 bits
    char *buf = malloc(max_digits);
    if (buf == NULL) goto error;
    char *p = buf;
    // write the number backwards
    while (i->n_digits > 1 || i->num[0] != 0) {
        uint32_t digit = _kdl_ubigint_divide_inplace(i, 10);
        *(p++) = (char)('0' + digit);
    }
    _kdl_ubigint_free(i);
    // flip the number and add the sign
    size_t len = p - buf;
    if (sign < 0) ++len;
    char *buf2 = malloc(len + 1);
    if (buf2 == NULL) goto error;
    char *p2 = buf2;
    if (sign < 0) *(p2++) = '-';
    while (p > buf) *(p2++) = *(--p);
    *p2 = '\0';
    free(buf);
    return (kdl_owned_string){ buf2, len };

error:
	return (kdl_owned_string){ NULL, 0 };
}
