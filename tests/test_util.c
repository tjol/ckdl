#include "test_util.h"

#include <stdio.h>
#include <setjmp.h>

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
        }
    }
}

void run_test(char const *name, void (*func)())
{
    fprintf(stderr, "TEST %s\n", name);
    run_test_active = true;
    switch (setjmp(run_test_env)) {
    case 0:
        // notmal execution - run the test!
        func();
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

int main()
{
    TEST_MAIN();
    if (failures != 0) {
        fprintf(stderr, "\n--- %d tests failed ---\n", failures);
        return 1;
    } else {
        return 0;
    }
}
