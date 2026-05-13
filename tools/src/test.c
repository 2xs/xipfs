#include "mkxipfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test.h"

static int test_fill_buffer(char *buf, size_t n, char seed)
{
    size_t i;
    if (buf == NULL) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        buf[i] = (char)(seed + (char)(i % 23));
    }
    return 0;
}

int cmd_test(void)
{
    test_state_t ts = {0, 0};
    char tmp_template[] = "/tmp/mkxipfs-test-XXXXXX";
    app_ctx_t ctx;
    int tmp_fd;
    int ret;

    tmp_fd = mkstemp(tmp_template);
    /* Test setup: create a temporary flash image file for the main test suite. */
    test_expect(&ts, tmp_fd >= 0, "mkstemp should succeed");
    if (tmp_fd < 0) {
        fprintf(stderr, "Cannot create temp file for tests.\n");
        return 1;
    }
    (void)close(tmp_fd);

    ret = create_image(tmp_template, "128k");
    /* Test setup: format a fresh xipfs image with enough room for all tests. */
    test_expect(&ts, ret == 0, "create_image should succeed");
    if (ret != 0) {
        (void)unlink(tmp_template);
        fprintf(stderr, "Test aborted: unable to create test image.\n");
        return 1;
    }

    ret = open_image(&ctx, tmp_template, true);
    /* Test setup: mount the formatted image in read-write mode. */
    test_expect(&ts, ret == 0, "open_image writable should succeed");
    if (ret != 0) {
        (void)unlink(tmp_template);
        fprintf(stderr, "Test aborted: unable to open test image.\n");
        return 1;
    }

    {
        /* Filesystem metadata test: statvfs must work on a new filesystem. */
        struct xipfs_statvfs vfs;
        ret = xipfs_statvfs(&ctx.mount, "/", &vfs);
        test_expect(&ts, ret == 0, "xipfs_statvfs should succeed on fresh fs");
        if (ret == 0) {
            test_expect(&ts, vfs.f_bfree == vfs.f_blocks, "fresh fs should have all blocks free");
        }
    }

    {
        /* Negative open test: opening a missing file must fail with -ENOENT. */
        xipfs_file_desc_t d;
        memset(&d, 0, sizeof(d));
        ret = xipfs_open(&ctx.mount, &d, "/missing", O_RDONLY, 0);
        test_expect(&ts, ret == -ENOENT, "open missing file should return -ENOENT");
    }

    /* Directory creation tests: create nested directories and reject duplicates. */
    ret = xipfs_mkdir(&ctx.mount, "/dir", 0);
    test_expect(&ts, ret == 0, "mkdir /dir should succeed");
    ret = xipfs_mkdir(&ctx.mount, "/dir/sub", 0);
    test_expect(&ts, ret == 0, "mkdir /dir/sub should succeed");
    ret = xipfs_mkdir(&ctx.mount, "/dir", 0);
    test_expect(&ts, ret == -EEXIST, "mkdir existing /dir should return -EEXIST");

    {
        /* Directory iteration test: opendir/readdir should expose immediate children. */
        xipfs_dir_desc_t dd;
        xipfs_dirent_t de;
        int seen_sub = 0;
        ret = xipfs_opendir(&ctx.mount, &dd, "/dir");
        test_expect(&ts, ret == 0, "opendir /dir should succeed");
        if (ret == 0) {
            while ((ret = xipfs_readdir(&ctx.mount, &dd, &de)) > 0) {
                if (strcmp(de.dirname, "sub/") == 0) {
                    seen_sub = 1;
                }
            }
            test_expect(&ts, ret == 0, "readdir should terminate with 0");
            test_expect(&ts, seen_sub == 1, "readdir(/dir) should expose sub/");
            ret = xipfs_closedir(&ctx.mount, &dd);
            test_expect(&ts, ret == 0, "closedir should succeed");
        }
    }

    {
        /* File I/O test: create, write, close, reopen, read back, then hit EOF. */
        xipfs_file_desc_t d;
        const char *msg = "hello";
        char got[16];
        memset(&d, 0, sizeof(d));
        ret = xipfs_open(&ctx.mount, &d, "/dir/sub/f.txt", O_CREAT | O_WRONLY, 0);
        test_expect(&ts, ret == 0, "open create /dir/sub/f.txt should succeed");
        if (ret == 0) {
            ssize_t w = xipfs_write(&ctx.mount, &d, msg, strlen(msg));
            test_expect(&ts, w == (ssize_t)strlen(msg), "write full payload should succeed");
            ret = xipfs_close(&ctx.mount, &d);
            test_expect(&ts, ret == 0, "close after write should succeed");
        }

        memset(&d, 0, sizeof(d));
        ret = xipfs_open(&ctx.mount, &d, "/dir/sub/f.txt", O_RDONLY, 0);
        test_expect(&ts, ret == 0, "open existing file for read should succeed");
        if (ret == 0) {
            ssize_t r = xipfs_read(&ctx.mount, &d, got, sizeof(got));
            test_expect(&ts, r == (ssize_t)strlen(msg), "read should return file size");
            if (r > 0) {
                got[r] = '\0';
                test_expect(&ts, strcmp(got, msg) == 0, "read data should match written payload");
            }
            r = xipfs_read(&ctx.mount, &d, got, sizeof(got));
            test_expect(&ts, r == 0, "second read should return EOF (0)");
            ret = xipfs_close(&ctx.mount, &d);
            test_expect(&ts, ret == 0, "close after read should succeed");
        }
    }

    {
        /* Stat test: verify file metadata (at least size) on an existing file. */
        struct stat st;
        ret = xipfs_stat(&ctx.mount, "/dir/sub/f.txt", &st);
        test_expect(&ts, ret == 0, "stat on existing file should succeed");
        if (ret == 0) {
            test_expect(&ts, st.st_size == 5, "file size should be 5 bytes");
        }
    }

    {
        /* Seek boundary tests: negative seek must fail, seek beyond EOF then read must return 0. */
        xipfs_file_desc_t d;
        memset(&d, 0, sizeof(d));
        ret = xipfs_open(&ctx.mount, &d, "/dir/sub/f.txt", O_RDONLY, 0);
        test_expect(&ts, ret == 0, "open for lseek tests should succeed");
        if (ret == 0) {
            off_t p = xipfs_lseek(&ctx.mount, &d, -1, SEEK_SET);
            test_expect(&ts, p < 0, "lseek negative position should fail");
            p = xipfs_lseek(&ctx.mount, &d, 100, SEEK_SET);
            test_expect(&ts, p == 100, "lseek beyond EOF should succeed");
            {
                char c;
                ssize_t r = xipfs_read(&ctx.mount, &d, &c, 1);
                test_expect(&ts, r == 0, "read beyond EOF should return 0");
            }
            ret = xipfs_close(&ctx.mount, &d);
            test_expect(&ts, ret == 0, "close after lseek tests should succeed");
        }
    }

    /* Rename tests: rename inside one directory, then move across directories. */
    ret = xipfs_rename(&ctx.mount, "/dir/sub/f.txt", "/dir/sub/g.txt");
    test_expect(&ts, ret == 0, "rename within same directory should succeed");
    ret = xipfs_rename(&ctx.mount, "/dir/sub/g.txt", "/dir/g2.txt");
    test_expect(&ts, ret == 0, "rename across directories should succeed");

    /* Deletion tests: directory/file semantics and non-empty directory protection. */
    ret = xipfs_unlink(&ctx.mount, "/dir");
    test_expect(&ts, ret == -EISDIR, "unlink on directory should fail with -EISDIR");
    ret = xipfs_rmdir(&ctx.mount, "/dir");
    test_expect(&ts, ret == -ENOTEMPTY, "rmdir non-empty directory should fail with -ENOTEMPTY");

    ret = xipfs_unlink(&ctx.mount, "/dir/g2.txt");
    test_expect(&ts, ret == 0, "unlink regular file should succeed");
    ret = xipfs_rmdir(&ctx.mount, "/dir/sub");
    test_expect(&ts, ret == 0, "rmdir empty subdirectory should succeed");
    ret = xipfs_rmdir(&ctx.mount, "/dir");
    test_expect(&ts, ret == 0, "rmdir now-empty directory should succeed");

    {
        /* Capacity-per-file test: writes larger than reserved space must eventually stop. */
        xipfs_file_desc_t d;
        size_t i;
        ssize_t w = 0;
        char *big = malloc(20000);
        test_expect(&ts, big != NULL, "allocate large write buffer should succeed");
        if (big != NULL) {
            (void)test_fill_buffer(big, 20000, 'A');
            memset(&d, 0, sizeof(d));
            ret = xipfs_open(&ctx.mount, &d, "/big", O_CREAT | O_WRONLY, 0);
            test_expect(&ts, ret == 0, "open /big should succeed");
            if (ret == 0) {
                w = xipfs_write(&ctx.mount, &d, big, 20000);
                test_expect(&ts, w > 0 && w < 20000, "write bigger than reserved should be short");
                for (i = 0; i < 4; i++) {
                    w = xipfs_write(&ctx.mount, &d, big, 20000);
                    if (w == 0) {
                        break;
                    }
                }
                test_expect(&ts, w == 0, "eventually write should hit max file position and return 0");
                ret = xipfs_close(&ctx.mount, &d);
                test_expect(&ts, ret == 0, "close /big should succeed");
            }
            free(big);
        }
    }

    {
        /* Filesystem saturation test: keep creating files until quota is exhausted. */
        int created = 0;
        for (int idx = 0; idx < 1024; idx++) {
            char path[32];
            ret = snprintf(path, sizeof(path), "/full_%03d", idx);
            if (ret <= 0 || (size_t)ret >= sizeof(path)) {
                break;
            }
            ret = xipfs_new_file(&ctx.mount, path, 0, 0);
            if (ret == 0) {
                created++;
                continue;
            }
            test_expect(&ts, ret == -EDQUOT, "filesystem full should return -EDQUOT on new_file");
            break;
        }
        test_expect(&ts, created > 0, "fill test should create at least one file");
    }

    {
        /* Regression test for mv/rename:
         * In a nearly full image with empty directories, moving a file across
         * directories must not corrupt the filesystem (mount must still work). */
        char reg_template[] = "/tmp/mkxipfs-test-mv-XXXXXX";
        int reg_fd = mkstemp(reg_template);
        app_ctx_t reg_ctx;
        struct stat st_new;
        struct stat st_old;
        const char payload[] = "x";
        xipfs_file_desc_t d;
        int reopen_ret;

        test_expect(&ts, reg_fd >= 0, "mv regression: mkstemp should succeed");
        if (reg_fd >= 0) {
            (void)close(reg_fd);
            ret = create_image(reg_template, "20k");
            test_expect(&ts, ret == 0, "mv regression: create minimal image should succeed");
            if (ret == 0) {
                ret = open_image(&reg_ctx, reg_template, true);
                test_expect(&ts, ret == 0, "mv regression: open image should succeed");
                if (ret == 0) {
                    ret = xipfs_mkdir(&reg_ctx.mount, "/dirA", 0);
                    test_expect(&ts, ret == 0, "mv regression: mkdir /dirA should succeed");
                    ret = xipfs_mkdir(&reg_ctx.mount, "/dirB", 0);
                    test_expect(&ts, ret == 0, "mv regression: mkdir /dirB should succeed");

                    memset(&d, 0, sizeof(d));
                    ret = xipfs_open(&reg_ctx.mount, &d, "/dirA/a.txt", O_CREAT | O_WRONLY, 0);
                    test_expect(&ts, ret == 0, "mv regression: create /dirA/a.txt should succeed");
                    if (ret == 0) {
                        ssize_t w = xipfs_write(&reg_ctx.mount, &d, payload, sizeof(payload) - 1);
                        test_expect(&ts, w == (ssize_t)(sizeof(payload) - 1),
                                    "mv regression: write /dirA/a.txt should succeed");
                        ret = xipfs_close(&reg_ctx.mount, &d);
                        test_expect(&ts, ret == 0, "mv regression: close /dirA/a.txt should succeed");
                    }

                    ret = xipfs_rename(&reg_ctx.mount, "/dirA/a.txt", "/dirB/a.txt");
                    test_expect(&ts, ret == 0, "mv regression: cross-directory rename should succeed");

                    close_image(&reg_ctx);

                    reopen_ret = open_image(&reg_ctx, reg_template, false);
                    test_expect(&ts, reopen_ret == 0, "mv regression: image should remount after rename");
                    if (reopen_ret == 0) {
                        ret = xipfs_stat(&reg_ctx.mount, "/dirB/a.txt", &st_new);
                        test_expect(&ts, ret == 0, "mv regression: destination file should exist after remount");
                        ret = xipfs_stat(&reg_ctx.mount, "/dirA/a.txt", &st_old);
                        test_expect(&ts, ret == -ENOENT, "mv regression: source file should be gone after remount");
                        close_image(&reg_ctx);
                    }
                }
            }
            (void)unlink(reg_template);
        }
    }

    /* Test teardown: unmount and remove the temporary main test image. */
    close_image(&ctx);
    (void)unlink(tmp_template);

    return test_report(&ts);
}
