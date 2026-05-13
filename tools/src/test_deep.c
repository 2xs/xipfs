#include "mkxipfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/buffer.h"
#include "include/errno.h"
#include "include/file.h"
#include "include/fs.h"

#include "test.h"

typedef struct deep_entry_s {
    xipfs_file_t *filp;
    xipfs_memory_offset_t off;
    xipfs_memory_offset_t next;
    uint32_t reserved;
    bool is_dir;
    char path[XIPFS_PATH_MAX];
} deep_entry_t;

enum deep_loc_e {
    LOC_NONE = 0,
    LOC_A = 1,
    LOC_B = 2,
    LOC_C = 3,
};

static int deep_collect_entries(app_ctx_t *ctx, deep_entry_t **out_entries, size_t *out_count) {
    deep_entry_t *entries = NULL;
    size_t count = 0;
    size_t cap = 0;
    xipfs_file_t *filp;
    xipfs_errno = XIPFS_OK;
    filp = xipfs_fs_head(&ctx->mount);
    if (filp == NULL && xipfs_errno != XIPFS_OK) return -1;
    if (filp == NULL) {
        *out_entries = NULL;
        *out_count = 0;
        return 0;
    }
    while (filp != NULL) {
        size_t len;
        if (count == cap) {
            size_t new_cap = (cap == 0) ? 16 : cap * 2;
            deep_entry_t *new_entries = realloc(entries, new_cap * sizeof(*new_entries));
            if (new_entries == NULL) {
                free(entries);
                return -1;
            }
            entries = new_entries;
            cap = new_cap;
        }
        entries[count].filp = filp;
        entries[count].off = xipfs_pointer_to_offset(&ctx->mount, filp);
        entries[count].next = filp->next;
        entries[count].reserved = filp->reserved;
        len = strnlen(filp->path, XIPFS_PATH_MAX);
        if (len >= XIPFS_PATH_MAX) {
            free(entries);
            return -1;
        }
        memcpy(entries[count].path, filp->path, len + 1);
        entries[count].is_dir = (len > 0 && filp->path[len - 1] == '/');
        count++;
        xipfs_errno = XIPFS_OK;
        filp = xipfs_fs_next(&ctx->mount, filp);
        if (filp == NULL && xipfs_errno != XIPFS_OK) {
            free(entries);
            return -1;
        }
    }
    *out_entries = entries;
    *out_count = count;
    return 0;
}

static int deep_get_file_entry(app_ctx_t *ctx, const char *path, deep_entry_t *out) {
    deep_entry_t *entries = NULL;
    size_t count = 0;
    size_t i;
    if (deep_collect_entries(ctx, &entries, &count) < 0) return -1;
    for (i = 0; i < count; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            *out = entries[i];
            free(entries);
            return 0;
        }
    }
    free(entries);
    return -1;
}

static bool path_is_child_of(const char *parent_dir, const char *path) {
    size_t plen = strnlen(parent_dir, XIPFS_PATH_MAX);
    if (strncmp(parent_dir, path, plen) != 0) return false;
    return strcmp(parent_dir, path) != 0;
}

static bool has_explicit_dir(const deep_entry_t *entries, size_t count, const char *dir_path) {
    size_t i;
    for (i = 0; i < count; i++)
        if (entries[i].is_dir && strcmp(entries[i].path, dir_path) == 0) return true;
    return false;
}

static bool directory_exists_logically(const deep_entry_t *entries, size_t count, const char *dir_path) {
    size_t i;
    if (has_explicit_dir(entries, count, dir_path)) return true;
    for (i = 0; i < count; i++)
        if (path_is_child_of(dir_path, entries[i].path)) return true;
    return false;
}

static int check_erased_tail(app_ctx_t *ctx, size_t free_off) {
    const unsigned char *p = (const unsigned char *)ctx->mapping;
    size_t i;
    if (free_off > ctx->image_size) return -1;
    for (i = free_off; i < ctx->image_size; i++)
        if (p[i] != (unsigned char)XIPFS_NVM_ERASE_STATE) return -1;
    return 0;
}

static void deep_check_layout(app_ctx_t *ctx, test_state_t *ts, const char *where) {
    deep_entry_t *entries = NULL;
    size_t count = 0;
    size_t i;
    size_t free_off = 0;
    char msg[192];
    if (xipfs_buffer_flush() < 0) {
        snprintf(msg, sizeof(msg), "%s: buffer flush failed before check", where);
        test_expect(ts, false, msg);
        return;
    }
    if (deep_collect_entries(ctx, &entries, &count) < 0) {
        snprintf(msg, sizeof(msg), "%s: failed to walk file list", where);
        test_expect(ts, false, msg);
        return;
    }
    if (count == 0) {
        if (check_erased_tail(ctx, 0) < 0) {
            snprintf(msg, sizeof(msg), "%s: empty fs must be fully erased", where);
            test_expect(ts, false, msg);
        }
        free(entries);
        return;
    }
    snprintf(msg, sizeof(msg), "%s: first file must start at flash base", where);
    test_expect(ts, entries[0].off == 0, msg);
    for (i = 0; i < count; i++) {
        size_t j;
        xipfs_memory_offset_t expected_next = entries[i].off + entries[i].reserved;
        snprintf(msg, sizeof(msg), "%s: entry %zu must be page-aligned", where, i);
        test_expect(ts, (entries[i].off % XIPFS_NVM_PAGE_SIZE) == 0, msg);
        snprintf(msg, sizeof(msg), "%s: entry %zu reserved must be page-aligned", where, i);
        test_expect(ts, (entries[i].reserved % XIPFS_NVM_PAGE_SIZE) == 0, msg);
        snprintf(msg, sizeof(msg), "%s: entry %zu reserved must be >= one page", where, i);
        test_expect(ts, entries[i].reserved >= XIPFS_NVM_PAGE_SIZE, msg);
        snprintf(msg, sizeof(msg), "%s: entry %zu must fit in image bounds", where, i);
        test_expect(ts, expected_next <= ctx->image_size, msg);
        for (j = i + 1; j < count; j++) {
            snprintf(msg, sizeof(msg), "%s: duplicate file path found", where);
            test_expect(ts, strcmp(entries[i].path, entries[j].path) != 0, msg);
        }
        if (i + 1 < count) {
            snprintf(msg, sizeof(msg), "%s: list must be packed (next offset)", where);
            test_expect(ts, entries[i].next == expected_next, msg);
            snprintf(msg, sizeof(msg), "%s: list must be packed (next file start)", where);
            test_expect(ts, entries[i + 1].off == expected_next, msg);
        } else {
            if (expected_next == ctx->image_size) {
                snprintf(msg, sizeof(msg), "%s: full fs tail must point to itself", where);
                test_expect(ts, entries[i].next == entries[i].off, msg);
                free_off = ctx->image_size;
            } else {
                snprintf(msg, sizeof(msg), "%s: tail must point to first free offset", where);
                test_expect(ts, entries[i].next == expected_next, msg);
                free_off = expected_next;
            }
        }
    }
    if (check_erased_tail(ctx, free_off) < 0) {
        snprintf(msg, sizeof(msg), "%s: free tail region must be erased", where);
        test_expect(ts, false, msg);
    }
    for (i = 0; i < count; i++) {
        size_t k;
        size_t len = strnlen(entries[i].path, XIPFS_PATH_MAX);
        size_t limit = len;
        if (entries[i].is_dir) {
            size_t j;
            for (j = 0; j < count; j++) {
                if (i == j) continue;
                snprintf(msg, sizeof(msg), "%s: explicit dir entry must be empty", where);
                test_expect(ts, !path_is_child_of(entries[i].path, entries[j].path), msg);
            }
        }
        if (len > 1 && entries[i].path[len - 1] == '/') limit = len - 1;
        for (k = 1; k < limit; k++) {
            char parent[XIPFS_PATH_MAX];
            if (entries[i].path[k] != '/') continue;
            if (k + 1 >= sizeof(parent)) continue;
            memcpy(parent, entries[i].path, k + 1);
            parent[k + 1] = '\0';
            if (strcmp(parent, "/") == 0) continue;
            snprintf(msg, sizeof(msg), "%s: parent directory must exist (explicit or implicit)", where);
            test_expect(ts, directory_exists_logically(entries, count, parent), msg);
        }
    }
    free(entries);
}

static int deep_write_small_file(app_ctx_t *ctx, const char *path, unsigned seq) {
    xipfs_file_desc_t d;
    char payload[96];
    int ret;
    ssize_t w;
    ret = snprintf(payload, sizeof(payload), "deep-%03u:%s", seq, path);
    if (ret < 0 || (size_t)ret >= sizeof(payload)) return -1;
    if (xipfs_new_file(&ctx->mount, path, (uint32_t)strlen(payload), 0) < 0) return -1;
    memset(&d, 0, sizeof(d));
    if (xipfs_open(&ctx->mount, &d, path, O_WRONLY, 0) < 0) return -1;
    w = xipfs_write(&ctx->mount, &d, payload, strlen(payload));
    if (w != (ssize_t)strlen(payload)) {
        (void)xipfs_close(&ctx->mount, &d);
        return -1;
    }
    if (xipfs_close(&ctx->mount, &d) < 0) return -1;
    return 0;
}

static unsigned char deep_pattern_byte(size_t i, unsigned seed) {
    return (unsigned char)((i * 37u + seed * 17u + 11u) & 0xffu);
}

static int deep_write_pattern_file(app_ctx_t *ctx, const char *path, size_t size, unsigned seed) {
    xipfs_file_desc_t d;
    unsigned char buf[1024];
    size_t off = 0;
    int ret;
    if (size > UINT32_MAX) return -1;
    if (xipfs_new_file(&ctx->mount, path, (uint32_t)size, 0) < 0) return -1;
    memset(&d, 0, sizeof(d));
    if (xipfs_open(&ctx->mount, &d, path, O_WRONLY, 0) < 0) return -1;
    while (off < size) {
        size_t n = size - off;
        size_t i;
        ssize_t w;
        if (n > sizeof(buf)) n = sizeof(buf);
        for (i = 0; i < n; i++) buf[i] = deep_pattern_byte(off + i, seed);
        w = xipfs_write(&ctx->mount, &d, buf, n);
        if (w != (ssize_t)n) {
            (void)xipfs_close(&ctx->mount, &d);
            return -1;
        }
        off += n;
    }
    ret = xipfs_close(&ctx->mount, &d);
    if (ret < 0) return -1;
    return 0;
}

static int deep_read_verify_pattern_file(app_ctx_t *ctx, const char *path, size_t expected, unsigned seed) {
    xipfs_file_desc_t d;
    unsigned char buf[1024];
    size_t off = 0;
    int ret;
    memset(&d, 0, sizeof(d));
    if (xipfs_open(&ctx->mount, &d, path, O_RDONLY, 0) < 0) return -1;
    while (1) {
        ssize_t r = xipfs_read(&ctx->mount, &d, buf, sizeof(buf));
        if (r < 0) {
            (void)xipfs_close(&ctx->mount, &d);
            return -1;
        }
        if (r == 0) break;
        for (size_t i = 0; i < (size_t)r; i++) {
            if (off + i >= expected) {
                (void)xipfs_close(&ctx->mount, &d);
                return -1;
            }
            if (buf[i] != deep_pattern_byte(off + i, seed)) {
                (void)xipfs_close(&ctx->mount, &d);
                return -1;
            }
        }
        off += (size_t)r;
    }
    ret = xipfs_close(&ctx->mount, &d);
    if (ret < 0) return -1;
    return (off == expected) ? 0 : -1;
}

static int deep_read_exact_file(app_ctx_t *ctx, const char *path, unsigned char *buf, size_t expected) {
    xipfs_file_desc_t d;
    size_t off = 0;
    int ret;
    memset(&d, 0, sizeof(d));
    if (xipfs_open(&ctx->mount, &d, path, O_RDONLY, 0) < 0) return -1;
    while (off < expected) {
        ssize_t r = xipfs_read(&ctx->mount, &d, buf + off, expected - off);
        if (r <= 0) {
            (void)xipfs_close(&ctx->mount, &d);
            return -1;
        }
        off += (size_t)r;
    }
    ret = xipfs_read(&ctx->mount, &d, buf, 1);
    if (ret != 0) {
        (void)xipfs_close(&ctx->mount, &d);
        return -1;
    }
    if (xipfs_close(&ctx->mount, &d) < 0) return -1;
    return 0;
}

static void deep_phase_rw_append(app_ctx_t *ctx, test_state_t *ts) {
    const char *path = "/rw/data.bin";
    deep_entry_t ent;
    struct stat st;
    xipfs_file_desc_t d;
    unsigned char *expected = NULL;
    unsigned char *readback = NULL;
    off_t max_pos;
    size_t cap_payload;
    size_t expected_size = 1024u;
    size_t patch_off = 256u;
    size_t patch_len = 128u;
    size_t append1_len;
    size_t remain;
    ssize_t w;
    ssize_t w_over;
    int ret;
    ret = xipfs_mkdir(&ctx->mount, "/rw", 0);
    test_expect(ts, ret == 0, "phase4: mkdir /rw should succeed");
    ret = deep_write_pattern_file(ctx, path, expected_size, 21u);
    test_expect(ts, ret == 0, "phase4: create pattern file should succeed");
    deep_check_layout(ctx, ts, "phase4 after create");
    ret = deep_get_file_entry(ctx, path, &ent);
    test_expect(ts, ret == 0, "phase4: collect file entry should succeed");
    if (ret < 0) return;
    max_pos = xipfs_file_get_max_pos(&ctx->mount, ent.filp);
    test_expect(ts, max_pos > 0, "phase4: xipfs_file_get_max_pos should succeed");
    if (max_pos <= 0) return;
    cap_payload = (size_t)max_pos;
    append1_len = (cap_payload > expected_size + 128u) ? 128u : ((cap_payload > expected_size) ? ((cap_payload - expected_size) / 2u) : 0u);
    test_expect(ts, append1_len > 0, "phase4: payload capacity should allow first append");
    if (append1_len == 0) return;
    expected = malloc(cap_payload);
    readback = malloc(cap_payload);
    test_expect(ts, expected != NULL && readback != NULL, "phase4: allocate expected/readback buffers should succeed");
    if (expected == NULL || readback == NULL) {
        free(expected);
        free(readback);
        return;
    }
    for (size_t i = 0; i < expected_size; i++) expected[i] = deep_pattern_byte(i, 21u);
    ret = xipfs_stat(&ctx->mount, path, &st);
    test_expect(ts, ret == 0, "phase4: stat after create should succeed");
    if (ret == 0) test_expect(ts, st.st_size == (off_t)expected_size, "phase4: stat size after create should match");
    memset(&d, 0, sizeof(d));
    ret = xipfs_open(&ctx->mount, &d, path, O_RDWR, 0);
    test_expect(ts, ret == 0, "phase4: open O_RDWR should succeed");
    if (ret == 0) {
        ret = xipfs_fstat(&ctx->mount, &d, &st);
        test_expect(ts, ret == 0, "phase4: fstat on O_RDWR descriptor should succeed");
        if (ret == 0) test_expect(ts, st.st_size == (off_t)expected_size, "phase4: fstat size should match");
        ret = xipfs_lseek(&ctx->mount, &d, (off_t)patch_off, SEEK_SET);
        test_expect(ts, ret == (off_t)patch_off, "phase4: lseek to patch offset should succeed");
        for (size_t i = 0; i < patch_len; i++) readback[i] = (unsigned char)(0xA0u + (i % 29u));
        w = xipfs_write(&ctx->mount, &d, readback, patch_len);
        test_expect(ts, w == (ssize_t)patch_len, "phase4: in-place patch write should succeed");
        if (w == (ssize_t)patch_len) memcpy(expected + patch_off, readback, patch_len);
        ret = xipfs_close(&ctx->mount, &d);
        test_expect(ts, ret == 0, "phase4: close after patch should succeed");
    }
    ret = deep_read_exact_file(ctx, path, readback, expected_size);
    test_expect(ts, ret == 0, "phase4: readback after patch should succeed");
    if (ret == 0) test_expect(ts, memcmp(readback, expected, expected_size) == 0, "phase4: patched contents should match oracle");
    deep_check_layout(ctx, ts, "phase4 after patch");
    memset(&d, 0, sizeof(d));
    ret = xipfs_open(&ctx->mount, &d, path, O_WRONLY | O_APPEND, 0);
    test_expect(ts, ret == 0, "phase4: open O_APPEND should succeed");
    if (ret == 0) {
        for (size_t i = 0; i < append1_len; i++) readback[i] = (unsigned char)(0x30u + (i % 61u));
        w = xipfs_write(&ctx->mount, &d, readback, append1_len);
        test_expect(ts, w == (ssize_t)append1_len, "phase4: first append should fully fit");
        if (w == (ssize_t)append1_len) {
            memcpy(expected + expected_size, readback, append1_len);
            expected_size += append1_len;
        }
        ret = xipfs_fsync(&ctx->mount, &d, d.pos);
        test_expect(ts, ret == 0, "phase4: fsync after first append should succeed");
        ret = xipfs_fstat(&ctx->mount, &d, &st);
        test_expect(ts, ret == 0, "phase4: fstat after first append should succeed");
        if (ret == 0) test_expect(ts, st.st_size == (off_t)expected_size, "phase4: fstat size after first append should match");
        remain = cap_payload - expected_size;
        for (size_t i = 0; i < remain + 400u && i < cap_payload; i++) readback[i] = (unsigned char)(0x55u + (i % 113u));
        w_over = xipfs_write(&ctx->mount, &d, readback, remain + 400u);
        test_expect(ts, w_over >= 0 && (size_t)w_over <= remain, "phase4: overflow append should be bounded by remaining capacity");
        if (w_over > 0) {
            memcpy(expected + expected_size, readback, (size_t)w_over);
            expected_size += (size_t)w_over;
        }
        w = xipfs_write(&ctx->mount, &d, readback, 64u);
        if (remain == (size_t)((w_over > 0) ? w_over : 0)) test_expect(ts, w == 0, "phase4: append on full file should return 0");
        else test_expect(ts, w >= 0, "phase4: second append should not fail");
        ret = xipfs_fsync(&ctx->mount, &d, d.pos);
        test_expect(ts, ret == 0, "phase4: fsync after overflow append should succeed");
        ret = xipfs_fstat(&ctx->mount, &d, &st);
        test_expect(ts, ret == 0, "phase4: fstat after overflow append should succeed");
        if (ret == 0) test_expect(ts, st.st_size == (off_t)expected_size, "phase4: fstat size after overflow append should match");
        ret = xipfs_close(&ctx->mount, &d);
        test_expect(ts, ret == 0, "phase4: close after append should succeed");
    }
    ret = xipfs_stat(&ctx->mount, path, &st);
    test_expect(ts, ret == 0, "phase4: stat after append sequence should succeed");
    if (ret == 0) test_expect(ts, st.st_size == (off_t)expected_size, "phase4: stat size after append sequence should match");
    ret = deep_read_exact_file(ctx, path, readback, expected_size);
    test_expect(ts, ret == 0, "phase4: final readback should succeed");
    if (ret == 0) test_expect(ts, memcmp(readback, expected, expected_size) == 0, "phase4: final contents should match oracle");
    deep_check_layout(ctx, ts, "phase4 after append overflow");
    free(expected);
    free(readback);
}

static void deep_phase_rmdir_compaction_and_multiblock(app_ctx_t *ctx, test_state_t *ts) {
    struct xipfs_statvfs v0, v1, v2;
    int ret;
    ret = xipfs_mkdir(&ctx->mount, "/keepA", 0);
    test_expect(ts, ret == 0, "phase2: mkdir /keepA should succeed");
    ret = xipfs_mkdir(&ctx->mount, "/mid", 0);
    test_expect(ts, ret == 0, "phase2: mkdir /mid should succeed");
    ret = xipfs_mkdir(&ctx->mount, "/keepB", 0);
    test_expect(ts, ret == 0, "phase2: mkdir /keepB should succeed");
    deep_check_layout(ctx, ts, "phase2 after mkdir");
    ret = deep_write_pattern_file(ctx, "/keepB/big1.bin", 10000u, 7u);
    test_expect(ts, ret == 0, "phase2: write /keepB/big1.bin should succeed");
    ret = deep_write_pattern_file(ctx, "/keepA/big2.bin", 18000u, 13u);
    test_expect(ts, ret == 0, "phase2: write /keepA/big2.bin should succeed");
    deep_check_layout(ctx, ts, "phase2 after multiblock writes");
    ret = xipfs_statvfs(&ctx->mount, "/", &v0);
    test_expect(ts, ret == 0, "phase2: statvfs before rmdir should succeed");
    ret = xipfs_rmdir(&ctx->mount, "/mid");
    test_expect(ts, ret == 0, "phase2: rmdir /mid should succeed");
    deep_check_layout(ctx, ts, "phase2 after rmdir /mid");
    ret = deep_read_verify_pattern_file(ctx, "/keepB/big1.bin", 10000u, 7u);
    test_expect(ts, ret == 0, "phase2: verify /keepB/big1.bin after rmdir should succeed");
    ret = deep_read_verify_pattern_file(ctx, "/keepA/big2.bin", 18000u, 13u);
    test_expect(ts, ret == 0, "phase2: verify /keepA/big2.bin after rmdir should succeed");
    ret = xipfs_statvfs(&ctx->mount, "/", &v1);
    test_expect(ts, ret == 0, "phase2: statvfs after rmdir should succeed");
    if (ret == 0) test_expect(ts, v1.f_bfree >= v0.f_bfree, "phase2: rmdir should not reduce free blocks");
    ret = xipfs_unlink(&ctx->mount, "/keepA/big2.bin");
    test_expect(ts, ret == 0, "phase2: unlink /keepA/big2.bin should succeed");
    ret = xipfs_unlink(&ctx->mount, "/keepB/big1.bin");
    test_expect(ts, ret == 0, "phase2: unlink /keepB/big1.bin should succeed");
    ret = xipfs_rmdir(&ctx->mount, "/keepA");
    test_expect(ts, ret == 0, "phase2: rmdir /keepA should succeed");
    ret = xipfs_rmdir(&ctx->mount, "/keepB");
    test_expect(ts, ret == 0, "phase2: rmdir /keepB should succeed");
    deep_check_layout(ctx, ts, "phase2 cleanup");
    ret = xipfs_statvfs(&ctx->mount, "/", &v2);
    test_expect(ts, ret == 0, "phase2: statvfs after cleanup should succeed");
    if (ret == 0) test_expect(ts, v2.f_bfree >= v1.f_bfree, "phase2: cleanup should increase free blocks");
}

static void deep_phase_saturation(app_ctx_t *ctx, test_state_t *ts) {
    struct xipfs_statvfs v_prev, v_cur;
    int ret;
    int created = 0;
    int removed = 0;
    ret = xipfs_mkdir(&ctx->mount, "/sat", 0);
    test_expect(ts, ret == 0, "phase3: mkdir /sat should succeed");
    deep_check_layout(ctx, ts, "phase3 after mkdir /sat");
    ret = xipfs_statvfs(&ctx->mount, "/", &v_prev);
    test_expect(ts, ret == 0, "phase3: initial statvfs should succeed");
    for (int i = 0; i < 5000; i++) {
        char path[64];
        ret = snprintf(path, sizeof(path), "/sat/f%04d.bin", i);
        if (ret <= 0 || (size_t)ret >= sizeof(path)) {
            test_expect(ts, false, "phase3: snprintf path should succeed");
            break;
        }
        ret = xipfs_new_file(&ctx->mount, path, 0, 0);
        if (ret == 0) {
            created++;
            if ((created % 8) == 0) {
                ret = xipfs_statvfs(&ctx->mount, "/", &v_cur);
                test_expect(ts, ret == 0, "phase3: periodic statvfs should succeed");
                if (ret == 0) {
                    test_expect(ts, v_cur.f_bfree <= v_prev.f_bfree, "phase3: free blocks should not increase during fill");
                    v_prev = v_cur;
                }
            }
            continue;
        }
        if (!(xipfs_errno == XIPFS_ENOSPACE || xipfs_errno == XIPFS_EFULL))
            fprintf(stderr, "phase3: unexpected xipfs_errno=%d (%s)\n", xipfs_errno, xipfs_strerror(xipfs_errno));
        test_expect(ts, xipfs_errno == XIPFS_ENOSPACE || xipfs_errno == XIPFS_EFULL, "phase3: fill should stop only on no-space condition");
        break;
    }
    test_expect(ts, created > 0, "phase3: saturation should create files");
    deep_check_layout(ctx, ts, "phase3 after fill");
    ret = xipfs_statvfs(&ctx->mount, "/", &v_cur);
    test_expect(ts, ret == 0, "phase3: statvfs after fill should succeed");
    if (ret == 0) test_expect(ts, v_cur.f_bfree == 0, "phase3: filesystem should be fully allocated at saturation");
    for (int i = 0; i < created; i += 2) {
        char path[64];
        ret = snprintf(path, sizeof(path), "/sat/f%04d.bin", i);
        if (ret <= 0 || (size_t)ret >= sizeof(path)) continue;
        ret = xipfs_unlink(&ctx->mount, path);
        if (ret == 0) removed++;
    }
    test_expect(ts, removed > 0, "phase3: removing half the files should succeed");
    deep_check_layout(ctx, ts, "phase3 after partial cleanup");
    ret = xipfs_statvfs(&ctx->mount, "/", &v_prev);
    test_expect(ts, ret == 0, "phase3: statvfs after cleanup should succeed");
    if (ret == 0) test_expect(ts, v_prev.f_bfree > 0, "phase3: free blocks should be available after cleanup");
    for (int i = 0; i < removed; i++) {
        char path[64];
        ret = snprintf(path, sizeof(path), "/sat/r%04d.bin", i);
        if (ret <= 0 || (size_t)ret >= sizeof(path)) break;
        ret = xipfs_new_file(&ctx->mount, path, 0, 0);
        if (ret < 0) break;
    }
    deep_check_layout(ctx, ts, "phase3 refill");
}

static void deep_make_path(enum deep_loc_e loc, unsigned idx, char *out, size_t out_sz) {
    const char *dir = NULL;
    switch (loc) {
    case LOC_A: dir = "/dirA"; break;
    case LOC_B: dir = "/dirB"; break;
    case LOC_C: dir = "/dirC"; break;
    default: dir = "/"; break;
    }
    (void)snprintf(out, out_sz, "%s/f%u.bin", dir, idx);
}

int cmd_test_deep(void) {
    test_state_t ts = {0, 0};
    char tmp_template[] = "/tmp/mkxipfs-test-deep-XXXXXX";
    int tmp_fd;
    int ret;
    app_ctx_t ctx;
    enum deep_loc_e loc[8];
    unsigned dir_counts[3] = {0, 0, 0};
    unsigned round;
    tmp_fd = mkstemp(tmp_template);
    test_expect(&ts, tmp_fd >= 0, "setup: mkstemp should succeed");
    if (tmp_fd < 0) {
        fprintf(stderr, "Cannot create temp file for deep tests.\n");
        return 1;
    }
    (void)close(tmp_fd);
    ret = create_image(tmp_template, "128k");
    test_expect(&ts, ret == 0, "setup: create_image should succeed");
    if (ret != 0) {
        (void)unlink(tmp_template);
        return 1;
    }
    ret = open_image(&ctx, tmp_template, true);
    test_expect(&ts, ret == 0, "setup: open_image should succeed");
    if (ret != 0) {
        (void)unlink(tmp_template);
        return 1;
    }
    deep_check_layout(&ctx, &ts, "initial");
    ret = xipfs_mkdir(&ctx.mount, "/dirA", 0);
    test_expect(&ts, ret == 0, "mkdir /dirA should succeed");
    ret = xipfs_mkdir(&ctx.mount, "/dirB", 0);
    test_expect(&ts, ret == 0, "mkdir /dirB should succeed");
    ret = xipfs_mkdir(&ctx.mount, "/dirC", 0);
    test_expect(&ts, ret == 0, "mkdir /dirC should succeed");
    deep_check_layout(&ctx, &ts, "after mkdir dirA/dirB/dirC");
    memset(loc, 0, sizeof(loc));
    for (round = 0; round < 400; round++) {
        unsigned idx = round % 8;
        char src[XIPFS_PATH_MAX];
        char dst[XIPFS_PATH_MAX];
        enum deep_loc_e from = loc[idx];
        enum deep_loc_e to = LOC_NONE;
        unsigned action = round % 6;
        bool op_ok = false;
        deep_entry_t *entries = NULL;
        size_t count = 0;
        if (action == 0 || action == 5) {
            if (from == LOC_NONE) {
                to = (action == 0) ? LOC_A : LOC_B;
                deep_make_path(to, idx, dst, sizeof(dst));
                if (deep_write_small_file(&ctx, dst, round) == 0) {
                    loc[idx] = to;
                    dir_counts[to - 1]++;
                    op_ok = true;
                }
            } else op_ok = true;
        } else if (action == 1 && from == LOC_A) {
            to = LOC_B;
            deep_make_path(from, idx, src, sizeof(src));
            deep_make_path(to, idx, dst, sizeof(dst));
            if (xipfs_rename(&ctx.mount, src, dst) == 0) {
                loc[idx] = to;
                dir_counts[LOC_A - 1]--;
                dir_counts[LOC_B - 1]++;
                op_ok = true;
            }
        } else if (action == 2 && from == LOC_B) {
            to = LOC_C;
            deep_make_path(from, idx, src, sizeof(src));
            deep_make_path(to, idx, dst, sizeof(dst));
            if (xipfs_rename(&ctx.mount, src, dst) == 0) {
                loc[idx] = to;
                dir_counts[LOC_B - 1]--;
                dir_counts[LOC_C - 1]++;
                op_ok = true;
            }
        } else if (action == 3 && from == LOC_C) {
            to = LOC_A;
            deep_make_path(from, idx, src, sizeof(src));
            deep_make_path(to, idx, dst, sizeof(dst));
            if (xipfs_rename(&ctx.mount, src, dst) == 0) {
                loc[idx] = to;
                dir_counts[LOC_C - 1]--;
                dir_counts[LOC_A - 1]++;
                op_ok = true;
            }
        } else if (action == 4 && from != LOC_NONE) {
            deep_make_path(from, idx, src, sizeof(src));
            if (xipfs_unlink(&ctx.mount, src) == 0) {
                dir_counts[from - 1]--;
                loc[idx] = LOC_NONE;
                op_ok = true;
            }
        } else op_ok = true;
        test_expect(&ts, op_ok, "round operation should succeed");
        deep_check_layout(&ctx, &ts, "round");
        if (deep_collect_entries(&ctx, &entries, &count) == 0) {
            bool has_a = has_explicit_dir(entries, count, "/dirA/");
            bool has_b = has_explicit_dir(entries, count, "/dirB/");
            bool has_c = has_explicit_dir(entries, count, "/dirC/");
            test_expect(&ts, has_a == (dir_counts[LOC_A - 1] == 0), "dirA explicit entry should match emptiness");
            test_expect(&ts, has_b == (dir_counts[LOC_B - 1] == 0), "dirB explicit entry should match emptiness");
            test_expect(&ts, has_c == (dir_counts[LOC_C - 1] == 0), "dirC explicit entry should match emptiness");
            free(entries);
        } else test_expect(&ts, false, "failed to collect entries after round");
    }
    deep_phase_rmdir_compaction_and_multiblock(&ctx, &ts);
    deep_phase_rw_append(&ctx, &ts);
    if (xipfs_buffer_flush() < 0) test_expect(&ts, false, "mid flush before remount should succeed");
    close_image(&ctx);
    ret = open_image(&ctx, tmp_template, true);
    test_expect(&ts, ret == 0, "mid remount after phase2 should succeed");
    if (ret == 0) deep_check_layout(&ctx, &ts, "after phase2 remount");
    else {
        (void)unlink(tmp_template);
        return test_report(&ts);
    }
    deep_phase_saturation(&ctx, &ts);
    if (xipfs_buffer_flush() < 0) test_expect(&ts, false, "final flush before close should succeed");
    close_image(&ctx);
    ret = open_image(&ctx, tmp_template, false);
    test_expect(&ts, ret == 0, "final remount should succeed");
    if (ret == 0) {
        deep_check_layout(&ctx, &ts, "after remount");
        close_image(&ctx);
    }
    (void)unlink(tmp_template);
    return test_report(&ts);
}
