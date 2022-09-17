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
#    ifdef BUILDING_KDL
#        define KDL_EXPORT __declspec(dllexport)
#        define KDL_EXPORT_INLINE inline
#    elif !defined(KDL_STATIC_LIB)
#        define KDL_EXPORT __declspec(dllimport)
#    else
#        define KDL_EXPORT
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define KDL_EXPORT __attribute__((visibility("default")))
#else
#    define KDL_EXPORT
#endif

#ifndef KDL_EXPORT_INLINE
#define KDL_EXPORT_INLINE KDL_EXPORT inline
#endif

#endif // KDL_COMMON_MACROS_H_
