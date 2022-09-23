#ifndef KDL_INTERNAL_COMPAT_H_
#define KDL_INTERNAL_COMPAT_H_

#if defined(__cplusplus)
#define _fallthrough_ [[fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
#define _fallthrough_ __attribute__((fallthrough))
#else
#define _fallthrough_
#endif

#if !defined(HAVE_REALLOCF)
#include <stddef.h>
void *reallocf(void *ptr, size_t size);
#endif

#endif // KDL_INTERNAL_COMPAT_H_
