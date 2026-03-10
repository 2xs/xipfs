#include "mkxipfs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ls_item_s {
    char *name;
    bool is_dir;
    bool is_exec;
    unsigned int support_kb;
    long long effective;
    unsigned int inode;
} ls_item_t;

static int join_child_path(const char *base, const char *name, char *out, size_t out_sz) {
    size_t blen = strnlen(base, XIPFS_PATH_MAX);
    if (blen == 0 || blen >= out_sz) return -1;
    if (strcmp(base, "/") == 0) {
        if (snprintf(out, out_sz, "/%s", name) >= (int)out_sz) return -1;
    } else {
        if (base[blen - 1] == '/') {
            if (snprintf(out, out_sz, "%s%s", base, name) >= (int)out_sz) return -1;
        } else {
            if (snprintf(out, out_sz, "%s/%s", base, name) >= (int)out_sz) return -1;
        }
    }
    return 0;
}

static void normalize_child_name(const char *name, char *out, size_t out_sz) {
    size_t i = 0;
    size_t nlen = strnlen(name, XIPFS_PATH_MAX);
    while (i < nlen && name[i] == '/') i++;
    if (i >= nlen) {
        (void)snprintf(out, out_sz, "");
        return;
    }
    if (snprintf(out, out_sz, "%s", name + i) >= (int)out_sz) {
        out[0] = '\0';
        return;
    }
    nlen = strnlen(out, out_sz);
    while (nlen > 0 && out[nlen - 1] == '/') {
        out[nlen - 1] = '\0';
        nlen--;
    }
}

static const file_entry_t *find_entry_by_path(const file_entry_t *entries, size_t count, const char *path) {
    size_t i;
    for (i = 0; i < count; i++) {
        const char *ep = entries[i].path;
        if (strcmp(ep, path) == 0) return &entries[i];
        if (path[0] == '/' && strcmp(ep, path + 1) == 0) return &entries[i];
        if (ep[0] == '/' && strcmp(ep + 1, path) == 0) return &entries[i];
    }
    return NULL;
}

static int cmp_ls_items(const void *a, const void *b) {
    const ls_item_t *ia = (const ls_item_t *)a;
    const ls_item_t *ib = (const ls_item_t *)b;
    if (ia->is_dir != ib->is_dir)
        return ia->is_dir ? -1 : 1;
    return strcmp(ia->name, ib->name);
}

static void free_ls_items(ls_item_t *items, size_t count) {
    size_t i;
    if (items == NULL) return;
    for (i = 0; i < count; i++) {
        free(items[i].name);
    }
    free(items);
}

static void print_ls_item(const ls_item_t *item, bool color, bool long_mode) {
    char type_char = item->is_dir ? 'd' : (item->is_exec ? 'x' : '-');
    if (long_mode) {
        if (color && item->is_dir) {
            printf("%s%s%s\n", color_blue(), item->name, color_reset());
        } else if (color && item->is_exec) {
            printf("%s%s%s\n", color_green(), item->name, color_reset());
        } else {
            printf("%s\n", item->name);
        }
        return;
    }
    if (color && item->is_dir) {
        printf("%6uK %10lld %c %09u %s%s%s\n",
               item->support_kb, item->effective, type_char, item->inode,
               color_blue(), item->name, color_reset());
    } else if (color && item->is_exec) {
        printf("%6uK %10lld %c %09u %s%s%s\n",
               item->support_kb, item->effective, type_char, item->inode,
               color_green(), item->name, color_reset());
    } else {
        printf("%6uK %10lld %c %09u %s\n",
               item->support_kb, item->effective, type_char, item->inode, item->name);
    }
}

int cmd_ls(app_ctx_t *ctx, int argc, char **argv) {
    file_entry_t *entries = NULL;
    size_t count = 0;
    char dir_path[XIPFS_PATH_MAX] = "/";
    struct stat st;
    xipfs_dir_desc_t dd;
    xipfs_dirent_t de;
    int ret;
    const bool color = is_stdout_tty();
    char **printed = NULL;
    size_t printed_count = 0;
    size_t printed_cap = 0;
    ls_item_t *items = NULL;
    size_t item_count = 0;
    size_t item_cap = 0;
    const char *target = NULL;
    bool long_mode = false;
    int i;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            long_mode = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s' for ls.\n", argv[i]);
            return 1;
        }
        if (target != NULL) {
            fprintf(stderr, "Error: ls expects at most one optional path.\n");
            return 1;
        }
        target = argv[i];
    }
    if (target != NULL) {
        if (make_xipfs_path(target, dir_path, sizeof(dir_path)) < 0) {
            fprintf(stderr, "Error: invalid target path '%s'.\n", target);
            return 1;
        }
    }
    ret = xipfs_stat(&ctx->mount, dir_path, &st);
    if (ret == 0 && S_ISREG(st.st_mode)) {
        if (collect_entries(ctx, dir_path, &entries, &count) < 0) return 1;
        if (count == 1) {
            const file_entry_t *e = &entries[0];
            ls_item_t only;
            only.name = (char *)e->path;
            only.is_dir = e->is_dir;
            only.is_exec = e->is_exec;
            only.support_kb = e->filp->reserved / 1024U;
            only.effective = (long long)e->size;
            only.inode = (unsigned int)xipfs_ptr_to_off(e->filp);
            print_ls_item(&only, color, long_mode);
        }
        free_entries(entries);
        return 0;
    }
    if (ret < 0) {
        fprintf(stderr, "Error: target '%s' not found (%s).\n", dir_path, strerror(-ret));
        return 1;
    }
    if (xipfs_path_is_dir(dir_path) == false) {
        size_t len = strnlen(dir_path, sizeof(dir_path));
        if (len + 1 >= sizeof(dir_path)) {
            fprintf(stderr, "Error: target path too long.\n");
            return 1;
        }
        dir_path[len] = '/';
        dir_path[len + 1] = '\0';
    }
    if (collect_entries(ctx, dir_path, &entries, &count) < 0) return 1;
    memset(&dd, 0, sizeof(dd));
    ret = xipfs_opendir(&ctx->mount, &dd, dir_path);
    if (ret < 0) {
        free_entries(entries);
        fprintf(stderr, "Error: cannot open directory '%s' (%s).\n", dir_path, strerror(-ret));
        return 1;
    }
    while ((ret = xipfs_readdir(&ctx->mount, &dd, &de)) > 0) {
        size_t pi;
        bool already_printed = false;
        char full_path[XIPFS_PATH_MAX];
        char norm_name[XIPFS_PATH_MAX];
        char dedup_key[XIPFS_PATH_MAX];
        const file_entry_t *entry;
        ls_item_t item;
        size_t nlen;
        normalize_child_name(de.dirname, norm_name, sizeof(norm_name));
        if (norm_name[0] == '\0') continue;
        if (join_child_path(dir_path, norm_name, dedup_key, sizeof(dedup_key)) < 0) continue;
        for (pi = 0; pi < printed_count; pi++) {
            if (strcmp(printed[pi], dedup_key) == 0) {
                already_printed = true;
                break;
            }
        }
        if (already_printed) continue;
        if (printed_count == printed_cap) {
            size_t new_cap = (printed_cap == 0) ? 16 : printed_cap * 2;
            char **new_printed = realloc(printed, new_cap * sizeof(*new_printed));
            if (new_printed == NULL) {
                fprintf(stderr, "Error: out of memory while listing directory.\n");
                (void)xipfs_closedir(&ctx->mount, &dd);
                free_entries(entries);
                free(printed);
                return 1;
            }
            printed = new_printed;
            printed_cap = new_cap;
        }
        printed[printed_count] = strdup(dedup_key);
        if (printed[printed_count] == NULL) {
            fprintf(stderr, "Error: out of memory while listing directory.\n");
            (void)xipfs_closedir(&ctx->mount, &dd);
            free_entries(entries);
            for (pi = 0; pi < printed_count; pi++) {
                free(printed[pi]);
            }
            free(printed);
            return 1;
        }
        printed_count++;
        if (join_child_path(dir_path, norm_name, full_path, sizeof(full_path)) < 0) continue;
        entry = find_entry_by_path(entries, count, full_path);
        if (entry == NULL) {
            char full_path_dir[XIPFS_PATH_MAX];
            size_t flen = strnlen(full_path, sizeof(full_path));
            if (flen + 1 < sizeof(full_path_dir)) {
                memcpy(full_path_dir, full_path, flen + 1);
                if (flen == 0 || full_path_dir[flen - 1] != '/') {
                    full_path_dir[flen] = '/';
                    full_path_dir[flen + 1] = '\0';
                    entry = find_entry_by_path(entries, count, full_path_dir);
                }
            }
        }
        if (entry == NULL) {
            nlen = strnlen(de.dirname, XIPFS_PATH_MAX);
            if (!(nlen > 0 && de.dirname[nlen - 1] == '/')) continue;
            item.is_dir = true;
            item.is_exec = false;
            item.support_kb = 0U;
            item.effective = 0LL;
            item.inode = 0U;
        } else {
            item.is_dir = entry->is_dir;
            item.is_exec = entry->is_exec;
            item.support_kb = entry->filp->reserved / 1024U;
            item.effective = (long long)entry->size;
            item.inode = (unsigned int)xipfs_ptr_to_off(entry->filp);
        }
        if (item_count == item_cap) {
            size_t new_cap = (item_cap == 0) ? 16 : item_cap * 2;
            ls_item_t *new_items = realloc(items, new_cap * sizeof(*new_items));
            if (new_items == NULL) {
                fprintf(stderr, "Error: out of memory while listing directory.\n");
                (void)xipfs_closedir(&ctx->mount, &dd);
                free_entries(entries);
                for (size_t pj = 0; pj < printed_count; pj++) {
                    free(printed[pj]);
                }
                free(printed);
                free_ls_items(items, item_count);
                return 1;
            }
            items = new_items;
            item_cap = new_cap;
        }
        item.name = strdup(norm_name);
        if (item.name == NULL) {
            fprintf(stderr, "Error: out of memory while listing directory.\n");
            (void)xipfs_closedir(&ctx->mount, &dd);
            free_entries(entries);
            for (size_t pj = 0; pj < printed_count; pj++)
                free(printed[pj]);
            free(printed);
            free_ls_items(items, item_count);
            return 1;
        }
        items[item_count++] = item;
    }
    if (ret < 0) {
        fprintf(stderr, "Error: readdir failed (%s).\n", strerror(-ret));
        (void)xipfs_closedir(&ctx->mount, &dd);
        free_entries(entries);
        for (size_t pi = 0; pi < printed_count; pi++) {
            free(printed[pi]);
        }
        free(printed);
        free_ls_items(items, item_count);
        return 1;
    }
    (void)xipfs_closedir(&ctx->mount, &dd);
    free_entries(entries);
    for (size_t pi = 0; pi < printed_count; pi++) free(printed[pi]);
    free(printed);
    if (item_count > 1)
        qsort(items, item_count, sizeof(*items), cmp_ls_items);
    for (size_t k = 0; k < item_count; k++)
        print_ls_item(&items[k], color, long_mode);
    free_ls_items(items, item_count);
    return 0;
}
