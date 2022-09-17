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
#    ifdef KDL_STATIC_LIB
#        define KDL_EXPORT
#    else
#        ifdef BUILDING_KDL
#            define KDL_EXPORT __declspec(dllexport)
#        else
#            define KDL_EXPORT __declspec(dllimport)
#        endif
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define KDL_EXPORT __attribute__((visibility("default")))
#else
#    define KDL_EXPORT
#endif

#endif // KDL_COMMON_MACROS_H_
