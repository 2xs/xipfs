#include "mkxipfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>

#include "include/path.h"
#include "include/errno.h"

#include "test.h"


static bool verbose = false;

static xipfs_mount_t mountpoint;

/* We use this variable instead of ts->total, because
 * test_expect is also called when preparing tests internal data.
 * This variable is modified by actual calls to xipfs_path_check.
 */
static size_t tests_checks_count = 0;

/* We use this variable to track checks on valid tests checks. */
static size_t tests_check_valid_checks_count = 0;

/* We use this variable to track checks on invalid tests checks. */
static size_t tests_check_invalid_checks_count = 0;

static const char *valid_filename_prefixes[] = {
    "",
    ".",
    "..",
    "...",
};

static const char *valid_filename_suffixes[] = {
    "",

    ".x",
    ".x.",
    ".x.y",
    ".x.y.",
    ".x.y..",
    ".x.y...",
    ".x..",
    ".x..y",
    ".x..y.",
    ".x..y..",
    ".x..y...",
    ".x...",
    ".x...y",
    ".x...y.",
    ".x...y..",
    ".x...y...",

    "..x",
    "..x.",
    "..x.y",
    "..x.y.",
    "..x.y..",
    "..x.y...",
    "..x..",
    "..x..y",
    "..x..y.",
    "..x..y..",
    "..x..y...",
    "..x...",
    "..x...y",
    "..x...y.",
    "..x...y..",
    "..x...y...",

    "...x",
    "...x.",
    "...x.y",
    "...x.y.",
    "...x.y..",
    "...x.y...",
    "...x..",
    "...x..y",
    "...x..y.",
    "...x..y..",
    "...x..y...",
    "...x...",
    "...x...y",
    "...x...y.",
    "...x...y..",
    "...x...y...",
};

typedef void (*test_check_path_callback_t)(const char *str, test_state_t *ts);

static inline void test_check_path_verbose(const char *label, const char *path) {
    if (verbose) {
        fprintf(stdout, "test %zu : %s : \"%s\".\n", tests_checks_count, label, path);
    }
}

static inline void test_check_path_verbose_rename(const char *label,
                                                  const char *path, const char *to_path) {
    if (verbose) {
        fprintf(stdout, "test %zu : %s :\n"
                        "\t-\"%s\"\n"
                        "\t-\"%s\".\n", tests_checks_count, label, path, to_path);
    }
}

static inline void test_check_path_track_valid_case(void) {
    tests_checks_count++;
    tests_check_valid_checks_count++;
}

static bool test_check_path_valid_path(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_valid_path", path);

    int res = xipfs_path_check(path);
    bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_path: "
                "xipfs_path_check(\"%s\") should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                path,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    return test_res;
}

static bool test_check_path_valid_unlink(const char *filename, test_state_t *ts) {
    test_check_path_verbose("test_check_path_valid_unlink", filename);
    int res = xipfs_unlink(&mountpoint, filename);
    const bool test_res = (res == 0);
    test_expect(ts, test_res,
                "test_check_path_valid_unlink: "
                "xipfs_unlink(%p, \"%s\") should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, filename,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    return test_res;
}

static bool test_check_path_valid_new_file(const char *filename, test_state_t *ts) {
    const xipfs_file_position_t size = 0;
    const uint32_t exec = 0;

    test_check_path_verbose("test_check_path_valid_new_file", filename);

    int res = xipfs_new_file(&mountpoint, filename, size, exec);
    const bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_new_file: "
                "xipfs_new_file(%p, \"%s\", %" XIPFS_FILE_POSITION_FORMAT ", %" PRIu32 ") "
                "should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, filename, size, exec,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    return test_res;
}

static bool test_check_path_valid_open(const char *filename, test_state_t *ts) {
    xipfs_file_desc_t desc;
    const int flags = O_CREAT;
    const mode_t mode = 0;

    test_check_path_verbose("test_check_path_valid_open", filename);

    int res = xipfs_open(&mountpoint, &desc, filename, flags, mode);
    const bool test_res = (res >= 0);
    test_expect(ts, test_res, "test_check_path_valid_open: "
                "xipfs_open(%p, %p, \"%s\", %x, %x) should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, &desc, filename, flags, (int)mode,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    if (test_res == true)
        xipfs_close(&mountpoint, &desc);
    return test_res;
}

static void test_check_path_valid_stat(const char *path, test_state_t *ts) {
    struct stat stats;

    test_check_path_verbose("test_check_path_valid_stat", path);

    int res = xipfs_stat(&mountpoint, path, &stats);
    test_expect(ts, res == 0, "test_check_path_valid_stat: "
                "xipfs_stat(%p, \"%s\", %p) should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, path, &stats,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
}

static void test_check_path_valid_statvfs(const char *path, test_state_t *ts) {
    struct xipfs_statvfs stats;

    test_check_path_verbose("test_check_path_valid_statvfs", path);

    int res = xipfs_statvfs(&mountpoint, path, &stats);
    test_expect(ts, res == 0, "test_check_path_valid_statvfs: "
                "xipfs_statvfs(%p, \"%s\", %p) should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, path, &stats,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
}

static void test_check_path_rename_build_to_upper_basename(const char *path,
                                                           char to_path_buffer[XIPFS_PATH_MAX]) {
    bool slash_encountered = false;
    int len;
    int i;
    char c;

    if (path == NULL) {
        to_path_buffer = '\0';
        return;
    }

    if (path[0] == '\0') {
        to_path_buffer = '\0';
        return;
    }

    len = (int)strnlen(path, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        memcpy(to_path_buffer, path, XIPFS_PATH_MAX - 1);
        to_path_buffer[XIPFS_PATH_MAX - 1] = toupper(path[XIPFS_PATH_MAX]);
        return;
    }

    to_path_buffer[len] = '\0';
    if (path[len - 1] == '/') {
        i = len - 2;
    } else {
        i = len - 1;
    }
    while (i >=0) {
        if (slash_encountered == false) {
            if (path[i] == '/') {
                slash_encountered = true;
                c ='/';
            } else {
                c = (char)toupper(path[i]);
            }
        } else {
            c = path[i];
        }

        to_path_buffer[i] = c;
        --i;
    }
}

static void test_check_path_rename_build_long_basename(const char *path,
                                                       char to_path_buffer[XIPFS_PATH_MAX]) {
    int len;
    int i, last_slash_pos;
    bool path_terminated_by_slash;

    if (path == NULL) {
        to_path_buffer = '\0';
        return;
    }

    if (path[0] == '\0') {
        to_path_buffer = '\0';
        return;
    }

    len = (int)strnlen(path, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        memcpy(to_path_buffer, path, XIPFS_PATH_MAX - 1);
        to_path_buffer[XIPFS_PATH_MAX - 1] = toupper(path[XIPFS_PATH_MAX]);
        return;
    }

    if (path[len - 1] == '/') {
        i = len - 2;
        path_terminated_by_slash = true;
    } else {
        i = len - 1;
        path_terminated_by_slash = false;
    }
    /* recopy dirname */
    last_slash_pos = -1;
    while (i >=0) {
        if (last_slash_pos < 0) {
            if (path[i] == '/') {
                last_slash_pos = i;
                to_path_buffer[i] = '/';
            }
            /* skip basename chars */
        } else {
            /* recopy dirname chars */
            to_path_buffer[i] = path[i];
        }

        --i;
    }

    /* Replace basename by padding */
    for (i = last_slash_pos + 1; i < (XIPFS_PATH_MAX - 2); i++) {
        to_path_buffer[i] = 'A' + (i % 26);
    }

    /* Terminate the new path according to path termination */
    if (path_terminated_by_slash)
        to_path_buffer[XIPFS_PATH_MAX - 2] = '/';
    else
        to_path_buffer[XIPFS_PATH_MAX - 2] = 'A' + (i % 26);

    to_path_buffer[XIPFS_PATH_MAX - 1] = '\0';
}

static void test_check_path_valid_rename_leaf(const char *path, const char *to_path,
                                              test_state_t *ts) {
    test_check_path_verbose_rename("test_check_path_valid_rename", path, to_path);

    int res = xipfs_rename(&mountpoint, path, to_path);
    const bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_rename: "
                "xipfs_rename(%p, \"%s\", \"%s\") should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, path, to_path,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();

    if (test_res == true) {
        /* revert rename */
        (void)xipfs_rename(&mountpoint, to_path, path);
    }
}

static void test_check_path_valid_rename(const char *path, test_state_t *ts) {
    char to_path_buffer[XIPFS_PATH_MAX];

    test_check_path_rename_build_to_upper_basename(path, to_path_buffer);
    test_check_path_valid_rename_leaf(path, to_path_buffer, ts);

    test_check_path_rename_build_long_basename(path, to_path_buffer);
    test_check_path_valid_rename_leaf(path, to_path_buffer, ts);
}

static void test_check_path_valid_filename(const char *filename, test_state_t *ts) {
    if (test_check_path_valid_path(filename, ts) == false) {
        return;
    }
    if (test_check_path_valid_new_file(filename, ts) == false) {
        return;
    }
    if (test_check_path_valid_unlink(filename, ts) == false) {
        return;
    }
    if (test_check_path_valid_open(filename, ts) == false) {
        return;
    }
    test_check_path_valid_stat(filename, ts);
    test_check_path_valid_statvfs(filename, ts);
    test_check_path_valid_rename(filename, ts);
    /* There is no reason for an xipfs_unlink failure at this point,
     * since it has been alrady performed successfully above. */
    (void)xipfs_unlink(&mountpoint, filename);
}

static bool test_check_path_valid_mkdir(const char *dirname, test_state_t *ts) {
    test_check_path_verbose("test_check_path_valid_mkdir", dirname);

    const mode_t mode = 0;

    int res = xipfs_mkdir(&mountpoint, dirname, mode);
    const bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_mkdir: "
                "xipfs_mkdir(%p, \"%s\", %d) should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, dirname, (int)mode,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    return test_res;
}

static bool test_check_path_valid_opendir(const char *dirname, test_state_t *ts) {
    xipfs_dir_desc_t desc;

    test_check_path_verbose("test_check_path_valid_opendir", dirname);

    int res = xipfs_opendir(&mountpoint, &desc, dirname);
    const bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_opendir: "
                "xipfs_opendir(%p, %p, \"%s\") should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, &desc, dirname,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
    if (test_res == true) {
        (void)xipfs_closedir(&mountpoint, &desc);
    }
    return test_res;
}

static void test_check_path_valid_rmdir(const char *dirname, test_state_t *ts) {
    test_check_path_verbose("test_check_path_valid_rmdir", dirname);

    int res = xipfs_rmdir(&mountpoint, dirname);
    const bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_rmdir: "
                "xipfs_rmdir(%p, \"%s\") should have succeeded but has failed :\n"
                "\t- return value : %s\n"
                "\t- xipfs_errno : %s",
                &mountpoint, dirname,
                strerror(-res), xipfs_strerror(xipfs_errno));
    test_check_path_track_valid_case();
}

static void test_check_path_valid_dirname(const char *dirname, test_state_t *ts) {
    if (test_check_path_valid_path(dirname, ts) == false) {
        return;
    }
    if (test_check_path_valid_mkdir(dirname, ts) == false) {
        return;
    }
    if (test_check_path_valid_opendir(dirname, ts) == false) {
        return;
    }
    test_check_path_valid_stat(dirname, ts);
    test_check_path_valid_statvfs(dirname, ts);
    test_check_path_valid_rename(dirname, ts);
    test_check_path_valid_rmdir(dirname, ts);
}

static void test_check_path_case(const char *dirname,
                                 const char **prefixes, size_t prefixes_count,
                                 const char *name,
                                 const char **suffixes, size_t suffixes_count,
                                 test_check_path_callback_t test_callback,
                                 test_state_t *ts) {
    char buffer[XIPFS_PATH_MAX];

    for (size_t prefix_i = 0; prefix_i < prefixes_count; prefix_i++) {
        const char *prefix = prefixes[prefix_i];

        for (size_t suffix_i = 0; suffix_i < suffixes_count; suffix_i++) {
            const char *suffix = suffixes[suffix_i];

            int res = snprintf(buffer, XIPFS_PATH_MAX, "%s%s%s%s", dirname, prefix, name, suffix);
            bool build_res = (res > 0) && ((size_t)res < XIPFS_PATH_MAX);
            test_expect(ts, build_res == true,
                        "test_check_path_case: failed to build path "
                        "with \"%s\" \"%s\" \"%s\" \"%s\".", dirname, prefix, name, suffix);
            if (build_res == false)
                continue;

            test_callback(buffer, ts);
        }
    }
}

static void test_check_path_valid_filename_cases_base(const char *dirname, test_state_t *ts) {
    const size_t prefixes_count = sizeof(valid_filename_prefixes) / sizeof(valid_filename_prefixes[0]);
    const size_t suffixes_count = sizeof(valid_filename_suffixes) / sizeof(valid_filename_suffixes[0]);

    test_check_path_case(dirname,
                         valid_filename_prefixes, prefixes_count,
                         "file",
                         valid_filename_suffixes, suffixes_count,
                         test_check_path_valid_filename,
                         ts);
}

#define valid_dirname_prefixes valid_filename_prefixes

static const char *valid_dirname_suffixes[] = {
    "/",
    "./",
    "../",
    ".../",
};

static void test_check_path_valid_dirname_cases_base(const char *dirname, test_state_t *ts) {
    const size_t prefixes_count = sizeof(valid_dirname_prefixes) / sizeof(valid_dirname_prefixes[0]);
    const size_t suffixes_count = sizeof(valid_dirname_suffixes) / sizeof(valid_dirname_suffixes[0]);
    test_check_path_case(dirname,
                         valid_dirname_prefixes, prefixes_count,
                         "folder",
                         valid_dirname_suffixes, suffixes_count,
                         test_check_path_valid_dirname,
                         ts);
}

static void test_check_path_valid_filename_cases_base_nested_wrapper(const char *dirname,test_state_t *ts) {
    int res = xipfs_mkdir(&mountpoint, dirname, 0);
    test_expect(ts, res == 0, "test_check_path_valid_filename_cases_base_nested_wrapper: "
                "failed to make directory \"%s\".", dirname);
    if (res == 0) {
        test_check_path_valid_filename_cases_base(dirname, ts);
        (void)xipfs_rmdir(&mountpoint, dirname);
    }
}

static void test_check_path_valid_nested_cases_base(const char *dirname, test_state_t *ts) {
    const size_t prefixes_count = sizeof(valid_dirname_prefixes) / sizeof(valid_dirname_prefixes[0]);
    const size_t suffixes_count = sizeof(valid_dirname_suffixes) / sizeof(valid_dirname_suffixes[0]);

    test_check_path_case(dirname,
                         valid_dirname_prefixes, prefixes_count,
                         "DIR",
                         valid_dirname_suffixes, suffixes_count,
                         test_check_path_valid_filename_cases_base_nested_wrapper,
                         ts);
}

static inline void test_check_path_valid_cases_step(const char *dirname, test_state_t *ts) {
    test_check_path_valid_filename_cases_base(dirname, ts);
    test_check_path_valid_dirname_cases_base(dirname, ts);
    test_check_path_valid_nested_cases_base(dirname, ts);
}

static void test_check_path_valid_cases(test_state_t *ts) {
    test_check_path_valid_path("/", ts);

    const char *dirname = "/";
    test_check_path_valid_cases_step(dirname, ts);

    dirname = "/a/";
    mode_t mode = 0;
    int res = xipfs_mkdir(&mountpoint, dirname, mode);
    bool test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_cases: "
                "failed to create \"%s\" directory with mode %d.",
                dirname, (int)mode);
    if (test_res == true) {
        test_check_path_valid_cases_step(dirname, ts);

        (void)xipfs_rmdir(&mountpoint, dirname);
    }

    dirname = "/some/";
    res = xipfs_mkdir(&mountpoint, dirname, mode);
    test_res = (res == 0);
    test_expect(ts, test_res, "test_check_path_valid_cases: "
                "failed to create \"%s\" directory with mode %d.",
                dirname, (int)mode);
    if (test_res == true) {
        dirname = "/some/dir/";
        res = xipfs_mkdir(&mountpoint, dirname, mode);
        test_res = (res == 0);
        test_expect(ts, test_res, "test_check_path_valid_cases: "
                "failed to create \"%s\" directory with mode %d.",
                dirname, (int)mode);
        if (test_res == true) {
            test_check_path_valid_cases_step(dirname, ts);
            (void)xipfs_rmdir(&mountpoint, dirname);
        }

        dirname = "/some/";
        (void)xipfs_rmdir(&mountpoint, dirname);
    }
}

static void test_check_path_track_invalid_case(void) {
    tests_checks_count++;
    tests_check_invalid_checks_count++;
}

static bool test_check_path_invalid_path(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_path", path);
    const int expected_res = -1;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_xipfs_errno = XIPFS_ENULLP;
    } else if (path[0] == '\0') {
        expected_xipfs_errno = XIPFS_EEMPTY;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len >= XIPFS_PATH_MAX) {
            expected_xipfs_errno = XIPFS_ENULTER;
        }
    }

    int res = xipfs_path_check(path);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_path: "
                "xipfs_path_check(\"%s\") : "
                "return value (%d) doesn't match expectation (%d).",
                path,
                res, expected_res);
    test_check_path_track_invalid_case();
    if (test_res == false) {
        return false;
    }

    test_res = (xipfs_errno == expected_xipfs_errno);
    test_expect(ts, test_res, "test_check_path_invalid_path: "
                "xipfs_path_check(\"%s\") : "
                "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                path,
                xipfs_strerror(xipfs_errno), xipfs_errno,
                xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
    test_check_path_track_invalid_case();

    return test_res;
}

static void test_check_path_invalid_new_file(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_new_file", path);
    const xipfs_file_position_t size = 0;
    const uint32_t exec = 0;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = -EISDIR;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len - 1] == '/') {
                if (xipfs_path_check(path) == 0) {
                    /* Path is a valid dirname */
                    expected_res = -EISDIR;
                } else {
                    expected_res = -EIO;
                }
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_new_file(&mountpoint, path, size, exec);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_new_file: "
                "xipfs_new_file(%p, \"%s\", %" XIPFS_FILE_POSITION_FORMAT ", %" PRIu32 ") : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path, size, exec,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
        /* A file has been created when it should not.
         * Let's try to remove it.
         */
        (void)xipfs_unlink(&mountpoint, path);
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_new_file: "
                    "xipfs_new_file(%p, \"%s\", %" XIPFS_FILE_POSITION_FORMAT ", %" PRIu32 ") : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path, size, exec,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_unlink(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_unlink", path);

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = -EISDIR;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len - 1] == '/') {
                if (xipfs_path_check(path) == 0) {
                    /* Path is a valid dirname */
                    expected_res = -EISDIR;
                } else {
                    expected_res = -EIO;
                }
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_unlink(&mountpoint, path);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_unlink: "
                "xipfs_unlink(%p, \"%s\") : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
        /* A file has been removed when it obviously should not.
         * There is not much to do in this case, except from bailing out.
         */
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_unlink: "
                    "xipfs_unlink(%p, \"%s\") : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_open(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_open", path);
    xipfs_file_desc_t desc;
    const int flags = 0;
    const mode_t mode = 0;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = -EISDIR;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len - 1] == '/') {
                if (xipfs_path_check(path) == 0) {
                    /* Path is a valid dirname */
                    expected_res = -EISDIR;
                } else {
                    expected_res = -EIO;
                }
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_open(&mountpoint, &desc, path, flags, mode);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_open: "
                "xipfs_open(%p, %p, \"%s\", %x, %x) : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, &desc, path, flags, (int)mode,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res >= 0) {
        /* A file has been created when it should not.
         * Let's try to close and remove it.
         */
        (void)xipfs_close(&mountpoint, &desc);
        (void)xipfs_unlink(&mountpoint, path);
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_open: "
                    "xipfs_open(%p, %p, \"%s\", %x, %x) : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, &desc, path, flags, (int)mode,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_stat(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_stat", path);
    struct stat stats;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = 0;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len - 1] == '/') {
                if (xipfs_path_check(path) == 0) {
                    /* Path is a valid dirname */
                    expected_res = -EISDIR;
                } else {
                    expected_res = -EIO;
                }
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_stat(&mountpoint, path, &stats);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_stat: "
                "xipfs_stat(%p, \"%s\", %p) : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path, &stats,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_stat: "
                    "xipfs_stat(%p, \"%s\", %p) : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path, &stats,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_statvfs(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_statvfs", path);
    struct xipfs_statvfs stats;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = 0;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len - 1] == '/') {
                if (xipfs_path_check(path) == 0) {
                    /* Path is a valid dirname */
                    expected_res = -EISDIR;
                } else {
                    expected_res = -EIO;
                }
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_statvfs(&mountpoint, path, &stats);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_statvfs: "
                "xipfs_statvfs(%p, \"%s\", %p) : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path, &stats,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_statvfs: "
                    "xipfs_statvfs(%p, \"%s\", %p) : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path, &stats,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static const char *invalid_suffixes[] = {
    "//",
    "/.",
    "//.",
    "/./",
    "//./",
    "//.//",

    "/..",
    "//..",
    "/../",
    "//../",
    "//..//",

    "/b//",

    "/b/.",
    "/b//.",
    "/b/./",
    "/b//./",
    "/b//.//",

    "/b/..",
    "/b//..",
    "/b/../",
    "/b//../",
    "/b//..//",

    "//b",
    "//b/",
    "//b//",

    "//b/.",
    "//b//.",
    "//b/./",
    "//b//./",
    "//b//.//",

    "//b/..",
    "//b//..",
    "//b/../",
    "//b//../",
    "//b//..//",
};

static void test_check_path_invalid_rename_leaf(const char *path, const char *to_path,
                                                int expected_res, test_state_t *ts) {
    test_check_path_verbose_rename("test_check_path_invalid_rename", path, to_path);

    int res = xipfs_rename(&mountpoint, path, to_path);
    const int saved_xipfs_errno = xipfs_errno;
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_rename: "
                "xipfs_rename(%p, \"%s\", \"%s\") : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path, to_path,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
        /* Revert rename */
        (void)xipfs_rename(&mountpoint, to_path, path);
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        const int expected_xipfs_errno = XIPFS_EINVALP;

        test_res = (expected_xipfs_errno == saved_xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_rename: "
                    "xipfs_rename(%p, \"%s\", \"%s\") : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path, to_path,
                    xipfs_strerror(saved_xipfs_errno), saved_xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static int rename_choose_res(int expected_first_res, int expected_second_res) {
    switch(expected_first_res) {
        case 0 :
            return expected_second_res;
        case -EFAULT:
            return -EFAULT;
        case -ENOENT: {
            if (expected_second_res == -EFAULT) {
                return -EFAULT;
            }
            return -ENOENT;
        }
        case -ENAMETOOLONG: {
            if ( (expected_second_res == -EFAULT) || (expected_second_res == -ENOENT) ) {
                return expected_second_res;
            }
            return -ENAMETOOLONG;
        }
        case -EIO: {
            if (   (expected_second_res == -EFAULT) || (expected_second_res == -ENOENT)
                || (expected_second_res == -ENAMETOOLONG) ) {
                return expected_second_res;
            }
            return -EIO;
        }
        default :
            return INT_MIN;
    }
}

static void
test_check_path_invalid_rename_long_path(char *buffer, size_t buffer_size,
                                         const char *path, int expected_res,
                                         test_state_t *ts) {
    const size_t invalid_suffixes_count =
        sizeof(invalid_suffixes) / sizeof(invalid_suffixes[0]);
    const char *too_long_path_prefixes[] = {
        "/",
        "//",
    };
    const size_t too_long_path_prefixes_count =
        sizeof(too_long_path_prefixes) / sizeof(too_long_path_prefixes[0]);
    for (size_t prefix_i = 0; prefix_i < too_long_path_prefixes_count; prefix_i++) {
        const char *prefix = too_long_path_prefixes[prefix_i];
        const size_t prefix_len = strlen(prefix);

        size_t char_i;
        /* Copy prefix */
        for (char_i = 0; char_i < prefix_len; char_i++)
            buffer[char_i] = prefix[char_i];

        const size_t char_i_backup = char_i;
        for (size_t suffix_i = 0; suffix_i < invalid_suffixes_count; suffix_i++) {
            const char *suffix = invalid_suffixes[suffix_i];
            const size_t suffix_len = strlen(suffix);

            char_i = char_i_backup;

            /* Fill with padding */
            const size_t padding_count = XIPFS_PATH_MAX - suffix_len;
            for (size_t padding_i = char_i_backup;
                 (padding_i < padding_count) && (char_i < (buffer_size));
                 padding_i++, char_i++) {
                buffer[char_i] = 'A' + (padding_i % 26);
            }

            /* Copy suffix */
            for (size_t suffix_char_i = 0;
                 (suffix_char_i < suffix_len) && (char_i < buffer_size);
                 suffix_char_i++, char_i++) {
                buffer[char_i] = suffix[suffix_char_i];
            }

            buffer[buffer_size - 1] = '\0';
            test_check_path_invalid_rename_leaf(path, buffer, expected_res, ts);
        }
    }
}

static void test_check_path_invalid_rename_ex(const char *path, int expected_res, test_state_t *ts) {
    test_check_path_invalid_rename_leaf(path, NULL,
                                        rename_choose_res(expected_res, -EFAULT),
                                        ts);
    test_check_path_invalid_rename_leaf(path, "",
                                        rename_choose_res(expected_res, -ENOENT),
                                        ts);
    test_check_path_invalid_rename_leaf(path, "/",
                                        rename_choose_res(expected_res, 0),
                                        ts);
    test_check_path_invalid_rename_leaf(path, "/ok",
                                        rename_choose_res(expected_res, 0),
                                        ts);
    test_check_path_invalid_rename_leaf(path, "/ok/",
                                        rename_choose_res(expected_res, 0),
                                        ts);
    const char *invalid_dirnames[] = {
        "//",
        "/ok//",
        "//ok",
        "//ok/",
        "//ok//",
    };
    const size_t invalid_dirnames_count = sizeof(invalid_dirnames) / sizeof(invalid_dirnames[0]);
    for (size_t i = 0; i < invalid_dirnames_count; i++) {
        const char *invalid_dirname = invalid_dirnames[i];

        test_check_path_invalid_rename_leaf(path, invalid_dirname,
                                            rename_choose_res(expected_res, -EIO),
                                            ts);
    }
    char buffer[XIPFS_PATH_MAX + 1];
    test_check_path_invalid_rename_long_path(buffer, XIPFS_PATH_MAX + 1,
                                             path,
                                             rename_choose_res(expected_res, -ENAMETOOLONG),
                                             ts);
    test_check_path_invalid_rename_long_path(buffer, XIPFS_PATH_MAX,
                                             path,
                                             rename_choose_res(expected_res, -EIO),
                                             ts);
}
static void test_check_path_invalid_rename(const char *path, test_state_t *ts) {
    int expected_res = 0;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = 0;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (xipfs_path_check(path) < 0) {
                expected_res = -EIO;
            } else {
                expected_res = 0;
            }
        }
    }

    test_check_path_invalid_rename_ex(path, expected_res, ts);
}

static void test_check_path_invalid_mkdir(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_mkdir", path);
    const mode_t mode = 0;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = -EEXIST;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            expected_res = -EIO;
        }
    }

    int res = xipfs_mkdir(&mountpoint, path, mode);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_mkdir: "
                "xipfs_mkdir(%p, \"%s\", %d) : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path, (int)mode,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
        /* A directory has been created when it should not.
         * Let's try to remove it.
         */
        (void)xipfs_rmdir(&mountpoint, path);
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_mkdir: "
                    "xipfs_mkdir(%p, \"%s\", %d) : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path, (int)mode,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_opendir(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_opendir", path);
    xipfs_dir_desc_t desc;

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            expected_res = -EIO;
        }
    }

    int res = xipfs_opendir(&mountpoint, &desc, path);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_opendir: "
                "xipfs_opendir(%p, %p, \"%s\") : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, &desc, path,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
        /* A directory has been opened when it should not.
         * Let's try to close and remove it.
         */
        (void)xipfs_closedir(&mountpoint, &desc);
        (void)xipfs_rmdir(&mountpoint, path);
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_opendir: "
                    "xipfs_opendir(%p, %p, \"%s\") : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, &desc, path,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid_rmdir(const char *path, test_state_t *ts) {
    test_check_path_verbose("test_check_path_invalid_rmdir", path);

    int expected_res = 0;
    int expected_xipfs_errno = XIPFS_EINVALP;

    if (path == NULL) {
        expected_res = -EFAULT;
    } else if (path[0] == '\0') {
        expected_res = -ENOENT;
    } else if ( (path[0] == '/') && (path[1] == '\0') ) {
        expected_res = -EBUSY;
    } else {
        size_t len = strnlen(path, XIPFS_PATH_MAX);
        if (len == XIPFS_PATH_MAX) {
            expected_res = -ENAMETOOLONG;
        } else {
            if (path[len-1] == '.') {
                expected_res = -EINVAL;
            } else {
                expected_res = -EIO;
            }
        }
    }

    int res = xipfs_rmdir(&mountpoint, path);
    bool test_res = (res == expected_res);
    test_expect(ts, test_res, "test_check_path_invalid_rmdir: "
                "xipfs_rmdir(%p, \"%s\") : "
                "return value (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                &mountpoint, path,
                strerror(-res), res,
                strerror(-expected_res), expected_res);
    test_check_path_track_invalid_case();
    if (res == 0) {
         /* A directory has been removed when it should not.
         * There is not much to do in this case, except from bailing out.
         */
        return;
    }
    if (test_res == false) {
        return;
    }

    if (res == -EIO) {
        test_res = (expected_xipfs_errno == xipfs_errno);
        test_expect(ts, test_res, "test_check_path_invalid_rmdir: "
                    "xipfs_rmdir(%p, \"%s\") : "
                    "xipfs_errno (\"%s\" aka %d) doesn't match expectation (\"%s\" aka %d).",
                    &mountpoint, path,
                    xipfs_strerror(xipfs_errno), xipfs_errno,
                    xipfs_strerror(expected_xipfs_errno), expected_xipfs_errno);
        test_check_path_track_invalid_case();
    }
}

static void test_check_path_invalid(const char *path, test_state_t *ts) {
    const bool res = test_check_path_invalid_path(path, ts);
    if (res == false) {
        /* When test_check_path_invalid_path has failed, there is no need
         * to go further, since all following functions should rely on
         * xipfs_path_check at a moment or another.
         */
        return;
    }

    test_check_path_invalid_new_file(path, ts);
    test_check_path_invalid_unlink(path, ts);
    test_check_path_invalid_open(path, ts);
    test_check_path_invalid_stat(path, ts);
    test_check_path_invalid_statvfs(path, ts);
    test_check_path_invalid_rename(path, ts);
    test_check_path_invalid_mkdir(path, ts);
    test_check_path_invalid_opendir(path, ts);
    test_check_path_invalid_rmdir(path, ts);
}

static void test_check_path_invalid_path_no_return_wrapper(const char *path, test_state_t *ts) {
    (void)test_check_path_invalid_path(path, ts);
}

static void test_check_path_invalid_path_no_starting_slash_cases(test_state_t *ts) {
    const size_t suffixes_count = sizeof(invalid_suffixes) / sizeof(invalid_suffixes[0]);

    const char *prefixes[] = {
        ".",
        "..",
        "./",
        "../",
        ".//",
        "..//",
    };
    const size_t prefixes_count = sizeof(prefixes) / sizeof(prefixes[0]);

    test_check_path_case("",
                         prefixes, prefixes_count,
                         "A",
                         invalid_suffixes, suffixes_count,
                         test_check_path_invalid_path_no_return_wrapper,
                         ts);
}

static void test_check_path_invalid_cases_base(const char *dirname, test_state_t *ts) {
    const size_t suffixes_count = sizeof(invalid_suffixes) / sizeof(invalid_suffixes[0]);

    const char *prefixes[] = {""};
    const size_t prefixes_count = sizeof(prefixes) / sizeof(prefixes[0]);

    test_check_path_case(dirname,
                         prefixes, prefixes_count,
                         "a",
                         invalid_suffixes, suffixes_count,
                         test_check_path_invalid,
                         ts);
}

static void
test_check_path_invalid_cases_with_valid_nested_dir(const char *dirname, test_state_t *ts) {
    const size_t prefixes_count = sizeof(valid_dirname_prefixes) / sizeof(valid_dirname_prefixes[0]);
    const size_t suffixes_count = sizeof(valid_dirname_suffixes) / sizeof(valid_dirname_suffixes[0]);

    test_check_path_case(dirname,
                         valid_dirname_prefixes, prefixes_count,
                         "FOLDER",
                         valid_dirname_suffixes, suffixes_count,
                         test_check_path_invalid_cases_base,
                         ts);
}

static void test_check_path_invalid_cases_no_nt_char(const char *dirname, test_state_t *ts) {
    char buffer[XIPFS_PATH_MAX + 1];
    int i;

    for (i = 0; (i < XIPFS_PATH_MAX) && (dirname[i] != '\0'); i++) {
        buffer[i] = dirname[i];
    }

    for (; i < XIPFS_PATH_MAX; i++)
        buffer[i] = 'A' + (i % 26);

    buffer[XIPFS_PATH_MAX] = '\0';
    test_check_path_invalid(buffer, ts);
}

static void test_check_path_invalid_cases_step(const char *dirname, test_state_t *ts) {
    test_check_path_invalid_cases_base(dirname, ts);
    test_check_path_invalid_cases_with_valid_nested_dir(dirname, ts);
}

static void test_check_path_invalid_cases(test_state_t *ts) {
    test_check_path_invalid(NULL, ts);
    test_check_path_invalid("", ts);

    test_check_path_invalid_path_no_starting_slash_cases(ts);

    test_check_path_invalid_cases_step("/", ts);
    test_check_path_invalid_cases_no_nt_char("/", ts);

    test_check_path_invalid_cases_step("//", ts);
    /* Cannot test no_nt_char because all paths will start with an invalid path*/

    const char *dirname = "/some/";
    int res = xipfs_mkdir(&mountpoint, dirname, 0);
    bool test_res = (res == 0);
    test_expect(ts, "test_check_path_invalid_cases : "
                "failed to create \"%\" directory.",
                dirname);
    if (test_res == true) {
        dirname = "/some/dir/";
        res = xipfs_mkdir(&mountpoint, dirname, 0);
        test_res = (res == 0);
        test_expect(ts, "test_check_path_invalid_cases : "
                    "failed to create \"%\" directory.",
                    dirname);
        if (test_res == true) {
            test_check_path_invalid_cases_step(dirname, ts);
            test_check_path_invalid_cases_no_nt_char(dirname, ts);
            (void)xipfs_rmdir(&mountpoint, dirname);
        }

        dirname = "/some/";
        (void)xipfs_rmdir(&mountpoint, dirname);
    }

    // TODO Test paths with invalid characters ? Antislash tests ?
}

static int allocate_and_initialize_mounpoint(void) {
    const size_t nvm_bytesize =
        xipfs_workstation_nvm_numof * xipfs_workstation_nvm_page_size;
    void *nvm = NULL;
    int res = posix_memalign(&nvm, xipfs_workstation_nvm_page_size, nvm_bytesize);
    if ( (res != 0)  || (nvm == NULL) ) {
        fprintf(stderr, "cmd_test_check_path: failed to allocate %zu bytes of NVM (%d->%s).\n",
                nvm_bytesize, res, strerror(res));
        return 1;
    }
    xipfs_workstation_nvm_base = (uintptr_t)nvm;

    init_mountpoint(&mountpoint, nvm_bytesize);

    int ret = xipfs_format(&mountpoint);
    if (ret < 0) {
        fprintf(stderr, "cmd_test_check_path: xipfs_format failed: %s\n", strerror(-ret));
        if (xipfs_errno != XIPFS_OK) {
            fprintf(stderr, "xipfs errno: %s\n", xipfs_strerror(xipfs_errno));
        }
        free(nvm);
        xipfs_workstation_nvm_base = 0;
        return 1;
    }

    ret = xipfs_mount(&mountpoint);
    if (ret < 0) {
        fprintf(stderr, "cmd_test_check_path: xipfs_mount failed: %s\n", strerror(-ret));
        if (xipfs_errno != XIPFS_OK) {
            fprintf(stderr, "xipfs errno: %s\n", xipfs_strerror(xipfs_errno));
        }
        free(nvm);
        xipfs_workstation_nvm_base = 0;
        return 1;
    }

    return 0;
}

static void release_mountpoint(void) {
    (void)xipfs_umount(&mountpoint);

    if (xipfs_workstation_nvm_base != (uintptr_t)NULL) {
        free((void *)xipfs_workstation_nvm_base);
    }
}

int cmd_test_check_path(int argc, char **argv)
{
    if (argc > 1) {
        fprintf(stderr, "Error: test_check_path accepts only one optional extra argument [verbose].\n");
        return 1;
    }

    verbose = false;

    if (argc == 1) {
        if (strcmp(argv[0], "verbose") != 0) {
            fprintf(stderr, "Error: test_check_path accepts only \"verbose\" as extra argument.\n");
            return 1;
        }
        verbose = true;
    }

    int res = allocate_and_initialize_mounpoint();
    if (res != 0)
        return res;

    test_state_t ts = {0, 0};
    tests_checks_count = 0;
    tests_check_valid_checks_count = 0;
    tests_check_invalid_checks_count = 0;

    test_check_path_valid_cases(&ts);
    test_check_path_invalid_cases(&ts);

    release_mountpoint();

    fprintf(stdout, "Framework internal tests count : %zu.\n", ts.total - tests_checks_count);
    fprintf(stdout, "Actual tests count : %zu (%zu valid ones, %zu invalid ones).\n",
            tests_checks_count, tests_check_valid_checks_count, tests_check_invalid_checks_count);
    return test_report(&ts);
}
