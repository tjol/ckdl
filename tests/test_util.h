#ifndef KDL_TEST_TEST_UTIL_H_
#define KDL_TEST_TEST_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdbool.h>

#define _stringify(a) _stringify_2(a)
#define _stringify_2(a) #a
#define ASSERT(q) assert_true((q), #q, __FILE__  "("  _stringify(__LINE__)  ")")
#define ASSERT2(q, msg) assert_true((q), msg, __FILE__  "("  _stringify(__LINE__)  ")")
void assert_true(bool assertion, char const *assertion_s, char const *context);

void run_test(char const *name, void (*func)(void));
void run_test_d(char const *name, void (*func)(void *), void *user_data);

int test_argc(void);
char const *test_arg(int idx);

extern void TEST_MAIN(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // KDL_TEST_TEST_UTIL_H_
