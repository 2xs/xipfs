#ifndef MKXIPFS_H
#define MKXIPFS_H

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

#include "include/xipfs.h"

typedef struct app_ctx_s {
    int fd;
    void *mapping;
    size_t image_size;
    bool writable;
    xipfs_mount_t mount;
} app_ctx_t;

typedef struct file_entry_s {
    const xipfs_file_t *filp;
    const char *path;
    size_t path_len;
    off_t size;
    bool is_dir;
    bool is_exec;
} file_entry_t;

extern uintptr_t xipfs_workstation_nvm_base;
extern size_t xipfs_workstation_nvm_numof;
extern uint8_t xipfs_workstation_nvm_erase_state;
extern uintptr_t xipfs_workstation_nvm_write_block_alignment;
extern size_t xipfs_workstation_nvm_write_block_size;
extern size_t xipfs_workstation_nvm_page_size;

void usage(const char *prog);

int parse_size(const char *raw, size_t *out_size);
int ask_alignment(size_t initial_size, size_t *aligned_size);
const char *resolve_flash_image(const char *flash_opt);
const char *resolve_target(const char *target_opt);
void print_target_names(void);
void print_target_configuration(const char *target_name);
void print_current_target_configuration(void);
int set_target_nvm_configuration(const char *target_opt);
int make_xipfs_path(const char *input, char *out, size_t out_sz);
int open_image(app_ctx_t *ctx, const char *flash_path, bool writable);
void close_image(app_ctx_t *ctx);
int create_image(const char *flash_path, const char *size_raw);

int read_all_fd(int fd, unsigned char **buf_out, size_t *size_out);
int write_all_fd(int fd, const unsigned char *buf, size_t size);

int collect_entries(app_ctx_t *ctx, const char *target, file_entry_t **entries_out, size_t *count_out);
void free_entries(file_entry_t *entries);
int cmp_entries(const void *a, const void *b);

bool is_stdout_tty(void);
const char *color_blue(void);
const char *color_green(void);
const char *color_reset(void);

bool xipfs_path_is_dir(const char *path);
bool is_fae_payload(const unsigned char *buf, size_t size);
int host_file_is_fae(const char *host_path, bool *is_fae);

int cmd_create(int argc, char **argv);
int cmd_build(int argc, char **argv);
int cmd_ls(app_ctx_t *ctx, int argc, char **argv);
int cmd_tree(app_ctx_t *ctx, int argc, char **argv);
int cmd_put(app_ctx_t *ctx, int argc, char **argv);
int cmd_get(app_ctx_t *ctx, int argc, char **argv);
int cmd_mkdir(app_ctx_t *ctx, int argc, char **argv);
int cmd_rm(app_ctx_t *ctx, int argc, char **argv);
int cmd_rmdir(app_ctx_t *ctx, int argc, char **argv);
int cmd_mv(app_ctx_t *ctx, int argc, char **argv);
int cmd_test(void);
int cmd_test_deep(void);
int cmd_test_build(void);

#endif
