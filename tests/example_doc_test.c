#include "test_util.h"
#include "fs_util.h"

#include "ckdl-cat.h"

// Std C
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// UNIX and Windows-specific file APIs
#if defined(CHDIR_IN_UNISTD_H)
#include <unistd.h>
#elif defined(CHDIR_IN_DIRECT_H)
#include <direct.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>

static int strcmp_p(void const *s1, void const *s2)
{
    char const *const *s1_ = (char const *const *)s1;
    char const *const *s2_ = (char const *const *)s2;
    return strcmp(*s1_, *s2_);
}

struct test_case {
    char const *input_path;
    char const *ground_truth_path;
    bool ignore_output;
};

static void do_test_case(void *user_data)
{
    // configure float output to match kdl4j's test suite
    kdl_emitter_options test_opts = KDL_DEFAULT_EMITTER_OPTIONS;
    test_opts.float_mode.capital_e = true;
    test_opts.float_mode.always_write_decimal_point = true;
    test_opts.float_mode.exponent_plus = true;

    struct test_case *tc = (struct test_case *)user_data;
    FILE *in = fopen(tc->input_path, "r");
    ASSERT(in != NULL);
    kdl_owned_string s = kdl_cat_file_to_string_opt(in, &test_opts);
    if (tc->ground_truth_path == NULL) {
        // parse error expected
        ASSERT2(s.data == NULL, "Parsing should fail");
    } else {
        // should parse with no issue
        ASSERT2(s.data != NULL, "Parsing should not fail");
        if (!tc->ignore_output)
        {
            // compare results
            FILE *gt_fp = fopen(tc->ground_truth_path, "r");
            fseek(gt_fp, 0, SEEK_END);
            long filelen = ftell(gt_fp);
            fseek(gt_fp, 0, SEEK_SET);
            char *buf = malloc(filelen + 1);
            // fread is guaranteed to read the max, but due to Windows CRLF
            // translation this may be less than the file length.
            // We actually *want* CRLF translation here in order to compare
            // results!
            filelen = (long)fread(buf, 1, filelen, gt_fp);
            buf[filelen] = '\0';
            fclose(gt_fp);
            // Test cases may contain a lone newline where an empty file is more appropriate
            ASSERT(filelen == (long)s.len || (filelen == 1 && buf[0] == '\n' && s.len == 0));
            ASSERT(memcmp(buf, s.data, s.len) == 0);
            free(buf);
        }
    }
    kdl_free_string(&s);
    fclose(in);
}

void TEST_MAIN(void)
{
    // figure out where the test data is
    char const *test_cases_root;
    if (test_argc() == 2) {
        test_cases_root = test_arg(1);
    } else {
        test_cases_root = KDL_TEST_CASES_ROOT;
    }

    // prepare the list of fuzzy test cases (don't compare output)
    char *excl_buf = strdup(FUZZY_KDL_TESTS);
    size_t n_fuzzy = 0;
    if (*excl_buf) ++n_fuzzy;
    for (char *p = excl_buf; *p; ++p)
        if (*p == ':')
            ++n_fuzzy;
    char **fuzzy_test_cases = malloc(n_fuzzy * sizeof(char *));
    fuzzy_test_cases[0] = excl_buf;
    for (char *p = excl_buf, **e = &fuzzy_test_cases[1]; *p; ++p) {
        if (*p == ':') {
            *p = '\0';
            *(e++) = p + 1;
        }
    }

    printf("Test data root: %s\n", test_cases_root);

    // get file names
    size_t l = strlen(test_cases_root);
    if (test_cases_root[l-1] == '/') --l;
    char *input_dir = malloc(l + 16);
    char *expected_kdl_fn = malloc(l + 500);
    memcpy(input_dir, test_cases_root, l);
    memcpy(expected_kdl_fn, test_cases_root, l);
    strncpy(input_dir + l, "/input", 16);
    strncpy(expected_kdl_fn + l, "/expected_kdl/", 500);
    size_t expected_kdl_dirname_len = strlen(expected_kdl_fn);
    char *expected_basename_ptr = expected_kdl_fn + expected_kdl_dirname_len;
    size_t expected_basename_avail = (l + 500) - expected_kdl_dirname_len;

    char **filenames;
    size_t n_files;
    get_files_in_dir(input_dir, &filenames, &n_files, true);

    // sort file names
    qsort(filenames, n_files, sizeof(char*), strcmp_p);

    // chdir to the input directory so we don't have to manipulate so many paths...
    ASSERT(chdir(input_dir) == 0);

    // run the tests
    struct test_case tc;

    for (size_t i = 0; i < n_files; ++i) {
        struct stat st;
        // does the output file exist?
        char const* filename = filenames[i];
        tc.input_path = filename;
        tc.ignore_output = false;
        strncpy(expected_basename_ptr, filename, expected_basename_avail);
        if (stat(expected_kdl_fn, &st) == 0) {
            // file exists - expected to pass
            tc.ground_truth_path = expected_kdl_fn;
            // Check if this is a test we exclude
            for (size_t j = 0; j < n_fuzzy; ++j) {
                if (strcmp(fuzzy_test_cases[j], filename) == 0) {
                    tc.ignore_output = true;
                }
            }
        } else {
            if (errno == ENOENT) {
                // file does not exist - expected to fail
                tc.ground_truth_path = NULL;
            } else {
                // Other error?!
                perror("Error scanning data: ");
                ASSERT(0);
            }
        }
        // let's go
        run_test_d(filename, &do_test_case, &tc);
    }

    free(excl_buf);
    free(fuzzy_test_cases);
    free(filenames);
    free(input_dir);
    free(expected_kdl_fn);
}
