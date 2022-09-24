#include "fs_util.h"

#if defined(HAVE_DIRENT)
#  include <dirent.h>
#elif defined(HAVE_WIN32_FILE_API)
// MSVC generates warnings in Windows.h, which, frankly, is just embarrassing
#  ifdef _MSC_VER
#    pragma warning(push, 0)
#  endif
#  include <windows.h>
#  ifdef _MSC_VER
#    pragma warning(pop)
#  endif
#endif

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#if defined(HAVE_DIRENT)

#include <unistd.h>

static inline bool is_regular_file(DIR *d, struct dirent *de)
{
    struct stat st;
    int dfd;
#ifdef HAVE_DIRENT_D_TYPE
    switch (de->d_type) {
    case DT_REG:
        return true;
    case DT_UNKNOWN:
#endif
        // must stat
        dfd = dirfd(d);
        fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW);
        return (st.st_mode & S_IFREG) != 0;

#ifdef HAVE_DIRENT_D_TYPE
    default:
        return false;
    }
#endif
}

#elif defined(HAVE_WIN32_FILE_API)

static inline BOOL is_regular_file(WIN32_FIND_DATAA const *find_data)
{
    return (find_data->dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) == 0;
}

#endif

void get_files_in_dir(char const *dir_path, char ***filelist, size_t *n_files, bool regular_only)
{
    size_t list_buf_len = 4096;
    char *list_buf = malloc(list_buf_len);
    if (list_buf == NULL) goto error;
    char *list_ptr = list_buf;
    char *list_buf_tmp;
    size_t count = 0;

#if defined(HAVE_DIRENT)
    DIR *dir_p = opendir(dir_path);
    if (dir_p == NULL) goto error;
    struct dirent *de;
    while ((de = readdir(dir_p)) != NULL) {
        if (!regular_only || is_regular_file(dir_p, de)) {
            char const *fn = de->d_name;

#elif defined(HAVE_WIN32_FILE_API)
    WIN32_FIND_DATAA find_data;
    // prepare a glob pattern for FindFile
    size_t dir_path_len = strlen(dir_path);
    char *glob = malloc(dir_path_len + 3);
    if (glob == NULL) goto error;
    memcpy(glob, dir_path, dir_path_len);
    memcpy(glob + dir_path_len, "/*", 3);
    HANDLE h_find = FindFirstFileA(glob, &find_data);
    while (h_find != INVALID_HANDLE_VALUE) {
        if (!regular_only || is_regular_file(&find_data)) {
            char const *fn = find_data.cFileName;

#endif // HAVE_DIRENT / HAVE_WIN32_FILE_API

#if defined(HAVE_DIRENT_D_NAMLEN)
            size_t fn_len = de->d_namlen;
#else
            size_t fn_len = strlen(fn);
#endif // HAVE_DIRENT_D_NAMLEN

            ++count;

            // Copy the file name to the list
            size_t remaining_size = list_buf_len - (list_ptr - list_buf);
            if (remaining_size <= fn_len) {
                size_t increment = 4096;
                while (increment <= fn_len) increment <<= 1;
                size_t offset = list_ptr - list_buf;
                list_buf_len += increment;
                list_buf_tmp = realloc(list_buf, list_buf_len);
                if (list_buf_tmp == NULL) {
                    free(list_buf);
                    list_buf = NULL;
                    break;
                } else {
                    list_buf = list_buf_tmp;
                }
                list_ptr = list_buf + offset;
            }
            memcpy(list_ptr, fn, fn_len);
            list_ptr += fn_len;
            *(list_ptr++) = '\0';

#if defined(HAVE_DIRENT)
        }
    }
    closedir(dir_p);
#elif defined(HAVE_WIN32_FILE_API)
        }
        if (!FindNextFileA(h_find, &find_data)) break;
    }
    FindClose(h_find);
    free(glob);
#endif
    if (list_buf == NULL) goto error;

    // We now have a null-terminated list of `count` file names
    // Prepare a buffer starting with `count` pointers, followed by the actual data
    size_t data_offset = count * sizeof(char*);
    size_t data_len = list_ptr - list_buf;
    list_buf_tmp = realloc(list_buf, data_offset + data_len);
    if (list_buf_tmp == NULL) goto error;
    else list_buf = list_buf_tmp;
    list_ptr = list_buf + data_offset;
    memmove(list_ptr, list_buf, data_len);

    // Write the array
    char **list = (char**)list_buf;
    for (size_t i = 0; i < count; ++i) {
        list[i] = list_ptr;
        while (*(list_ptr++) != 0);
    }

    // Return it
    *filelist = list;
    *n_files = count;

    return;

error:
    *filelist = NULL;
    *n_files = 0;
    if (list_buf != NULL) free(list_buf);
}
