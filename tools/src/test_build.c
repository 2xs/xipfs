#include "mkxipfs.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "test.h"

#define BUFFER_SIZE 256

typedef enum progression_e {
    PROGRESSION_ROOT_DIRECTORY = 0,
    PROGRESSION_EMPTY_DIRECTORY,
    PROGRESSION_A,
    PROGRESSION_A_EMPTY_FILE,
    PROGRESSION_A_1_PAGE_FILE,
    PROGRESSION_A_2_PAGES_FILE,
    PROGRESSION_B,
    PROGRESSION_B_NESTED,
    PROGRESSION_B_NESTED_DIRECTORY,
    PROGRESSION_ROOT_EMPTY_FILE,
    PROGRESSION_ROOT_ONE_PAGE_FILE,
    PROGRESSION_ROOT_TWO_PAGES_FILE,
} progression_t;

static void cleanup(const char *root_dir_filename, progression_t progression) {
    char buffer[BUFFER_SIZE];

    if (progression >= PROGRESSION_ROOT_TWO_PAGES_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/root_2_pages_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_ROOT_ONE_PAGE_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/root_1_page_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_ROOT_EMPTY_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/root_empty_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_B_NESTED_DIRECTORY) {
        (void)snprintf(buffer, sizeof(buffer), "%s/B/nested/directory", root_dir_filename);
        (void)rmdir(buffer);
    }
    if (progression >= PROGRESSION_B_NESTED) {
        (void)snprintf(buffer, sizeof(buffer), "%s/B/nested", root_dir_filename);
        (void)rmdir(buffer);
    }
    if (progression >= PROGRESSION_B) {
        (void)snprintf(buffer, sizeof(buffer), "%s/B", root_dir_filename);
        (void)rmdir(buffer);
    }
    if (progression >= PROGRESSION_A_2_PAGES_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/A/2_pages_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_A_1_PAGE_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/A/1_page_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_A_EMPTY_FILE) {
        (void)snprintf(buffer, sizeof(buffer), "%s/A/empty_file", root_dir_filename);
        (void)unlink(buffer);
    }
    if (progression >= PROGRESSION_A) {
        (void)snprintf(buffer, sizeof(buffer), "%s/A", root_dir_filename);
        (void)rmdir(buffer);
    }
    if (progression >= PROGRESSION_EMPTY_DIRECTORY) {
        (void)snprintf(buffer, sizeof(buffer), "%s/empty_directory", root_dir_filename);
        (void)rmdir(buffer);
    }
    (void)rmdir(root_dir_filename);
}

static int test_write_file(int fd, size_t filesize) {
    char write_buffer[128];
    const size_t nb_buffers      = filesize / sizeof(write_buffer);
    const size_t remaining_bytes = filesize % sizeof(write_buffer);
    size_t i;
    ssize_t res;
    for (i = 0; i < sizeof(write_buffer); i++) {
        write_buffer[i] = (char)i;
    }
    for (i = 0; i < nb_buffers; i++) {
        res = write(fd, write_buffer, sizeof(write_buffer));
        if ((res < 0) || ((size_t)res != sizeof(write_buffer))) {
            fprintf(stderr, "Failed to write %zuth block of bytes : %s.\n", i, strerror(errno));
            return -1;
        }
    }
    res = write(fd, write_buffer, remaining_bytes);
    if ((res < 0) || ((size_t)res != remaining_bytes)) {
        fprintf(stderr, "Failed to write %zu remaining bytes : %s.\n", i, strerror(errno));
        return -1;
    }
    return 0;
}

int cmd_test_build(void)
{
    const size_t one_page_file_size = XIPFS_NVM_PAGE_SIZE - sizeof(xipfs_file_t);
    const size_t two_pages_file_size = (XIPFS_NVM_PAGE_SIZE * 2) - sizeof(xipfs_file_t);
    char buffer[256];
    int ret;
    size_t expected_flash_size = 0;

    /* Set up the filesystem hierarchy to be flashed */
    char tmp_root_dir_template[] = "/tmp/mkxipfs-test-build-dir-XXXXXX";
    char *tmp_root_dir_filename = mkdtemp(tmp_root_dir_template);
    if (tmp_root_dir_filename == NULL) {
        fprintf(stderr, "Cannot create temporary directory for build test.\n");
        return 1;
    }
    /* Empty directory */
    ret = snprintf(buffer, sizeof(buffer), "%s/empty_directory", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct empty directory filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_DIRECTORY);
        return 1;
    }
    ret = mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create empty_directory : %s.", strerror(errno));
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_DIRECTORY);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* A directory */
    ret = snprintf(buffer, sizeof(buffer), "%s/A", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct A directory filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_EMPTY_DIRECTORY);
        return 1;
    }
    ret = mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create A directory.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_EMPTY_DIRECTORY);
        return 1;
    }
    /* A/empty_file
     * Memory footprint in XIPFS is equal to one page size */
    ret = snprintf(buffer, sizeof(buffer), "%s/A/empty_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct A/empty_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A);
        return 1;
    }
    ret = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create A/empty_file : %s.\n", strerror(errno));
        cleanup(tmp_root_dir_filename, PROGRESSION_A);
        return 1;
    }
    (void)close(ret);
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* A/1_page_file
     * Memory footprint in XIPFS is equal to one page size */
    ret = snprintf(buffer, sizeof(buffer), "%s/A/1_page_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct A/1_page_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_EMPTY_FILE);
        return 1;
    }
    int fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Failed to create A/1_page_file : %s.\n", strerror(errno));
        cleanup(tmp_root_dir_filename, PROGRESSION_A_EMPTY_FILE);
        return 1;
    }
    ret = test_write_file(fd, one_page_file_size);
    (void)close(fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to write A/1_page_file bytes.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_1_PAGE_FILE);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* A/2_pages_file
     * Memory footprint in XIPFS is equal to two page size */
    ret = snprintf(buffer, sizeof(buffer), "%s/A/2_pages_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct A/2_pages_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_1_PAGE_FILE);
        return 1;
    }
    fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Failed to create A/2_pages_file : %s.\n", strerror(errno));
        cleanup(tmp_root_dir_filename, PROGRESSION_A_1_PAGE_FILE);
        return 1;
    }
    ret = test_write_file(fd, two_pages_file_size);
    (void)close(fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to write A/2_pages_file bytes.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_2_PAGES_FILE);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE * 2;
    /* B directory */
    ret = snprintf(buffer, sizeof(buffer), "%s/B", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct B directory filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_2_PAGES_FILE);
        return 1;
    }
    ret = mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create B directory.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_A_2_PAGES_FILE);
        return 1;
    }
    /* B/nested */
    ret = snprintf(buffer, sizeof(buffer), "%s/B/nested", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct B/nested directory filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B);
        return 1;
    }
    ret = mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create B/nested directory.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B);
        return 1;
    }
    /* B/nested/directory */
    ret = snprintf(buffer, sizeof(buffer), "%s/B/nested/directory", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct B/nested/directory filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B_NESTED);
        return 1;
    }
    ret = mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret < 0) {
        fprintf(stderr, "Failed to create B/nested/directory.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B_NESTED);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* root_empty_file */
    ret = snprintf(buffer, sizeof(buffer), "%s/root_empty_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct root_empty_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B_NESTED_DIRECTORY);
        return 1;
    }
    fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Failed to create root_empty_file.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_B_NESTED_DIRECTORY);
        return 1;
    }
    (void)close(fd);
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* root_1_page_file */
    ret = snprintf(buffer, sizeof(buffer), "%s/root_1_page_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct root_1_page_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_EMPTY_FILE);
        return 1;
    }
    fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Failed to create root_1_page_file.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_EMPTY_FILE);
        return 1;
    }
    ret = test_write_file(fd, one_page_file_size);
    (void)close(fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to write root_1_page_file bytes.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_ONE_PAGE_FILE);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE;
    /* root_2_pages_file */
    ret = snprintf(buffer, sizeof(buffer), "%s/root_2_pages_file", tmp_root_dir_filename);
    if ((ret < 0) || ((size_t)ret >= sizeof(buffer))) {
        fprintf(stderr, "Failed to construct root_2_pages_file filename.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_ONE_PAGE_FILE);
        return 1;
    }
    fd = open(buffer, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        fprintf(stderr, "Failed to create root_2_pages_file.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_ONE_PAGE_FILE);
        return 1;
    }
    ret = test_write_file(fd, two_pages_file_size);
    (void)close(fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to write root_2_pages_file bytes.\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_TWO_PAGES_FILE);
        return 1;
    }
    expected_flash_size += XIPFS_NVM_PAGE_SIZE * 2;

    /* Build the flash from the filesystem hierarchy */
    test_state_t ts = {0, 0};
    char tmp_flash_template[] = "/tmp/mkxipfs-test-build-flash-XXXXXX";
    ret = mkstemp(tmp_flash_template);
    if (ret < 0) {
        fprintf(stderr, "Failed to create temporary flash file\n");
        cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_TWO_PAGES_FILE);
        return 1;
    }
    (void)close(ret);
    {
        char *args[] = {
            tmp_flash_template,
            tmp_root_dir_filename
        };
        ret = cmd_build(sizeof(args) / sizeof(args[0]), args);
        (void)snprintf(buffer, sizeof(buffer), "build %s %s should succeed.", args[0], args[1]);
    }
    test_expect(&ts, ret >= 0, buffer);
    if (ret < 0) {
        goto teardown;
    }
    struct stat stats;
    ret = stat(tmp_flash_template, &stats);
    (void)snprintf(buffer, sizeof(buffer), "stat on %s should succeed.", tmp_flash_template);
    test_expect(&ts, ret >= 0, buffer);
    (void)snprintf(buffer, sizeof(buffer),
                   "expected size %zu bytes differs from flash stats st_size %jd.",
                   expected_flash_size, (intmax_t)stats.st_size);
    test_expect(&ts, expected_flash_size == (size_t)stats.st_size, buffer);
    if (expected_flash_size != (size_t)stats.st_size) {
        goto teardown;
    }

    /* Check file sizes */
    {
        app_ctx_t ctx;
        ret = open_image(&ctx, tmp_flash_template, false);
        test_expect(&ts, ret >= 0, "opening temporary flash file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/root_empty_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /root_empty_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/root_empty_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, (size_t)0);
        test_expect(&ts, stats.st_size == 0, buffer);
        if (stats.st_size != 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/root_1_page_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /root_1_page_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/root_1_page_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, one_page_file_size);
        test_expect(&ts, (size_t)stats.st_size == one_page_file_size, buffer);
        if ((size_t)stats.st_size == one_page_file_size) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/root_2_pages_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /root_2_pages_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/root_2_pages_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, two_pages_file_size);
        test_expect(&ts, (size_t)stats.st_size == two_pages_file_size, buffer);
        if ((size_t)stats.st_size == two_pages_file_size) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/A/root_empty_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /A/root_empty_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/A/root_empty_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, (size_t)0);
        test_expect(&ts, stats.st_size == 0, buffer);
        if (stats.st_size != 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/A/root_1_page_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /A/root_1_page_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/A/root_1_page_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, one_page_file_size);
        test_expect(&ts, (size_t)stats.st_size == one_page_file_size, buffer);
        if ((size_t)stats.st_size == one_page_file_size) {
            (void)close_image(&ctx);
            goto teardown;
        }
        ret = xipfs_stat(&ctx.mount, "/A/root_2_pages_file", &stats);
        test_expect(&ts, ret >= 0, "stat on /A/root_2_pages_file should succeed");
        if (ret < 0) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)snprintf(buffer, sizeof(buffer),
                       "/A/root_2_pages_file stat.st_size (%jd bytes) differs from"
                       " expected size (%zu bytes)",
              (intmax_t)stats.st_size, two_pages_file_size);
        test_expect(&ts, (size_t)stats.st_size == two_pages_file_size, buffer);
        if ((size_t)stats.st_size == two_pages_file_size) {
            (void)close_image(&ctx);
            goto teardown;
        }
        (void)close_image(&ctx);
    }
    /* Teardown */
teardown :
    unlink(tmp_flash_template);
    cleanup(tmp_root_dir_filename, PROGRESSION_ROOT_TWO_PAGES_FILE);

    return test_report(&ts);
}
