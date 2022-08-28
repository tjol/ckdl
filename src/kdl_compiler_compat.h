#ifndef KDL_INTERNAL_COMPILER_COMPAT_H_
#define KDL_INTERNAL_COMPILER_COMPAT_H_

#ifdef __cplusplus
#define _fallthrough_ [[fallthrough]]
#elif __GNUC__
#define _fallthrough_ __attribute__((fallthrough))
#else
#define _fallthrough_
#endif

#endif // KDL_INTERNAL_COMPILER_COMPAT_H_
