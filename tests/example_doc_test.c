#include "test_util.h"

#include "ckdl-cat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
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
};

static void do_test_case(void *user_data)
{
    struct test_case *tc = (struct test_case *)user_data;
    FILE *in = fopen(tc->input_path, "r");
    ASSERT(in != NULL);
    kdl_owned_string s = kdl_cat_file_to_string(in);
    if (tc->ground_truth_path == NULL) {
        // parse error expected
        ASSERT2(s.data == NULL, "Parsing should fail");
    } else {
        // should parse with no issue
        ASSERT2(s.data != NULL, "Parsing should not fail");
        // compare results
        FILE *gt_fp = fopen(tc->ground_truth_path, "r");
        fseek(gt_fp, 0, SEEK_END);
        long filelen = ftell(gt_fp);
        fseek(gt_fp, 0, SEEK_SET);
        char *buf = malloc(filelen);
        fread(buf, 1, filelen, gt_fp);
        fclose(gt_fp);
        // Test cases may contain a lone newline where an empty file is more appropriate
        ASSERT(filelen == (long)s.len || (filelen == 1 && buf[0] == '\n' && s.len == 0));
        ASSERT(memcmp(buf, s.data, s.len) == 0);
        free(buf);
    }
    fclose(in);
}

void TEST_MAIN()
{
    // figure out where the test data is
    char const *test_cases_root;
    if (test_argc() == 2) {
        test_cases_root = test_arg(1);
    } else {
        test_cases_root = KDL_TEST_CASES_ROOT;
    }

    printf("Test data root: %s\n", test_cases_root);

    // find all the test cases
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

    DIR *input_dir_p = opendir(input_dir);
    ASSERT(input_dir_p != NULL);
    // count files and file lengths
    size_t n_files = 0;
    size_t longest_filename = 0;
    struct dirent *de;
    while ((de = readdir(input_dir_p)) != NULL) {
        if (de->d_type == DT_REG) {
            ++n_files;
            if (de->d_namlen > longest_filename)
                longest_filename = de->d_namlen;
        }
    }
    // allocate file list
    size_t fn_buf_len = n_files * (longest_filename + 1);
    char *fn_buf = malloc(fn_buf_len);
    char *fn_buf_end = fn_buf;
    char *fn_buf_very_end = fn_buf + fn_buf_len;
    char **filenames = malloc(n_files * sizeof(char *));
    char **fn_p = filenames;
    rewinddir(input_dir_p);
    while ((de = readdir(input_dir_p)) != NULL && (fn_p < filenames + (n_files * sizeof(char *)))) {
        if (de->d_type == DT_REG) {
            *(fn_p++) = fn_buf_end;
            fn_buf_end += snprintf(fn_buf_end, fn_buf_very_end - fn_buf_end, "%s", de->d_name) + 1;
        }
    }
    // sort file list
    n_files = (fn_p - filenames);
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
        strncpy(expected_basename_ptr, filename, expected_basename_avail);
        if (stat(expected_kdl_fn, &st) == 0) {
            // file exists - expected to pass
            tc.ground_truth_path = expected_kdl_fn;
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

    free(fn_buf);
    free(filenames);
    free(input_dir);
    free(expected_kdl_fn);
}
