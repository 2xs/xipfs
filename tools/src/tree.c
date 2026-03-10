#include "mkxipfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct tree_node_s {
    char *name;
    bool is_file;
    bool is_exec;
    struct tree_node_s **children;
    size_t child_count;
    size_t child_cap;
} tree_node_t;

static tree_node_t *tree_node_new(const char *name, bool is_file, bool is_exec) {
    tree_node_t *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->name = strdup(name);
    if (node->name == NULL) {
        free(node);
        return NULL;
    }
    node->is_file = is_file;
    node->is_exec = is_exec;
    return node;
}

static void tree_node_free(tree_node_t *node) {
    size_t i;
    if (node == NULL) return;
    for (i = 0; i < node->child_count; i++)
        tree_node_free(node->children[i]);
    free(node->children);
    free(node->name);
    free(node);
}

static int tree_node_find_child(tree_node_t *node, const char *name) {
    for (size_t i = 0; i < node->child_count; i++) {
        if (strcmp(node->children[i]->name, name) == 0) return i;
    }
    return -1;
}

static int tree_node_add_child(tree_node_t *parent, tree_node_t *child) {
    if (parent->child_count == parent->child_cap) {
        size_t new_cap = (parent->child_cap == 0) ? 8 : parent->child_cap * 2;
        tree_node_t **new_children = realloc(parent->children, new_cap * sizeof(*new_children));
        if (new_children == NULL) return -1;
        parent->children = new_children;
        parent->child_cap = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return 0;
}

static int cmp_tree_children(const void *a, const void *b) {
    const tree_node_t *na = *(const tree_node_t *const *)a;
    const tree_node_t *nb = *(const tree_node_t *const *)b;
    if (na->is_file != nb->is_file)
        return na->is_file ? 1 : -1;
    return strcmp(na->name, nb->name);
}

static int tree_insert_path(tree_node_t *root, const char *rel_path, bool is_exec) {
    char tmp[XIPFS_PATH_MAX];
    char *saveptr = NULL;
    char *token;
    tree_node_t *cur = root;
    size_t len = strnlen(rel_path, sizeof(tmp));
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, rel_path, len + 1);
    token = strtok_r(tmp, "/", &saveptr);
    while (token != NULL) {
        char *next = strtok_r(NULL, "/", &saveptr);
        bool is_file = (next == NULL);
        int idx = tree_node_find_child(cur, token);
        if (idx < 0) {
            tree_node_t *child = tree_node_new(token, is_file, (is_file && is_exec));
            if (child == NULL) return -1;
            if (tree_node_add_child(cur, child) < 0) {
                tree_node_free(child);
                return -1;
            }
            idx = (int)cur->child_count - 1;
        }
        cur = cur->children[idx];
        if (is_file) {
            cur->is_file = true;
            cur->is_exec = is_exec;
        }
        token = next;
    }
    return 0;
}

static void tree_sort(tree_node_t *node) {
    if (node->child_count > 1)
        qsort(node->children, node->child_count, sizeof(*node->children), cmp_tree_children);
    for (size_t i = 0; i < node->child_count; i++)
        tree_sort(node->children[i]);
}

static void print_name(const tree_node_t *node, const bool color) {
    if (color && !node->is_file) 
        printf("%s%s%s", color_blue(), node->name, color_reset());
    else if (color && node->is_exec) 
        printf("%s%s%s", color_green(), node->name, color_reset());
    else 
        printf("%s", node->name);
}

static void tree_print_rec(const tree_node_t *node, const char *prefix, const bool color) {
    size_t i;
    for (i = 0; i < node->child_count; i++) {
        const tree_node_t *child = node->children[i];
        bool last = (i + 1 == node->child_count);
        printf("%s%s", prefix, last ? "┗━ " : "┣━ ");
        print_name(child, color);
        printf("\n");
        if (!child->is_file) {
            char next_prefix[512];
            size_t p_len = strnlen(prefix, sizeof(next_prefix));
            const char *suffix = last ? "   " : "┃  ";
            size_t s_len = strlen(suffix);
            if (p_len + s_len + 1 < sizeof(next_prefix)) {
                memcpy(next_prefix, prefix, p_len);
                memcpy(next_prefix + p_len, suffix, s_len + 1);
                tree_print_rec(child, next_prefix, color);
            }
        }
    }
}

int cmd_tree(app_ctx_t *ctx, int argc, char **argv) {
    file_entry_t *entries = NULL;
    size_t count = 0;
    size_t i;
    char base[XIPFS_PATH_MAX] = "/";
    size_t shown = 0;
    tree_node_t *root = NULL;
    struct stat st;
    char file_target[XIPFS_PATH_MAX];
    const bool color = is_stdout_tty();
    if (argc > 1) {
        fprintf(stderr, "Error: tree expects at most one optional path.\n");
        return 1;
    }
    if (argc == 1) {
        if (make_xipfs_path(argv[0], base, sizeof(base)) < 0) {
            fprintf(stderr, "Error: invalid target path '%s'.\n", argv[0]);
            return 1;
        }
        if (make_xipfs_path(argv[0], file_target, sizeof(file_target)) == 0 &&
            xipfs_stat(&ctx->mount, file_target, &st) == 0 &&
            S_ISREG(st.st_mode)) {
            if (color)
                printf("%s%s%s\n\n1 entry\n", color_green(), file_target, color_reset());
            else
                printf("%s\n\n1 entry\n", file_target);
            return 0;
        }
    }
    if (collect_entries(ctx, (argc == 1) ? argv[0] : NULL, &entries, &count) < 0) return 1;
    root = tree_node_new(base, false, false);
    if (root == NULL) {
        perror("calloc");
        free_entries(entries);
        return 1;
    }
    for (i = 0; i < count; i++) {
        const char *path = entries[i].path;
        const char *rel = path;
        if (strcmp(base, "/") != 0) {
            size_t blen = strnlen(base, sizeof(base));
            if (strncmp(path, base, blen) == 0) {
                rel = path + blen;
            }
        } else if (path[0] == '/') {
            rel = path + 1;
        }
        if (rel[0] == '\0')
            rel = path[0] == '/' ? path + 1 : path;
        if (entries[i].is_dir && rel[strnlen(rel, XIPFS_PATH_MAX) - 1] == '/') {
            char tmp[XIPFS_PATH_MAX];
            size_t len = strnlen(rel, sizeof(tmp));
            if (len >= sizeof(tmp))
                continue;
            memcpy(tmp, rel, len + 1);
            if (len > 0)
                tmp[len - 1] = '\0';
            if (tmp[0] != '\0' && tree_insert_path(root, tmp, false) < 0) {
                fprintf(stderr, "Error: failed to build tree.\n");
                tree_node_free(root);
                free_entries(entries);
                return 1;
            }
        } else {
            if (tree_insert_path(root, rel, entries[i].is_exec) < 0) {
                fprintf(stderr, "Error: failed to build tree.\n");
                tree_node_free(root);
                free_entries(entries);
                return 1;
            }
        }
        shown++;
    }
    tree_sort(root);
    if (color)
        printf("%s%s%s\n", color_blue(), root->name, color_reset());
    else
        printf("%s\n", root->name);
    tree_print_rec(root, "", color);
    printf("\n%zu entr%s\n", shown, (shown == 1) ? "y" : "ies");
    tree_node_free(root);
    free_entries(entries);
    return 0;
}
