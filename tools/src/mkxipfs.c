#include "mkxipfs.h"

#include "mkxipfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include "include/errno.h"
#include "include/file.h"
#include "include/fs.h"

static void print_supported_targets_names(FILE *to);

void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s [--target <target_name>] create <filename.flash> <size>\n"
            "  %s [--target <target_name>] build <filename.flash> <host_dir> [size]\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] ls [-l] [path]\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] tree [path]\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] put <host_file> <xipfs_file>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] put <xipfs_file>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] get <xipfs_file> <host_file>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] get <xipfs_file>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] mkdir <xipfs_dir>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] rm <xipfs_file>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] rmdir <xipfs_dir>\n"
            "  %s [--target <target_name>] [--flash <filename.flash>] mv <xipfs_src> <xipfs_dst>\n"
            "  %s [--target <target_name>] test\n"
            "  %s [--target <target_name>] test_deep\n"
            "  %s [--target <target_name>] test_build\n"
            "\n"
            "Options:\n"
            "  --flash <filename.flash>   Flash image for commands other than create/build/test\n"
            "  --target <target_name>     Target device name, to be chosen among supported ones.\n"
            "                             Instead of this option, you can also use \"export XIPFS_TARGET=<target_name>\".\n"
            "\n"
            "Size format:\n"
            "  <bytes> or <kilobytes>k (example: 131072 or 128k)\n",
            prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);

    fprintf(stderr, "\n");
    print_supported_targets_names(stderr);
}

int parse_size(const char *raw, size_t *out_size)
{
    char *end = NULL;
    unsigned long long value;
    size_t multiplier = 1;

    errno = 0;
    value = strtoull(raw, &end, 10);
    if (errno != 0 || end == raw) {
        return -1;
    }

    if (*end == 'k' || *end == 'K') {
        multiplier = 1024;
        end++;
    }
    if (*end != '\0') {
        return -1;
    }

    if (value == 0 || value > (SIZE_MAX / multiplier)) {
        return -1;
    }

    *out_size = (size_t)value * multiplier;
    return 0;
}

int ask_alignment(size_t initial_size, size_t *aligned_size)
{
    const size_t page_size = XIPFS_NVM_PAGE_SIZE;
    size_t rounded;

    if (initial_size % page_size == 0) {
        *aligned_size = initial_size;
        return 0;
    }

    rounded = ((initial_size + page_size - 1) / page_size) * page_size;
    if (rounded < initial_size) {
        return -1;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr,
                "Error: size %zu is not aligned to page size %zu.\n",
                initial_size, page_size);
        fprintf(stderr, "Hint: use %zu bytes.\n", rounded);
        return -1;
    }

    fprintf(stderr,
            "Size %zu is not page-aligned (%zu).\n"
            "Round up to %zu bytes? [Y/n] ",
            initial_size, page_size, rounded);

    char answer[16];
    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return -1;
    }
    if (answer[0] == 'n' || answer[0] == 'N') {
        fprintf(stderr, "Aborted.\n");
        return -1;
    }

    *aligned_size = rounded;
    return 0;
}

int open_image(app_ctx_t *ctx, const char *flash_path, bool writable)
{
    struct stat st;
    int prot = PROT_READ;
    int flags = O_RDONLY;

    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
    ctx->mapping = MAP_FAILED;

    if (writable) {
        prot |= PROT_WRITE;
        flags = O_RDWR;
    }

    ctx->fd = open(flash_path, flags);
    if (ctx->fd < 0) {
        perror("open");
        return -1;
    }

    if (fstat(ctx->fd, &st) < 0) {
        perror("fstat");
        close(ctx->fd);
        return -1;
    }

    if (st.st_size <= 0 || ((size_t)st.st_size % XIPFS_NVM_PAGE_SIZE) != 0) {
        fprintf(stderr,
                "Error: invalid flash image size (%lld). Must be a positive multiple of %zu.\n",
                (long long)st.st_size, XIPFS_NVM_PAGE_SIZE);
        close(ctx->fd);
        return -1;
    }

    ctx->image_size = (size_t)st.st_size;
    ctx->mapping = mmap(NULL, ctx->image_size, prot, MAP_SHARED, ctx->fd, 0);
    if (ctx->mapping == MAP_FAILED) {
        perror("mmap");
        close(ctx->fd);
        return -1;
    }

    ctx->writable = writable;

    xipfs_workstation_nvm_base = (uintptr_t)ctx->mapping;

    memset(&ctx->mount, 0, sizeof(ctx->mount));
    ctx->mount.magic = XIPFS_MAGIC;
    ctx->mount.mount_path = "/";
    assert(XIPFS_NVM_PAGE_SIZE > 0);
    ctx->mount.page_num = ctx->image_size / XIPFS_NVM_PAGE_SIZE;
    ctx->mount.page_addr = xipfs_nvm_addr(0);
    ctx->mount.page_end_addr = (char *)ctx->mount.page_addr + ctx->image_size;
    /* For workstation, we don't need mutexes because they are not used
     * in code paths that are called.
     * Nonetheless, we need them to be different from NULL, because of safety checks
     * done within XIPFS.
     */
    ctx->mount.execution_mutex = (void *)0xDEADBEEF;
    ctx->mount.mutex = (void *)0xDEADBEEF;

    int mount_ret = xipfs_mount(&ctx->mount);
    if (mount_ret < 0) {
        fprintf(stderr, "xipfs_mount failed: %s\n", strerror(-mount_ret));
        if (xipfs_errno != XIPFS_OK) {
            fprintf(stderr, "xipfs errno: %s\n", xipfs_strerror(xipfs_errno));
        }
        (void)munmap(ctx->mapping, ctx->image_size);
        (void)close(ctx->fd);
        return -1;
    }

    return 0;
}

void close_image(app_ctx_t *ctx)
{
    if (ctx->mapping != MAP_FAILED && ctx->mapping != NULL) {
        if (ctx->writable) {
            (void)msync(ctx->mapping, ctx->image_size, MS_SYNC);
        }
        (void)munmap(ctx->mapping, ctx->image_size);
    }
    if (ctx->fd >= 0) {
        (void)close(ctx->fd);
    }

    xipfs_workstation_nvm_base = 0;
}

int create_image(const char *flash_path, const char *size_raw)
{
    int fd = -1;
    void *mapping = MAP_FAILED;
    size_t requested_size = 0;
    size_t image_size = 0;
    xipfs_mount_t mount;

    if (parse_size(size_raw, &requested_size) < 0) {
        fprintf(stderr, "Error: invalid size '%s'.\n", size_raw);
        return 1;
    }
    if (ask_alignment(requested_size, &image_size) < 0) {
        return 1;
    }

    const size_t target_nvm_bytesize =
        xipfs_workstation_nvm_numof * xipfs_workstation_nvm_page_size;
    if (image_size > target_nvm_bytesize) {
        fprintf(stderr,
                "Error: invalid size '%zu' bytes"
                " greater than target nvm bytesize '%zu' bytes\n",
                image_size, target_nvm_bytesize);
        return 1;
    }

    fd = open(flash_path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ftruncate(fd, (off_t)image_size) < 0) {
        perror("ftruncate");
        (void)close(fd);
        return 1;
    }

    mapping = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        (void)close(fd);
        return 1;
    }

    memset(mapping, XIPFS_NVM_ERASE_STATE, image_size);

    xipfs_workstation_nvm_base = (uintptr_t)mapping;

    memset(&mount, 0, sizeof(mount));
    mount.magic = XIPFS_MAGIC;
    mount.mount_path = "/";
    assert(XIPFS_NVM_PAGE_SIZE > 0);
    mount.page_num = image_size / XIPFS_NVM_PAGE_SIZE;
    mount.page_addr = xipfs_nvm_addr(0);
    mount.page_end_addr = (char *)mount.page_addr + image_size;
    /* For workstation, we don't need mutexes because they are not used
     * in code paths that are called.
     * Nonetheless, we need them to be different from NULL, because of safety checks
     * done within XIPFS.
     */
    mount.execution_mutex = (void *)0xDEADBEEF;
    mount.mutex = (void *)0xDEADBEEF;

    int fmt_ret = xipfs_format(&mount);
    if (fmt_ret < 0) {
        fprintf(stderr, "xipfs_format failed: %s\n", strerror(-fmt_ret));
        if (xipfs_errno != XIPFS_OK) {
            fprintf(stderr, "xipfs errno: %s\n", xipfs_strerror(xipfs_errno));
        }
        (void)munmap(mapping, image_size);
        (void)close(fd);
        return 1;
    }

    if (msync(mapping, image_size, MS_SYNC) < 0) {
        perror("msync");
        (void)munmap(mapping, image_size);
        (void)close(fd);
        return 1;
    }

    if (munmap(mapping, image_size) < 0) {
        perror("munmap");
        (void)close(fd);
        return 1;
    }
    if (close(fd) < 0) {
        perror("close");
        return 1;
    }

    printf("Created '%s' with %zu bytes (%zu pages of %zu bytes).\n",
           flash_path, image_size, mount.page_num, XIPFS_NVM_PAGE_SIZE);
    printf("To reuse this image in following commands:\n");
    printf("  export XIPFS_FILE_IMAGE='%s'\n", flash_path);

    return 0;
}

const char *resolve_flash_image(const char *flash_opt)
{
    if (flash_opt != NULL) {
        return flash_opt;
    }
    flash_opt = getenv("XIPFS_FILE_IMAGE");
    if (flash_opt != NULL && flash_opt[0] != '\0') {
        return flash_opt;
    }
    return NULL;
}

const char *resolve_target(const char *target_opt)
{
    if (target_opt != NULL) {
        return target_opt;
    }
    target_opt = getenv("XIPFS_TARGET");
    if (target_opt != NULL && target_opt[0] != '\0') {
        return target_opt;
    }
    return NULL;
}

typedef struct xipfs_target_nvm_configuration_s {
    const char *name;
    size_t numof;
    uint8_t erase_state;
    uintptr_t write_block_alignment;
    size_t write_block_size;
    size_t page_size;
} xipfs_target_nvm_configuration_t;

static const xipfs_target_nvm_configuration_t target_nvm_configurations[1] = {
    {
        .name                   = "dwm1001",
        .numof                  = 128,
        .erase_state            = 0xFF,
        .write_block_alignment  = 4,
        .write_block_size       = 4,
        .page_size              = 4096
    }
};

static void print_supported_targets_names(FILE *to)
{
    const size_t targets_count =
        sizeof(target_nvm_configurations) / sizeof(target_nvm_configurations[0]);
    const char *separator = "";

    if (to == NULL) {
        fprintf(stderr, "Error: print_supported_targets_names : NULL file.\n");
        return;
    }

    if (to == stdin) {
        fprintf(stderr, "Error: print_supported_targets_names : invalid stdin file.\n");
        return;
    }

    fprintf(to, "Supported targets are :\n");
    for(size_t i = 0; i < targets_count; i++) {
        fprintf(to, "  %s%s", separator, target_nvm_configurations[i].name);
        separator = ", ";
    }

    fprintf(to, "\n");
}

void print_target_names(void)
{
    print_supported_targets_names(stdout);
}

void print_target_configuration(const char *target_name)
{
    if (target_name == NULL) {
        fprintf(stderr, "Error : NULL target name\n");
        return;
    }

    const size_t targets_count =
        sizeof(target_nvm_configurations) / sizeof(target_nvm_configurations[0]);
    for(size_t i = 0; i < targets_count; i++) {
        if (strcmp(target_nvm_configurations[i].name, target_name) == 0) {
            const xipfs_target_nvm_configuration_t *configuration =
                &target_nvm_configurations[i];
            fprintf(stdout, "NVM configuration for target \"%s\" :\n", target_name);
            fprintf(stdout, "\t- Pages count : %zu\n", configuration->numof);
            fprintf(stdout, "\t- Page size   : %zu\n", configuration->page_size);
            fprintf(stdout, "\t- Erase state value   : %" PRIx8 "\n", configuration->erase_state);
            fprintf(stdout, "\t- Write block alignment  : %zu\n", configuration->write_block_alignment);
            fprintf(stdout, "\t- Write block size  : %zu\n", configuration->write_block_size);

            return;
        }
    }

    fprintf(stderr, "Error : unknown target name \"%s\"\n", target_name);
}

void print_current_target_configuration(void)
{
    fprintf(stdout, "Current NVM configuration :\n");
    fprintf(stdout, "\t- Pages count : %zu\n", xipfs_workstation_nvm_numof);
    fprintf(stdout, "\t- Page size   : %zu\n", xipfs_workstation_nvm_page_size);
    fprintf(stdout, "\t- Erase state value   : %" PRIx8 "\n", xipfs_workstation_nvm_erase_state);
    fprintf(stdout, "\t- Write block alignment  : %zu\n", xipfs_workstation_nvm_write_block_alignment);
    fprintf(stdout, "\t- Write block size  : %zu\n", xipfs_workstation_nvm_write_block_size);
}

int set_target_nvm_configuration(const char *target_opt)
{
    if (target_opt == NULL)
        return -1;

    const size_t targets_count =
        sizeof(target_nvm_configurations) / sizeof(target_nvm_configurations[0]);
    for(size_t i = 0; i < targets_count; i++) {
        if (strcmp(target_nvm_configurations[i].name, target_opt) == 0) {
            const xipfs_target_nvm_configuration_t *configuration =
                &target_nvm_configurations[i];

            xipfs_workstation_nvm_numof = configuration->numof;
            xipfs_workstation_nvm_erase_state = configuration->erase_state;
            xipfs_workstation_nvm_write_block_alignment = configuration->write_block_alignment;
            xipfs_workstation_nvm_write_block_size = configuration->write_block_size;
            xipfs_workstation_nvm_page_size = configuration->page_size;

            return 0;
        }
    }

    return -1;
}

int make_xipfs_path(const char *input, char *out, size_t out_sz)
{
    size_t len;

    if (input == NULL || out == NULL || out_sz == 0) {
        return -1;
    }
    if (input[0] == '\0') {
        return -1;
    }

    if (input[0] == '/') {
        len = strnlen(input, out_sz);
        if (len >= out_sz) {
            return -1;
        }
        memcpy(out, input, len + 1);
        return 0;
    }

    len = strnlen(input, out_sz - 1);
    if (len >= out_sz - 1) {
        return -1;
    }

    out[0] = '/';
    memcpy(out + 1, input, len + 1);
    return 0;
}

bool xipfs_path_is_dir(const char *path)
{
    size_t len = strnlen(path, XIPFS_PATH_MAX);
    return (len > 0 && path[len - 1] == '/');
}

int collect_entries(app_ctx_t *ctx, const char *target, file_entry_t **entries_out, size_t *count_out)
{
    file_entry_t *entries = NULL;
    size_t count = 0;
    size_t cap = 0;
    xipfs_file_t *filp;
    char target_path[XIPFS_PATH_MAX];
    char prefix[XIPFS_PATH_MAX];
    bool has_target = (target != NULL);
    bool as_file = false;
    size_t prefix_len = 0;
    struct stat st;

    if (has_target) {
        if (make_xipfs_path(target, target_path, sizeof(target_path)) < 0) {
            fprintf(stderr, "Error: invalid target path '%s'.\n", target);
            return -1;
        }

        if (xipfs_stat(&ctx->mount, target_path, &st) == 0 && S_ISREG(st.st_mode)) {
            as_file = true;
        }

        if (!as_file) {
            size_t len = strnlen(target_path, sizeof(target_path));
            if (len == 1 && target_path[0] == '/') {
                prefix[0] = '/';
                prefix[1] = '\0';
            }
            else if (target_path[len - 1] == '/') {
                memcpy(prefix, target_path, len + 1);
            }
            else {
                if (len + 1 >= sizeof(prefix)) {
                    fprintf(stderr, "Error: target path too long.\n");
                    return -1;
                }
                memcpy(prefix, target_path, len);
                prefix[len] = '/';
                prefix[len + 1] = '\0';
                len++;
            }
            prefix_len = strnlen(prefix, sizeof(prefix));
        }
    }

    xipfs_errno = XIPFS_OK;
    filp = xipfs_fs_head(&ctx->mount);
    if (xipfs_errno != XIPFS_OK) {
        fprintf(stderr, "Error: filesystem traversal failed (%s).\n", xipfs_strerror(xipfs_errno));
        return -1;
    }

    while (filp != NULL) {
        bool match = true;

        if (has_target) {
            if (as_file) {
                match = (strcmp(filp->path, target_path) == 0);
            }
            else if (!(prefix[0] == '/' && prefix[1] == '\0')) {
                match = (strncmp(filp->path, prefix, prefix_len) == 0);
            }
        }

        if (match) {
            struct stat st_local;
            if (xipfs_stat(&ctx->mount, filp->path, &st_local) < 0) {
                fprintf(stderr, "Error: failed to read file size for '%s'.\n", filp->path);
                free(entries);
                return -1;
            }

            if (count == cap) {
                size_t new_cap = (cap == 0) ? 16 : cap * 2;
                file_entry_t *new_entries = realloc(entries, new_cap * sizeof(*new_entries));
                if (new_entries == NULL) {
                    perror("realloc");
                    free(entries);
                    return -1;
                }
                entries = new_entries;
                cap = new_cap;
            }

            entries[count].filp = filp;
            entries[count].path = filp->path;
            entries[count].path_len = strnlen(filp->path, XIPFS_PATH_MAX);
            entries[count].size = st_local.st_size;
            entries[count].is_dir = xipfs_path_is_dir(filp->path);
            entries[count].is_exec = (filp->exec != 0);
            count++;
        }

        filp = xipfs_fs_next(&ctx->mount, filp);
        if (filp == NULL && xipfs_errno != XIPFS_OK) {
            fprintf(stderr, "Error: filesystem traversal failed (%s).\n", xipfs_strerror(xipfs_errno));
            free(entries);
            return -1;
        }
    }

    *entries_out = entries;
    *count_out = count;
    return 0;
}

void free_entries(file_entry_t *entries)
{
    free(entries);
}

int cmp_entries(const void *a, const void *b)
{
    const file_entry_t *ea = (const file_entry_t *)a;
    const file_entry_t *eb = (const file_entry_t *)b;
    return strcmp(ea->path, eb->path);
}

int read_all_fd(int fd, unsigned char **buf_out, size_t *size_out)
{
    unsigned char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;

    while (1) {
        unsigned char tmp[4096];
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n < 0) {
            perror("read");
            free(buf);
            return -1;
        }
        if (n == 0) {
            break;
        }

        if (len + (size_t)n > cap) {
            size_t new_cap = (cap == 0) ? 8192 : cap;
            while (new_cap < len + (size_t)n) {
                new_cap *= 2;
            }
            unsigned char *new_buf = realloc(buf, new_cap);
            if (new_buf == NULL) {
                perror("realloc");
                free(buf);
                return -1;
            }
            buf = new_buf;
            cap = new_cap;
        }

        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }

    *buf_out = buf;
    *size_out = len;
    return 0;
}

int write_all_fd(int fd, const unsigned char *buf, size_t size)
{
    size_t off = 0;
    while (off < size) {
        ssize_t n = write(fd, buf + off, size - off);
        if (n < 0) {
            perror("write");
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

bool is_stdout_tty(void)
{
    return isatty(STDOUT_FILENO) != 0;
}

const char *color_blue(void)
{
    return "\033[34m";
}

const char *color_green(void)
{
    return "\033[32m";
}

const char *color_reset(void)
{
    return "\033[0m";
}

bool is_fae_payload(const unsigned char *buf, size_t size)
{
    uint32_t marker;

    if (buf == NULL || size == 0) {
        return false;
    }
    if ((size % 32) != 0) {
        return false;
    }
    if (size < sizeof(marker)) {
        return false;
    }

    memcpy(&marker, buf + size - sizeof(marker), sizeof(marker));
    return marker == (uint32_t)XIPFS_CRT0_MAGIC_NUMBER_AND_VERSION;
}

int host_file_is_fae(const char *host_path, bool *is_fae)
{
    int fd;
    struct stat st;
    uint32_t marker;

    if (is_fae == NULL) {
        return -1;
    }
    *is_fae = false;

    fd = open(host_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    if (fstat(fd, &st) < 0) {
        (void)close(fd);
        return -1;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < (off_t)sizeof(marker) || ((size_t)st.st_size % 32) != 0) {
        (void)close(fd);
        return 0;
    }

    if (lseek(fd, -(off_t)sizeof(marker), SEEK_END) < 0) {
        (void)close(fd);
        return -1;
    }
    if (read(fd, &marker, sizeof(marker)) != (ssize_t)sizeof(marker)) {
        (void)close(fd);
        return -1;
    }
    (void)close(fd);

    *is_fae = (marker == (uint32_t)XIPFS_CRT0_MAGIC_NUMBER_AND_VERSION);
    return 0;
}
