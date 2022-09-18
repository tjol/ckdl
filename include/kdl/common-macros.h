#ifndef KDL_COMMON_MACROS_H_
#define KDL_COMMON_MACROS_H_

// define attributes for functions
#if defined(__cplusplus)
#    define KDL_NODISCARD [[nodiscard]]
#elif defined(__GNUC__)
#    define KDL_NODISCARD __attribute__((warn_unused_result))
#else
#    define KDL_NODISCARD
#endif

#if defined(_WIN32)
#    if defined(KDL_STATIC_LIB)
#        define KDL_EXPORT
#    elif defined(BUILDING_KDL)
#        define KDL_EXPORT __declspec(dllexport)
#    else
#        define KDL_EXPORT __declspec(dllimport)
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define KDL_EXPORT __attribute__((visibility("default")))
#else
#    define KDL_EXPORT
#endif

#if defined(_WIN32) && defined(__GNUC__)
#    define KDL_EXPORT_INLINE inline
#else
#    define KDL_EXPORT_INLINE KDL_EXPORT inline
#endif

#endif // KDL_COMMON_MACROS_H_
