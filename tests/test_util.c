#include "test_util.h"

#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

static jmp_buf run_test_env;
static bool run_test_active = false;
static int failures = 0;

void assert_true(bool assertion, char const *assertion_s, char const *context)
{
    if (assertion) {
        return;
    } else {
        // Failed
        fprintf(stderr, "- ASSERTION FAILED - %s\n    %s\n", context, assertion_s);
        if (run_test_active) {
            longjmp(run_test_env, 1);
        } else {
            abort();
        }
    }
}

static void _call_noarg_test(void* func_ptr_ptr)
{
    void (*func)(void) = *(void (**)(void))func_ptr_ptr;
    func();
}

void run_test(char const *name, void (*func)(void))
{
    run_test_d(name, _call_noarg_test, (void*)(&func));
}

void run_test_d(char const *name, void (*func)(void *), void *user_data)
{
    fprintf(stderr, "TEST %s\n", name);
    run_test_active = true;
    switch (setjmp(run_test_env)) {
    case 0:
        // normal execution - run the test!
        func(user_data);
        fprintf(stderr, "TEST %s - passed\n", name);
        break;
    case 1:
        // setjmp returned after test failure
        fprintf(stderr, "TEST %s - FAILED\n", name);
        ++failures;
        break;
    }
    run_test_active = false;
}

static int ARGC;
static char const *const *ARGV;

int test_argc(void)
{
    return ARGC;
}

char const *test_arg(int idx)
{
    return ARGV[idx];
}

int main(int argc, char **argv)
{
    ARGC = argc;
    ARGV = (char const *const *)argv;
    TEST_MAIN();
    if (failures != 0) {
        fprintf(stderr, "\n--- %d tests failed ---\n", failures);
        return 1;
    } else {
        return 0;
    }
}
