#include "test_util.h"

#include "ckdl-cat.h"

// Std C
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// POSIX, but supported on Windows
#include <fcntl.h>
#include <sys/stat.h>

// UNIX and Windows-specific file APIs
#if defined(CHDIR_IN_UNISTD_H)
#include <unistd.h>
#elif defined(CHDIR_IN_DIRECT_H)
#include <direct.h>
#endif

#if defined(HAVE_DIRENT)
#include <dirent.h>
#elif defined(HAVE_WIN32_FILE_API)
#include <windows.h>
#endif

#ifndef O_PATH
#define O_PATH O_RDONLY
#endif

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
    fclose(in);
}

#ifdef HAVE_DIRENT
static bool is_regular_file(DIR *d, struct dirent *de)
{
    struct stat st;
    int dfd, ffd;
    switch (de->d_type) {
    case DT_REG:
        return true;
    case DT_UNKNOWN:
        // must stat
        dfd = dirfd(d);
        ffd = openat(dfd, de->d_name, O_PATH);
        fstat(ffd, &st);
        close(ffd);
        return (st.st_mode & S_IFREG) != 0;
    default:
        return false;
    }
}
#endif

static void get_test_file_list(char const *input_dir, char ***filelist, size_t *n_files)
{
    // count files and file lengths
    size_t n_files_ = 0;
    size_t longest_filename = 0;
#if defined(HAVE_DIRENT)
    DIR *input_dir_p = opendir(input_dir);
    ASSERT(input_dir_p != NULL);
    struct dirent *de;
    while ((de = readdir(input_dir_p)) != NULL) {
        if (is_regular_file(input_dir_p, de)) {
            ++n_files_;
#ifdef HAVE_D_NAMLEN
            size_t fn_len = de->d_namlen;
#else
            size_t fn_len = strlen(de->d_name);
#endif
            if (fn_len > longest_filename)
                longest_filename = fn_len;
        }
    }
#elif defined(HAVE_WIN32_FILE_API)
    WIN32_FIND_DATAA find_data;
    size_t input_dirname_len = strlen(input_dir);
    char *input_glob = malloc(input_dirname_len + 3);
    memcpy(input_glob, input_dir, input_dirname_len);
    memcpy(input_glob + input_dirname_len, "/*", 3);
    HANDLE h_find = FindFirstFileA(input_glob, &find_data);
    while (h_find != INVALID_HANDLE_VALUE) {
        if ((find_data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) == 0) {
            ++n_files_;
            size_t fn_len = strlen(find_data.cFileName);
            if (fn_len > longest_filename)
                longest_filename = fn_len;
        }
        if (!FindNextFileA(h_find, &find_data)) break;
    }
    FindClose(h_find);
#endif

    // allocate file list
    size_t fn_buf_len = n_files_ * (longest_filename + 1);
    size_t filenames_len = n_files_ * sizeof(char *);
    char **filenames = malloc(filenames_len + fn_buf_len);
    char *fn_buf = (char *)filenames + filenames_len;
    char **filenames_end = (char**)fn_buf;
    char *fn_buf_end = fn_buf;
    char *fn_buf_very_end = fn_buf + fn_buf_len;
    char **fn_p = filenames;
#if defined(HAVE_DIRENT)
    rewinddir(input_dir_p);
    while ((de = readdir(input_dir_p)) != NULL && (fn_p < filenames_end)) {
        if (is_regular_file(input_dir_p, de)) {
            char const *fn = de->d_name;
#elif defined(HAVE_WIN32_FILE_API)
    h_find = FindFirstFileA(input_glob, &find_data);
    while (h_find != INVALID_HANDLE_VALUE && (fn_p < filenames_end)) {
        if ((find_data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) == 0) {
            char const *fn = find_data.cFileName;
#endif
            *(fn_p++) = fn_buf_end;
            fn_buf_end += snprintf(fn_buf_end, fn_buf_very_end - fn_buf_end, "%s", fn) + 1;
#if defined(HAVE_DIRENT)
        }
    }
#elif defined(HAVE_WIN32_FILE_API)
        }
        if (!FindNextFileA(h_find, &find_data)) break;
    }
    FindClose(h_find);
    free(input_glob);
#endif
    // return result
    *n_files = (fn_p - filenames);
    *filelist = filenames;
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
    get_test_file_list(input_dir, &filenames, &n_files);

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
