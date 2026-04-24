#include "mkxipfs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct host_entry_s {
    char *rel_path;
    char *abs_path;
    bool is_dir, is_dir_empty;
    off_t size;
} host_entry_t;

static int host_entry_push(host_entry_t **entries, size_t *count, size_t *cap,
        const char *rel_path, const char *abs_path, bool is_dir, off_t size) {
    if (*count == *cap) {
        size_t new_cap = (*cap == 0) ? 32 : (*cap * 2);
        host_entry_t *new_entries = realloc(*entries, new_cap * sizeof(*new_entries));
        if (new_entries == NULL) return -1;
        *entries = new_entries;
        *cap = new_cap;
    }
    (*entries)[*count].rel_path = strdup(rel_path);
    (*entries)[*count].abs_path = strdup(abs_path);
    if ((*entries)[*count].rel_path == NULL || (*entries)[*count].abs_path == NULL) {
        free((*entries)[*count].rel_path);
        free((*entries)[*count].abs_path);
        return -1;
    }
    (*entries)[*count].is_dir = is_dir;
    (*entries)[*count].size = size;
    (*count)++;
    return 0;
}

static void host_entries_free(host_entry_t *entries, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(entries[i].rel_path);
        free(entries[i].abs_path);
    }
    free(entries);
}

static int host_collect_recursive(const char *root_dir, const char *rel_dir,
        host_entry_t **entries, size_t *count, size_t *cap) {
    char scan_path[4096];
    DIR *dir = NULL;
    struct dirent *de;
    if (rel_dir[0] == '\0') {
        if (snprintf(scan_path, sizeof(scan_path), "%s", root_dir) >= (int)sizeof(scan_path)) return -1;
    } else {
        if (snprintf(scan_path, sizeof(scan_path), "%s/%s", root_dir, rel_dir) >= (int)sizeof(scan_path)) return -1;
    }

    dir = opendir(scan_path);
    if (dir == NULL) return -1;
    while ((de = readdir(dir)) != NULL) {
        char rel_path[4096];
        char abs_path[4096];
        struct stat st;
        if ((strcmp(de->d_name, ".") == 0) || (strcmp(de->d_name, "..") == 0)) continue;
        if (rel_dir[0] == '\0') {
            if (snprintf(rel_path, sizeof(rel_path), "%s", de->d_name) >= (int)sizeof(rel_path)) {
                (void)closedir(dir);
                return -1;
            }
        }
        else {
            if (snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_dir, de->d_name) >= (int)sizeof(rel_path)) {
                (void)closedir(dir);
                return -1;
            }
        }
        if (snprintf(abs_path, sizeof(abs_path), "%s/%s", root_dir, rel_path) >= (int)sizeof(abs_path)) {
            (void)closedir(dir);
            return -1;
        }
        if (lstat(abs_path, &st) < 0) {
            (void)closedir(dir);
            return -1;
        }
        if (S_ISDIR(st.st_mode)) {

            if (host_entry_push(entries, count, cap, rel_path, abs_path, true, 0) < 0) {
                (void)closedir(dir);
                return -1;
            }

            const size_t dir_index = *count;

            /* Collect all files in the sub-hierarchy */
            if (host_collect_recursive(root_dir, rel_path, entries, count, cap) < 0) {
                (void)closedir(dir);
                return -1;
            }

            (*entries)[dir_index - 1].is_dir_empty = (*count == dir_index);
        }
        else if (S_ISREG(st.st_mode)) {
            if (host_entry_push(entries, count, cap, rel_path, abs_path, false, st.st_size) < 0) {
                (void)closedir(dir);
                return -1;
            }
        }
        else {
            fprintf(stderr, "Error: unsupported file type '%s'.\n", abs_path);
            (void)closedir(dir);
            return -1;
        }
    }
    if (closedir(dir) < 0) return -1;
    return 0;
}

static int make_xipfs_path_from_rel(const char *rel_path, bool is_dir, char
        *out, size_t out_size) {
    int n;
    if (is_dir)
        n = snprintf(out, out_size, "/%s/", rel_path);
    else
        n = snprintf(out, out_size, "/%s", rel_path);
    return (n > 0 && (size_t)n < out_size) ? 0 : -1;
}

static size_t xipfs_reserved_for_payload(size_t payload) {
    if (payload > 0) {
        return ((payload + sizeof(xipfs_file_t) + XIPFS_NVM_PAGE_SIZE - 1) / XIPFS_NVM_PAGE_SIZE) * XIPFS_NVM_PAGE_SIZE;
    }
    return XIPFS_NVM_PAGE_SIZE;
}

static int copy_host_file_to_xipfs(app_ctx_t *ctx, const char *host_path, const
        char *xipfs_path, off_t size) {
    int host_fd = -1;
    xipfs_file_desc_t desc;
    unsigned char buf[4096];
    uint32_t exec = 0;
    bool is_fae = false;
    if (size < 0 || (uint64_t)size > XIPFS_FILE_POSITION_MAX_AS_SIZE_T) {
        fprintf(stderr, "Error: file too large for xipfs '%s'.\n", host_path);
        return -1;
    }
    if (host_file_is_fae(host_path, &is_fae) == 0 && is_fae) {
        exec = 1;
    }
    if (xipfs_new_file(&ctx->mount, xipfs_path, (uint32_t)size, exec) < 0) return -1;
    memset(&desc, 0, sizeof(desc));
    if (xipfs_open(&ctx->mount, &desc, xipfs_path, O_WRONLY, 0) < 0) return -1;
    host_fd = open(host_path, O_RDONLY);
    if (host_fd < 0) {
        (void)xipfs_close(&ctx->mount, &desc);
        return -1;
    }
    while (1) {
        ssize_t r = read(host_fd, buf, sizeof(buf));
        if (r < 0) {
            (void)close(host_fd);
            (void)xipfs_close(&ctx->mount, &desc);
            return -1;
        }
        if (r == 0) break;
        size_t off = 0;
        while (off < (size_t)r) {
            ssize_t w = xipfs_write(&ctx->mount, &desc, buf + off, (size_t)r - off);
            if (w <= 0) {
                (void)close(host_fd);
                (void)xipfs_close(&ctx->mount, &desc);
                return -1;
            }
            off += (size_t)w;
        }
    }
    if (close(host_fd) < 0) {
        (void)xipfs_close(&ctx->mount, &desc);
        return -1;
    }
    if (xipfs_close(&ctx->mount, &desc) < 0) return -1;
    return 0;
}

int cmd_build(int argc, char **argv) {
    struct stat st;
    host_entry_t *entries = NULL;
    size_t count = 0;
    size_t cap = 0;
    size_t i;
    size_t needed_pages = 0;
    size_t needed_size;
    size_t final_size;
    app_ctx_t ctx;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Error: build requires <filename.flash> <host_dir> [size].\n");
        return 1;
    }
    const char *flash_path = argv[0];
    const char *host_dir = argv[1];
    const char *size_opt = (argc == 3) ? argv[2] : NULL;
    if (stat(host_dir, &st) < 0) {
        perror("stat");
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", host_dir);
        return 1;
    }
    if (host_collect_recursive(host_dir, "", &entries, &count, &cap) < 0) {
        perror("build scan");
        host_entries_free(entries, count);
        return 1;
    }
    size_t payload;
    for (i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            if (entries[i].is_dir_empty == false)
                continue;
            payload = 0;
        } else {
            payload = (size_t)entries[i].size;
        }
        needed_pages += xipfs_reserved_for_payload(payload) / XIPFS_NVM_PAGE_SIZE;
    }
    if (needed_pages == 0)
        needed_pages = 1;
    if (needed_pages > XIPFS_NVM_NUMOF) {
        fprintf(stderr,
                "Error: invalid needed pages count '%zu'"
                " greater than target nvm pages count '%zu'\n",
                needed_pages, XIPFS_NVM_NUMOF);
        host_entries_free(entries, count);
        return 1;
    }
    needed_size = needed_pages * XIPFS_NVM_PAGE_SIZE;
    if (size_opt != NULL) {
        size_t requested;
        if (parse_size(size_opt, &requested) < 0) {
            fprintf(stderr, "Error: invalid size '%s'.\n", size_opt);
            host_entries_free(entries, count);
            return 1;
        }
        if (ask_alignment(requested, &final_size) < 0) {
            host_entries_free(entries, count);
            return 1;
        }
        if (final_size < needed_size) {
            fprintf(stderr, "Error: requested size (%zu bytes) is smaller than required minimum (%zu bytes).\n",
                    final_size, needed_size);
            host_entries_free(entries, count);
            return 1;
        }
    }
    else
        final_size = needed_size;
    char size_buf[64];
    (void)snprintf(size_buf, sizeof(size_buf), "%zu", final_size);
    /* Create image will check final size against max theorical supported image size
     * So there is no extra check on size here.
     */
    if (create_image(flash_path, size_buf) != 0) {
        host_entries_free(entries, count);
        return 1;
    }
    if (open_image(&ctx, flash_path, true) < 0) {
        host_entries_free(entries, count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            char xipfs_path[XIPFS_PATH_MAX];
            if (make_xipfs_path_from_rel(entries[i].rel_path, true, xipfs_path, sizeof(xipfs_path)) < 0) {
                fprintf(stderr, "Error: xipfs path too long for '%s'.\n", entries[i].rel_path);
                close_image(&ctx);
                host_entries_free(entries, count);
                return 1;
            }
            if (xipfs_mkdir(&ctx.mount, xipfs_path, 0) < 0) {
                fprintf(stderr, "Error: failed to create directory '%s' in xipfs.\n", xipfs_path);
                close_image(&ctx);
                host_entries_free(entries, count);
                return 1;
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (!entries[i].is_dir) {
            char xipfs_path[XIPFS_PATH_MAX];
            if (make_xipfs_path_from_rel(entries[i].rel_path, false, xipfs_path, sizeof(xipfs_path)) < 0) {
                fprintf(stderr, "Error: xipfs path too long for '%s'.\n", entries[i].rel_path);
                close_image(&ctx);
                host_entries_free(entries, count);
                return 1;
            }
            if (copy_host_file_to_xipfs(&ctx, entries[i].abs_path, xipfs_path, entries[i].size) < 0) {
                fprintf(stderr, "Error: failed to copy '%s' to '%s'.\n", entries[i].abs_path, xipfs_path);
                close_image(&ctx);
                host_entries_free(entries, count);
                return 1;
            }
        }
    }
    close_image(&ctx);
    host_entries_free(entries, count);
    printf("Build complete: %s from %s (%zu pages, %zu bytes).\n",
           flash_path, host_dir, final_size / XIPFS_NVM_PAGE_SIZE, final_size);
    return 0;
}
