#ifndef KDL_TEST_FS_UTIL_H_
#define KDL_TEST_FS_UTIL_H_

#include <stdbool.h>
#include <stddef.h>

void get_files_in_dir(char const *dir_path, char ***filelist, size_t *n_files, bool regular_only);

#endif // KDL_TEST_FS_UTIL_H_
