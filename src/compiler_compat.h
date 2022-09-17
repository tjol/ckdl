#ifndef KDL_INTERNAL_COMPILER_COMPAT_H_
#define KDL_INTERNAL_COMPILER_COMPAT_H_

#if defined(__cplusplus)
#define _fallthrough_ [[fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
#define _fallthrough_ __attribute__((fallthrough))
#else
#define _fallthrough_
#endif

#if defined(_WIN32) && defined(__GNUC__) && !defined(KDL_STATIC_LIB)
#define KDL_EXPORT_EXTERN_INLINE __declspec(dllexport) extern inline
#else
#define KDL_EXPORT_EXTERN_INLINE extern inline
#endif

#endif // KDL_INTERNAL_COMPILER_COMPAT_H_
